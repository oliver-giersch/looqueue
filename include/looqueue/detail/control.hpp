#ifndef LOO_QUEUE_DETAIL_CONTROL_HPP
#define LOO_QUEUE_DETAIL_CONTROL_HPP

#include <bit>

#include "looqueue/def.hpp"

namespace loo {
namespace detail {
struct ctrl_block {
	using scalar = u64;

	enum counter : u64 { enqueue, dequeue };

	enum flags : u64 {
		all_slots_verified = 0x1,
	};

	template <counter kind>
	static inline constexpr u64
	add_flags(bool is_verified, u64 total_count = 0)
	{
		ctrl_block block {};

		if (is_verified)
			block.flags = flags::all_slots_verified;

		switch (kind) {
		case counter::enqueue:
			block.enqueue_current = 1;
			block.enqueue_total = total_count;
			break;
		case counter::dequeue:
			block.dequeue_current = 1;
			block.dequeue_total = total_count;
			break;
		}

		return std::bit_cast<u64>(block);
	}

	u64 flags : 4;
	u64 enqueue_current : 15;
	u64 enqueue_total : 15;
	u64 dequeue_current : 15;
	u64 dequeue_total : 15;

	bool
	can_reclaim() const
	{
		if ((this->flags & flags::all_slots_verified) == 0)
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

static_assert(sizeof(ctrl_block) == sizeof(ctrl_block::scalar),
	"control block size must be 8 bytes");
}
}

#endif /* LOO_QUEUE_DETAIL_CONTROL_HPP */
