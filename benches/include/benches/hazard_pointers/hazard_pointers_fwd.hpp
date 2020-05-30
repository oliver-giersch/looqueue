#ifndef LOO_QUEUE_BENCHES_HAZARD_POINTERS_FWD_HPP
#define LOO_QUEUE_BENCHES_HAZARD_POINTERS_FWD_HPP

#include <array>
#include <atomic>
#include <vector>

#include "looqueue/align.hpp"

namespace memory {
template <typename T>
class hazard_pointers final {
public:
  using pointer = T*;

  /** constructor */
  explicit hazard_pointers(
    std::size_t num_threads,
    std::size_t num_hazard_pointers = MAX_HAZARD_POINTERS,
    std::size_t scan_threshold = DEFAULT_SCAN_THRESHOLD
  );

  /** destructor - deletes all remaining retired objects */
  ~hazard_pointers() noexcept;
  /** clears all hazard pointers for the thread with the given id */
  void clear(std::size_t thread_id);
  /** clears one hazard pointer for the thread with the given id */
  void clear_one(std::size_t thread_id, std::size_t hp);

  pointer protect(
      const std::atomic<pointer>& atomic,
      std::size_t thread_id,
      std::size_t hp
    );

  pointer protect_ptr(pointer ptr, std::size_t thread_id, std::size_t hp);

  void retire(pointer ptr, std::size_t thread_id);

  hazard_pointers(const hazard_pointers&)            = delete;
  hazard_pointers(hazard_pointers&&)                 = delete;
  hazard_pointers& operator=(const hazard_pointers&) = delete;
  hazard_pointers& operator=(hazard_pointers&&)      = delete;

private:
  static constexpr std::size_t MAX_HAZARD_POINTERS = 8;
  static constexpr std::size_t DEFAULT_SCAN_THRESHOLD = 0;

  using hazard_pointer_array_t = std::array<std::atomic<pointer>, MAX_HAZARD_POINTERS>;

  struct thread_block_t {
    thread_block_t() = default;
    thread_block_t(thread_block_t&&) noexcept = default;

    alignas(CACHE_LINE_ALIGN) std::vector<pointer> retired_objects{};
    alignas(CACHE_LINE_ALIGN) hazard_pointer_array_t hazard_pointers{};
  };

  bool can_reclaim(pointer retired) const;

  const std::size_t m_num_hazard_pointers;
  const std::size_t m_scan_threshold;
  std::vector<thread_block_t> m_thread_blocks;
};
}

#endif /* LOO_QUEUE_BENCHES_HAZARD_POINTERS_FWD_HPP */
