#ifndef LOO_QUEUE_BENCHES_HAZARD_POINTERS_HPP
#define LOO_QUEUE_BENCHES_HAZARD_POINTERS_HPP

#include "benches/hazard_pointers/hazard_pointers_fwd.hpp"

#include <stdexcept>

namespace memory {
template<typename T>
hazard_pointers<T>::hazard_pointers(
    std::size_t num_threads,
    std::size_t num_hazard_pointers,
    std::size_t scan_threshold
) : m_num_hazard_pointers{ num_hazard_pointers },
    m_scan_threshold{ scan_threshold },
    m_thread_blocks{ num_threads }
{
  if (num_hazard_pointers > MAX_HAZARD_POINTERS) {
    throw std::invalid_argument("`num_hazard_pointers` must be <= 8");
  }
}

template<typename T>
hazard_pointers<T>::~hazard_pointers<T>() noexcept {
  for (auto& thread_block : this->m_thread_blocks) {
    for (auto retired : thread_block.retired_objects) {
      delete retired;
    }
  }
}

template<typename T>
void hazard_pointers<T>::clear_one(std::size_t thread_id, std::size_t hp) {
  auto& thread_block = this->m_thread_blocks[thread_id];
  thread_block.hazard_pointers[hp].ptr.store(nullptr, std::memory_order_release);
}

template<typename T>
void hazard_pointers<T>::clear(std::size_t thread_id) {
  auto& thread_block = this->m_thread_blocks[thread_id];
  for (auto& hazard_ptr : thread_block.hazard_pointers) {
    hazard_ptr.ptr.store(nullptr, std::memory_order_relaxed);
  }

  std::atomic_thread_fence(std::memory_order_release);
}

template<typename T>
typename hazard_pointers<T>::pointer hazard_pointers<T>::protect(
    const std::atomic<hazard_pointers::pointer>& atomic,
    std::size_t thread_id,
    std::size_t hp
) {
  auto& hazard_ptr = this->m_thread_blocks[thread_id].hazard_pointers[hp];
  auto curr = atomic.load();
  while (true) {
    hazard_ptr.ptr.store(curr);
    const auto temp = atomic.load();
    if (curr == temp) {
      return curr;
    }

    curr = temp;
  }
}

template<typename T>
typename hazard_pointers<T>::pointer hazard_pointers<T>::protect_ptr(
    hazard_pointers::pointer ptr,
    std::size_t thread_id,
    std::size_t hp
) {
  auto& hazard_ptr = this->m_thread_blocks[thread_id].hazard_pointers[hp];
  hazard_ptr.ptr.store(ptr);

  return ptr;
}

template<typename T>
void hazard_pointers<T>::retire(hazard_pointers::pointer ptr, std::size_t thread_id) {
  auto& thread_retired_objects = this->m_thread_blocks[thread_id].retired_objects;
  thread_retired_objects.push_back(ptr);

  if (thread_retired_objects.size() < this->m_scan_threshold) {
    return;
  }

  std::size_t curr = 0;
  while (curr < thread_retired_objects.size()) {
    const auto retired = thread_retired_objects[curr];
    if (this->can_reclaim(retired)) {
      auto back = thread_retired_objects.back();
      if (retired != back) {
        std::swap(thread_retired_objects[curr], back);
      }

      thread_retired_objects.pop_back();
      delete retired;
      continue;
    }

    ++curr;
  }
}

template <typename T>
bool hazard_pointers<T>::can_reclaim(hazard_pointers::pointer retired) const {
  for (const auto& thread_block : this->m_thread_blocks) {
    for (std::size_t hp = 0; hp < this->m_num_hazard_pointers; ++hp) {
      if (thread_block.hazard_pointers[hp].ptr.load() == retired) {
        return false;
      }
    }
  }

  return true;
}
}

#endif /* LOO_QUEUE_BENCHES_HAZARD_POINTERS_HPP */
