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

/** parses the given string to the corresponding queue type */
queue_type_t parse_queue_str(const std::string& queue);
/** pins the thread with the given id to the core with the same number */
void pin_current_thread(std::size_t thread_id);
}

#endif /* LOO_QUEUE_BENCHES_COMMON_HPP */
