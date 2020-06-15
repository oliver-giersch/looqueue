#ifndef LOO_QUEUE_BENCHMARK_FAA_ARRAY_NODE_HPP
#define LOO_QUEUE_BENCHMARK_FAA_ARRAY_NODE_HPP

#include "benches/queues/faa/faa_array_fwd.hpp"

#include <cstdint>
#include <atomic>
#include <array>

namespace faa {
template <typename T>
struct queue<T>::node_t {
  using slot_array_t = std::array<
      std::atomic<queue::pointer>,
      queue::NODE_SIZE
  >;

  std::atomic<std::uint32_t> enq_idx{ 0 };
  slot_array_t               slots{{ nullptr }};
  std::atomic<std::uint32_t> deq_idx{ 0 };
  std::atomic<node_t*>       next{ nullptr };

  node_t() = default;
  explicit node_t(queue::pointer first) : enq_idx{ 1 } {
    this->slots[0].store(first, std::memory_order_relaxed);
  }

  bool cas_next(node_t* curr, node_t next_node) {
    return this->next.compare_exchange_strong(curr, next_node);
  }
};
}

#endif /* LOO_QUEUE_BENCHMARK_FAA_ARRAY_NODE_HPP */
