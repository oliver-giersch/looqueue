#ifndef LOO_QUEUE_DETAIL_CONTROL_HPP
#define LOO_QUEUE_DETAIL_CONTROL_HPP

#include <atomic>
#include <bit>
#include <cstdint>

namespace loo {
namespace detail {
	struct ctrl_block_t {
		using scalar_t = std::uint64_t;

		enum counter_kind_t : std::uint64_t { ENQUEUE, DEQUEUE };

		enum flags : std::uint64_t {
			SLOTS_VERIFIED = 0x1,
		};

		template <counter_kind_t kind>
		static inline constexpr uint64_t
		add_flags(bool is_verified, std::uint16_t total_count = 0)
		{
			ctrl_block_t block {};

			if (is_verified)
				block.flags = flags::SLOTS_VERIFIED;

			switch (kind) {
			case counter_kind_t::ENQUEUE:
				block.enqueue_current = 1;
				block.enqueue_total = total_count;
				break;
			case counter_kind_t::DEQUEUE:
				block.dequeue_current = 1;
				block.dequeue_total = total_count;
				break;
			}

			return std::bit_cast<std::uint64_t>(block);
		}

		std::uint64_t flags : 4;
		std::uint64_t enqueue_current : 15;
		std::uint64_t enqueue_total : 15;
		std::uint64_t dequeue_current : 15;
		std::uint64_t dequeue_total : 15;

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
