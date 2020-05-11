#ifndef LOO_QUEUE_DETAIL_ALIGN_HPP
#define LOO_QUEUE_DETAIL_ALIGN_HPP

#include <cstdint>

/** reasonable effort approach to detect x86-64 target architecture, where the pre-fetcher fetches
 *  two consecutive cache lines by default */
#if defined(__x86_64__) || defined(_M_AMD64)
#define DESTRUCTIVE_INFERENCE_SIZE 128
#else
#define DESTRUCTIVE_INFERENCE_SIZE 64
#endif

constexpr std::size_t CACHE_LINE_ALIGN = DESTRUCTIVE_INFERENCE_SIZE;

#endif /* LOO_QUEUE_DETAIL_ALIGN_HPP */
