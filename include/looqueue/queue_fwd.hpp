#ifndef LOO_QUEUE_QUEUE_FWD_HPP
#define LOO_QUEUE_QUEUE_FWD_HPP

#include <atomic>

#include "detail/align.hpp"
#include "detail/marked_ptr.hpp"

namespace loo {
template <typename T>
class queue {
  static_assert(sizeof(T*) == 8, "loo::queue is only valid for 64-bit architectures");
public:
  using pointer = T*;

  /** constructor */
  queue();
  /** destructor */
  ~queue();
  /** enqueue (back) */
  void enqueue(pointer elem);
  /** dequeue (front) */
  pointer dequeue();

  /** deleted constructors & operators */
  queue(const queue&)            = delete;
  queue(queue&&)                 = delete;
  queue& operator=(const queue&) = delete;
  queue& operator(queue&&)       = delete;

private:
  /** the number of slots for storing individual elements in each node */
  static constexpr std::size_t NODE_SIZE = 1024;
  /** the node size results in nodes that are approximately 8192 bytes (plus some extra),
   *  allowing for 13 tag bits */
  static constexpr std::size_t TAG_BITS  = 13;

  struct alignas(1 << TAG_BITS) node_t;
  using marked_ptr_t = typename detail::marked_ptr_t<node_t, TAG_BITS>;

  /** returns true if the queue is determined to be empty */
  static bool is_empty(marked_ptr_t::decomposed_t head, marked_ptr::decomposed_t tail) const;

  /** loops and attempts to CAS `expected` with `desired` until either the CAS succeeds
   *  or the loaded pointer value (failure case) no longer matches `old_node` */
  static bool bounded_cas_loop(
    std::atomic<std::uint64_t>& node,
    marked_ptr_t& expected,
    marked_ptr_t desired,
    node_t* old_node,
  );

  /** result type for `try_advance_head` */
  enum class advance_head_res_t {
    QUEUE_EMPTY,
    ADVANCED,
  };

  /** result type for `try_advance_tail` */
  enum class advance_tail_res_t {
    ADVANCED,
    ADVANCED_AND_INSERTED,
  };

  advance_head_res_t try_advance_head(marked_ptr_t curr, node_t* head, node_t* tail);
  advance_tail_res_t try_advance_tail(pointer elem, node_t* tail);

  alignas(CACHE_LINE_ALIGN) std::atomic<std::uint64_t> m_head{ 0 };
  alignas(CACHE_LINE_ALIGN) std::atomic<std::uint64_t> m_tail{ 0 };
};
}

#endif /* LOO_QUEUE_QUEUE_FWD_HPP */