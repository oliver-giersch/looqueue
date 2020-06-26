#include <array>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "boost/thread/barrier.hpp"

#include "benches/common.hpp"
#include "benches/queues/faa/faa_array.hpp"
#include "benches/queues/lcr/lcrq.hpp"
#include "benches/queues/msc/michael_scott.hpp"
#include "benches/queues/queue_ref.hpp"

#include "looqueue/queue.hpp"

using nanosecs = std::chrono::nanoseconds;

constexpr std::size_t RUNS = 50;
constexpr std::size_t TOTAL_OPS = 10'000'000;

constexpr std::array<std::size_t, 15> THREADS{ 1, 2, 4, 8, 12, 16, 20, 24, 32, 40, 48, 56, 64, 80, 96 };

/********** queue aliases *****************************************************/

using loo_queue     = loo::queue<std::size_t>;

using faa_queue     = faa::queue<std::size_t>;
using faa_queue_ref = queue_ref<faa_queue>;
using lcr_queue     = lcr::queue<std::size_t>;
using lcr_queue_ref = queue_ref<lcr_queue>;
using msc_queue     = msc::queue<std::size_t>;
using msc_queue_ref = queue_ref<msc_queue>;

/********** function pointer aliases ******************************************/

template <typename Q, typename R>
using make_queue_ref_fn = std::function<R(Q&, std::size_t)>;

/********** helper structs ****************************************************/

struct burst_measurements_t {
  nanosecs enq, deq;
};

/** runs all bench iterations for the specified bench and queue */
template <typename Q, typename R>
void run_benches(
    const std::string& bench,
    const std::string& queue_name,
    make_queue_ref_fn<Q, R> make_queue_ref
);

template <typename Q, typename R>
void bench_pairwise(
    std::size_t threads,
    const std::string& queue_name,
    make_queue_ref_fn<Q, R> make_queue_ref
);

template <typename Q, typename R>
void bench_bursts(
    std::size_t threads,
    const std::string& queue_name,
    make_queue_ref_fn<Q, R> make_queue_ref
);

int main(int argc, char* argv[3]) {
  if (argc < 3) {
    throw std::invalid_argument("too few program arguments");
  }

  const std::string queue(argv[1]);
  const std::string bench(argv[2]);

  if (bench != "pairs" && bench != "bursts") {
    throw std::invalid_argument("argument `bench` must be 'pairs' or 'bursts'");
  }

  const auto queue_type = bench::parse_queue_str(queue);
  const std::string queue_name{ bench::display_str(queue_type) };

  switch (queue_type) {
    case bench::queue_type_t::LCR:
      run_benches<lcr_queue , lcr_queue_ref>(
        bench,
        queue_name,
        [](auto& queue, auto thread_id) -> auto {
          return lcr_queue_ref(queue, thread_id);
        }
      );
      break;
    case bench::queue_type_t::LOO:
      run_benches<loo_queue, loo_queue&>(
          bench,
          queue_name,
          [](auto& queue, auto) -> auto& { return queue; }
      );
      break;
    case bench::queue_type_t::FAA:
      run_benches<faa_queue, faa_queue_ref>(
          bench,
          queue_name,
          [](auto& queue, auto thread_id) -> auto {
            return faa_queue_ref(queue, thread_id);
          }
      );
      break;
    case bench::queue_type_t::MSC:
      run_benches<msc_queue, msc_queue_ref>(
          bench,
          queue_name,
          [](auto& queue, auto thread_id) -> auto {
            return msc_queue_ref(queue, thread_id);
          }
      );
      break;
  }
}

template <typename Q, typename R>
void run_benches(
    const std::string& bench,
    const std::string& queue_name,
    make_queue_ref_fn<Q, R> make_queue_ref
) {
  for (auto threads : THREADS) {
    // aborts if hyper-threads would be used (assuming 2 HT per core)
    if (threads > std::thread::hardware_concurrency() / 2) {
      break;
    }

    if (bench == "pairs") {
      bench_pairwise<Q, R>(threads, queue_name, make_queue_ref);
    } else if (bench == "bursts") {
      bench_bursts<Q, R>(threads, queue_name, make_queue_ref);
    }
  }
}

template <typename Q, typename R>
void bench_pairwise(
    const std::size_t threads,
    const std::string& queue_name,
    make_queue_ref_fn<Q, R> make_queue_ref
) {
  const auto ops_per_threads = TOTAL_OPS / threads;

  // pre-allocates a vector for storing the elements enqueued by each thread;
  std::vector<std::size_t> thread_ids{};
  thread_ids.reserve(threads);
  for (std::size_t thread = 0; thread < threads; ++thread) {
    thread_ids.push_back(thread);
  }

  for (std::size_t run = 0; run < RUNS; ++run) {
    auto queue = std::make_unique<Q>();
    boost::barrier barrier{ static_cast<unsigned>(threads + 1) };

    // pre-allocates a vector for storing each thread's join handle
    std::vector<std::thread> thread_handles{};
    thread_handles.reserve(threads);

    // spawns threads and performs pairwise enqueue and dequeue operations
    for (std::size_t thread = 0; thread < threads; ++thread) {
      thread_handles.emplace_back(std::thread([&, thread] {
        bench::pin_current_thread(thread);

        auto&& queue_ref = make_queue_ref(*queue, thread);

        // all threads synchronize at this barrier before starting
        barrier.wait();

        for (std::size_t op = 0; op < ops_per_threads; ++op) {
          queue_ref.enqueue(&thread_ids.at(thread));

          auto elem = queue_ref.dequeue();
          if (
              elem != nullptr
              && (elem < &thread_ids.front() || elem > &thread_ids.back())
          ) {
            throw std::runtime_error(
                "invalid element retrieved (detected undefined behaviour)"
            );
          }
        }

        // all threads synchronize at this barrier before completing
        barrier.wait();
      }));
    }

    barrier.wait();
    // measures total time once all threads have arrived at the barrier
    const auto start = std::chrono::high_resolution_clock::now();
    barrier.wait();
    const auto stop = std::chrono::high_resolution_clock::now();
    const auto duration = stop - start;

    // joins all threads
    for (auto& handle : thread_handles) {
      handle.join();
    }

    // print measurements to stdout
    std::cout
        << queue_name
        << "," << threads
        << "," << duration.count()
        << "," << TOTAL_OPS << std::endl;
  }
}

template <typename Q, typename R>
void bench_bursts(
    const std::size_t threads,
    const std::string& queue_name,
    make_queue_ref_fn<Q, R> make_queue_ref
) {
  const auto ops_per_threads = TOTAL_OPS / threads;

  // pre-allocates a vector for storing the elements enqueued by each thread;
  std::vector<std::size_t> thread_ids{};
  thread_ids.reserve(threads);
  for (std::size_t thread = 0; thread < threads; ++thread) {
    thread_ids.push_back(thread);
  }

  for (std::size_t run = 0; run < RUNS; ++run) {
    auto queue = std::make_unique<Q>();
    boost::barrier barrier{ static_cast<unsigned>(threads + 1) };

    // pre-allocates a vector for storing each thread's join handle
    std::vector<std::thread> thread_handles{};
    thread_handles.reserve(threads);

    // spawns threads and performs pairwise enqueue and dequeue operations
    for (std::size_t thread = 0; thread < threads; ++thread) {
      thread_handles.emplace_back(std::thread([&, thread] {
        bench::pin_current_thread(thread);

        auto&& queue_ref = make_queue_ref(*queue, thread);

        // (1) all threads synchronize at this barrier before starting
        barrier.wait();

        for (std::size_t op = 0; op < ops_per_threads; ++op) {
          queue_ref.enqueue(&thread_ids.at(thread));
        }

        // (2) all threads synchronize at this barrier after completing their
        // enqueue burst
        barrier.wait();

        for (std::size_t op = 0; op < ops_per_threads; ++op) {
          auto elem = queue_ref.dequeue();
          if (
              elem != nullptr
              && (elem < &thread_ids.front() || elem > &thread_ids.back())
          ) {
            throw std::runtime_error(
                "invalid element retrieved (detected undefined behaviour)"
            );
          }
        }

        // (3) all threads synchronize at this barrier before completing
        barrier.wait();
      }));
    }

    // (1)
    barrier.wait();
    // measures total time of enqueue burst once all threads have arrived at the
    // barrier
    const auto enq_start = std::chrono::high_resolution_clock::now();
    // (2)
    barrier.wait();
    const auto enq_stop = std::chrono::high_resolution_clock::now();
    // (3)
    barrier.wait();
    const auto deq_stop = std::chrono::high_resolution_clock::now();

    const auto enq = enq_stop - enq_start;
    const auto deq = deq_stop - enq_stop;

    // joins all threads
    for (auto& handle : thread_handles) {
      handle.join();
    }

    // print measurements to stdout
    std::cout
        << queue_name
        << "," << threads
        << "," << enq.count()
        << "," << deq.count()
        << "," << TOTAL_OPS << std::endl;
  }
}

