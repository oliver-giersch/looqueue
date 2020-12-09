#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

#include "looqueue/queue.hpp"

int main() {
  const std::size_t thread_count = 8;
  const std::size_t count = 100 * 10000;

  std::vector<std::vector<std::size_t>> thread_elements{};
  thread_elements.reserve(thread_count);

  for (auto t = 0; t < thread_count; ++t) {
    thread_elements.emplace_back();
    thread_elements[t].reserve(count);

    for (auto i = 0; i < count; ++i) {
      thread_elements[t].push_back(i);
    }
  }

  std::vector<std::thread> threads{};
  threads.reserve(thread_count * 2);

  std::atomic_bool start{ false };
  std::atomic_uint64_t sum{ 0 };

  loo::queue<std::size_t> queue{};

  for (auto thread = 0; thread < thread_count; ++thread) {
    // producer thread
    threads.emplace_back([&, thread] {
      while (!start.load());

      for (auto op = 0; op < count; ++op) {
        queue.enqueue(&thread_elements.at(thread).at(op));
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
          if (*res >= count) {
            throw std::runtime_error("must not retrieve element larger than `count`");
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
