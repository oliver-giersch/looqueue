#ifndef LOO_QUEUE_BENCHMARK_FAA_ARRAY_FWD_HPP
#define LOO_QUEUE_BENCHMARK_FAA_ARRAY_FWD_HPP

#include <atomic>

#include "benches/hazard_pointers/hazard_pointers.hpp"
#include "looqueue/align.hpp"

namespace faa {
/** implementation of FAA-Array queue by Correia/Ramalhete */
template <typename T>
class queue {
public:
  using pointer = T*;

  explicit queue(std::size_t max_threads = MAX_THREADS);
  ~queue() noexcept;
  __attribute__ ((noinline)) void enqueue(pointer elem, std::size_t thread_id);
  __attribute__ ((noinline)) pointer dequeue(std::size_t thread_id);

  queue(const queue&)             = delete;
  queue(queue&&)                  = delete;
  queue& operator=(const queue&&) = delete;
  queue& operator=(queue&&)       = delete;

private:
  static constexpr std::size_t NODE_SIZE   = 1024;
  static constexpr std::size_t MAX_THREADS = 128;

  /** enqueue and dequeue use the same hazard pointer */
  static constexpr std::size_t HP_ENQ_TAIL = 0;
  static constexpr std::size_t HP_DEQ_HEAD = 0;

  static constexpr std::size_t TAKEN = 0x1;

  struct node_t;

  bool cas_head(node_t* curr, node_t* next);
  bool cas_tail(node_t* curr, node_t* next);

  alignas(CACHE_LINE_ALIGN) std::atomic<node_t*> m_head;
  alignas(CACHE_LINE_ALIGN) std::atomic<node_t*> m_tail;

  memory::hazard_pointers<node_t> m_hazard_pointers;
};
}

#endif /* LOO_QUEUE_BENCHMARK_FAA_ARRAY_FWD_HPP */
