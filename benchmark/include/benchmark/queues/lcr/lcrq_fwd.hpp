#ifndef LOO_QUEUE_BENCHMARK_LCRQ_FWD_HPP
#define LOO_QUEUE_BENCHMARK_LCRQ_FWD_HPP

#include <atomic>
#include <utility>

#include <cstdint>

namespace lcr {
template <typename T>
/** LCRQ */
class queue {
public:
  using pointer = T*;

  explicit queue(std::size_t max_threads = MAX_THREADS);
  ~queue() noexcept;

  queue(const queue&)             = delete;
  queue(queue&&)                  = delete;
  queue& operator=(const queue&&) = delete;
  queue& operator=(queue&&)       = delete;

private:
  static constexpr std::size_t MAX_THREADS = 128;
  static constexpr std::size_t RING_SIZE   = 1024;

  using cell_pair_t = typename std::pair<std::uint64_t, pointer>;

  struct alignas(64) cell_t;
  struct crq_node_t;
  struct decomposed_t;

  alignas(128) std::atomic<crq_node_t*> m_head;
  alignas(128) std::atomic<crq_node_t*> m_tail;
};

/** thread-local reference to an LCRQ instance */
template <typename T>
class queue_ref {
public:
  using pointer = typename queue<T>::pointer;

  explicit queue_ref(queue<T>& queue, std::size_t thread_id) noexcept :
    m_queue(queue), m_thread_id(thread_id) {}
  ~queue_ref() noexcept = delete;

  void enqueue(pointer elem) {
    this->m_queue.enqueue(elem, this->m_thread_id);
  }

  pointer dequeue() {
    this->m_queue.dequeue(this->m_thread_id);
  }

  queue_ref(const queue_ref&)                     = default;
  queue_ref(queue_ref&&) noexcept                 = default;
  queue_ref& operator=(const queue_ref&) noexcept = default;
  queue_ref& operator=(queue_ref&&) noexcept      = default;

private:
  queue<T>& m_queue;
  std::size_t m_thread_id;
};
}

#endif /* LOO_QUEUE_BENCHMARK_LCRQ_FWD_HPP */
