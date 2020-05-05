#ifndef LOO_QUEUE_QUEUE_HPP
#define LOO_QUEUE_QUEUE_HPP

#define likely(cond)   __builtin_expect ((cond), 1)
#define unlikely(cond) __builtin_expect ((cond), 0)

#include <stdexcept>

#include <looqueue/queue_fwd.hpp>

namespace loo {
template <typename T>
queue<T>::queue() {
  // initially head and tail point at the same node
  auto head = new node_t();
  this->m_head.store(reinterpret_cast<std::uint64_t>(head), std::memory_order_relaxed);
  this->m_tail.store(reinterpret_cast<std::uint64_t>(head), std::memory_order_relaxed);
}

template <typename T>
void queue<T>::enqueue(queue::pointer elem) {
  // validate `elem` argument
  if (elem == nullptr) {
    throw std::invalid_argument("`elem` must not be nullptr");
  } else if (reinterpret_cast<std::uint64_t>(elem) & node_t::slot_consts_t::STATE_MASK != 0) {
    throw std::invalid_argument("`elem` must be at least 4-byte aligned");
  }

  while (true) {
    // increment the enqueue index, retrieve the tail pointer and previous index value
    // see PROOF.md regarding the (im)possibility of overflows
    const auto curr = marked_ptr_t(this->m_tail.fetch_add(1, std::memory_order_acquire));
    const auto tail = curr.decompose();

    if (likely(tail.idx < NODE_SIZE)) {
      // ** fast path ** write access to the slot at tail.idx was uniquely reserved
      // write the `elem` bits into the slot (unique access ensures this is done exactly once)
      const auto state = tail.ptr->slots[idx].fetch_add(
        reinterpret_cast<std::uint64_t>(elem),
        std::memory_order_release
      );


      if (likely(state <= node_t::slot_consts_t::RESUME)) {
        // no READ bit is set, RESUME may or may not be set - the element was successfully inserted
        // if the RESUME bit is set, the corresponding dequeue operation will act accordingly.
        return;
      } else if (prev == (node_t::slot_consts_t::READ | node_t::slot_consts_t::RESUME)) {
        // both READ and RESUME are set, so this must be the final operation visiting this slot
        // hence the slot must be abandoned (dequeue finished too early) and `try_reclaim` must be
        // resumed
        tail.ptr->try_reclaim(idx + 1);
      }

      // only READ bit is set so the slot must be abandoned and both operations must retry on
      // another slots
      continue;
    } else {
      // ** slow path ** no free slot is available in this node, so a new node has to be appended
      // the attempts to directly insert `elem` in the newly appended node's first slot and the
      // enqueue procedure is completed on success; in any case `tail` points at some successor node
      // when this sub-procedure completes
      switch (this->try_advance_tail(elem, tail.ptr)) {
        case queue::advance_tail_res_t::ADVANCED_AND_INSERTED: return;
        case queue::advance_tail_res_t::ADVANCED:              continue;
      }
    }
  }
}

template <typename T>
queue<T>::pointer queue<T>::dequeue() {
  while (true) {
    // load head & tail for subsequent empty check
    auto curr = marked_ptr_t(this->m_tail.load(std::memory_order_relaxed));
    const auto tail = marked_ptr_t(this->m_tail.load(std::memory_order_relaxed)).decompose();

    // check if queue is empty BEFORE incrementing the dequeue index
    if (queue::is_empty(curr.decompose(), tail)) {
      return nullptr;
    }

    // increment the dequeue index, retrieve the head pointer and previous index value
    // see PROOF.md regarding the (im)possibility of overflows
    curr = marked_ptr_t(this->m_head.fetch_add(1, std::memory_order_acquire));
    const auto head = curr.decompose();

    if (head.idx < NODE_SIZE) {
      // ** fast path ** read access to the slot at tail.idx was uniquely reserved
      // set the READ bit in the slot (unique access ensures this is done exactly once)
      const auto state = head.ptr->slots[idx].fetch_add(
        node_t::slot_consts_t::READ,
        std::memory_order_acquire
      );

      // extract the pointer bits from the retrieved value
      const auto res = reinterpret_cast<pointer>(state & node_t::slot_consts_t::ELEM_MASK);

      // read access on the final slot in a node initiates the reclamation checks for that node,
      // this ensures the procedure is most likely to succeede on the first attempt since all
      // previous enqueue and dequeue operations must have already been initiated (but not
      // necessarily completed)
      if (head.idx == NODE_SIZE - 1) {
        head.ptr->try_reclaim(0);
      }

      // check the extracted pointer bits, if the result is null, the dequeing thread must have set
      // the READ bit before the pointer bits have been set by the corresponding enqueue operation,
      // yet
      if (res != nullptr) {
        if (state & node_t::slot_consts_t::RESUME != 0) {
          head.ptr->try_reclaim(idx + 1);
        }

        return res;
      }

      continue;
    } else {
      // ** slow path **
      switch (this->try_advance_head(curr, head.ptr, tail.ptr)) {
        case queue::advance_head_res_t::ADVANCED:    continue;
        case queue::advance_head_res_t::QUEUE_EMPTY: return nullptr;
      }
    }
  }
}

/********** private static functions **************************************************************/

template <typename T>
bool queue<T>::bounded_cas_loop(
  std::atomic<std::uint64_t>& node,
  queue::marked_ptr_t&        expected,
  queue::marked_ptr_t         desired,
  queue::node_t*              old_node
) {
  while (!node.compare_exchange_strong(
    expected.as_integer(),
    desired.to_integer(),
    std::memory_order_release,
    std::memory_order_relaxed,
  )) {
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
  return (head.idx >= NODE_SIZE || head.idx >= tail.idx) && head.ptr == tail.ptr;
}

/********** private methods ***********************************************************************/

template <typename T>
queue<T>::advance_head_res_t queue<T>::try_advance_head(
  queue::marked_ptr_t curr,
  queue::node_t*      head,
  queue::node_t*      tail
) {
  const auto next = head->next.load(std::memory_order_acquire);
  if (next == nullptr || head == tail) {
    head->try_reclaim_dequeue();
    return queue::advance_head_res_t::QUEUE_EMPTY;
  }

  curr.inc_tag();
  if (queue::bounded_cas_loop(this->m_head, curr, marked_ptr_t(next, 0), head)) {
    head->try_reclaim_dequeue_final(curr.decompose_tag() - NODE_SIZE);
  } else {
    head->try_reclaim_dequeue();
  }

  return queue::advance_head_res_t::ADVANCED;
}

template <typename T>
queue<T>::advance_tail_res_t queue<T>::try_advance_tail(
  queue::pointer elem,
  queue::node_t* tail
) {
  while (true) {
    auto curr = marked_ptr_t(this->m_tail.load(std::memory_order_relaxed));

    if (tail != curr.decompose_ptr()) {
      tail->try_reclaim_enqueue();
      return queue::advance_tail_res_t::ADVANCED;
    }

    auto next = tail->next.load();

    if (next == nullptr) {
      auto node = new node_t(elem);
      const auto res = tail->next.compare_exchange_strong(
        next,
        node,
        std::memory_order_release,
        std::memory_order_relaxed
      );

      if (res) {
        if (queue::bounded_cas_loop(this->m_tail, curr, marked_ptr_t(node, 1), tail)) {
          tail->try_reclaim_enqueue_final(curr.decompose_tag() - queue::NODE_SIZE);
        } else {
          tail->try_reclaim_enqueue();
        }

        return queue::advance_tail_res_t::ADVANCED_AND_INSERTED;
      } else {
        delete node;
        continue;
      }
    } else {
      if (queue::bounded_cas_loop(this->m_tail, curr, marked_ptr_t(next, 1), tail)) {
        tail->try_reclaim_enqueue_final(curr.decompose_tag() - queue::NODE_SIZE);
      } else {
        tail->try_reclaim_enqueue();
      }

      return queue::advance_tail_res_t::ADVANCED;
    }
  }
}
}

#endif /* LOO_QUEUE_QUEUE_HPP */