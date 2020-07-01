#ifndef LOO_QUEUE_QUEUE_FWD_HPP
#define LOO_QUEUE_QUEUE_FWD_HPP

#include <atomic>

#include "align.hpp"
#include "detail/marked_ptr.hpp"

namespace loo {
namespace detail {
/** result type for private `try_advance_head` method */
enum class advance_head_res_t {
  QUEUE_EMPTY,
  ADVANCED,
};

/** result type for private `try_advance_tail` method */
enum class advance_tail_res_t {
  ADVANCED,
  ADVANCED_AND_INSERTED,
};
}

template <typename T>
class queue {
  static_assert(sizeof(T*) == 8, "loo::queue is only valid for 64-bit architectures");

  /** the number of slots for storing individual elements in each node */
  static constexpr std::size_t NODE_SIZE = 128;
  /** the base node size is approximately 1024 bytes (plus some extra) and
   *  over-aligning them to that size results in 13 usable tag bits. */
  static constexpr std::size_t TAG_BITS  = 11;

public:
  /** see PROOF.md for the reasoning behind these constants */
  static constexpr std::size_t MAX_PRODUCER_THREADS = (1ull << TAG_BITS) - NODE_SIZE + 1;
  static constexpr std::size_t MAX_CONSUMER_THREADS = ((1ull << TAG_BITS) - NODE_SIZE + 1) / 2;

  using pointer = T*;

  /** constructor */
  queue();
  /** destructor */
  ~queue() noexcept;
  /** enqueue an element to the queue's back */
  void enqueue(pointer elem);
  /** dequeue an element from the queue's front */
  pointer dequeue();

  /** deleted constructors & operators */
  queue(const queue&)            = delete;
  queue(queue&&)                 = delete;
  queue& operator=(const queue&) = delete;
  queue& operator=(queue&&)      = delete;

private:
  static constexpr std::size_t NODE_ALIGN = 1ull << TAG_BITS;

  /** see queue::node_t::slot_consts_t */
  using slot_t        = std::uint64_t;
  using atomic_slot_t = std::atomic<slot_t>;

  struct node_t;
  using marked_ptr_t = typename detail::marked_ptr_t<node_t, TAG_BITS>;

  /** returns true if the queue is determined to be empty */
  static bool is_empty(
    typename marked_ptr_t::decomposed_t head,
    typename marked_ptr_t::decomposed_t tail
  );

  /** loops and attempts to CAS `expected` with `desired` until either the CAS succeeds
   *  or the loaded pointer value (failure case) no longer matches `old_node` */
  static bool bounded_cas_loop(
    atomic_slot_t& node,
    marked_ptr_t&  expected,
    marked_ptr_t   desired,
    node_t*        old_node
  );

  /** attempts to advance the head node to its successor if there is one */
  detail::advance_head_res_t try_advance_head(marked_ptr_t curr, node_t* head, node_t* tail);
  /** attempts to advance the tail node to its successor if there is one or attempts to append a
   *  new node with `elem` stored in the first slot otherwise */
  detail::advance_tail_res_t try_advance_tail(pointer elem, node_t* tail);

  alignas(CACHE_LINE_ALIGN) atomic_slot_t m_head{ 0 };
  alignas(CACHE_LINE_ALIGN) atomic_slot_t m_tail{ 0 };
};
}

#endif /* LOO_QUEUE_QUEUE_FWD_HPP */
