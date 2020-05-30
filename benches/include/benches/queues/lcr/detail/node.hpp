#ifndef LOO_QUEUE_BENCHMARK_LCRQ_DETAIL_NODE_HPP
#define LOO_QUEUE_BENCHMARK_LCRQ_DETAIL_NODE_HPP

#include "benches/queues/lcr/lcrq_fwd.hpp"

#include <array>

#include "looqueue/align.hpp"

namespace lcr {
namespace detail {
  bool test_and_set_mst(std::atomic<std::uint64_t>& atomic) {
    std::uint8_t res;
    asm volatile("lock btsq $63, %0; setnc %1" : "+m"(*&atomic), "=a"(res) :: "memory");
    return res != 0;
  }
}

/** queue::cell_t definition */
template <typename T>
struct alignas(64) queue<T>::cell_t {
  std::atomic<std::uint64_t>  idx;
  std::atomic<queue::pointer> val;

  bool cmpxchg16b(queue::cell_pair_t exp, queue::cell_pair_t des) {
    std::uint8_t res;
    asm volatile(
      "lock; cmpxchg16b %0; setz %1"
      : "=m"(*this), "=a"(res)
      : "m"(*this), "a"(exp.first), "d"(exp.second), "b"(des.first), "c"(des.second)
      : "memory"
    );

    return res != 0;
  }
};

/** queue::crq_node_t definition */
template <typename T>
struct queue<T>::crq_node_t {
  alignas(CACHE_LINE_SIZE) std::atomic<std::uint64_t>    head_ticket{};
  alignas(CACHE_LINE_SIZE) std::atomic<std::uint64_t>    tail_ticket{};
  alignas(CACHE_LINE_SIZE) std::atomic<crq_node_t*>      next;
  alignas(CACHE_LINE_SIZE) std::array<cell_t, RING_SIZE> cells;

  crq_node_t();
  explicit crq_node_t(pointer first);

  void init_cells();
  bool enqueue(queue::pointer elem);
  queue::pointer dequeue();
  bool cas_next(crq_node_t* cmp, crq_node_t* val);
  void fix_state();
};

template <typename T>
queue<T>::crq_node_t::crq_node_t() :
  head_ticket{ 0 }, tail_ticket{ 0 }, next{ nullptr }
{
  this->init_cells();
}

template <typename T>
queue<T>::crq_node_t::crq_node_t(queue::pointer first) : crq_node_t() {
  this->cells[0].val.store(first, std::memory_order_relaxed);
  this->cells[0].idx.store(STATUS_BIT | 0ull, std::memory_order_relaxed);
}

template <typename T>
void queue<T>::crq_node_t::init_cells() {
  for (std::size_t idx = 0; idx < RING_SIZE; ++idx) {
    std::atomic_init(&this->cells[idx].idx, STATUS_BIT | idx);
    std::atomic_init(&this->cells[idx].val, nullptr);
  }
}

template <typename T>
bool queue<T>::crq_node_t::enqueue(queue::pointer elem) {
  auto attempts = 0;
  while (true) {
    const auto decomposed_tail = decompose_index(this->tail_ticket.fetch_add(1));
    if (decomposed_tail.status_bit == STATUS_BIT) {
      return false;
    }

    auto& cell = this->cells[decomposed_tail.idx % RING_SIZE];
    auto val = cell.val.load();

    const auto composed_idx = cell.idx.load();
    const auto decomposed_idx = decompose_index(composed_idx);
    const auto safe_bit = decomposed_idx.status_bit;
    const auto idx = decomposed_idx.idx;

    if (val == nullptr) {
      if (
          idx <= decomposed_tail.idx
          && (
              safe_bit == STATUS_BIT
              || this->head_ticket.load() <= decomposed_tail.idx
          )
      ) {
        const cell_pair_t desired = { (STATUS_BIT | decomposed_tail.idx), elem };
        if (cell.dw_cas({ composed_idx, nullptr }, desired)) {
          return true;
        }
      }
    }

    auto head = this->head_ticket.load();
    if (decomposed_tail.idx - head >= RING_SIZE or attempts >= 10) {
      detail::test_and_set_mst(this->tail_ticket);
      return false;
    }

    ++attempts;
  }
}

template <typename T>
typename queue<T>::pointer queue<T>::crq_node_t::dequeue() {
  while (true) {
    const auto head = this->head_ticket.fetch_add(1);
    auto& cell = this->cells[head % RING_SIZE];

    while (true) {
      auto val = cell.val.load();
      const auto composed_idx = cell.idx.load();

      const auto decomp_idx = decompose_index(composed_idx);
      const auto safe_bit = decomp_idx.first;
      const auto idx = decomp_idx.idx;

      if (idx > head) {
        break;
      }

      if (val != nullptr) {
        if (idx == head) {
          const cell_pair_t expected = { safe_bit | head, val };
          const cell_pair_t desired  = { safe_bit | (head + RING_SIZE), nullptr };

          if (cell.double_cas(expected, desired)) {
            return val;
          }
        } else {
          // marks cell as unsafe to prevent further enqueues
          if (cell.double_cas({ composed_idx, val }, { idx, val })) {
            break;
          }
        }
      } else {
        // attempt empty transition
        const cell_pair_t expected = { (safe_bit | (head + RING_SIZE)), nullptr };
        if (cell.double_cas({ composed_idx, nullptr }, expected)) {
          break;
        }
      }
    }

    const auto tail = decompose_index_value(this->tail_ticket.load());
    if (tail <= head + 1) {
      this->fix_state();
      return nullptr;
    }
  }
}

template <typename T>
bool queue<T>::crq_node_t::cas_next(
    queue<T>::crq_node_t* cmp,
    queue<T>::crq_node_t* val
) {
  return this->next.compare_exchange_strong(cmp, val);
}

template <typename T>
void queue<T>::crq_node_t::fix_state() {
  while (true) {
    const auto tail = this->tail_ticket.fetch_add(0);
    const auto head = this->head_ticket.fetch_add(0);

    if (this->tail_ticket.load() != tail) {
      continue;
    }

    if (head <= tail) {
      return;
    }

    auto expected = tail;
    if (this->tail_ticket.compare_exchange_strong(expected, head)) {
      return;
    }
  }
}

}

#endif /* LOO_QUEUE_BENCHMARK_LCRQ_DETAIL_NODE_HPP */
