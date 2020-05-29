#ifndef LOO_QUEUE_BENCHES_COMMON_HPP
#define LOO_QUEUE_BENCHES_COMMON_HPP

#include <string>

namespace bench {
enum class queue_type_t {
  LCR,
  LOO,
  FAA,
  MSC
};

constexpr const char* display_str(queue_type_t queue) {
  switch (queue) {
    case queue_type_t::LCR: return "LCR";
    case queue_type_t::LOO: return "LOO";
    case queue_type_t::FAA: return "FAA";
    case queue_type_t::MSC: return "MSC";
    default: return "unknown";
  }
}

queue_type_t parse_queue_str(const std::string& queue);
}

#endif /* LOO_QUEUE_BENCHES_COMMON_HPP */
