#include "benches/common.hpp"

#include <stdexcept>

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
}
