#include <atomic>
#include <bit>
#include <iostream>

#include "looqueue/queue.hpp"

static int test_control_block_internals();
static int test_one_enqueue_dequeue();
static int test_half_drain_one_node();
static int test_drain_one_node();

int
main()
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

	return (0);
}

static int
test_control_block_internals()
{
	std::atomic<std::uint64_t> state {};

	loo::detail::ctrl_block_t block {};
	block.enqueue_current = 123;
	block.dequeue_current = 56;

	const auto flags = loo::detail::ctrl_block_t::add_flags<
		loo::detail::ctrl_block_t::counter_kind_t::ENQUEUE>(false, 125);

	state.store(std::bit_cast<std::uint64_t>(block));
	state.fetch_add(flags);

	const auto new_state = state.load();
	const auto new_block = std::bit_cast<loo::detail::ctrl_block_t>(new_state);

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

static int
test_one_enqueue_dequeue()
{
	int elem = 4711;
	loo::queue<int> queue {};

	queue.enqueue(&elem);

	const auto ptr = queue.dequeue();
	if (ptr == nullptr)
		return 1;

	if (*ptr != 4711)
		return 1;

	return 0;
}

static int
test_half_drain_one_node()
{
	int elems[500];

	loo::queue<int> queue {};

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

static int
test_drain_one_node()
{
	int elems[1025];

	loo::queue<int> queue {};

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