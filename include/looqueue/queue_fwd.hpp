#ifndef LOO_QUEUE_FWD_HPP
#define LOO_QUEUE_FWD_HPP

#include <atomic>

#include "align.hpp"
#include "detail/native_tag_ptr.hpp"

namespace loo {
namespace detail {
	/* The result type for `try_advance_head`. */
	enum class advance_head_res_t { QUEUE_EMPTY, ADVANCED };
	/* The result type for `try_advance_tail`. */
	enum class advance_tail_res_t { ADVANCED, ADVANCED_AND_INSERTED };
}
template <typename T>
class queue {
	static_assert(sizeof(T *) == 8,
		"loo::queue is only compatible with 64-bit architectures");
	static_assert(alignof(T) >= 4,
		"all T pointers must be at least 4-byte aligned");

	/* The number of slots for storing individual elements in each node */
	static constexpr auto NODE_SIZE = std::size_t { 1024 };
	static constexpr auto TAG_BITS = std::size_t { 16 };

	static constexpr auto relaxed = std::memory_order_relaxed;
	static constexpr auto acquire = std::memory_order_acquire;
	static constexpr auto release = std::memory_order_release;
	static constexpr auto acq_rel = std::memory_order_acq_rel;

	using node_tag_ptr_t = std::uintptr_t;
	using atomic_node_tag_ptr_t = std::atomic<node_tag_ptr_t>;

	struct node_t;
	using tag_ptr_t = typename detail::native_tag_ptr_t<node_t, TAG_BITS>;

	/* The head node pointer & dequeue index pair tag pointer. */
	alignas(CACHE_LINE_ALIGN) atomic_node_tag_ptr_t m_head;
	/* The tail node pointer & enqueue index pair tag pointer. */
	alignas(CACHE_LINE_ALIGN) atomic_node_tag_ptr_t m_tail;
	/* The cached value of the last observed tail pointer. */
	alignas(CACHE_LINE_ALIGN) std::atomic<node_t *> m_cached_tail;

public:
	using pointer = T *;

	/** See PROOF.md for the reasoning behind these constants */

	static constexpr std::size_t MAX_PRODUCER_THREADS
		= (1ull << TAG_BITS) - NODE_SIZE + 1;
	static constexpr std::size_t MAX_CONSUMER_THREADS
		= ((1ull << TAG_BITS) - NODE_SIZE + 1) / 2;

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
	using advance_head_res_t = detail::advance_head_res_t;
	using advance_tail_res_t = detail::advance_tail_res_t;

	static constexpr auto ONE = tag_ptr_t::ONE;

	/* Returns true if the queue is empty. */
	bool is_empty() noexcept;

	/*
	 * Attempts to advance the tail node to its successor if there is one or
	 * attempts to append a new node with `elem` stored in the first slot
	 * otherwise.
	 */
	advance_tail_res_t try_advance_tail(tag_ptr_t tag_tail, node_t *tail,
		pointer elem) noexcept;

	/* Attempts to advance the head node to its successor, if there is one. */
	advance_head_res_t try_advance_head(tag_ptr_t tag_head, node_t *head,
		bool verify) noexcept;

	/* Loops until the queue's tail is updated by any thread. */
	bool cas_tail(tag_ptr_t &expected, tag_ptr_t desired, node_t *tail);

	/* Loops until the queue's head is updated by any thread. */
	bool cas_head(tag_ptr_t &expected, tag_ptr_t desired, node_t *head);

	/*
	 * Loops and attempts to CAS `expected` with `desired` until either the CAS
	 * succeeds or the loaded pointer value (failure case) no longer matches
	 * `old_node`
	 */
	static bool bounded_cas_loop(atomic_node_tag_ptr_t &node, tag_ptr_t &expected,
		tag_ptr_t desired, const node_t *old_node, std::memory_order order);
};
}

#endif /* LOO_QUEUE_FWD_HPP */
