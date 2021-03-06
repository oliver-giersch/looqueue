#ifndef LOO_QUEUE_HPP
#define LOO_QUEUE_HPP

#include <stdexcept>

#include "looqueue/queue_fwd.hpp"
#include "looqueue/detail/node.hpp"

namespace loo {
template <typename T>
queue<T>::queue() {
  // initially head and tail point at the same node
  auto head = new(std::align_val_t{ NODE_ALIGN }) node_t();
  this->m_head.store(reinterpret_cast<slot_t>(head), relaxed);
  this->m_tail.store(reinterpret_cast<slot_t>(head), relaxed);
  this->m_curr_tail.store(head, relaxed);
}

template <typename T>
queue<T>::~queue() noexcept {
  // de-allocate all remaining nodes in the queue
  auto curr = marked_ptr_t(this->m_head.load(relaxed)).decompose_ptr();
  while (curr != nullptr) {
    auto next = curr->next.load(relaxed);
    delete curr;
    curr = next;
  }
}

template <typename T>
void queue<T>::enqueue(queue::pointer elem) {
  // validate `elem` argument (must not be null and 4 byte aligned so it can store 2 bits)
  if (elem == nullptr) [[unlikely]] {
    throw std::invalid_argument("enqueue element must not be null");
  }

  while (true) {
    // increment the enqueue index, retrieve the tail pointer and previous index value
    // see PROOF.md regarding the (im)possibility of overflows
    const auto curr = marked_ptr_t(this->m_tail.fetch_add(1, acquire));
    const auto [tail, idx] = curr.decompose();

    if (idx < NODE_SIZE) [[likely]]  {
      // ** fast path ** write access to the slot at tail.idx was uniquely reserved write the `elem`
      // bits into the slot (unique access ensures this is done exactly once)
      const auto state = tail->slots[idx].fetch_add(reinterpret_cast<slot_t>(elem), release);
      if (state <= node_t::slot_flags_t::RESUME) [[likely]] {
        // no READ bit is set, RESUME may or may not be set - the element was successfully inserted
        // if the RESUME bit is set, the corresponding dequeue operation will act accordingly.
        return;
      } else if (state == (node_t::slot_flags_t::READER | node_t::slot_flags_t::RESUME)) {
        // READ and RESUME are set, so this must be the final operation visiting this slot hence the
        // slot must be abandoned (dequeue finished too early) and `try_reclaim` must be resumed
        tail->try_reclaim(idx + 1);
      }

      // only the READ bit is set so the slot must be abandoned and both operations must retry on
      // another slot
      continue;
    } else {
      // ** slow path ** no free slot is available in this node, so a new node has to be appended
      // that attempts to directly insert `elem` in the newly appended node's first slot and the
      // enqueue procedure is completed on success; in any case `tail` points at some successor node
      // when this sub-procedure completes
      switch (this->try_advance_tail(elem, tail)) {
        case detail::advance_tail_res_t::ADVANCED_AND_INSERTED: return;
        case detail::advance_tail_res_t::ADVANCED: continue;
      }
    }
  }
}

template <typename T>
typename queue<T>::pointer queue<T>::dequeue() {
  while (true) {
    // check if the queue is empty
    if (this->is_empty()) {
      return nullptr;
    }

    // increment the dequeue index, retrieve the head pointer and previous index value see PROOF.md
    // regarding the (im)possibility of overflows
    const auto curr = marked_ptr_t(this->m_head.fetch_add(1, acquire));
    const auto [head, idx] = curr.decompose();

    if (idx < NODE_SIZE) [[likely]] {
      // ** fast path ** read access to the slot at tail.idx was uniquely reserved
      // set the READ bit in the slot (unique access ensures this is done exactly once)
      const auto state = head->slots[idx].fetch_add(node_t::slot_flags_t::READER, acquire);
      // extract the pointer bits from the retrieved value
      const auto res = reinterpret_cast<pointer>(state & node_t::slot_flags_t::ELEM_MASK);

      // check the extracted pointer bits, if the result is null, the deque thread must have set the
      // READ bit before the pointer bits have been set by the corresponding enqueue operation, yet
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
        case detail::advance_head_res_t::ADVANCED:    continue;
        case detail::advance_head_res_t::QUEUE_EMPTY: return nullptr;
      }
    }
  }
}

/********** private static functions **************************************************************/

template <typename T>
bool queue<T>::bounded_cas_loop(
  queue::atomic_slot_t& node,
  queue::marked_ptr_t&  expected,
  queue::marked_ptr_t   desired,
  const queue::node_t*  old_node,
  std::memory_order     order
) {
  // this loop attempts to exchange the expected (pointer, tag) pair with the desired pair,
  // the expected value is updated after each unsuccessful invocation so it always contains the
  // latest observed index value
  while (!node.compare_exchange_weak(expected.as_uintptr(), desired.to_uintptr(), order, relaxed)) {
    // the CAS failed but the read pointer value no longer matches the previous
    // value, so another thread must have updated the pointer
    if (expected.decompose_ptr() != old_node) {
      return false;
    }
  }

  return true;
}

/********** private methods ***********************************************************************/

template <typename T>
bool queue<T>::is_empty() noexcept {
  // using a read-modify-write operation that does not actually modify the value but acquires
  // ownership of the variable's cache-line, making the subsequent FAA potentially more efficient
  // (at least on x86)
  const auto [head, deq_idx] = marked_ptr_t{ this->m_head.fetch_add(0, relaxed) }.decompose();
  if (auto curr_tail = this->m_curr_tail.load(acquire); head == curr_tail) {
    const auto [tail, enq_idx] = marked_ptr_t(this->m_tail.load(relaxed)).decompose();
    if (curr_tail != tail) {
      this->m_curr_tail.compare_exchange_strong(curr_tail, tail, release, relaxed);
    }

    if (head == tail && (deq_idx >= NODE_SIZE || enq_idx <= deq_idx)) {
      return true;
    }
  }

  return false;
}

template <typename T>
detail::advance_head_res_t queue<T>::try_advance_head(
  queue::marked_ptr_t  curr,
  queue::node_t* const head,
  std::size_t idx
) {
  // the first slow-path operation initiates the reclamation checks for the current node, which
  // ensures the procedure is most likely to succeed on the first attempt since all previous enqueue
  // and dequeue operations must have already been initiated (but not necessarily completed)
  if (idx == NODE_SIZE) {
    head->try_reclaim(0);
  }

  if (head == marked_ptr_t{ this->m_tail.load(acquire) }.decompose_ptr()) {
    // if the tail has not yet been updated, the head must not be advanced ahead of it, even if
    // there already is a new node installed through the next pointer
    head->increment_dequeue_count();
    return detail::advance_head_res_t::QUEUE_EMPTY;
  }

  // load the current head's next pointer, which must have been set BEFORE updating the tail
  const auto next = head->next.load(acquire);

  // attempt to exchange the current head node (with its last observed index) with the next node
  curr.inc_idx();
  if (bounded_cas_loop(this->m_head, curr, marked_ptr_t(next, 0), head, release)) {
    // the current thread succeeded in exchanging the node and is hence the operation that observed
    // the final index value (count) of a all dequeue operations accessing this node
    head->increment_dequeue_count(curr.decompose_tag() - NODE_SIZE);
  } else {
    // some other node succeeded in exchanging the head and the operation is also complete
    head->increment_dequeue_count();
  }

  return detail::advance_head_res_t::ADVANCED;
}

template <typename T>
detail::advance_tail_res_t queue<T>::try_advance_tail(
    queue::pointer elem,
    queue::node_t* const tail
) {
  std::uint64_t final_count = 0;
  // re-load the tail pointer to check if it has already been advanced
  auto curr = marked_ptr_t(this->m_tail.load(relaxed));

  // another thread has already advanced the tail pointer, so this thread can retry and will likely
  // enter the fast-path
  if (tail != curr.decompose_ptr()) {
    tail->increment_enqueue_count();
    return detail::advance_tail_res_t::ADVANCED;
  }

  // load the current tail's next pointer to check if another thread has already appended a new
  // node to the queue but has not yet updated the tail pointer
  auto next = tail->next.load(relaxed);
  if (next == nullptr) {
    // there is no new node yet, allocate a new one and attempt to append it
    auto node = new(std::align_val_t{ NODE_ALIGN }) node_t(elem);
    auto advanced = detail::advance_tail_res_t::ADVANCED;
    const auto res = tail->next.compare_exchange_strong(next, node, release, relaxed);
    if (res) {
      // the CAS succeeded in appending the node after the tail, now the tail has to be updated
      if (bounded_cas_loop(this->m_tail, curr, marked_ptr_t(node, 1), tail, release)) {
        final_count = curr.decompose_tag() - NODE_SIZE;
      }

      // it doesn't matter, which thread succeeded in updating the tail, since all must attempt to
      // set it to previous tail's next pointer, which was set by this thread and contains `elem`
      next = node;
      advanced = detail::advance_tail_res_t::ADVANCED_AND_INSERTED;
    } else {
      if (bounded_cas_loop(this->m_tail, curr, marked_ptr_t(next, 1), tail, release)) {
        final_count = curr.decompose_tag() - NODE_SIZE;
      }
    }

    // update the cached tail pointer
    auto expected = tail;
    this->m_curr_tail.compare_exchange_strong(expected, next, release, relaxed);
    // conclude the operation by increasing the enqueue count to allow reclamation
    tail->increment_enqueue_count(final_count);

    if (!res) {
      // the CAS failed so another thread must have succeeded in appending a node, delete the node
      // allocated by this thread and try again
      delete node;
    }

    return advanced;
  } else {
    // there is already a new node after the current tail, so this thread has to help updating the
    // queue's tail pointer and retry once the tail has been advanced
    if (bounded_cas_loop(this->m_tail, curr, marked_ptr_t(next, 1), tail, release)) {
      final_count = curr.decompose_tag() - NODE_SIZE;
    }

    // update the cached tail pointer
    auto expected = tail;
    this->m_curr_tail.compare_exchange_strong(expected, next, release, relaxed);
    // conclude the operation by increasing the enqueue count to allow reclamation
    tail->increment_enqueue_count(final_count);

    return detail::advance_tail_res_t::ADVANCED;
  }
}
}

#endif /* LOO_QUEUE_HPP */
