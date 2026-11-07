#ifndef LOO_QUEUE_FWD_HPP
#define LOO_QUEUE_FWD_HPP

#include <atomic>

#include "align.hpp"
#include "def.hpp"

#ifndef OPTION_TAG_HIGH
#define OPTION_TAG_HIGH 0
#endif /* OPTION_TAG_HIGH */

#if (OPTION_TAG_HIGH == 1)
#include "looqueue/detail/native_tag_ptr.hpp"
#else
#include "looqueue/detail/tag_ptr.hpp"
#endif /* OPTION_TAG_HIGH */

namespace loo {
namespace detail {
/* The result type for `try_advance_head`. */
enum class advance_head { empty_queue, advanced };
/* The result type for `try_advance_tail`. */
enum class advance_tail { advanced, advanced_and_inserted };

namespace config {
inline constexpr auto option_tag_high = OPTION_TAG_HIGH == 1;
inline constexpr auto tag_bits = 12;
}
}

template <typename T>
class queue {
public:
	using pointer = T *;

private:
	static_assert(sizeof(T *) == sizeof(u64),
		"loo::queue is only compatible with 64-bit architectures");
	static_assert(alignof(T) >= 4,
		"all T pointers must be at least 4-byte aligned");

	/*
	 * The number of slots for storing individual elements in each node.
	 *
	 * NOTE: At 8-byte control blocks and slots and next pointer, this
	 * value results in nodes that fit neatly into a single 4 KiB page.
	 */
	static constexpr auto node_size = usize { 510 };
	static constexpr auto tag_bits = detail::config::tag_bits;

	static constexpr auto relaxed = std::memory_order_relaxed;
	static constexpr auto acquire = std::memory_order_acquire;
	static constexpr auto release = std::memory_order_release;
	static constexpr auto acq_rel = std::memory_order_acq_rel;

	using node_tag_ptr = usize;
	using atomic_node_tag_ptr = std::atomic<node_tag_ptr>;

	template <usize N>
	struct queue_node;

	using node = queue_node<node_size>;

	static_assert(sizeof(node) <= 4096, "node size must be <= 4 KiB");

#if (OPTION_TAG_HIGH == 1)
	using tag_ptr = detail::native_tag_ptr<node, tag_bits>;
#else
	using tag_ptr = detail::tag_ptr<node, tag_bits>;
	static_assert(alignof(node) >= (1ull << tag_bits),
		"node alignment must be >= 4 KiB");
#endif /* OPTION_TAG_HIGH */

	/* The head node pointer & dequeue index pair tag pointer. */
	alignas(detail::cache_alignment) atomic_node_tag_ptr head;
	/* The head node epoch counter (same cache-line as head). */
	std::atomic<u64> head_epoch;

	/* The tail node pointer & enqueue index pair tag pointer. */
	alignas(detail::cache_alignment) atomic_node_tag_ptr tail;

	/* The cached value of the last observed tail pointer. */
	alignas(detail::cache_alignment) std::atomic<node *> cached_tail;

public:
	/** See PROOF.md for the reasoning behind these constants */

	static constexpr usize max_producer_threads =
		(usize { 1 } << tag_bits) - node_size - 1;
	static constexpr usize max_consumer_threads = max_producer_threads / 2;

	queue();
	~queue() noexcept;

	/* Enqueue an element at the back of the queue. */
	void enqueue(pointer elem);
	/* Dequeue an element from the front of the queue. */
	pointer dequeue();

	queue(const queue &) = delete;
	queue(queue &&) = delete;
	queue &operator=(const queue &) = delete;
	queue &operator=(queue &&) = delete;

private:
	/* Returns true if the queue is empty. */
	bool is_empty() noexcept;

	/*
	 * Attempts to advance the tail node to its successor if there is one or
	 * attempts to append a new node with `elem` stored in the first slot
	 * otherwise.
	 */
	detail::advance_tail try_advance_tail(tag_ptr tag_tail, node *tail,
		pointer elem);

	/* Attempts to advance the head node to its successor, if there is one. */
	detail::advance_head try_advance_head(tag_ptr tag_head, node *head,
		bool verify) noexcept;

	/*
	 * Loops and attempts to CAS `expected` with `desired` until either the CAS
	 * succeeds or the loaded pointer value (failure case) no longer matches
	 * `old_node`
	 */
	static bool bounded_cas_loop(atomic_node_tag_ptr &ptr, tag_ptr &expected,
		tag_ptr desired, const node *old_node, std::memory_order order);

	/* Loops until the queue's tail is updated by any thread. */
	inline bool cas_tail(tag_ptr &expected, tag_ptr desired, node *tail)
	{
		return bounded_cas_loop(this->tail, expected, desired, tail, release);
	}

	/* Loops until the queue's head is updated by any thread. */
	inline bool cas_head(tag_ptr &expected, tag_ptr desired, node *head)
	{
		return bounded_cas_loop(this->head, expected, desired, head, release);
	}

	/* Attempts once to CAS the cached tail if it equals the current head. */
	inline void cas_cached_tail(node *head, node *tail)
	{
		this->cached_tail.compare_exchange_strong(head, tail, release, relaxed);
	}
};
}

#endif /* LOO_QUEUE_FWD_HPP */
