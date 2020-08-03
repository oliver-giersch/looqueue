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

constexpr std::array<std::size_t, 9>  THREADS_MACRO{ 4, 8, 16, 24, 32, 48, 64, 80, 96 };
constexpr std::array<std::size_t, 11> THREADS_MICRO{ 1, 2, 4, 8, 16, 24, 32, 48, 64, 80, 96 };

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

/********** functions *********************************************************/

/** runs all bench iterations for the specified bench and queue */
template <typename Q, typename R>
void run_benches(
    const std::string&      queue_name,
    bench::bench_type_t     bench_type,
    std::size_t             total_ops,
    std::size_t             runs,
    make_queue_ref_fn<Q, R> make_queue_ref
);

/** runs the pairwise enqueue/dequeue benchmark */
template <typename Q, typename R>
void bench_pairwise(
    const std::string&      queue_name,
    std::size_t             total_ops,
    std::size_t             runs,
    std::size_t             threads,
    make_queue_ref_fn<Q, R> make_queue_ref
);

/** runs the burst benchmarks */
template <typename Q, typename R>
void bench_bursts(
    const std::string&      queue_name,
    std::size_t             total_ops,
    std::size_t             runs,
    std::size_t             threads,
    make_queue_ref_fn<Q, R> make_queue_ref
);

/** runs either the read-heavy or write-heavy benchmark */
template <typename Q, typename R>
void bench_reads_or_writes(
    const std::string&      queue_name,
    bench::bench_type_t     bench_type,
    std::size_t             total_ops,
    std::size_t             runs,
    std::size_t             threads,
    make_queue_ref_fn<Q, R> make_queue_ref
);

int main(int argc, char* argv[5]) {
  if (argc < 5) {
    throw std::invalid_argument("too few program arguments");
  }

  const std::string queue(argv[1]);
  const std::string bench(argv[2]);
  const std::string total_ops_str(argv[3]);
  const std::string runs_str(argv[4]);

  const auto queue_type = bench::parse_queue_str(queue);
  const auto bench_type = bench::parse_bench_str(bench);
  const auto total_ops = bench::parse_total_ops_str(total_ops_str);
  const auto runs = bench::parse_runs_str(runs_str);

  const std::string queue_name{ bench::display_str(queue_type) };

  switch (queue_type) {
    case bench::queue_type_t::LCR:
      run_benches<lcr_queue, lcr_queue_ref>(
          queue_name,
          bench_type,
          total_ops,
          runs,
          [](auto& queue, auto thread_id) -> auto {
            return lcr_queue_ref(queue, thread_id);
          }
      );
      break;
    case bench::queue_type_t::LOO:
      run_benches<loo_queue, loo_queue&>(
          queue_name,
          bench_type,
          total_ops,
          runs,
          [](auto& queue, auto) -> auto& { return queue; }
      );
      break;
    case bench::queue_type_t::FAA:
      run_benches<faa_queue, faa_queue_ref>(
          queue_name,
          bench_type,
          total_ops,
          runs,
          [](auto& queue, auto thread_id) -> auto {
            return faa_queue_ref(queue, thread_id);
          }
      );
      break;
    case bench::queue_type_t::MSC:
      run_benches<msc_queue, msc_queue_ref>(
          queue_name,
          bench_type,
          total_ops,
          runs,
          [](auto& queue, auto thread_id) -> auto {
            return msc_queue_ref(queue, thread_id);
          }
      );
      break;
  }
}

template <typename Q, typename R>
void run_benches(
    const std::string&      queue_name,
    bench::bench_type_t     bench_type,
    std::size_t             total_ops,
    std::size_t             runs,
    make_queue_ref_fn<Q, R> make_queue_ref
) {
  if (bench_type == bench::bench_type_t::PAIRS || bench_type == bench::bench_type_t::BURSTS) {
    for (auto threads : THREADS_MICRO) {
      // aborts if hyper-threads would be used (assuming 2 HT per core)
      if (threads > std::thread::hardware_concurrency() / 2) {
        break;
      }

      switch (bench_type) {
        case bench::bench_type_t::PAIRS:
          bench_pairwise<Q, R>(queue_name, total_ops, runs, threads, make_queue_ref);
          break;
        case bench::bench_type_t::BURSTS:
          bench_bursts<Q, R>(queue_name, total_ops, runs, threads, make_queue_ref);
          break;
        default: throw std::runtime_error("unreachable branch");
      }
    }
  } else {
    for (auto threads : THREADS_MACRO) {
      // aborts if hyper-threads would be used (assuming 2 HT per core)
      if (threads > std::thread::hardware_concurrency() / 2) {
        //break;
      }

      bench_reads_or_writes<Q, R>(queue_name, bench_type, total_ops, runs, threads, make_queue_ref);
    }
  }
}

template <typename Q, typename R>
void bench_pairwise(
    const std::string&      queue_name,
    std::size_t             total_ops,
    std::size_t             runs,
    std::size_t             threads,
    make_queue_ref_fn<Q, R> make_queue_ref
) {
  const auto ops_per_threads = total_ops / threads;

  // pre-allocates a vector for storing the elements enqueued by each thread;
  std::vector<std::size_t> thread_ids{};
  thread_ids.reserve(threads);
  for (std::size_t thread = 0; thread < threads; ++thread) {
    thread_ids.push_back(thread);
  }

  for (std::size_t run = 0; run < runs; ++run) {
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
          if (op % 2 == 0) {
            queue_ref.enqueue(&thread_ids.at(thread));
          } else {
            auto elem = queue_ref.dequeue();
            if (elem != nullptr && (elem < &thread_ids.front() || elem > &thread_ids.back())) {
              throw std::runtime_error(
                  "invalid element retrieved (undefined behaviour detected)"
              );
            }
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
        << "," << total_ops << std::endl;
  }
}

template <typename Q, typename R>
void bench_bursts(
    const std::string&      queue_name,
    std::size_t             total_ops,
    std::size_t             runs,
    std::size_t             threads,
    make_queue_ref_fn<Q, R> make_queue_ref
) {
  const auto ops_per_threads = total_ops / threads;

  // pre-allocates a vector for storing the elements enqueued by each thread;
  std::vector<std::size_t> thread_ids{};
  thread_ids.reserve(threads);
  for (std::size_t thread = 0; thread < threads; ++thread) {
    thread_ids.push_back(thread);
  }

  for (std::size_t run = 0; run < runs; ++run) {
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
        // respective enqueue burst
        barrier.wait();

        for (std::size_t op = 0; op < ops_per_threads; ++op) {
          auto elem = queue_ref.dequeue();
          if (elem != nullptr && (elem < &thread_ids.front() || elem > &thread_ids.back())) {
            throw std::runtime_error(
                "invalid element retrieved (undefined behaviour detected)"
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
        << "," << total_ops << std::endl;
  }
}

template <typename Q, typename R>
void bench_reads_or_writes(
    const std::string&      queue_name,
    bench::bench_type_t     bench_type,
    std::size_t             total_ops,
    std::size_t             runs,
    std::size_t             threads,
    make_queue_ref_fn<Q, R> make_queue_ref
) {
  const auto ops_per_thread = total_ops / threads;

  // pre-allocates a vector for storing the elements enqueued by each thread;
  std::vector<std::size_t> thread_ids{};
  thread_ids.reserve(threads);
  for (std::size_t thread = 0; thread < threads; ++thread) {
    thread_ids.push_back(thread);
  }

  for (std::size_t run = 0; run < runs; ++run) {
    auto queue = std::make_unique<Q>();
    boost::barrier barrier{ static_cast<unsigned>(threads + 1) };

    // pre-allocates a vector for storing each thread's join handle
    std::vector<std::thread> thread_handles{};
    thread_handles.reserve(threads);

    // for read-heavy benchmarks, seed the queue with large number of values before
    // starting the benchmark
    if (bench_type == bench::bench_type_t::READS) {
      auto&& queue_ref = make_queue_ref(*queue, 0);
      for (std::size_t op = 0; op < (3 * total_ops) / 4; ++op) {
        queue_ref.enqueue(&thread_ids.at(0));
      }
    }

    // spawns threads and performs the required operations
    for (std::size_t thread = 0; thread < threads; ++thread) {
      thread_handles.emplace_back(std::thread([&, thread] {
        bench::pin_current_thread(thread);

        auto&& queue_ref = make_queue_ref(*queue, thread);

        // all threads synchronize at this barrier before starting
        barrier.wait();

        const auto writer_thread = [&]() {
          for (auto op = 0; op < ops_per_thread; ++op) {
            queue_ref.enqueue(&thread_ids.at(thread));
          }
        };

        const auto reader_thread = [&]() {
          for (auto op = 0; op < ops_per_thread; ++op) {
            auto elem = queue_ref.dequeue();
            if (elem == nullptr) {
              bench::spin_for_ns(50);
            } else {
              if (elem < &thread_ids.front() || elem > &thread_ids.back()) {
                throw std::runtime_error(
                    "invalid element retrieved (undefined behaviour detected)"
                );
              }
            }
          }
        };

        switch (bench_type) {
          case bench::bench_type_t::WRITES:
            if (thread % 4 == 0) {
              reader_thread();
            } else {
              writer_thread();
            }
            break;
          case bench::bench_type_t::READS:
            if (thread % 4 == 0) {
              writer_thread();
            } else {
              reader_thread();
            }
            break;
          default: throw std::runtime_error("unreachable branch");
        }

        // all threads synchronize at this barrier before finishing
        barrier.wait();
      }));
    }

    // synchronize with threads before starting
    barrier.wait();
    // measures total time once all threads have arrived at the barrier
    const auto start = std::chrono::high_resolution_clock::now();
    // synchronize with threads after finishing
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
        << "," << total_ops << std::endl;
  }
}
