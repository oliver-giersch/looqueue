#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

#include "looqueue/queue.hpp"

int main() {
  const std::size_t thread_count = 128;
  const std::size_t count = 100'000;

  std::vector<std::size_t> thread_elements{};
  thread_elements.reserve(count);

  for (auto i = 0; i < count; ++i) {
    thread_elements.push_back(i);
  }

  auto in_bounds = [&thread_elements](const std::size_t* pointer) {
    return pointer >= &thread_elements.front() && pointer <= &thread_elements.back();
  };

  std::vector<std::thread> threads{};
  threads.reserve(thread_count * 2);

  std::atomic_bool start{ false };
  std::atomic_uint64_t sum{ 0 };

  loo::queue<std::size_t> queue{};

  for (auto thread = 0; thread < thread_count; ++thread) {
    // producer thread
    threads.emplace_back([&] {
      while (!start.load());

      for (auto op = 0; op < count; ++op) {
        queue.enqueue(&thread_elements.at(op));
      }
    });

    // consumer thread
    threads.emplace_back([&] {
      uint64_t thread_sum = 0;
      uint64_t deq_count = 0;

      while (!start.load()) {}

      while (deq_count < count) {
        const auto res = queue.dequeue();
        if (res != nullptr) {
          if (!in_bounds(res)) {
            std::cerr << "error: invalid pointer retrieved " << res << std::endl;
            throw std::runtime_error("invalid dequeue result");
          }

          thread_sum += *res;
          deq_count += 1;
        }
      }

      sum.fetch_add(thread_sum);
    });
  }

  start.store(true);

  for (auto& thread : threads) {
    thread.join();
  }

  if (queue.dequeue() != nullptr) {
    std::cerr << "queue not empty after count * threads dequeue operations" << std::endl;
    return 1;
  }

  const auto res = sum.load();
  const auto expected = thread_count * (count * (count - 1) / 2);
  if (res != expected) {
    std::cerr << "incorrect element sum, got " << res << ", expected " << expected << std::endl;
    return 1;
  }

  std::cout << "test successful" << std::endl;
}
