#include "benches/common.hpp"

#include <iostream>
#include <stdexcept>

#include <pthread.h>

namespace bench {
queue_type_t parse_queue_str(const std::string& queue) {
  if (queue == "lcr") {
    return queue_type_t::LCR;
  }

  if (queue == "loo") {
    return queue_type_t::LOO;
  }

  if (queue == "faa") {
    return queue_type_t::FAA;
  }

  if (queue == "msc") {
    return queue_type_t::MSC;
  }

  throw std::invalid_argument(
      "argument `queue` must be 'lcr', 'loo', 'faa' or 'msc'"
  );
}

std::size_t parse_size_str(const std::string& size) {
  if (size == "1K") {
    return 1024;
  }

  if (size == "10K") {
    return 10 * 1024;
  }

  if (size == "50K") {
    return 50 * 1024;
  }

  if (size == "100K") {
    return 100 * 1024;
  }

  if (size == "1M") {
    return 1024 * 1024;
  }

  if (size == "10M") {
    return 10 * 1024 * 1024;
  }

  if (size == "50M") {
    return 50 * 1024 * 1024;
  }

  if (size == "100M") {
    return 100 * 1024 * 1024;
  }

  throw std::invalid_argument("argument `size` must be one of [1K|10K|50K|100K|1M|10M|50M|100M]");
}

std::size_t parse_runs_str(const std::string& runs) {
  auto val = std::stoi(runs);
  if (val < 0 || val > 50) {
    throw std::invalid_argument("argument `runs` must be between 0 and 50");
  }

  return static_cast<std::size_t>(val);
}

void pin_current_thread(std::size_t thread_id) {
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(thread_id, &set);

  const auto res = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &set);
  if (res != 0) {
    throw std::system_error(res, std::generic_category(), "failed to pin thread");
  }
}
}
