#ifndef LOO_QUEUE_QUEUE_HPP
#define LOO_QUEUE_QUEUE_HPP

#if defined(__GNUG__) || defined(__clang__) || defined(__INTEL_COMPILER)
#define likely(cond)   __builtin_expect ((cond), 1)
#else
#define likely(cond)   cond
#endif

#include <stdexcept>

#include "looqueue/queue_fwd.hpp"
#include "looqueue/detail/node.hpp"
#include "looqueue/detail/ordering.hpp"

namespace loo {
template <typename T>
queue<T>::queue() {
  // initially head and tail point at the same node
  auto head = reinterpret_cast<queue::slot_t>(new node_t());
  std::atomic_init(&this->m_head, head);
  std::atomic_init(&this->m_tail, head);
}

template <typename T>
queue<T>::~queue() noexcept {
  // de-allocate all remaining nodes in the queue
  auto curr = marked_ptr_t(this->m_head.load(std::memory_order_relaxed)).decompose_ptr();
  while (curr != nullptr) {
    auto next = curr->next.load(std::memory_order_relaxed);
    delete curr;
    curr = next;
  }
}

template <typename T>
void queue<T>::enqueue(queue::pointer elem) {
  // validate `elem` argument (must not be null and aligned so it can store 2 bits)
  if (elem == nullptr) {
    throw std::invalid_argument("`elem` must not be nullptr");
  } else if ((reinterpret_cast<queue::slot_t>(elem) & node_t::slot_consts_t::STATE_MASK) != 0) {
    throw std::invalid_argument("`elem` must be at least 4-byte aligned");
  }

  while (true) {
    // increment the enqueue index, retrieve the tail pointer and previous index value
    // see PROOF.md regarding the (im)possibility of overflows
    const auto curr = marked_ptr_t(this->m_tail.fetch_add(marked_ptr_t::INCREMENT, ACQUIRE));
    const auto tail = curr.decompose();

    if (likely(tail.idx < queue::NODE_SIZE)) {
      // ** fast path ** write access to the slot at tail.idx was uniquely reserved
      // write the `elem` bits into the slot (unique access ensures this is done exactly once)
      const auto state = tail.ptr->slots[tail.idx].fetch_add(
        reinterpret_cast<queue::slot_t>(elem),
        RELEASE
      );

      if (likely(state <= node_t::slot_consts_t::RESUME)) {
        // no READ bit is set, RESUME may or may not be set - the element was successfully inserted
        // if the RESUME bit is set, the corresponding dequeue operation will act accordingly.
        return;
      } else if (state == (node_t::slot_consts_t::READER | node_t::slot_consts_t::RESUME)) {
        // both READ and RESUME are set, so this must be the final operation visiting this slot
        // hence the slot must be abandoned (dequeue finished too early) and `try_reclaim` must be
        // resumed
        tail.ptr->try_reclaim(tail.idx + 1);
      }

      // only READ bit is set so the slot must be abandoned and both operations
      // must retry on another slots
      continue;
    } else {
      // ** slow path ** no free slot is available in this node, so a new node has to be appended
      // the attempts to directly insert `elem` in the newly appended node's first slot and the
      // enqueue procedure is completed on success; in any case `tail` points at some successor node
      // when this sub-procedure completes
      switch (this->try_advance_tail(elem, tail.ptr)) {
        case detail::advance_tail_res_t::ADVANCED_AND_INSERTED: return;
        case detail::advance_tail_res_t::ADVANCED:              continue;
      }
    }
  }
}

template <typename T>
typename queue<T>::pointer queue<T>::dequeue() {
  while (true) {
    // load head & tail for subsequent empty check
    auto curr = marked_ptr_t(this->m_head.load(RELAXED));
    const auto tail = marked_ptr_t(this->m_tail.load(RELAXED)).decompose();

    // check if queue is empty BEFORE incrementing the dequeue index
    if (queue::is_empty(curr.decompose(), tail)) {
      return nullptr;
    }

    // increment the dequeue index, retrieve the head pointer and previous index
    // value see PROOF.md regarding the (im)possibility of overflows
    curr = marked_ptr_t(this->m_head.fetch_add(marked_ptr_t::INCREMENT, ACQUIRE));
    const auto head = curr.decompose();

    if (head.idx < queue::NODE_SIZE) {
      // ** fast path ** read access to the slot at tail.idx was uniquely reserved
      // set the READ bit in the slot (unique access ensures this is done exactly once)
      const auto state = head.ptr->slots[head.idx].fetch_add(
        node_t::slot_consts_t::READER,
        ACQUIRE
      );

      // extract the pointer bits from the retrieved value
      const auto res = reinterpret_cast<pointer>(state & node_t::slot_consts_t::ELEM_MASK);

      // check the extracted pointer bits, if the result is null, the deque thread must have set
      // the READ bit before the pointer bits have been set by the corresponding enqueue operation,
      // yet
      if (res != nullptr) {
        if ((state & node_t::slot_consts_t::RESUME) != 0) {
          head.ptr->try_reclaim(head.idx + 1);
        }

        return res;
      }

      continue;
    } else {
      // the first slow-path operation initiates the reclamation checks for the current node,
      // which ensures the procedure is most likely to succeed on the first attempt since all
      // previous enqueue and dequeue operations must have already been initiated (but not
      // necessarily completed)
      if (head.idx == queue::NODE_SIZE) {
        head.ptr->try_reclaim(0, true);
      }

      // ** slow path ** the current head node has been fully consumed and must
      // be replaced by its successor, if there is one
      switch (this->try_advance_head(curr, head.ptr, tail.ptr)) {
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
  queue::node_t*        old_node
) {
  // this loop attempts to exchange the expected (pointer, tag) pair with the desired pair,
  // the expected value is updated after each unsuccessful invocation so it always contains the
  // latest observed index value
  while (!node.compare_exchange_strong(expected.as_int(), desired.to_int(), RELEASE, RELAXED)) {
    // the CAS failed but the read pointer value no longer matches the previous
    // value, so another thread must have updated the pointer
    if (expected.decompose_ptr() != old_node) {
      return false;
    }
  }

  return true;
}

template <typename T>
bool queue<T>::is_empty(
  typename queue::marked_ptr_t::decomposed_t head,
  typename queue::marked_ptr_t::decomposed_t tail
) {
  return (head.idx >= queue::NODE_SIZE || head.idx >= tail.idx) && head.ptr == tail.ptr;
}

/********** private methods ***********************************************************************/

template <typename T>
detail::advance_head_res_t queue<T>::try_advance_head(
  queue::marked_ptr_t curr,
  queue::node_t*      head,
  queue::node_t*      tail
) {
  // load the current head's next pointer
  const auto next = head->next.load(ACQUIRE);
  if (next == nullptr || head == tail) {
    // if there is no next node yet or if there is one but the tail pointer does
    // not yet point at, the queue is determined to be empty
    head->incr_dequeue_count();
    return detail::advance_head_res_t::QUEUE_EMPTY;
  }

  // attempt to exchange the current head node (with its last observed index) with the next node
  curr.inc_idx();
  if (queue::bounded_cas_loop(this->m_head, curr, marked_ptr_t(next, 0), head)) {
    // the current thread succeeded in exchanging the node and is hence the operation that observed
    // the final index value (count) of a all dequeue operations accessing this node
    head->incr_dequeue_count(curr.decompose_tag() - queue::NODE_SIZE);
  } else {
    // some other node succeeded in exchanging the head and the operation is also complete
    head->incr_dequeue_count();
  }

  return detail::advance_head_res_t::ADVANCED;
}

template <typename T>
detail::advance_tail_res_t queue<T>::try_advance_tail(queue::pointer elem, queue::node_t* tail) {
  while (true) {
    // re-load the tail pointer to check if it has already been advanced
    auto curr = marked_ptr_t(this->m_tail.load(RELAXED));

    // another thread has already advanced the tail pointer, so this thread can
    // retry and will likely enter the fast-path
    if (tail != curr.decompose_ptr()) {
      tail->incr_enqueue_count();
      return detail::advance_tail_res_t::ADVANCED;
    }

    // load the current tail's next pointer to check if another thread has already appended a new
    // node to the queue but has not yet updated the tail pointer
    auto next = tail->next.load();

    if (next == nullptr) {
      // there is no new node yet, allocate a new one and attempt to append it
      auto node = new node_t(elem);
      const auto res = tail->next.compare_exchange_strong(next, node, RELEASE, RELAXED);

      if (res) {
        // the CAS succeeded in appending the node after the tail, now the tail has to be updated
        if (queue::bounded_cas_loop(this->m_tail, curr, marked_ptr_t(node, 1), tail)) {
          tail->incr_enqueue_count(curr.decompose_tag() - queue::NODE_SIZE);
        } else {
          tail->incr_enqueue_count();
        }

        // it doesn't matter which thread succeeded in updating the tail, since all must attempt to
        // set it to previous tail's next pointer, which was set by this thread and contains `elem`
        return detail::advance_tail_res_t::ADVANCED_AND_INSERTED;
      } else {
        // the CAS failed so another thread must have succeeded in appending a node, delete the node
        // allocated by this thread and try again
        delete node;
        continue;
      }
    } else {
      // there is already a new node after the current tail, so this thread has to help updating the
      // queue's tail pointer and retry once the tail has been advanced
      if (queue::bounded_cas_loop(this->m_tail, curr, marked_ptr_t(next, 1), tail)) {
        tail->incr_enqueue_count(curr.decompose_tag() - queue::NODE_SIZE);
      } else {
        tail->incr_enqueue_count();
      }

      return detail::advance_tail_res_t::ADVANCED;
    }
  }
}
}

#endif /* LOO_QUEUE_QUEUE_HPP */
