#ifndef LOO_QUEUE_HPP
#define LOO_QUEUE_HPP

#include <atomic>
#include <stdexcept>

#include "looqueue/detail/node.hpp"
#include "looqueue/queue_fwd.hpp"

namespace loo {
template <typename T>
queue<T>::queue()
{
	using slot = detail::slot::slot;

	// Initially, head and tail point at the same node.
	const auto head = new node();
	this->head.store(reinterpret_cast<slot>(head), relaxed);
	this->tail.store(reinterpret_cast<slot>(head), relaxed);
	this->cached_tail.store(head, relaxed);
}

template <typename T>
queue<T>::~queue() noexcept
{
	// Deallocate all remaining nodes in the queue.
	auto curr = tag_ptr { this->head.load(relaxed) }.decompose_ptr();
	while (curr != nullptr) {
		auto next = curr->next.load(relaxed);
		delete curr;
		curr = next;
	}
}

template <typename T>
void queue<T>::enqueue(pointer elem)
{
	// Validate `elem` argument (must not be null and 4 byte aligned so it
	// can store 2 bits).
	if (elem == nullptr) [[unlikely]]
		throw std::invalid_argument("enqueue element must not be null");

	while (true) {
		// Increment the enqueue index, retrieve the tail pointer and
		// previous index value.
		// See PROOF.md regarding the (im)possibility of overflows.
		const tag_ptr tag_tail { this->tail.fetch_add(tag_ptr::one, acquire) };
		const auto [tail, idx] = tag_tail.decompose();

		if (idx < node_size) [[likely]] {
			// ** fast path **: Write access to the slot at tail.idx was
			// exclusively reserved by the previous FAA.
			// Write the `elem` bits into the slot (exclusive access ensures
			// this is done exactly once).
			const auto bits = reinterpret_cast<detail::slot::slot>(elem);
			// HACK(x86): We use FAA here because FOR is compiled as CAS when
			// setting more than one bit and/or returning the enitre memory
			// word. Since only one operation ever writes to the higher bits of
			// a slot, FAA and FOR are logically equivalent here.
			const auto slot = tail->slots[idx].fetch_add(bits, release);
			if (slot <= detail::slot::flags::resume) [[likely]]
				// No READ bit was set; RESUME may or may not be set - the
				// element was successfully inserted, regardless. If the RESUME
				// bit was set, the corresponding dequeue operation will act
				// accordingly, once it modifies the slot.
				return;

			// Otherwise, the READ bit is set and the slot must be abandoned,
			// i.e., the dequeue operation finished before the enqueue operation
			// for the same slot. In this case, both operations must retry on
			// another slot.
			//
			// If the READ and RESUME flags are both set, this must be the final
			// operation visiting this slot. In that case, the operation must
			// resume the reclamation procedure, which may result in deleting
			// the node.
			if (detail::slot::is_abandoned(slot))
				tail->try_reclaim(idx + 1);

			continue;
		}

		// ** slow path ** no free slot is available in this node, so a new node
		// has to be appended that attempts to directly insert `elem` in the
		// newly appended node's first slot and the enqueue procedure is
		// completed on success; in any case `tail` points at some successor
		// node when this function completes.
		switch (this->try_advance_tail(tag_tail + 1, tail, elem)) {
		case detail::advance_tail::advanced_and_inserted:
			return;
		case detail::advance_tail::advanced:
			continue;
		}
	}
}

template <typename T>
typename queue<T>::pointer queue<T>::dequeue()
{
	while (true) {
		// Check, if the queue is empty.
		if (this->is_empty())
			return nullptr;

		// Increment the dequeue index, retrieve the head pointer and previous
		// index value.
		// See PROOF.md regarding the (im)possibility of overflows.
		const tag_ptr tag_head { this->head.fetch_add(tag_ptr::one, acquire) };
		const auto [head, idx] = tag_head.decompose();

		if (idx < node_size) [[likely]] {
			// ** fast path **: Read access to the slot at tail.idx was uniquely
			// reserved by the previous FAA.
			// Set the READ bit in the slot and retrieve the `elem` bits (unique
			// access ensures this is done exactly once).
			const auto res = head->consume_slot(idx);
			if (res == nullptr) [[unlikely]]
				continue;

			return res;
		}

		// ** slow path ** The current head node has been fully
		// consumed (i.e., dequeue operations for every slot have at least
		// started and incremented the dequeue index) and must be replaced
		// by its successor, if there already is one.
		const auto verify = idx == node_size;
		switch (this->try_advance_head(tag_head + 1, head, verify)) {
		case detail::advance_head::advanced:
			continue;
		case detail::advance_head::empty_queue:
			return nullptr;
		}
	}
}

template <typename T>
bool queue<T>::is_empty() noexcept
{
	while (true) {
		// We use an RMW operation here that does not modify the value in order
		// to retrieve its value. This acquires ownership of the cache-line.
		// The subsequent FAA in deque (a few instructions later) then has a
		// higher chance of still having ownership, in which case the FAA can
		// retire much faster.
		const auto head_epoch = this->head_epoch.fetch_add(0, acquire);
		const auto tag_head = tag_ptr { this->head.load(acquire) };
		const auto [head, deq_idx] = tag_head.decompose();

#ifdef LOO_QUEUE_TEST_HEAD_SNAPSHOT
		LOO_QUEUE_TEST_HEAD_SNAPSHOT(head, deq_idx, node_size);
#endif /* LOO_QUEUE_TEST_HEAD_SNAPSHOT */

		// Load the cached tail since it hass less contention than the
		// actual tail.
		// The cached tail is allowed to lag behind the actual tail, it is only
		// used as a means to avoid having to read the highly contended tail
		// variable, in cases where there is more than one node in the list.
		const auto cached = this->cached_tail.load(acquire);
		if (head != cached) {
			if (this->head_epoch.load(relaxed) != head_epoch) [[unlikely]]
				continue;

			return false;
		}

		const auto tag_tail = this->tail.load(acquire);
		const auto [tail, enq_idx] = tag_ptr { tag_tail }.decompose();

#ifdef LOO_QUEUE_TEST_TAIL_SNAPSHOT
		LOO_QUEUE_TEST_TAIL_SNAPSHOT(tail, enq_idx);
#endif /* LOO_QUEUE_TEST_TAIL_SNAPSHOT */

		if (this->head_epoch.load(acquire) != head_epoch) [[unlikely]]
			continue;

		return head == tail && (deq_idx >= node_size || enq_idx <= deq_idx);
	}
}

template <typename T>
detail::advance_tail queue<T>::try_advance_tail(tag_ptr tag_tail, node *tail,
	pointer elem)
{
	/* The RAII guard for ensuring node reference counting and reclamation. */
	struct reclaimer {
		node *ptr;
		u16 total_count = 0;

		// Whenever the parent function returns, we must update the node's
		// count of concluded operations and possibly reclaim the node.
		~reclaimer() noexcept
		{
			if (this->ptr->increment_enqueue_count(total_count))
				this->ptr->free(false);
		}
	};

	auto guard = reclaimer { tail };

	// Optimistically allocate a new node and try to append it to the last known
	// tail node.
	// This may fail if another enqueue thread manages to succeed here
	// before us.
	// In this case we must free the allocated node which is cheap because it is
	// simply cached in thread-local storage.
	const auto new_node = node::alloc();
	new_node->init(elem);

	auto advanced = detail::advance_tail::advanced;
	node *next = nullptr;
	const auto success =
		tail->next.compare_exchange_strong(next, new_node, acq_rel, acquire);
	if (success) {
		next = new_node;
		advanced = detail::advance_tail::advanced_and_inserted;
	} else
		new_node->free(true);

	// Now advance the queue's tail pointer to either our allocated and inserted
	// node or the actual next node observed during the previous CAS.
	// If we succeed, we have conclusively observed the final reference to the
	// previous tail node and therefore now know the total count of all enqueue
	// operations that may ever have held a live reference to this node and we
	// know that no future references are possible.
	// This total count must be entered into the node's control block, as it
	// represents the number of operations that must be guaranteed to have
	// concluded and are no longer accessing the node (which they denote by
	// increasing the respective counter in the control block).
	// The operation that suceeds here does *both* atomically: it increases the
	// counter of concluded operations and enters the total count of all
	// operations that need to be accounted for eventually.
	if (this->cas_tail(tag_tail, tag_ptr { next, 1 }, tail))
		guard.total_count = tag_tail.decompose_tag() - node_size;

	// Finally, try update the cached tail pointer.
	// We don't care, which thread succeeds in advancing the cached tail.
	this->cas_cached_tail(tail, next);

	return advanced;
}

template <typename T>
detail::advance_head queue<T>::try_advance_head(tag_ptr tag_head, node *head,
	bool verify) noexcept
{
	struct reclaimer {
		node *ptr;
		bool verify;
		u16 total_count = 0;

		~reclaimer() noexcept
		{
			const auto is_verified =
				verify ? this->ptr->verify_slots_consumed(0) : false;
			if (this->ptr->increment_dequeue_count(is_verified, total_count))
				this->ptr->free(false);
		}
	};

	reclaimer guard { head, verify };

	// We must make sure to not advance the head before the tail, even if the
	// next tail node is already published, as this would break the empty check.
	const tag_ptr tag_tail { this->tail.load(acquire) };
	const auto tail = tag_tail.decompose_ptr();

	if (head == tail)
		return detail::advance_head::empty_queue;

	// ...
	this->cas_cached_tail(head, tail);

	// The the head's next pointer, which was set before the tail was updated.
	const auto next = head->next.load(acquire);

	// Attempt to advance the head. If we succeed, set the total dequeue count
	// in the node's control block on return.
	// On success, increment the head epoch in order to prevent ABA in the
	// empty check.
	if (this->cas_head(tag_head, tag_ptr { next, 0 }, head)) {
		this->head_epoch.fetch_add(1, release);
		guard.total_count = tag_head.decompose_tag() - node_size;
	}

	return detail::advance_head::advanced;
}

template <typename T>
bool queue<T>::bounded_cas_loop(atomic_node_tag_ptr &ptr, tag_ptr &expected,
	tag_ptr desired, const node *old_node, std::memory_order order)
{
	// Attempt to exchange the expected (pointer, tag) pair with the desired
	// pair, the expected value is updated after each unsuccessful invocation
	// so it always contains the latest observed index value.
	auto &exp = expected.as_uintptr();
	const auto des = desired.to_uintptr();

	while (!ptr.compare_exchange_weak(exp, des, order, relaxed)) {
		// If the CAS failed but the pointer value no longer matches the
		// previous one, another thread must have updated the pointer, rather
		// than just incremented the index.
		if (expected.decompose_ptr() != old_node)
			return false;
	}

	return true;
}
} // namespace loo

#endif /* LOO_QUEUE_HPP */
