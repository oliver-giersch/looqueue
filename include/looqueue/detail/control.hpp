#ifndef LOO_QUEUE_DETAIL_CONTROL_HPP
#define LOO_QUEUE_DETAIL_CONTROL_HPP

#include <atomic>
#include <bit>
#include <cstdint>

namespace loo {
namespace detail {
	struct ctrl_block_t {
		using scalar_t = std::uint64_t;

		enum counter_kind_t : std::uint64_t {
			ENQUEUE = 4,
			DEQUEUE = ENQUEUE + 30,
		};

		enum flags : std::uint64_t {
			SLOTS_VERIFIED = 0x1,
		};

		static constexpr std::uint64_t TOTAL_SHIFT = 15;

		template <counter_kind_t kind>
		static inline constexpr uint64_t
		add_flags(std::uint16_t total_count = 0)
		{
			const auto current_shift = static_cast<std::uint64_t>(kind);
			const auto current = std::uint64_t { 1 } << current_shift;
			const auto total_shift = current_shift + TOTAL_SHIFT;

			return (std::uint64_t { total_count } << total_shift) + current;
		}

		uint8_t flags : 4;
		uint16_t enqueue_current : 15;
		uint16_t enqueue_total : 15;
		uint16_t dequeue_current : 15;
		uint16_t dequeue_total : 15;

		bool
		can_reclaim() const
		{
			if ((this->flags & flags::SLOTS_VERIFIED) == 0)
				return false;
			if (this->enqueue_total == 0 || this->dequeue_total == 0)
				return false;
			if (this->enqueue_current != this->enqueue_total)
				return false;
			if (this->dequeue_current != this->dequeue_total)
				return false;

			return true;
		}
	} __attribute__((packed));

	static_assert(sizeof(ctrl_block_t) == 8, "control block must be 8 bytes");
}
}

#endif /* LOO_QUEUE_DETAIL_CONTROL_HPP */
