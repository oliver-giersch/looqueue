#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <stdexcept>

#include "boost/thread/barrier.hpp"

#include "benches/queues/lcr/lcrq.hpp"

constexpr std::size_t THREADS = 32;
constexpr std::size_t OPS_PER_THREAD = 1024 * 1024;

struct data_t {
  std::size_t thread_id;
  std::size_t index;
};

std::array<std::array<data_t, OPS_PER_THREAD>, THREADS> data{};

int main() {
  for (std::size_t thread_id = 0; thread_id < THREADS; ++thread_id) {
    for (std::size_t op_no = 0; op_no < OPS_PER_THREAD; ++op_no) {
      data[thread_id][op_no] = { thread_id, op_no };
    }
  }

  std::vector<std::thread> thread_handles{};
  thread_handles.reserve(THREADS);

  auto queue = std::make_unique<lcr::queue<data_t>>();
  boost::barrier barrier{ static_cast<unsigned>(THREADS) };

  std::mutex retrieved_mutex{};
  std::vector<data_t*> retrieved{};
  retrieved.reserve(THREADS * OPS_PER_THREAD);

  for (std::size_t thread = 0; thread < THREADS; ++thread) {
    thread_handles.emplace_back(std::thread([&, thread] {
      std::vector<data_t*> thread_retrieved{};
      thread_retrieved.reserve(OPS_PER_THREAD);

      barrier.wait();

      data_t* deq = nullptr;
      for (std::size_t op = 0; op < OPS_PER_THREAD; ++op) {
        queue->enqueue(&data[thread][op], thread);

        if (op % 2 == 0) {
          deq = queue->dequeue(thread);
          if (deq != nullptr) {
            thread_retrieved.push_back(deq);
          }
        }
      }

      while ((deq = queue->dequeue(thread)) != nullptr) {
        thread_retrieved.push_back(deq);
      }

      barrier.wait();

      std::unique_lock<std::mutex> lock(retrieved_mutex);
      for (auto ret : thread_retrieved) {
        retrieved.push_back(ret);
      }
    }));
  }

  for (auto& handle : thread_handles) {
    handle.join();
  }

  unsigned count = 0;
  std::sort(retrieved.begin(), retrieved.end(), [&](const auto a, const auto b) {
    if (a->thread_id != b->thread_id) {
      return a->thread_id < b->thread_id;
    }

    return a->index < b->index;
  });

  if (retrieved.size() != THREADS * OPS_PER_THREAD) {
    throw std::runtime_error("unexpected number of elements");
  }

  std::size_t idx = 0;
  for (std::size_t thread_id = 0; thread_id < THREADS; ++thread_id) {
    for (std::size_t op = 0; op < OPS_PER_THREAD; ++op) {
      const auto& elem = retrieved.at(idx);
      if (elem->thread_id != thread_id || elem->index != op) {
        throw std::runtime_error("retrieved unexpected element");
      }

      idx++;
    }
  }

  std::cout << "test success: " << retrieved.size() << " elements retrieved as expected" << std::endl;
}
