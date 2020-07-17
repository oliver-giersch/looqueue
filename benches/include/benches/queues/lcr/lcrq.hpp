#ifndef LOO_QUEUE_BENCHMARK_LCRQ_HPP
#define LOO_QUEUE_BENCHMARK_LCRQ_HPP

#include "benches/queues/lcr/lcrq_fwd.hpp"
#include "benches/queues/lcr/detail/node.hpp"

namespace lcr {
template <typename T>
queue<T>::queue(std::size_t max_threads) :
  m_hazard_pointers{ max_threads }
{
  auto head = new crq_node_t();
  std::atomic_init(&this->m_head, head);
  std::atomic_init(&this->m_tail, head);
}

template <typename T>
queue<T>::~queue<T>() noexcept {
  auto curr = this->m_head.load(std::memory_order_relaxed);
  while (curr != nullptr) {
    const auto next = curr->next.load(std::memory_order_relaxed);
    delete curr;
    curr = next;
  }
}

template <typename T>
void queue<T>::enqueue(queue::pointer elem, std::size_t thread_id) {
  if (elem == nullptr) {
    throw std::invalid_argument("enqueue element must not be nullptr");
  }

  while (true) {
    auto tail = this->m_hazard_pointers.protect_ptr(
        this->m_tail.load(),
        thread_id,
        HP_ENQ_TAIL
    );

    if (tail != this->m_tail.load()) {
      continue;
    }

    auto next = tail->next.load();
    if (next != nullptr) {
      this->m_tail.compare_exchange_strong(tail, next);
      continue;
    }

    if (tail->enqueue(elem)) {
      this->m_hazard_pointers.clear_one(thread_id, HP_ENQ_TAIL);
      return;
    }

    auto crq = new crq_node_t(elem);

    if (tail->cas_next(nullptr, crq)) {
      this->m_tail.compare_exchange_strong(tail, crq);
      this->m_hazard_pointers.clear_one(thread_id, HP_ENQ_TAIL);
      return;
    } else {
      delete crq;
    }
  }
}

template <typename T>
typename queue<T>::pointer queue<T>::dequeue(std::size_t thread_id) {
  pointer res;
  while (true) {
    auto head = this->m_hazard_pointers.protect_ptr(
        this->m_head.load(),
        thread_id,
        HP_DEQ_HEAD
    );

    if (head != this->m_head.load()) {
      continue;
    }

    res = head->dequeue();
    if (res != nullptr) {
      break;
    }

    if (head->next.load() == nullptr) {
      res = nullptr;
      break;
    }

    res = head->dequeue();
    if (res != nullptr) {
      break;
    }

    auto next = head->next.load();
    if (next == nullptr) {
      continue;
    } else if (this->m_head.compare_exchange_strong(head, next)) {
      this->m_hazard_pointers.retire(head, thread_id);
    }
  }

  this->m_hazard_pointers.clear_one(thread_id, HP_DEQ_HEAD);
  return res;
}
}

#endif /* LOO_QUEUE_BENCHMARK_LCRQ_HPP */
