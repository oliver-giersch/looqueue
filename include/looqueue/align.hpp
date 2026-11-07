#ifndef LOO_QUEUE_ALIGN_HPP
#define LOO_QUEUE_ALIGN_HPP

// Reasonable effort approach to detect x86-64 target architecture, where the
// pre-fetcher fetches two consecutive cache lines by default.
#if defined(__x86_64__) || defined(_M_AMD64)
#define DESTRUCTIVE_INTERFERENCE_SIZE 128
#else
#include <new>
#define DESTRUCTIVE_INTERFERENCE_SIZE                                          \
	std::hardware_destructive_interference_size
#endif

namespace loo::detail {
inline constexpr auto cache_alignment = DESTRUCTIVE_INTERFERENCE_SIZE;
}

#endif /* LOO_QUEUE_ALIGN_HPP */
