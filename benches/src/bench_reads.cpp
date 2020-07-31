#include <array>
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

constexpr std::array<std::size_t, 8> THREADS{ 4, 8, 16, 24, 32, 48, 64, 96 };

/********** queue type aliases ************************************************/

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

template <typename Q, typename R>
void run_benches(
    const std::string&      queue_name,
    std::size_t             total_ops,
    std::size_t             runs,
    make_queue_ref_fn<Q, R> make_queue_ref
);

template <typename Q, typename R>
void bench_reads(
    const std::string&      queue_name,
    std::size_t             threads,
    std::size_t             total_ops,
    std::size_t             runs,
    make_queue_ref_fn<Q, R> make_queue_ref
);

template <typename R>
void reader_thread(R queue_ref, std::size_t ops_per_thread);

template <typename R>
void writer_thread(R queue_ref, std::size_t  ops_per_thread, std::size_t* thread_id);

int main(int argc, const char* argv[4]) {
  if (argc < 4) {
    throw std::invalid_argument("too few program arguments");
  }

  const std::string queue(argv[1]);
  const std::string size_str(argv[2]);
  const std::string runs_str(argv[3]);

  const auto total_ops = bench::parse_size_str(size_str);
  const auto runs = bench::parse_runs_str(runs_str);
  const auto queue_type = bench::parse_queue_str(queue);
  const std::string queue_name{ bench::display_str(queue_type) };

  switch (bench::parse_queue_str(queue)) {
    case bench::queue_type_t::LCR:
      run_benches<lcr_queue, lcr_queue_ref>(
          queue_name,
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
          total_ops,
          runs,
          [](auto& queue, auto) -> auto& { return queue; }
      );
      break;
    case bench::queue_type_t::FAA:
      run_benches<faa_queue, faa_queue_ref>(
          queue_name,
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
          total_ops,
          runs,
          [](auto& queue, auto thread_id) -> auto {
            return msc_queue_ref(queue, thread_id);
          }
      );
      break;
  }

  return 0;
}

template <typename Q, typename R>
void run_benches(
    const std::string&      queue_name,
    std::size_t             total_ops,
    std::size_t             runs,
    make_queue_ref_fn<Q, R> make_queue_ref
) {
  for (auto threads : THREADS) {
    // aborts if hyper-threads would be used (assuming 2 HT per core)
    if (threads > std::thread::hardware_concurrency() / 2) {
      break;
    }

    bench_reads<Q, R>(queue_name, threads, total_ops, runs, make_queue_ref);
  }
}

template <typename Q, typename R>
void bench_reads(
    const std::string& queue_name,
    std::size_t threads,
    std::size_t total_ops,
    std::size_t runs,
    make_queue_ref_fn<Q, R> make_queue_ref
) {
  if (threads % 4 != 0) {
    throw std::invalid_argument("`threads` must be divisible by four");
  }

  const auto ops_per_thread = total_ops / threads;

  std::vector<std::size_t> thread_ids{};
  thread_ids.reserve(threads);
  for (std::size_t thread = 0; thread < threads; ++thread) {
    thread_ids.push_back(thread);
  }

  for (std::size_t run = 0; run < runs; ++run) {
    auto queue = std::make_unique<Q>();
    boost::barrier barrier{ static_cast<unsigned>(threads + 1) };

    std::vector<std::thread> thread_handles{};
    thread_handles.reserve(threads);

    auto&& queue_ref = make_queue_ref(*queue, 0);
    for (std::size_t op = 0; op < total_ops / 2; ++op) {
      queue_ref.enqueue(&thread_ids.at(0));
    }

    for (std::size_t thread = 0; thread < threads; ++thread) {
      thread_handles.emplace_back(std::thread([&, thread] {
        bench::pin_current_thread(thread);

        auto&& queue_ref = make_queue_ref(*queue, thread);

        // all threads synchronize at this barrier before starting
        barrier.wait();

        if (thread % 4 == 0) {
          writer_thread<R>(queue_ref, ops_per_thread, &thread_ids.at(thread));
        } else {
          reader_thread<R>(queue_ref, ops_per_thread);
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

template <typename R>
void reader_thread(R queue_ref, std::size_t ops_per_thread) {
  for (std::size_t op = 0; op < ops_per_thread; ++op) {
    auto elem = queue_ref.dequeue();
    if (elem == nullptr) {
      bench::spin_for_ns(75);
    } else {
      bench::spin_for_ns(50);
    }
  }
}

template <typename R>
void writer_thread(R queue_ref, std::size_t ops_per_thread, std::size_t* thread_id) {
  for (std::size_t op = 0; op < ops_per_thread; ++op) {
    queue_ref.enqueue(thread_id);
    bench::spin_for_ns(25);
  }
}
