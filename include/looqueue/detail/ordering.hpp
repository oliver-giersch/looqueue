#ifndef LOO_QUEUE_DETAIL_ORDERING_HPP
#define LOO_QUEUE_DETAIL_ORDERING_HPP

#include <atomic>

#ifdef LOO_USE_SEQ_CST
#define RELAXED std::memory_order_seq_cst
#define ACQUIRE std::memory_order_seq_cst
#define RELEASE std::memory_order_seq_cst
#define ACQ_REL std::memory_order_seq_cst
#else
#define RELAXED std::memory_order_relaxed
#define ACQUIRE std::memory_order_acquire
#define RELEASE std::memory_order_release
#define ACQ_REL std::memory_order_acq_rel
#endif

#endif /** LOO_QUEUE_DETAIL_ORDERING_HPP */
