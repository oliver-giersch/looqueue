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
