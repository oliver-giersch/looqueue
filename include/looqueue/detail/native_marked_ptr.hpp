#ifndef LOO_QUEUE_NATIVE_MARKED_PTR_HPP
#define LOO_QUEUE_NATIVE_MARKED_PTR_HPP

#include <cstdint>
#include <utility>

namespace loo::detail {
template <typename T, std::uint8_t N>
class native_marked_ptr_t final {
  static_assert(N <= 9, "only up to 16 tag bits allowed");
public:
  using pointer  = T*;
  using tag_type = std::uint64_t;

  static constexpr std::uint64_t TAG_SHIFT = 55;
  static constexpr std::uint64_t TAG_MASK  = 0x1FFull << TAG_SHIFT;
  static constexpr std::uint64_t PTR_MASK  = ~TAG_MASK;
  static constexpr std::uint64_t INCREMENT = 1ull << TAG_SHIFT;

  struct decomposed_t {
    pointer  ptr;
    tag_type idx;
  };

  /** constructor (default) */
  native_marked_ptr_t() = default;
  /** constructor(s) */
  explicit native_marked_ptr_t(std::uint64_t marked) : m_marked{ marked } {}
  explicit native_marked_ptr_t(pointer ptr, tag_type idx) :
      native_marked_ptr_t(idx << TAG_SHIFT | reinterpret_cast<std::uint64_t>(ptr)) {}

  decomposed_t decompose() const {
    return { this->decompose_ptr(), this->decompose_tag() };
  }

  pointer decompose_ptr() const {
    return reinterpret_cast<pointer>(this->m_marked & PTR_MASK);
  }

  tag_type decompose_tag() const {
    return m_marked >> TAG_SHIFT;
  }

  /** returns the underlying integer value */
  std::uint64_t to_int() const {
    return this->m_marked;
  }

  /** returns a reference to the underlying integer value */
  std::uint64_t& as_int() {
    return this->m_marked;
  }

  void inc_idx(tag_type add = 1) {
    this->m_marked += (add << TAG_SHIFT);
  }

private:
  std::uint64_t m_marked{ 0 };
};
}

#endif /* LOO_QUEUE_NATIVE_MARKED_PTR_HPP */
