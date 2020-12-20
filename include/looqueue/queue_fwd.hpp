#ifndef LOO_QUEUE_FWD_HPP
#define LOO_QUEUE_FWD_HPP

#include <atomic>

#include "align.hpp"
#include "detail/marked_ptr.hpp"

namespace loo {
namespace detail {
/** result type for private `try_advance_head` method */
enum class advance_head_res_t { QUEUE_EMPTY, ADVANCED };
/** result type for private `try_advance_tail` method */
enum class advance_tail_res_t { ADVANCED, ADVANCED_AND_INSERTED };
}

template <typename T>
class queue {
  static_assert(sizeof(T*) == 8, "loo::queue is only valid for 64-bit architectures");
  static_assert(alignof(T) >= 4, "all T pointers must be at least 4-byte aligned");
  /** the number of slots for storing individual elements in each node */
  static constexpr auto NODE_SIZE = std::size_t{ 1024 };
  static constexpr auto TAG_BITS  = std::size_t{ 11 };
  /** ordering constants */
  static constexpr auto relaxed = std::memory_order_relaxed;
  static constexpr auto acquire = std::memory_order_acquire;
  static constexpr auto release = std::memory_order_release;
  static constexpr auto acq_rel = std::memory_order_acq_rel;
  /** see queue::node_t::slot_flags_t */
  using slot_t        = std::uintptr_t;
  using atomic_slot_t = std::atomic<slot_t>;

  /**
   * Each node must be aligned to this value in order to be able to store the
   * required number of tag bits in every node pointer.
   */
  static constexpr auto NODE_ALIGN = std::size_t{ 1 } << TAG_BITS;

  struct alignas(NODE_ALIGN) node_t;
  using marked_ptr_t = typename detail::marked_ptr_t<node_t, TAG_BITS>;

  alignas(CACHE_LINE_ALIGN) atomic_slot_t        m_head{ 0 };
  alignas(CACHE_LINE_ALIGN) atomic_slot_t        m_tail{ 0 };
  alignas(CACHE_LINE_ALIGN) std::atomic<node_t*> m_curr_tail;

public:
  using pointer = T*;

  /** see PROOF.md for the reasoning behind these constants */
  static constexpr std::size_t MAX_PRODUCER_THREADS = (1ull << TAG_BITS) - NODE_SIZE + 1;
  static constexpr std::size_t MAX_CONSUMER_THREADS = ((1ull << TAG_BITS) - NODE_SIZE + 1) / 2;

  /** constructor */
  queue();
  /** destructor */
  ~queue() noexcept;
  /** enqueue an element to the queue's back */
  void enqueue(pointer elem);
  /** dequeue an element from the queue's front */
  pointer dequeue();

  /** deleted constructors & assignment operators */
  queue(const queue&)            = delete;
  queue(queue&&)                 = delete;
  queue& operator=(const queue&) = delete;
  queue& operator=(queue&&)      = delete;

private:
  /**
   * Loops and attempts to CAS `expected` with `desired` until either the CAS succeeds
   * or the loaded pointer value (failure case) no longer matches `old_node`
   */
  static bool bounded_cas_loop(
      atomic_slot_t&  node,
      marked_ptr_t&   expected,
      marked_ptr_t    desired,
      const node_t*   old_node,
      std::memory_order order
  );

  /** Attempts to advance the head node to its successor, if there is one. */
  detail::advance_head_res_t try_advance_head(marked_ptr_t curr, node_t* head, bool is_first);
  /**
   * Attempts to advance the tail node to its successor if there is one or
   * attempts to append a new node with `elem` stored in the first slot
   * otherwise.
   */
  detail::advance_tail_res_t try_advance_tail(pointer elem, node_t* tail);
};
}

#endif /* LOO_QUEUE_FWD_HPP */
