#ifndef LOO_QUEUE_HPP
#define LOO_QUEUE_HPP

#include <stdexcept>

#include "looqueue/detail/node.hpp"
#include "looqueue/queue_fwd.hpp"

namespace loo {
template <typename T>
queue<T>::queue()
{
	// Initially, head and tail point at the same node.
	const auto head = new node_t();
	this->m_head.store(reinterpret_cast<slot_t>(head), relaxed);
	this->m_tail.store(reinterpret_cast<slot_t>(head), relaxed);
	this->m_curr_tail.store(head, relaxed);
}

template <typename T>
queue<T>::~queue() noexcept
{
	// Deallocate all remaining nodes in the queue.
	auto curr = marked_ptr_t(this->m_head.load(relaxed)).decompose_ptr();
	while (curr != nullptr) {
		auto next = curr->next.load(relaxed);
		delete curr;
		curr = next;
	}
}

template <typename T>
void
queue<T>::enqueue(queue::pointer elem)
{
	// Validate `elem` argument (must not be null and 4 byte aligned so it
	// can store 2 bits).
	if (elem == nullptr) [[unlikely]] {
		throw std::invalid_argument("enqueue element must not be null");
	}

	while (true) {
		// Increment the enqueue index, retrieve the tail pointer and
		// previous index value.
		// See PROOF.md regarding the (im)possibility of overflows.
		const auto curr = marked_ptr_t(this->m_tail.fetch_add(1, acquire));
		const auto [tail, idx] = curr.decompose();

		if (idx < NODE_SIZE) [[likely]] {
			// ** fast path **: Write access to the slot at tail.idx was
			// exclusively reserved write the `elem` bits into the slot
			// (exclusive access ensures this is done exactly once).
			const auto bits = reinterpret_cast<slot_t>(elem);
			// HACK(x86): We use FAA here because FOR is compiled as CAS when setting
			// more than one bit and returning the enitre memory word. Since only one
			// operation ever writes to the higher bits of a slot, FAA and FOR are
			// logically equivalent here.
			const auto slot = tail->slots[idx].fetch_add(bits, release);
			if (slot <= node_t::slot_flags_t::RESUME) [[likely]] {
				// No READ bit was set; RESUME may or may not be set - the element was
				// successfully inserted, regardless. If the RESUME bit was set, the
				// corresponding dequeue operation will act accordingly, once it
				// modifies the slot.
				return;
			} else if (node_t::is_abandoned(slot)) {
				// READ and RESUME are set, so this must be the final operation visiting
				// this slot hence the slot must be abandoned (dequeue finished too
				// early) and `try_reclaim` must be resumed
				tail->try_reclaim(idx + 1);
			}

			// only the READ bit is set so the slot must be abandoned and both
			// operations must retry on another slot
			continue;
		} else {
			// ** slow path ** no free slot is available in this node, so a new node
			// has to be appended that attempts to directly insert `elem` in the newly
			// appended node's first slot and the enqueue procedure is completed on
			// success; in any case `tail` points at some successor node when this
			// sub-procedure completes
			switch (this->try_advance_tail(elem, tail)) {
			case detail::advance_tail_res_t::ADVANCED_AND_INSERTED:
				return;
			case detail::advance_tail_res_t::ADVANCED:
				continue;
			}
		}
	}
}

template <typename T>
typename queue<T>::pointer
queue<T>::dequeue()
{
	while (true) {
		// check if the queue is empty
		if (this->is_empty()) {
			return nullptr;
		}

		// increment the dequeue index, retrieve the head pointer and previous index
		// value see PROOF.md regarding the (im)possibility of overflows
		const auto curr = marked_ptr_t(this->m_head.fetch_add(1, acquire));
		const auto [head, idx] = curr.decompose();

		if (idx < NODE_SIZE) [[likely]] {
			// ** fast path ** read access to the slot at tail.idx was uniquely
			// reserved set the READ bit in the slot (unique access ensures this is
			// done exactly once)
			const auto state
				= head->slots[idx].fetch_add(node_t::slot_flags_t::READER, acquire);
			// extract the pointer bits from the retrieved value
			const auto res
				= reinterpret_cast<pointer>(state & node_t::slot_flags_t::ELEM_MASK);

			// check the extracted pointer bits, if the result is null, the deque
			// thread must have set the READ bit before the pointer bits have been set
			// by the corresponding enqueue operation, yet
			if (res != nullptr) [[likely]] {
				if ((state & node_t::slot_flags_t::RESUME) != 0) [[unlikely]] {
					head->try_reclaim(idx + 1);
				}

				return res;
			}

			// the slot must be abandoned
			continue;
		} else {
			// ** slow path ** the current head node has been fully consumed and must
			// be replaced by its successor, if there is one
			switch (this->try_advance_head(curr, head, idx)) {
			case detail::advance_head_res_t::ADVANCED:
				continue;
			case detail::advance_head_res_t::QUEUE_EMPTY:
				return nullptr;
			}
		}
	}
}

template <typename T>
bool
queue<T>::bounded_cas_loop(queue::atomic_node_tag_ptr_t &node,
	queue::marked_ptr_t &expected, queue::marked_ptr_t desired,
	const queue::node_t *old_node, std::memory_order order)
{
	// Attempt to exchange the expected (pointer, tag) pair with the desired pair,
	// the expected value is updated after each unsuccessful invocation so it
	// always contains the latest observed index value.
	while (!node.compare_exchange_weak(expected.as_uintptr(),
		desired.to_uintptr(), order, relaxed)) {
		// If the CAS failed but the pointer value no longer matches the previous
		// one, another thread must have updated the pointer, rather than just
		// incremented the index.
		if (expected.decompose_ptr() != old_node)
			return false;
	}

	return true;
}

template <typename T>
bool
queue<T>::is_empty() noexcept
{
	// We use an RMW operation here that does not modify the value in order to
	// retrieve its value. This acquires ownership of the cache-line.
	// The subsequent FAA in deque (a few instructions later) then has a higher
	// chance of still having ownership, in which case the FAA can retire
	// much faster.
	const auto curr_head = this->m_head.fetch_add(0, relaxed);
	const auto [head, deq_idx] = marked_ptr_t { curr_head }.decompose();

	// Load the cached tail since it hass less contention than the actual tail.
	auto cached_tail = this->m_curr_tail.load(relaxed);
	if (head != cached_tail)
		return false;

	const curr_tail = this->m_tail.load(relaxed);
	const auto [tail, enq_idx] = marked_ptr_t { curr_tail }.decompose();

	// If the cached tail is lagging behind, update the cached value.
	if (cached_tail != tail) {
		this->m_curr_tail.compare_exchange_strong(cached_tail, tail, relaxed,
			relaxed);
	}

	return (head == tail && (deq_idx >= NODE_SIZE || enq_idx <= deq_idx));
}

template <typename T>
detail::advance_head_res_t
queue<T>::try_advance_head(queue::marked_ptr_t curr, queue::node_t *const head,
	std::size_t idx)
{
	// the first slow-path operation initiates the reclamation checks for the
	// current node, which ensures the procedure is most likely to succeed on the
	// first attempt since all previous enqueue and dequeue operations must have
	// already been initiated (but not necessarily completed)
	if (idx == NODE_SIZE) {
		// FIXME: makes no sense to do this here, can never observe ::DEQ flag!
		head->try_reclaim(0);
	}

	if (head == marked_ptr_t { this->m_tail.load(acquire) }.decompose_ptr()) {
		// if the tail has not yet been updated, the head must not be advanced ahead
		// of it, even if there already is a new node installed through the next
		// pointer
		head->increment_dequeue_count();
		return detail::advance_head_res_t::QUEUE_EMPTY;
	}

	// load the current head's next pointer, which must have been set BEFORE
	// updating the tail
	const auto next = head->next.load(acquire);

	// attempt to exchange the current head node (with its last observed index)
	// with the next node
	curr.inc_idx();
	if (bounded_cas_loop(this->m_head, curr, marked_ptr_t(next, 0), head,
				release)) {
		// the current thread succeeded in exchanging the node and is hence the
		// operation that observed the final index value (count) of a all dequeue
		// operations accessing this node
		head->increment_dequeue_count(curr.decompose_tag() - NODE_SIZE);
	} else {
		// some other node succeeded in exchanging the head and the operation is
		// also complete
		head->increment_dequeue_count();
	}

	return detail::advance_head_res_t::ADVANCED;
}

template <typename T>
detail::advance_tail_res_t
queue<T>::try_advance_tail(queue::pointer elem, queue::node_t *const tail)
{
	std::uint64_t final_count = 0;
	// re-load the tail pointer to check if it has already been advanced
	auto curr = marked_ptr_t(this->m_tail.load(relaxed));

	// another thread has already advanced the tail pointer, so this thread can
	// retry and will likely enter the fast-path
	if (tail != curr.decompose_ptr()) {
		tail->increment_enqueue_count();
		return detail::advance_tail_res_t::ADVANCED;
	}

	// load the current tail's next pointer to check if another thread has already
	// appended a new node to the queue but has not yet updated the tail pointer
	auto next = tail->next.load(relaxed);
	if (next == nullptr) {
		// there is no new node yet, allocate a new one and attempt to append it
		auto node = new node_t(elem);
		auto advanced = detail::advance_tail_res_t::ADVANCED;
		const auto res
			= tail->next.compare_exchange_strong(next, node, release, relaxed);
		if (res) {
			// the CAS succeeded in appending the node after the tail, now the tail
			// has to be updated
			if (bounded_cas_loop(this->m_tail, curr, marked_ptr_t(node, 1), tail,
						release)) {
				final_count = curr.decompose_tag() - NODE_SIZE;
			}

			// it doesn't matter, which thread succeeded in updating the tail, since
			// all must attempt to set it to previous tail's next pointer, which was
			// set by this thread and contains `elem`
			next = node;
			advanced = detail::advance_tail_res_t::ADVANCED_AND_INSERTED;
		} else {
			if (bounded_cas_loop(this->m_tail, curr, marked_ptr_t(next, 1), tail,
						release)) {
				final_count = curr.decompose_tag() - NODE_SIZE;
			}
		}

		// update the cached tail pointer
		auto expected = tail;
		this->m_curr_tail.compare_exchange_strong(expected, next, release, relaxed);
		// conclude the operation by increasing the enqueue count to allow
		// reclamation
		tail->increment_enqueue_count(final_count);

		if (!res) {
			// the CAS failed so another thread must have succeeded in appending a
			// node, delete the node allocated by this thread and try again
			delete node;
		}

		return advanced;
	} else {
		// there is already a new node after the current tail, so this thread has to
		// help updating the queue's tail pointer and retry once the tail has been
		// advanced
		if (bounded_cas_loop(this->m_tail, curr, marked_ptr_t(next, 1), tail,
					release)) {
			final_count = curr.decompose_tag() - NODE_SIZE;
		}

		// update the cached tail pointer
		auto expected = tail;
		this->m_curr_tail.compare_exchange_strong(expected, next, release, relaxed);
		// conclude the operation by increasing the enqueue count to allow
		// reclamation
		tail->increment_enqueue_count(final_count);

		return detail::advance_tail_res_t::ADVANCED;
	}
}
} // namespace loo

#endif /* LOO_QUEUE_HPP */
