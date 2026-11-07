#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <semaphore>
#include <thread>
#include <vector>

namespace aba_test {
using namespace std::chrono_literals;

struct schedule {
	std::binary_semaphore head_sampled { 0 };
	std::binary_semaphore resume_reader { 0 };
	const void *head = nullptr;
	const void *tail = nullptr;
	std::size_t head_index = 0;
	std::size_t tail_index = 0;
	bool paused = false;
	bool tail_sampled = false;
};

thread_local std::size_t node_capacity = 0;
thread_local schedule *active_schedule = nullptr;

void
after_head_snapshot(const void *head, std::size_t index, std::size_t capacity)
{
	node_capacity = capacity;
	auto *state = active_schedule;
	if (state == nullptr || state->paused)
		return;

	// Pause only the first snapshot, allowing a repaired check to retry.
	state->paused = true;
	state->head = head;
	state->head_index = index;
	state->head_sampled.release();
	if (!state->resume_reader.try_acquire_for(10s)) {
		std::cerr << "ABA test: timed out waiting to resume the reader\n";
		std::_Exit(2);
	}
}

void
after_tail_snapshot(const void *tail, std::size_t index)
{
	auto *state = active_schedule;
	if (state == nullptr || state->tail_sampled)
		return;

	state->tail_sampled = true;
	state->tail = tail;
	state->tail_index = index;
}
}

// These hooks exist only in this single-translation-unit test executable.
// Ordinary builds contain neither callbacks nor extra branches.
#define LOO_QUEUE_TEST_HEAD_SNAPSHOT(head, index, capacity)                    \
	aba_test::after_head_snapshot(head, index, capacity)
#define LOO_QUEUE_TEST_TAIL_SNAPSHOT(tail, index)                              \
	aba_test::after_tail_snapshot(tail, index)
#include "looqueue/queue.hpp"
#undef LOO_QUEUE_TEST_HEAD_SNAPSHOT
#undef LOO_QUEUE_TEST_TAIL_SNAPSHOT

int
main()
{
	using namespace std::chrono_literals;

	loo::queue<std::size_t> queue;
	// Discover the private capacity through the hook without reserving a slot.
	if (queue.dequeue() != nullptr || aba_test::node_capacity < 8) {
		std::cerr << "ABA test: could not determine the empty queue's capacity\n";
		return 2;
	}
	const auto capacity = aba_test::node_capacity;
	const auto offset = capacity / 4;
	std::vector<std::size_t> elements(2 * capacity + 1);
	for (std::size_t i = 0; i < elements.size(); ++i)
		elements[i] = i;

	// A is full, B has `offset` elements, and head is partway through A.
	for (std::size_t i = 0; i < capacity + offset; ++i)
		queue.enqueue(&elements[i]);
	for (std::size_t i = 0; i < offset; ++i) {
		if (queue.dequeue() != &elements[i]) {
			std::cerr << "ABA test: initial FIFO check failed\n";
			return 2;
		}
	}

	aba_test::schedule state;
	std::size_t *result = nullptr;
	std::thread reader([&] {
		aba_test::active_schedule = &state;
		result = queue.dequeue();
		aba_test::active_schedule = nullptr;
	});
	if (!state.head_sampled.try_acquire_for(10s)) {
		state.resume_reader.release();
		reader.join();
		std::cerr << "ABA test: reader did not reach the head snapshot\n";
		return 2;
	}

	// While the reader retains A's unregistered snapshot:
	// 1. Finish A and consume B[0], reclaiming A into this thread's cache.
	// 2. Fill B and append C, reusing A's storage with enqueue index 1.
	// The backlog never drops below offset - 1, even before the new enqueues.
	bool setup_ok = true;
	for (std::size_t i = offset; i <= capacity; ++i) {
		if (queue.dequeue() != &elements[i]) {
			setup_ok = false;
			break;
		}
	}
	if (setup_ok) {
		for (std::size_t i = capacity + offset; i < elements.size(); ++i)
			queue.enqueue(&elements[i]);
	}
	state.resume_reader.release();
	reader.join();
	if (!setup_ok || state.head_index != offset) {
		std::cerr << "ABA test: could not establish the intended schedule\n";
		return 2;
	}

	std::cout << "paused head: " << state.head << ", index " << state.head_index
						<< "\nobserved tail: " << state.tail << ", index "
						<< state.tail_index
						<< "\nminimum backlog during the call: " << offset - 1 << '\n';

	// A correct dequeue must return the oldest remaining element, never null.
	// The return value is deliberately checked without assert(), also in release.
	const bool false_empty = result == nullptr;
	if (false_empty
		&& (!state.tail_sampled || state.head != state.tail
			|| state.tail_index != 1)) {
		std::cerr << "ABA test: unexpected empty result without confirmed reuse\n";
		return 2;
	}
	if (!false_empty && result != &elements[capacity + 1]) {
		std::cerr << "ABA test: reader returned an unexpected element\n";
		return 1;
	}

	// Confirm exact identities and count: a false-empty call leaves N elements;
	// a correct call consumes one and leaves N - 1.
	const auto first_remaining = capacity + (false_empty ? 1 : 2);
	for (std::size_t i = first_remaining; i < elements.size(); ++i) {
		if (queue.dequeue() != &elements[i]) {
			std::cerr << "ABA test: remaining FIFO check failed\n";
			return 1;
		}
	}
	if (queue.dequeue() != nullptr) {
		std::cerr << "ABA test: extra elements remained after draining\n";
		return 1;
	}
	if (false_empty) {
		std::cerr << "FAIL: dequeue returned nullptr although the queue stayed "
								 "nonempty; "
							<< capacity << " elements remained\n";
		return 1;
	}

	std::cout << "PASS: delayed dequeue returned the expected element\n";
	return 0;
}
