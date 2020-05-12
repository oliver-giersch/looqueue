#ifndef LOO_QUEUE_DETAIL_NODE_HPP
#define LOO_QUEUE_DETAIL_NODE_HPP

#include <array>
#include <atomic>
#include <cassert>
#include <limits>

#include <cstdlib>

#include <looqueue/queue_fwd.hpp>

namespace loo {
template <typename T>
struct queue<T>::node_t {
  /** alias for an array of `NODE_SIZE` atomic marked element pointers (64-bit) */
  using slot_array_t = std::array<queue::atomic_slot_t, queue::NODE_SIZE>;

  /** struct members */

  /** high 16 bit: final observed count of slow-path dequeue ops, low 16 bit: current count */
  std::atomic<std::uint32_t> head_cnt{ 0 };
  /** array of individual slots for storing elements + state bits */
  slot_array_t               slots;
  /** high 16 bit: final observed count of slow-path enqueue ops, low 16 bit: current count */
  std::atomic<std::uint32_t> tail_cnt{ 0 };
  /** pointer to successor node */
  std::atomic<node_t*>       next{ nullptr };
  /** bit mask for storing current reclamation status (all 3 bits set = node can be reclaimed) */
  std::atomic<std::uint8_t>  reclaim_flags{ 0 };

  /** slot flag constants */
  enum slot_consts_t : std::uint64_t {
    UNINIT     = 0b00ull,
    RESUME     = 0b01ull,
    READER     = 0b10ull,
    STATE_MASK = (READER | RESUME),
    ELEM_MASK  = ~STATE_MASK,
  };

  /** reclaim flag constants */
  enum reclaim_consts_t : std::uint8_t {
    /** all slots have been visited & determined to be consumed */
    SLOTS = 0b001,
    /** all slow path enqueue ops have completed and the node can't be observed by new operations */
    ENQUE = 0b010,
    /** all slow path dequeue ops have completed and the node can't be observed by new operations */
    DEQUE = 0b100,
  };

  /** ref-count constants */
  enum counter_consts_t : std::uint32_t {
    /** bitshift for accessing the high 16 bits (the final completed operations count) */
    SHIFT = 16,
    /** mask for the lower 16 bit (the current completed operations count) */
    MASK  = 0xFFFF,
  };

  /** returns true if a slot has been either consumed or abandoned */
  static constexpr bool is_consumed(const queue::slot_t slot) {
    if ((slot & slot_consts_t::ELEM_MASK) == 0 ) {
      // there are no pointer (elem) bits set, so no writer has visited the slot yet
      return false;
    }

    // no READ bit is set means no reader has visited the slot yet
    return (slot & slot_consts_t::READER) != 0;
  }

  void* operator new(std::size_t size) {
    return std::aligned_alloc(size, queue::NODE_ALIGN);
  }

  void operator delete(void* ptr) {
    free(ptr);
  }

  /** constructor (default) */
  node_t() {
    for (auto& slot : this->slots) {
      std::atomic_init(&slot, slot_consts_t::UNINIT);
    }
  };

  /** constructor w/ tentative first element */
  explicit node_t(pointer first) : node_t() {
    this->slots[0].store(
      reinterpret_cast<queue::slot_t>(first),
      std::memory_order_relaxed
    );
  }

  /** checks if all slots are consumed before attempting reclamation */
  void try_reclaim(const std::uint64_t start_idx) {
    // iterate all slots beginning at `start_idx`
    for (auto idx = start_idx; idx < queue::NODE_SIZE; ++idx) {
      auto& slot = this->slots[idx];
      if (!is_consumed(slot.load(std::memory_order_relaxed))) {
        // if the current slot has not already been consumed, set the RESUME bit, check again
        // and abort the iteration if it has still not been consumed
        // once the consuming thread(s) eventually arrive they will observe the RESUME bit and
        // the thread arriving last will resume the procedure from the following slot on
        if (!is_consumed(slot.fetch_add(slot_consts_t::RESUME, std::memory_order_relaxed))) {
          return;
        }
      }
    }

    // set the SLOTS bit, since all slots have been visited, so all fast path enqueue and dequeue
    // ops must have finished and are no longer accessing the node
    const auto flags = this->reclaim_flags.fetch_add(
      reclaim_consts_t::SLOTS,
      std::memory_order_acq_rel
    );

    // if all 3 bits are set after setting the SLOTS bit, the node can be reclaimed
    if (flags == (reclaim_consts_t::ENQUE | reclaim_consts_t::DEQUE)) {
      delete this;
    }
  }

  /** atomically increases the current operations count in `tail_cnt` and sets the final count if a
   *  value other than 0 is passed; if both counts are equal, the ENQUE bit is set and the node will
   *  be reclaimed, if the other 2 bits have already been set. */
  void incr_enqueue_count(const std::uint64_t final_count = 0) {
    assert(final_count < std::numeric_limits<std::uint16_t>::max());
    const auto counts = incr_count(this->tail_cnt, static_cast<std::uint16_t>(final_count));

    this->try_reclaim_after_incr(
      counts,
      reclaim_consts_t::ENQUE,
      reclaim_consts_t::SLOTS | reclaim_consts_t::DEQUE
    );
  }

  /** atomically increases the current operations count in `head_cnt` and sets the final count if a
   * value other than 0 is passed; if both counts are equal, the ENQUE bit is set and the node will
   * be reclaimed, if the other 2 bits have already been set. */
  void incr_dequeue_count(const std::uint64_t final_count = 0) {
    assert(final_count < std::numeric_limits<std::uint16_t>::max());
    const auto counts = incr_count(this->head_cnt, static_cast<std::uint16_t>(final_count));

    this->try_reclaim_after_incr(
      counts,
      reclaim_consts_t::DEQUE,
      reclaim_consts_t::ENQUE | reclaim_consts_t::SLOTS
    );
  }

private:
  struct counts_t {
    std::uint16_t curr_count, final_count;
  };

  /** increases the current count in `counter` and also (atomically) sets the final count, if a
   *  value other than 0 is passed to this function */
  static counts_t incr_count(
    std::atomic<std::uint32_t>& counter,
    std::uint16_t final_count = 0
  ) {
    const auto add = final_count == 0
      ? 1
      : 1 + (static_cast<std::uint32_t>(final_count) << counter_consts_t::SHIFT);
    const auto mask = counter.fetch_add(add, std::memory_order_relaxed);

    final_count = final_count == 0
      ? static_cast<std::uint16_t>(mask >> counter_consts_t::SHIFT)
      : final_count;

    return {
      static_cast<std::uint16_t>(1 + (mask & counter_consts_t::MASK)),
      final_count
    };
  }

  /** compares the two counts, sets `flag_bit` in this node's reclaim flags and proceeds to
   *  de-allocate the node if the reclaim flags before setting the bit were equal to
   *  `expected_flags` */
  void try_reclaim_after_incr(
    const counts_t     counts,
    const std::uint8_t flag_bit,
    const std::uint8_t expected_flags
  ) {
    if (counts.curr_count == counts.final_count) {
      const auto flags = this->reclaim_flags.fetch_add(flag_bit, std::memory_order_acq_rel);
      if (flags == expected_flags) {
        delete this;
      }
    }
  }
};
}

#endif /* LOO_QUEUE_DETAIL_NODE_HPP */
