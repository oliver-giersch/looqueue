#ifndef LOO_QUEUE_NATIVE_MARKED_PTR_HPP
#define LOO_QUEUE_NATIVE_MARKED_PTR_HPP

#include <cstdint>
#include <utility>

namespace loo::detail {
/*
 * A tag pointer that stores up to N tag bits in the upper 16 bits of the
 * pointer word.
 * On x86-64 in Linux, all virtual user addresses are naturally below
 * 0x7FFFFFFFFFFF by convention as of today.
 * This may change in the future, if 5-level paging becomes more common.
 */
template <typename T, std::uint8_t N>
class native_tag_ptr_t final {
	static_assert(N <= 16, "only up to 16 tag bits allowed");

public:
	using pointer = T *;
	using tag_type = std::uintptr_t;

	static constexpr std::uintptr_t TAG_SHIFT = 64 - 16;
	static constexpr std::uintptr_t TAG_MASK
		= std::uintptr_t { 0xFFFF } << TAG_SHIFT;
	static constexpr std::uintptr_t PTR_MASK = ~TAG_MASK;
	static constexpr std::uintptr_t ONE = 1ull << TAG_SHIFT;

	struct decomposed_t {
		pointer ptr;
		tag_type idx;
	};

	native_tag_ptr_t() = default;

	explicit native_tag_ptr_t(std::uintptr_t marked) : m_marked { marked } { }

	explicit native_tag_ptr_t(pointer ptr, tag_type idx)
			: native_tag_ptr_t { idx << TAG_SHIFT
				| reinterpret_cast<std::uintptr_t>(ptr) }
	{
	}

	decomposed_t
	decompose() const
	{
		return { this->decompose_ptr(), this->decompose_tag() };
	}

	pointer
	decompose_ptr() const
	{
		return reinterpret_cast<pointer>(this->m_marked & PTR_MASK);
	}

	tag_type
	decompose_tag() const
	{
		return m_marked >> TAG_SHIFT;
	}

	std::uintptr_t
	to_uintptr() const
	{
		return this->m_marked;
	}

	std::uintptr_t &
	as_uintptr()
	{
		return this->m_marked;
	}

	void
	inc_idx(tag_type add = 1)
	{
		this->m_marked += (add << TAG_SHIFT);
	}

	native_tag_ptr_t
	operator+(tag_type add) const
	{
		return native_tag_ptr_t { this->m_marked + (add << TAG_SHIFT) };
	}

	void
	operator+=(tag_type add)
	{
		this->m_marked += (add << TAG_SHIFT);
	}

private:
	std::uintptr_t m_marked { 0 };
};
}

#endif /* LOO_QUEUE_NATIVE_MARKED_PTR_HPP */
