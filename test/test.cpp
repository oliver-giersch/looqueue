#include <atomic>
#include <barrier>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

#include "looqueue/queue.hpp"

static int test_control_block_internals();
static int test_one_enqueue_dequeue();
static int test_half_drain_one_node();
static int test_drain_one_node();
static int test_drain_refill();
static int test_payload_publication_fifo();
static int test_concurrent_exactly_once();

int main()
{
	std::cout << "test_control_block_internals" << std::endl;
	if (test_control_block_internals())
		return (1);

	std::cout << "test_one_enqueue_dequeue" << std::endl;
	if (test_one_enqueue_dequeue())
		return (1);

	std::cout << "test_half_drain_one_node" << std::endl;
	if (test_half_drain_one_node())
		return (1);

	std::cout << "test_drain_one_node" << std::endl;
	if (test_drain_one_node())
		return (1);

	std::cout << "test_drain_refill" << std::endl;
	if (test_drain_refill())
		return 1;

	std::cout << "test_payload_publication_fifo" << std::endl;
	if (test_payload_publication_fifo())
		return 1;

	std::cout << "test_concurrent_exactly_once" << std::endl;
	if (test_concurrent_exactly_once())
		return 1;

	return (0);
}

static int test_control_block_internals()
{
	std::atomic<std::uint64_t> state { };

	loo::detail::ctrl_block block { };
	block.enqueue_current = 123;
	block.dequeue_current = 56;

	const auto flags = loo::detail::ctrl_block::add_flags<
		loo::detail::ctrl_block::counter::enqueue>(false, 125);

	state.store(std::bit_cast<std::uint64_t>(block));
	state.fetch_add(flags);

	const auto new_state = state.load();
	const auto new_block = std::bit_cast<loo::detail::ctrl_block>(new_state);

	if (new_block.flags != 0)
		return 1;
	if (new_block.enqueue_current != 124)
		return 1;
	if (new_block.enqueue_total != 125)
		return 1;
	if (new_block.dequeue_current != 56)
		return 1;
	if (new_block.dequeue_total != 0)
		return 1;
	return 0;
}

static int test_one_enqueue_dequeue()
{
	int elem = 4711;
	loo::queue<int> queue { };

	queue.enqueue(&elem);

	const auto ptr = queue.dequeue();
	if (ptr == nullptr)
		return 1;

	if (*ptr != 4711)
		return 1;

	return 0;
}

static int test_half_drain_one_node()
{
	int elems[500];

	loo::queue<int> queue { };

	for (auto i = 0; i < 500; ++i) {
		elems[i] = i;
		queue.enqueue(&elems[i]);
	}

	for (auto i = 0; i < 500; ++i) {
		auto elem = queue.dequeue();
		if (elem == nullptr)
			return 1;
		if (*elem != i)
			return 1;
	}

	if (queue.dequeue() != nullptr)
		return 1;

	return 0;
}

static int test_drain_one_node()
{
	int elems[1025];

	loo::queue<int> queue { };

	for (auto i = 0; i < 1025; ++i) {
		elems[i] = i;
		queue.enqueue(&elems[i]);
	}

	for (auto i = 0; i < 1025; ++i) {
		auto elem = queue.dequeue();
		if (elem == nullptr)
			return 1;
		if (*elem != i)
			return 1;
	}

	if (queue.dequeue() != nullptr)
		return 1;

	return 0;
}

static int test_drain_refill()
{
	loo::queue<std::size_t> queue;
	std::vector<std::size_t> elements(2049);
	for (std::size_t i = 0; i < elements.size(); ++i)
		elements[i] = i;

	// Cross several node boundaries, repeatedly reclaiming and reusing nodes.
	for (auto round = 0; round < 8; ++round) {
		for (auto &element : elements)
			queue.enqueue(&element);

		for (std::size_t i = 0; i < elements.size(); ++i) {
			if (queue.dequeue() != &elements[i]) {
				std::cerr << "drain/refill: incorrect FIFO result in round " << round
									<< ", element " << i << '\n';
				return 1;
			}
		}

		// More empty calls than the tag range also exercise cached-tail catch-up.
		for (auto i = 0; i < 10000; ++i) {
			if (queue.dequeue() != nullptr) {
				std::cerr << "drain/refill: empty queue returned an element\n";
				return 1;
			}
		}
	}

	return queue.dequeue() != nullptr;
}

struct test_payload {
	std::size_t id = 0;
	std::uint64_t checksum = 0;
};

static constexpr std::uint64_t payload_checksum(std::size_t id)
{
	return static_cast<std::uint64_t>(id) ^ 0x9e3779b97f4a7c15ULL;
}

static int test_payload_publication_fifo()
{
	using namespace std::chrono_literals;

	loo::queue<test_payload> queue;
	std::vector<test_payload> elements(16384);
	std::atomic<bool> failed { false };
	std::barrier start { 2 };
	std::thread producer([&] {
		start.arrive_and_wait();
		for (std::size_t i = 0; i < elements.size(); ++i) {
			if (failed.load(std::memory_order_relaxed))
				return;

			// Initialize after the barrier: only the queue publishes these writes.
			elements[i].id = i;
			elements[i].checksum = payload_checksum(i);
			queue.enqueue(&elements[i]);
		}
	});

	start.arrive_and_wait();
	const auto deadline = std::chrono::steady_clock::now() + 10s;
	for (std::size_t i = 0; i < elements.size();) {
		const auto element = queue.dequeue();
		if (element == nullptr) {
			if (std::chrono::steady_clock::now() >= deadline) {
				std::cerr << "payload publication: timed out waiting for element " << i
									<< '\n';
				failed.store(true, std::memory_order_relaxed);
				break;
			}
			std::this_thread::yield();
			continue;
		}

		if (element != &elements[i] || element->id != i
			|| element->checksum != payload_checksum(i)) {
			std::cerr << "payload publication: incorrect FIFO or payload at " << i
								<< '\n';
			failed.store(true, std::memory_order_relaxed);
			break;
		}
		++i;
	}

	producer.join();
	return failed.load(std::memory_order_relaxed) || queue.dequeue() != nullptr;
}

static int test_concurrent_exactly_once()
{
	using namespace std::chrono_literals;

	constexpr std::size_t producer_count = 4;
	constexpr std::size_t consumer_count = 4;
	constexpr std::size_t per_producer = 4096;
	constexpr auto count = producer_count * per_producer;

	loo::queue<test_payload> queue;
	std::vector<test_payload> elements(count);
	std::vector<std::atomic<unsigned>> seen(count);
	for (auto &occurrences : seen)
		occurrences.store(0, std::memory_order_relaxed);
	std::atomic<std::size_t> received { 0 };
	std::atomic<bool> failed { false };
	std::barrier start { static_cast<std::ptrdiff_t>(
		producer_count + consumer_count) };
	std::vector<std::thread> threads;
	threads.reserve(producer_count + consumer_count);

	const auto fail = [&](const char *message) {
		if (!failed.exchange(true, std::memory_order_relaxed))
			std::cerr << "concurrent delivery: " << message << '\n';
	};

	for (std::size_t producer = 0; producer < producer_count; ++producer) {
		threads.emplace_back([&, producer] {
			start.arrive_and_wait();
			for (std::size_t i = 0; i < per_producer; ++i) {
				if (failed.load(std::memory_order_relaxed))
					return;

				const auto id = producer * per_producer + i;
				elements[id].id = id;
				elements[id].checksum = payload_checksum(id);
				queue.enqueue(&elements[id]);
			}
		});
	}

	for (std::size_t consumer = 0; consumer < consumer_count; ++consumer) {
		threads.emplace_back([&] {
			start.arrive_and_wait();
			const auto deadline = std::chrono::steady_clock::now() + 10s;
			while (!failed.load(std::memory_order_relaxed)
				&& received.load(std::memory_order_relaxed) < count) {
				if (std::chrono::steady_clock::now() >= deadline) {
					fail("timed out waiting for all elements");
					return;
				}

				const auto element = queue.dequeue();
				if (element == nullptr) {
					std::this_thread::yield();
					continue;
				}

				// Validate the address before dereferencing a possibly bad result.
				const auto offset = reinterpret_cast<std::uintptr_t>(element)
					- reinterpret_cast<std::uintptr_t>(elements.data());
				if (offset >= elements.size() * sizeof(test_payload)
					|| offset % sizeof(test_payload) != 0) {
					fail("invalid element address");
					return;
				}

				const auto id = offset / sizeof(test_payload);
				if (element->id != id || element->checksum != payload_checksum(id)) {
					fail("incorrect published payload");
					return;
				}
				if (seen[id].fetch_add(1, std::memory_order_relaxed) != 0) {
					fail("duplicate element");
					return;
				}
				received.fetch_add(1, std::memory_order_relaxed);
			}
		});
	}

	for (auto &thread : threads)
		thread.join();

	if (failed.load(std::memory_order_relaxed))
		return 1;
	for (auto &occurrences : seen) {
		if (occurrences.load(std::memory_order_relaxed) != 1) {
			std::cerr << "concurrent delivery: missing element\n";
			return 1;
		}
	}
	return queue.dequeue() != nullptr;
}
