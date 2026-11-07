#ifndef LOO_QUEUE_NODE_HPP
#define LOO_QUEUE_NODE_HPP

#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <new>

#include "looqueue/def.hpp"
#include "looqueue/queue_fwd.hpp"

#include "looqueue/detail/control.hpp"

namespace loo {
namespace detail {
inline constexpr usize node_alignment =
	(!config::option_tag_high) ? usize { 1 } << config::tag_bits : 0;
namespace slot {
using slot = usize;
using atomic_slot = std::atomic<slot>;

/* The slot state flag constants. */
enum flags : slot {
	uninit = 0,
	resume = 0b01,
	reader = 0b10,
	elem_mask = ~(reader | resume),
};

/* Returns true if the slot must be abandoned. */
static constexpr bool is_abandoned(slot slot)
{
	return slot == (flags::resume | flags::reader);
}

/* Returns true if a slot has been either consumed or abandoned */
static constexpr bool is_consumed(slot slot)
{
	// Check, if READER and any element bits are set.
	// HACK: this is equivalent to:
	//     `(slot & READER) && (slot & ELEM_MASK)`,
	// but GCC 15 compiles slightly better code with this version.
	return ((slot >= 4) & ((slot >> 1) & 0x1));
}

template <typename T>
static constexpr T cast(slot slot)
{
	const auto bits = slot & flags::elem_mask;
	return reinterpret_cast<T>(bits);
}
}

struct tls_cache {
	void *node = nullptr;
	bool clean = false;

	~tls_cache() noexcept
	{
		if (this->node == nullptr)
			return;

		if constexpr (detail::config::option_tag_high)
			::operator delete(this->node);
		else
			::operator delete(this->node,
				std::align_val_t { detail::node_alignment });
	}

	std::pair<void *, bool> alloc() noexcept
	{
		if (this->node == nullptr)
			return { nullptr, false };

		const auto res = std::make_pair(this->node, this->clean);
		this->node = nullptr;
		this->clean = false;
		return res;
	}

	bool free(void *node, bool is_clean = false) noexcept
	{
		if (this->node == nullptr) {
			this->node = node;
			this->clean = is_clean;
			return true;
		}

		return false;
	}
};
}

template <typename T>
template <usize N>
struct alignas(detail::node_alignment) queue<T>::queue_node {
	using ctrl_block = detail::ctrl_block;
	using slot = detail::slot::slot;
	using atomic_slot = detail::slot::atomic_slot;
	using slot_array = std::array<atomic_slot, node_size>;

	static inline thread_local detail::tls_cache cache {};

	/* The control block for coordinating memory reclamation. */
	std::atomic<detail::ctrl_block::scalar> ctrl {};
	/* The pointer to this node's successor. */
	std::atomic<node *> next { nullptr };
	/* The array of individual slots for storing elements + state bits. */
	slot_array slots {};

	static queue_node *alloc()
	{
		auto [node, is_clean] = cache.alloc();
		if (node != nullptr && !is_clean)
			new (node) queue_node {};
		else if (node == nullptr)
			node = new queue_node {};

		return reinterpret_cast<queue_node *>(node);
	}

	void free(bool is_clean)
	{
		if (cache.free(this, is_clean))
			return;
		delete this;
	}

	void init(pointer elem)
	{
		this->slots[0] = reinterpret_cast<slot>(elem);
	}

	pointer consume_slot(usize idx)
	{
		using detail::slot::flags;

		slot state;
		pointer res;

		// We spin for a bounded number of times, in order to decrease the
		// probability of prematurely abandoning a slot and causing (temporary)
		// livelock situations.
		auto &slot = this->slots[idx];
		for (auto i = 0; i < 16; i++) {
			state = slot.load(relaxed);
			res = detail::slot::cast<pointer>(state);

			if (res != nullptr) {
				state = slot.fetch_add(flags::reader, acq_rel);
				goto found;
			}
		}

		// Check the extracted pointer bits, if the result is null, the deque
		// thread must have set the READ bit before the pointer bits have been
		// set by the corresponding enqueue operation, yet.
		state = slot.fetch_add(flags::reader, acq_rel);
		res = detail::slot::cast<pointer>(state);

		// The RESUME bit may be set, but cleanup is not our responsibility,
		// if the element bits were not yet set, in either case, the slot must
		// be abandoned.
		if (res == nullptr) [[unlikely]]
			return nullptr;

	found:
		if (state & flags::resume) [[unlikely]]
			this->try_reclaim(idx + 1);

		return res;
	}

	/*
	 * NOTE: bitwise-add and bitwise-or behave exactly the same, as long as
	 * there is no bitwise carry.
	 * As long as we can guarantee, that we never add a 1 bit to another 1 bit,
	 * FAA can be used instead of FOR.
	 * This is useful, because FOR is generally implemented as a CAS-loop, even
	 * on x86-64.
	 */

	void try_reclaim(usize start)
	{
		if (!verify_slots_consumed(start))
			return;

		const auto flag = ctrl_block::flags::all_slots_verified;
		const auto state = flag + this->ctrl.fetch_add(flag, acq_rel);
		const auto block = std::bit_cast<ctrl_block>(state);

		if (block.can_reclaim())
			this->free(false);
	}

	bool verify_slots_consumed(usize start)
	{
		using detail::slot::flags;

		for (usize idx = start; idx < node_size; ++idx) {
			const auto old = this->slots[idx].fetch_add(flags::resume, acq_rel);
			if (!detail::slot::is_consumed(old))
				return false;
		}

		return true;
	}

	bool increment_enqueue_count(u16 total_count = 0)
	{
		constexpr auto kind = ctrl_block::counter::enqueue;

		const auto flags = ctrl_block::add_flags<kind>(false, total_count);
		const auto state = flags + this->ctrl.fetch_add(flags, acq_rel);
		const auto block = std::bit_cast<ctrl_block>(state);

		return block.can_reclaim();
	}

	bool increment_dequeue_count(bool is_verified, u16 total_count = 0)
	{
		constexpr auto kind = ctrl_block::counter::dequeue;

		const auto flags = ctrl_block::add_flags<kind>(is_verified, total_count);
		const auto state = flags + this->ctrl.fetch_add(flags, acq_rel);
		const auto block = std::bit_cast<ctrl_block>(state);

		return block.can_reclaim();
	}
};
}

#endif /* LOO_QUEUE_NODE_HPP */
