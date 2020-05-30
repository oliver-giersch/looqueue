#ifndef LOO_QUEUE_BENCHES_HAZARD_POINTERS_HPP
#define LOO_QUEUE_BENCHES_HAZARD_POINTERS_HPP

#include "benches/hazard_pointers/hazard_pointers_fwd.hpp"

#include <stdexcept>

namespace memory {
template <typename T>
hazard_pointers<T>::hazard_pointers(
    const std::size_t num_threads,
    const std::size_t num_hazard_pointers,
    const std::size_t scan_threshold
) : m_num_hazard_pointers{ num_hazard_pointers },
    m_scan_threshold{ scan_threshold },
    m_thread_blocks{ num_threads }
{
  if (num_hazard_pointers > MAX_HAZARD_POINTERS) {
    throw std::invalid_argument("`num_hazard_pointers` must be <= 8");
  }
}

template <typename T>
hazard_pointers<T>::~hazard_pointers<T>() noexcept {
  for (auto& thread_block : this->m_thread_blocks) {
    for (auto retired : thread_block.retired_objects) {
      delete retired;
    }
  }
}

template <typename T>
void hazard_pointers<T>::clear_one(
    const std::size_t thread_id,
    const std::size_t hp
) {
  auto& thread_block = this->m_thread_blocks.at(thread_id);
  thread_block.hazard_pointers.at(hp).store(nullptr, std::memory_order_release);
}

template <typename T>
void hazard_pointers<T>::clear(const std::size_t thread_id) {
  auto& thread_block = this->m_thread_blocks.at(thread_id);
  for (auto& hp : thread_block.hazard_pointers) {
    hp.store(nullptr, std::memory_order_relaxed);
  }

  std::atomic_thread_fence(std::memory_order_release);
}

template <typename T>
typename hazard_pointers<T>::pointer hazard_pointers<T>::protect(
    const std::atomic<hazard_pointers::pointer>& atomic,
    const std::size_t thread_id,
    const std::size_t hp
) {
  auto& hazard_pointer = this->m_thread_blocks.at(thread_id).hazard_pointers.at(hp);
  auto curr = atomic.load();
  while (true) {
    hazard_pointer.store(curr);
    const auto temp = atomic.load();
    if (curr == temp) {
      return curr;
    }

    curr = temp;
  }
}

template <typename T>
typename hazard_pointers<T>::pointer hazard_pointers<T>::protect_ptr(
    const hazard_pointers::pointer ptr,
    const std::size_t thread_id,
    const std::size_t hp
) {
  auto& hazard_pointer = this->m_thread_blocks.at(thread_id).hazard_pointers.at(hp);
  hazard_pointer.store(ptr);
  return ptr;
}

template <typename T>
void hazard_pointers<T>::retire(
    hazard_pointers::pointer ptr,
    const std::size_t thread_id
) {
  throw std::runtime_error("not yet implemented");
}
}

#endif /* LOO_QUEUE_BENCHES_HAZARD_POINTERS_HPP */
