#ifndef LOO_QUEUE_BENCHMARK_QUEUE_REF_HPP
#define LOO_QUEUE_BENCHMARK_QUEUE_REF_HPP

#include <cstddef>

template <typename Q>
/** thread-local reference to an concurrent queue instance */
class queue_ref final {
public:
  using queue   = Q;
  using pointer = typename queue::pointer;

  explicit queue_ref(Q& queue, const std::size_t thread_id) noexcept :
    m_queue{queue}, m_thread_id{thread_id} {}

  void enqueue(pointer elem) {
    this->m_queue.enqueue(elem, this->m_thread_id);
  }

  pointer dequeue() {
    return this->m_queue.dequeue(this->m_thread_id);
  }

  queue_ref(const queue_ref&)                     = default;
  queue_ref(queue_ref&&) noexcept                 = default;
  queue_ref& operator=(const queue_ref&) noexcept = default;
  queue_ref& operator=(queue_ref&&) noexcept      = default;

private:
  queue& m_queue;
  std::size_t m_thread_id;
};

#endif /* LOO_QUEUE_BENCHMARK_QUEUE_REF_HPP */
