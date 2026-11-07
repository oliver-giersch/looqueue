#ifndef LOO_QUEUE_NATIVE_MARKED_PTR_HPP
#define LOO_QUEUE_NATIVE_MARKED_PTR_HPP

#include "looqueue/def.hpp"

namespace loo::detail {
/*
 * This representation requires internal node addresses below 2^48.
 * Linux normally allocates within that range *by default*, but does not
 * guarantee it for arbitrary allocators or mappings.
 */
template <typename T, u8 N>
class native_tag_ptr final {
	static_assert(N <= 16, "only up to 16 tag bits allowed");

public:
	using pointer = T *;
	using tag_type = usize;

	static constexpr usize tag_shift = 64 - 16;
	static constexpr usize tag_mask = usize { 0xFFFF } << tag_shift;
	static constexpr usize ptr_mask = ~tag_mask;
	static constexpr usize one = 1ull << tag_shift;

	struct decomposed {
		pointer ptr;
		tag_type idx;
	};

	native_tag_ptr() = default;

	explicit native_tag_ptr(tag_type marked) : marked { marked } { }

	explicit native_tag_ptr(pointer ptr, tag_type idx)
			: native_tag_ptr { idx << tag_shift | reinterpret_cast<tag_type>(ptr) }
	{
	}

	decomposed decompose() const
	{
		return { this->decompose_ptr(), this->decompose_tag() };
	}

	pointer decompose_ptr() const
	{
		return reinterpret_cast<pointer>(this->marked & ptr_mask);
	}

	tag_type decompose_tag() const
	{
		return marked >> tag_shift;
	}

	usize to_uintptr() const
	{
		return this->marked;
	}

	usize &as_uintptr()
	{
		return this->marked;
	}

	void inc_idx(tag_type add = 1)
	{
		this->marked += (add << tag_shift);
	}

	native_tag_ptr operator+(tag_type add) const
	{
		return native_tag_ptr { this->marked + (add << tag_shift) };
	}

	void operator+=(tag_type add)
	{
		this->marked += (add << tag_shift);
	}

private:
	usize marked { 0 };
};
}

#endif /* LOO_QUEUE_NATIVE_MARKED_PTR_HPP */
