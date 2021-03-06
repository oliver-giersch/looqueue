#ifndef LOO_QUEUE_NODE_HPP
#define LOO_QUEUE_NODE_HPP

#include <array>
#include <atomic>
#include <cassert>
#include <limits>

#include "looqueue/queue_fwd.hpp"

namespace loo {
template <typename T>
struct queue<T>::node_t {
  using slot_array_t = std::array<atomic_slot_t, NODE_SIZE>;
  /** the control block for managing safe memory reclamation */
  struct ctrl_block_t {
    /** high 16 bits: final observed count of slow-path enqueue ops, low 16 bits: current count */
    std::atomic_uint32_t tail_cnt{ 0 };
    /** high 16 bits: final observed count of slow-path dequeue ops, low 16 bits: current count */
    std::atomic_uint32_t head_cnt{ 0 };
    /** bit mask for storing current reclamation status (all 3 bits set = node can be reclaimed) */
    std::atomic_uint8_t  reclaim_flags{ 0 };
  };

  /** struct members */

  /** control block for memory reclamation */
  ctrl_block_t ctrl{ };
  /** pointer to successor node */
  std::atomic<node_t*> next{ nullptr };
  /** array of individual slots for storing elements + state bits */
  slot_array_t slots{ };

  /** slot flag constants */
  enum slot_flags_t : std::uintptr_t {
    UNINIT    = std::uintptr_t{ 0 },
    RESUME    = std::uintptr_t{ 0b01 },
    READER    = std::uintptr_t{ 0b10 },
    ELEM_MASK = ~(READER | RESUME),
  };

  /** ref-count constants */
  enum counter_flags_t : std::uint32_t {
    /** bit-shift for accessing the high 16 bits (the final completed operations count) */
    SHIFT = std::uint32_t{ 16 },
    /** mask for the lower 16 bit (the current completed operations count) */
    MASK  = std::uint32_t{ 0xFFFF },
  };

  /** reclaim flag constants */
  enum reclaim_flags_t : std::uint8_t {
    /** all slots have been visited & determined to be consumed */
    ARR = std::uint8_t{ 0b001 },
    /** all slow path enqueue ops have completed and the node can't be observed by new operations */
    ENQ = std::uint8_t{ 0b010 },
    /** all slow path dequeue ops have completed and the node can't be observed by new operations */
    DEQ = std::uint8_t{ 0b100 },
  };

  /** returns true if a slot has been either consumed or abandoned */
  static constexpr auto is_consumed(slot_t slot) {
    if ((slot & slot_flags_t::ELEM_MASK) == 0 ) {
      // there are no pointer (elem) bits set, so no writer has visited the slot yet
      return false;
    }

    // no READ bit set means no reader has visited the slot yet
    return (slot & slot_flags_t::READER) == slot_flags_t::READER;
  }

  /** constructor (default) */
  node_t() {
    for (auto& slot : this->slots) {
      slot.store(slot_flags_t::UNINIT, relaxed);
    }
  };

  /** constructor w/ tentative first element */
  explicit node_t(pointer first) : node_t() {
    this->slots[0].store(reinterpret_cast<slot_t>(first), relaxed);
  }

  /** checks if all slots are consumed before attempting reclamation */
  void try_reclaim(std::uint64_t start_idx) {
    // iterate all slots beginning at `start_idx`
    for (std::uint64_t idx = start_idx; idx < NODE_SIZE; ++idx) {
      auto& slot = this->slots[idx];
      if (!is_consumed(slot.load(acquire))) {
        // if the current slot has not already been consumed, set the RESUME bit, check again
        // and abort the iteration if it has still not been consumed
        // once the consuming thread(s) eventually arrive they will observe the RESUME bit and
        // the thread arriving last will resume the procedure from the following slot on
        if (!is_consumed(slot.fetch_add(slot_flags_t::RESUME, relaxed))) {
          return;
        }
      }
    }

    // set the ARR bit, since all slots have been visited, so all fast path enqueue and dequeue
    // ops must have finished and are no longer accessing the node
    const auto flags = this->ctrl.reclaim_flags.fetch_add(reclaim_flags_t::ARR, acq_rel);
    // if all 3 bits are set after setting the SLOTS bit, the node can be reclaimed
    if (flags == (reclaim_flags_t::ENQ | reclaim_flags_t::DEQ)) {
      delete this;
    }
  }

  /**
   * Atomically increases the current operations count in `tail_cnt` and sets the final count if a
   * value other than 0 is passed; if both counts are equal, the ENQ bit is set and the node will
   * be reclaimed, if the other 2 bits have already been set.
   */
  void increment_enqueue_count(std::uint64_t final_count = 0) {
    constexpr auto EXPECTED_FLAGS = reclaim_flags_t::ARR | reclaim_flags_t::DEQ;
    const auto counts = final_count == 0
        ? increment_counter(this->ctrl.tail_cnt)
        : increment_counter_final(this->ctrl.tail_cnt, static_cast<std::uint16_t>(final_count));

    this->try_reclaim_post_increment(counts, reclaim_flags_t::ENQ, EXPECTED_FLAGS);
  }

  /** atomically increases the current operations count in `head_cnt` and sets the final count if a
   * value other than 0 is passed; if both counts are equal, the ENQ bit is set and the node will
   * be reclaimed, if the other 2 bits have already been set. */
  void increment_dequeue_count(std::uint64_t final_count = 0) {
    constexpr auto EXPECTED_FLAGS = reclaim_flags_t::ARR | reclaim_flags_t::ENQ;
    const auto counts = final_count == 0
        ? increment_counter(this->ctrl.head_cnt)
        : increment_counter_final(this->ctrl.head_cnt, static_cast<std::uint16_t>(final_count));

    this->try_reclaim_post_increment(counts, reclaim_flags_t::DEQ, EXPECTED_FLAGS);
  }

private:
  struct counts_t {
    std::uint16_t curr_count, final_count;
  };

  /** increases the current count in `counter` */
  static counts_t increment_counter(std::atomic_uint32_t& counter) {
    const auto mask = counter.fetch_add(std::uint32_t{ 1 }, release);
    const auto final_count = static_cast<std::uint16_t>(mask >> counter_flags_t::SHIFT);

    return {
      static_cast<std::uint16_t>((mask & counter_flags_t::MASK) + 1),
      final_count
    };
  }

  /** increases the current count in `counter` and also (atomically) sets the final count */
  static counts_t increment_counter_final(
      std::atomic_uint32_t& counter,
      std::uint16_t final_count
  ) {
    const auto add = std::uint32_t{ 1 } + (std::uint32_t{ final_count } << counter_flags_t::SHIFT);
    const auto mask = counter.fetch_add(add, release);

    return {
        static_cast<std::uint16_t>((mask & counter_flags_t::MASK) + 1),
        final_count
    };
  }

  /**
   * Compares the two counts, sets `flag_bit` in this node's reclaim flags and proceeds to
   * de-allocate the node if the reclaim flags before setting the bit were equal to
   * `expected_flags`.
   */
  void try_reclaim_post_increment(
      counts_t counts,
      std::uint8_t flag_bit,
      std::uint8_t expected_flags
  ) {
    if (counts.curr_count == counts.final_count) {
      const auto flags = this->ctrl.reclaim_flags.fetch_add(flag_bit, acq_rel);
      if (flags == expected_flags) {
        delete this;
      }
    }
  }
};
}

#endif /* LOO_QUEUE_NODE_HPP */
