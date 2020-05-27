#ifndef LOO_QUEUE_BENCHMARK_LCRQ_DETAIL_NODE_HPP
#define LOO_QUEUE_BENCHMARK_LCRQ_DETAIL_NODE_HPP

#include "benchmark/queues/lcr/lcrq_fwd.hpp"

#include <array>

#include "looqueue/align.hpp"

namespace lcr {
/** queue::cell_t definition */
template <typename T>
struct alignas(64) queue<T>::cell_t {
  std::atomic<std::uint64_t>  idx;
  std::atomic<queue::pointer> val;

  bool dw_cas(queue::cell_pair_t expected, queue::cell_pair_t desired) {
    return false;
  }
};

/** queue::crq_node_t definition */
template <typename T>
struct queue<T>::crq_node_t {
  alignas(CACHE_LINE_SIZE) std::atomic<std::uint64_t>    head_ticket;
  alignas(CACHE_LINE_SIZE) std::atomic<std::uint64_t>    tail_ticket;
  alignas(CACHE_LINE_SIZE) std::atomic<crq_node_t*>      next;
  alignas(CACHE_LINE_SIZE) std::array<cell_t, RING_SIZE> cells;
};
}

#endif /* LOO_QUEUE_BENCHMARK_LCRQ_DETAIL_NODE_HPP */
