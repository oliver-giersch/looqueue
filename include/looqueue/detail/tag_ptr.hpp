#ifndef LOO_QUEUE_DETAIL_MARKED_PTR_HPP
#define LOO_QUEUE_DETAIL_MARKED_PTR_HPP

#include <cstdint>

#include "looqueue/def.hpp"

namespace loo::detail {
/*
 * A tag pointer that stores up to N tag bits in the lower bits of the
 * pointer word.
 * The pointer value must have its lower N bits zeroed, meaning the value
 * pointed at must have 2^N alignment.
 */
template <typename T, u8 N>
class tag_ptr final {
public:
	using pointer = T *;
	using tag_type = usize;

	static constexpr usize one = 1;
	static constexpr usize tag_bits = N;
	static constexpr usize tag_mask = (one << tag_bits) - one;
	static constexpr usize ptr_mask = ~tag_mask;

	struct decomposed {
		pointer ptr;
		tag_type idx;
	};

	tag_ptr() = default;

	explicit tag_ptr(tag_type marked) : marked { marked } { }

	explicit tag_ptr(pointer ptr, tag_type idx)
			: tag_ptr(reinterpret_cast<usize>(ptr) | idx)
	{
	}

	decomposed
	decompose() const
	{
		return { this->decompose_ptr(), this->decompose_tag() };
	}

	pointer
	decompose_ptr() const
	{
		return reinterpret_cast<pointer>(this->marked & ptr_mask);
	}

	tag_type
	decompose_tag() const
	{
		return marked & tag_mask;
	}

	std::uintptr_t
	to_uintptr() const
	{
		return this->marked;
	}

	std::uintptr_t &
	as_uintptr()
	{
		return this->marked;
	}

	void
	inc_idx(tag_type add = 1)
	{
		this->marked += add;
	}

	tag_ptr
	operator+(tag_type add) const
	{
		return tag_ptr { this->marked + add };
	}

	void
	operator+=(tag_type add)
	{
		this->marked += add;
	}

private:
	usize marked { 0 };
};
}

#endif /* LOO_QUEUE_DETAIL_MARKED_PTR_HPP */
