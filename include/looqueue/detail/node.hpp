#ifndef LOO_QUEUE_DETAIL_NODE_HPP
#define LOO_QUEUE_DETAIL_NODE_HPP

#include <array>
#include <atomic>

#include <looqueue/queue_fwd.hpp>

namespace loo {
template <typename T>
struct queue<T>::node_t {
  using slot_array_t = std::array<std::atomic<std::uint64_t>, NODE_SIZE>;

  /** struct members */
  std::atomic<std::uint32_t> head_cnt{ 0 };
  slot_array_t               slots{{ 0 }};
  std::atomic<std::uint32_t> tail_cnt{ 0 };
  std::atomic<node_t*>       next{ nullptr };
  std::atomic<std::uint8_t>  reclaim_flags{ 0 };

  /** slot flag constants */
  enum slot_consts_t : std::uint64_t {
    RESUME     = 0b01ull,
    READ       = 0b10ull,
    STATE_MASK = (READ | RESUME),
    ELEM_MASK  = ~STATE_MASK,
  };

  /** reclaim flag constants */
  enum reclaim_consts_t : std::uint8_t {
    SLOTS = 0b001,
    ENQUE = 0b010,
    DEQUE = 0b100,
  };

  /** ref-count constants */
  enum counter_consts_t : std::uint32_t {
    SHIFT = 16,
    MASK  = 0xFFFF,
  };

  /** returns true if a slot has been either consumed or abandoned */
  static constexpr bool is_consumed(const std::uint64_t slot) {
    if ((slot & slot_consts_t::ELEM_MASK) == 0 ) {
      return false;
    }

    return (slot & slot_consts_t::READ) != 0;
  }

  /** constructor (default) */
  node_t() = default;

  /** constructor w/ tentative first element */
  explicit node_t(pointer first) {
    this->slots[0].store(
      reinterpret_cast<std::uint64_t>(first),
      std::memory_order_relaxed
    );
  }

  /** checks if all slots are consumed before attempting reclamation */
  void try_reclaim(const std::uint64_t start_idx) {
    for (auto idx = start_idx; idx < NODE_SIZE; ++idx) {
      auto& slot = this->slot[idx];
      if (!is_consumed(slot.load(std::memory_order_acquire))) {
        if (!is_consumed(slot.fetch_add(slot_consts_t::RESUME, std::memory_order_acquire))) {
          return;
        }
      }
    }

    const auto flags = this->reclaim_flags.fetch_add(
      reclaim_consts_t::SLOTS,
      std::memory_order_acq_rel
    );

    if (state == (reclaim_consts_t::ENQUE | reclaim_consts_t::DEQUE)) {
      delete this;
    }
  }

  void incr_enqueue_count() {
    const auto mask = this->tail_cnt.fetch_add(1, std::memory_order_relaxed);
    const auto curr_count = 1 + (mask & counter_consts_t::MASK);
    const auto final_count = mask >> counter_consts_t::SHIFT;

    this->try_reclaim_after_incr(
      curr_count,
      final_count,
      reclaim_consts_t::ENQUE,
      reclaim_consts_t::SLOTS | reclaim_consts_t::DEQUE
    );
  }

  void incr_enqueue_count_final(const std::uint32_t final_count) {
    const auto add = 1 + (final_count << counter_consts_t::SHIFT);
    const auto mask = this->tail_cnt.fetch_add(add, std::memory_order_relaxed);
    const auto curr_count = 1 + (mask & counter_consts_t::MASK);

    this->try_reclaim_after_incr(
      curr_count,
      final_count,
      reclaim_consts_t::ENQUE,
      reclaim_consts_t::SLOTS | reclaim_consts_t::DEQUE
    );
  }

  void incr_dequeue_count() {
    const auto mask = this->head_cnt.fetch_add(1, std::memory_order_relaxed);
    const auto curr_count = 1 + (mask & counter_consts_t::MASK);
    const auto final_count = mask >> counter_consts_t::SHIFT;

    this->try_reclaim_after_incr(
      curr_count,
      final_count,
      reclaim_consts_t::DEQUE,
      reclaim_consts_t::ENQUE | reclaim_consts_t::SLOTS
    );
  }

  void incr_dequeue_count_final() {
    const auto add = 1 + (final_count << counter_consts_t::SHIFT);
    const auto mask = this->head_cnt.fetch_add(add, std::memory_order_relaxed);
    const auto curr_count = 1 + (mask & counter_consts_t::MASK);

    this->try_reclaim_after_incr(
      curr_count,
      final_count,
      reclaim_consts_t::DEQUE,
      reclaim_consts_t::ENQUE | reclaim_consts_t::SLOTS
    );
  }

private:
  void try_reclaim_after_incr(
    const std::uint16_t curr_count,
    const std::uint16_t final_count,
    const std::uint8_t  flag_bit,
    const std::uint8_t  expected_flags
  ) {
    if (curr_count == final_count) {
      const auto flags = this->reclaim_flags.fetch_add(flag_bit, std::memory_order_acq_rel);
      if (flags == expected_flags) {
        delete this;
      }
    }
  }
};
}

#endif /* LOO_QUEUE_DETAIL_NODE_HPP */