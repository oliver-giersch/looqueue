#include <atomic>
#include <barrier>
#include <iostream>
#include <thread>
#include <vector>

#include "looqueue/queue.hpp"

using elem_t = std::size_t;

static bool
is_in_bounds(std::vector<elem_t> &thread_elements, const elem_t *ptr)
{
	return ptr >= &thread_elements.front() && ptr <= &thread_elements.back();
}

int
main()
{
	const std::size_t thread_count = 128;
	const std::size_t count = 1'000'000;
	const auto tenth = count / 10;

	std::vector<elem_t> thread_elements {};
	thread_elements.reserve(count);

	for (auto i = 0; i < count; ++i) {
		thread_elements.push_back(i);
	}

	std::vector<std::thread> threads {};
	threads.reserve(thread_count * 2);

	std::barrier barrier { thread_count * 2 };
	std::atomic_uint64_t sum { 0 };

	loo::queue<elem_t> queue {};

	for (auto thread = 0; thread < thread_count; ++thread) {
		// Spawn producer threads.
		threads.emplace_back([&, thread] {
			const auto is_first = thread == 0;

			barrier.arrive_and_wait();

			for (auto op = 0; op < count; ++op) {
				if (is_first) {
					if (op % tenth == 0)
						std::cout << "progress: " << (op * 10) / tenth << "%" << std::endl;
				}

				queue.enqueue(&thread_elements.at(op));
			}
		});

		// Spawn consumer threads.
		threads.emplace_back([&] {
			uint64_t thread_sum = 0;
			uint64_t deq_count = 0;

			barrier.arrive_and_wait();

			while (deq_count < count) {
				const auto res = queue.dequeue();
				if (res != nullptr) {
					if (!is_in_bounds(thread_elements, res)) {
						std::cerr << "error: invalid pointer retrieved " << res
											<< std::endl;
						throw std::runtime_error("invalid dequeue result");
					}

					thread_sum += *res;
					deq_count += 1;
				}
			}

			sum.fetch_add(thread_sum);
		});
	}

	for (auto &thread : threads) {
		thread.join();
	}

	if (queue.dequeue() != nullptr) {
		std::cerr << "queue not empty after count * threads dequeue operations"
							<< std::endl;
		return 1;
	}

	const auto res = sum.load();
	const auto expected = thread_count * (count * (count - 1) / 2);
	if (res != expected) {
		std::cerr << "incorrect element sum, got " << res << ", expected "
							<< expected << std::endl;
		return 1;
	}

	std::cout << "test successful" << std::endl;
}
