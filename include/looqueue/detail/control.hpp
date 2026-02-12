#ifndef LOO_QUEUE_DETAIL_CONTROL_HPP
#define LOO_QUEUE_DETAIL_CONTROL_HPP

#include <atomic>
#include <bit>
#include <cstdint>

namespace loo {
namespace detail {
	struct counts_t {
		std::uint16_t current;
		std::uint16_t total;
	};

	struct ctrl_block_t {
		using scalar_t = std::uint64_t;

		enum counter_kind_t : std::uint64_t {
			ENQUEUE = 4,
			DEQUEUE = ENQUEUE + 30,
		};

		enum counter_flags_t : std::uint64_t {
			ARR = 0b001,
			ENQ = 0b010,
			DEQ = 0b100,
		};

		static constexpr std::uint64_t TOTAL_SHIFT = 15;

		template <counter_kind_t kind>
		static inline constexpr uint64_t
		add_flags(std::uint16_t final_count = 0)
		{
			const auto current_shift = static_cast<std::uint64_t>(kind);
			const auto current = std::uint64_t { 1 } << current_shift;
			const auto total_shift = current_shift + TOTAL_SHIFT;
			return (std::uint64_t { final_count } << total_shift) + current;
		}

		uint8_t flags : 4;
		uint16_t enqueue_current : 15;
		uint16_t enqueue_total : 15;
		uint16_t dequeue_current : 15;
		uint16_t dequeue_total : 15;
	} __attribute__((packed));

	static_assert(sizeof(ctrl_block_t) == 8, "control block must be 8 bytes");

	template <ctrl_block_t::counter_kind_t kind>
	static counts_t
	increment_count(std::atomic<ctrl_block_t::scalar_t> &ctrl,
		std::uint16_t final_count = 0)
	{
		const auto flags = ctrl_block_t::add_flags<kind>(final_count);
		const old = ctrl.fetch_add(flags, release);

		const block = std::bit_cast<ctrl_block_t>(old);
		switch (kind) {
		case ENQUEUE:
			return {
				.current = block.enqueue_current,
				.total = block.enqueue_total,
			};
		case DEQUEUE:
			return {
				.current = block.dequeue_current,
				.total = block.dequeue_total,
			};
		}
	}
}
}

#endif /* LOO_QUEUE_DETAIL_CONTROL_HPP */
