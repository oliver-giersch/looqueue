#ifndef LOO_QUEUE_NODE_HPP
#define LOO_QUEUE_NODE_HPP

#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <limits>

#include "looqueue/queue_fwd.hpp"

#include "looqueue/detail/control.hpp"

namespace loo {
namespace detail {
	namespace slot {
		using slot_t = std::uintptr_t;
		using atomic_slot_t = std::atomic<slot_t>;

		/* The slot state flag constants. */
		enum flags : slot_t {
			UNINIT = slot_t { 0 },
			RESUME = slot_t { 0b01 },
			READER = slot_t { 0b10 },
			ELEM_MASK = ~(READER | RESUME),
		};

		/* Returns if the slot must be abandoned. */
		static constexpr bool
		is_abandoned(slot_t slot)
		{
			return slot == (flags::RESUME | flags::READER);
		}

		/* Returns true if a slot has been either consumed or abandoned */
		static constexpr bool
		is_consumed(slot_t slot)
		{
			// Check, if READER and any element bits are set.
			// HACK: this is equivalent to `(slot & READER) && (slot & ELEM_MASK)`,
			// but GCC 15 compiles slightly better code with this version.
			return ((slot >= 4) & ((slot >> 1) & 0x1));
		}

		template <typename T>
		static constexpr T
		cast(slot_t slot)
		{
			const auto bits = slot & flags::ELEM_MASK;
			return reinterpret_cast<T>(bits);
		}
	}

	struct tls_cache {
		void *node = nullptr;

		~tls_cache() noexcept
		{
			if (this->node != nullptr)
				::operator delete(this->node);
		}

		void *
		alloc() noexcept
		{
			if (this->node == nullptr)
				return nullptr;

			const auto res = this->node;
			this->node = nullptr;
			return res;
		}

		bool
		free(void *node) noexcept
		{
			if (this->node == nullptr) {
				this->node = node;
				return true;
			}

			return false;
		}
	};
}

template <typename T>
struct queue<T>::node_t {
	using ctrl_block_t = detail::ctrl_block_t;
	using slot_t = detail::slot::slot_t;
	using atomic_slot_t = detail::slot::atomic_slot_t;
	using slot_array_t = std::array<atomic_slot_t, NODE_SIZE>;

	static inline thread_local detail::tls_cache cache {};

	/* The control block for coordinating memory reclamation. */
	std::atomic<detail::ctrl_block_t::scalar_t> ctrl {};
	/* The pointer to this node's successor. */
	std::atomic<node_t *> next { nullptr };
	/* The array of individual slots for storing elements + state bits. */
	slot_array_t slots;

	node_t() : slots {} { }

	explicit node_t(pointer e0)
	{
		this->slots[0].store(reinterpret_cast<slot_t>(e0), relaxed);
		for (auto i = 1; i < NODE_SIZE; ++i) {
			this->slots[i].store(detail::slot::flags::UNINIT, relaxed);
		}
	}

	void *
	operator new(std::size_t size)
	{
		const auto node = cache.alloc();
		if (node != nullptr)
			return node;
		return ::operator new(size);
	}

	void
	operator delete(void *node)
	{
		if (cache.free(node))
			return;
		::operator delete(node);
	}

	pointer
	consume_slot(std::size_t idx)
	{
		using detail::slot::flags;

		slot_t state;
		pointer res;

		// We spin for a bounded number of times, in order to decrease the
		// probability of prematurely abandoning a slot and causing (temporary)
		// livelock situations.
		auto &slot = this->slots[idx];
		for (auto i = 0; i < 16; i++) {
			state = slot.load(relaxed);
			res = detail::slot::cast<pointer>(state);

			if (res != nullptr) {
				state = slot.fetch_add(flags::READER, acquire);
				goto found;
			}
		}

		// Check the extracted pointer bits, if the result is null, the deque
		// thread must have set the READ bit before the pointer bits have been
		// set by the corresponding enqueue operation, yet.
		state = slot.fetch_add(flags::READER, acquire);
		res = detail::slot::cast<pointer>(state);

		// The RESUME bit may be set, but cleanup is not our responsibility,
		// if the element bits were not yet set, in either case, the slot must
		// be abandoned.
		if (res == nullptr) [[unlikely]]
			return nullptr;

	found:
		if (state & flags::RESUME) [[unlikely]]
			this->try_reclaim(idx + 1);

		return res;
	}

	void
	try_reclaim(std::size_t start_idx)
	{
		if (!verify_slots_consumed(start_idx))
			return;

		const auto flag = ctrl_block_t::flags::SLOTS_VERIFIED;
		const auto state = flag | this->ctrl.fetch_add(flag, acq_rel);
		const auto block = std::bit_cast<ctrl_block_t>(state);

		if (block.can_reclaim())
			delete this;
	}

	bool
	verify_slots_consumed(std::size_t start_idx)
	{
		using detail::slot::flags;

		for (std::size_t idx = start_idx; idx < NODE_SIZE; ++idx) {
			auto &slot = this->slots[idx];
			// TODO: I am not actually sure, if lock; xadd is faster than cmpxchg?
			// It is wait-free, but I don't know if it is also faster in case of no
			// contention
			const auto old = slot.fetch_add(flags::RESUME, relaxed);
			if (!detail::slot::is_consumed(old))
				return false;
		}

		return true;
	}

	bool
	increment_enqueue_count(std::uint16_t total_count = 0)
	{
		constexpr auto kind = ctrl_block_t::counter_kind_t::ENQUEUE;

		const auto flags = ctrl_block_t::add_flags<kind>(total_count);
		const auto state = flags | this->ctrl.fetch_add(flags, acq_rel);
		const auto block = std::bit_cast<ctrl_block_t>(state);

		return block.can_reclaim();
	}

	bool
	increment_dequeue_count(bool verified, std::uint16_t total_count = 0)
	{
		constexpr auto kind = ctrl_block_t::counter_kind_t::DEQUEUE;

		auto flags = ctrl_block_t::add_flags<kind>(total_count);
		if (verified)
			flags |= ctrl_block_t::flags::SLOTS_VERIFIED;
		const auto state = flags | this->ctrl.fetch_add(flags, acq_rel);
		const auto block = std::bit_cast<ctrl_block_t>(state);

		return block.can_reclaim();
	}
};

// template <typename T>
// thread_local detail::tls_cache queue<T>::node_t::cache {};
}

#endif /* LOO_QUEUE_NODE_HPP */
