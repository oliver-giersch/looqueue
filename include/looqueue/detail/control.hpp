#ifndef LOO_QUEUE_DETAIL_CONTROL_HPP
#define LOO_QUEUE_DETAIL_CONTROL_HPP

#include <bit>

#include "looqueue/def.hpp"

namespace loo {
namespace detail {
struct _ctrl_block {
	using scalar = u64;

	enum counter : u64 {
		enqueue = 0,
		dequeue = 1,
	};

	enum flags : u64 {
		all_slots_verified = 0x1,
	};

	template <counter kind>
	static inline constexpr u64 add_flags(bool is_verified, u64 total_count = 0)
	{
		_ctrl_block block {};

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

	bool can_reclaim() const
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

struct ctrl_block {
	using scalar = u64;

	u64 bits = 0;

	enum counter : u64 {
		enqueue = 0,
		dequeue = 1,
	};

	enum flags : u64 {
		all_slots_verified = 0x1,
	};

	enum : u64 {
		flags_mask = 0xF,
		count_mask = 0x7FFF,
		flags_shift = 4,
		count_shift = 15,
	};

	struct shifts {
		template <counter kind>
		static inline constexpr u64 current =
			flags_shift + (2 * u64 { kind } * count_shift);

		template <counter kind>
		static inline constexpr u64 total = current<kind> + count_shift;
	};

	template <counter kind>
	static inline constexpr u64 add_flags(bool is_verified, u64 total_count = 0)
	{
		u64 bits = is_verified ? flags::all_slots_verified : 0;
		bits |= u64 { 1 } << shifts::current<kind>;
		bits |= (total_count & count_mask) << shifts::total<kind>;

		return (bits);
	}

	inline constexpr u8 flags() const
	{
		return static_cast<u8>(this->bits & flags_mask);
	}

	template <counter kind>
	inline constexpr u16 current_count() const
	{
		const auto count = this->bits >> shifts::current<kind>;

		return static_cast<u16>(count & count_mask);
	}

	template <counter kind>
	inline constexpr u16 total_count() const
	{
		const auto count = this->bits >> shifts::total<kind>;

		return static_cast<u16>(count & count_mask);
	}

	bool can_reclaim() const
	{
		if ((this->flags() & flags::all_slots_verified) == 0)
			return false;

		const auto enqueue_total = this->total_count<enqueue>();
		const auto dequeue_total = this->total_count<dequeue>();

		if (enqueue_total == 0 || dequeue_total == 0)
			return false;
		if (this->current_count<enqueue>() != enqueue_total)
			return false;
		if (this->current_count<dequeue>() != dequeue_total)
			return false;

		return true;
	}
};

static_assert(sizeof(ctrl_block) == sizeof(ctrl_block::scalar),
	"control block size must be 8 bytes");
}
}

#endif /* LOO_QUEUE_DETAIL_CONTROL_HPP */
