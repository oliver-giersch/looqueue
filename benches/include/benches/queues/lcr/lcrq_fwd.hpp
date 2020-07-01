#ifndef LOO_QUEUE_BENCHMARK_LCRQ_FWD_HPP
#define LOO_QUEUE_BENCHMARK_LCRQ_FWD_HPP

#include <atomic>
#include <utility>

#include <cstdint>

#include "looqueue/align.hpp"

#include "benches/hazard_pointers/hazard_pointers.hpp"

namespace lcr {
template <typename T>
/** implementation of LCRQ(ueue) by Morrison/Afek */
class queue {
public:
  using pointer = T*;

  explicit queue(std::size_t max_threads = MAX_THREADS);
  ~queue() noexcept;
  void enqueue(pointer elem, std::size_t thread_id);
  pointer dequeue(std::size_t thread_id);

  queue(const queue&)             = delete;
  queue(queue&&)                  = delete;
  queue& operator=(const queue&&) = delete;
  queue& operator=(queue&&)       = delete;

private:
  static constexpr std::size_t MAX_THREADS = 128;
  static constexpr std::size_t RING_SIZE   = 128;

  /** enqueue and dequeue use the same hazard pointer */
  static constexpr std::size_t HP_ENQ_TAIL = 0;
  static constexpr std::size_t HP_DEQ_HEAD = 0;

  static constexpr std::uint64_t STATUS_BIT = 1ull << 63ull;
  static constexpr std::uint64_t INDEX_MASK = ~STATUS_BIT;

  using cell_pair_t = typename std::pair<std::uint64_t, pointer>;

  struct alignas(CACHE_LINE_SIZE) cell_t;
  struct crq_node_t;
  struct decomposed_t;

  alignas(CACHE_LINE_ALIGN) std::atomic<crq_node_t*> m_head;
  alignas(CACHE_LINE_ALIGN) std::atomic<crq_node_t*> m_tail;

  memory::hazard_pointers<crq_node_t> m_hazard_pointers;
};
}

#endif /* LOO_QUEUE_BENCHMARK_LCRQ_FWD_HPP */
