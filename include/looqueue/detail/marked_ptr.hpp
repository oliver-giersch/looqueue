#ifndef LOO_QUEUE_DETAIL_MARKED_PTR_HPP
#define LOO_QUEUE_DETAIL_MARKED_PTR_HPP

#include <cstdint>
#include <utility>

namespace loo {
namespace detail {
template <typename T, std::uint8_t N>
class marked_ptr_t final {
public:
  using pointer  = T*;
  using tag_type = std::uint64_t;

  static constexpr std::uint64_t TAG_BITS = N;
  static constexpr std::uint64_t TAG_MASK = (1ull << TAG_BITS) - 1;
  static constexpr std::uint64_t PTR_MASK = ~TAG_MASK;

  struct decomposed_t {
    pointer  ptr;
    tag_type idx;
  };

  /** constructor (default) */
  marked_ptr_t() = default;
  /** constructor(s) */
  explicit marked_ptr_t(std::uint64_t marked) : m_marked{ marked } {}
  explicit marked_ptr_t(pointer ptr, tag_type idx) :
    marked_ptr_t(reinterpret_cast<std::uint64_t>(ptr) | idx) {}

  decomposed_t decompose() const {
    return { this->decompose_ptr(), this->decompose_tag() };
  }

  pointer decompose_ptr() const {
    return reinterpret_cast<pointer>(this->m_marked & PTR_MASK);
  }

  tag_type decompose_tag() const {
    return m_marked & TAG_MASK;
  }

  /** returns the underlying integer value */
  std::uint64_t to_integer() const {
    return this->m_marked;
  }

  /** returns a reference to the underlying integer value */
  std::uint64_t& as_integer() {
    return this->m_marked;
  }

  void inc_idx(tag_type add = 1) {
    this->m_marked += add;
  }

private:
  std::uint64_t m_marked{ 0 };
};
}
}

#endif /* LOO_QUEUE_DETAIL_MARKED_PTR_HPP */
