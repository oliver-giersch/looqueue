# Algorithm Changes

This document lays out all changes to the algorithm as initially described and
their effects.

## 1. Moving the one-time iteration of all slots into the slow path

In the initial algorithm, the final fast-path dequeue operation for the current
head node, i.e. the one that reserved `NODE_SIZE - 1` as its slot was tasked
with initiating the `try_reclaim` procedure, which involves traversing every
slot in the node's array and checking if all previous fast-path operations have
already concluded. Otherwise, a special bit would be set and the iteration
aborted and later picked up again, once the respective delayed fast-path
operation reached the atomic "handshake" operation, detecting the bit and
resuming the procedure.

A alternative not affecting the algorithms performance was implemented, which
moved responsibility for initiating the `try_reclaim` procedure from the last
fast-path to the first slow-path operation, thereby speeding up the former and
further slowing down the latter. Overall, this change had no effect on
performance, since the vast majority (> 99%) is not affected by it at all.

## 2. Reordering the loads of `head` and `tail` in the dequeue procedure

Before incrementing the dequeue index at the start of each dequeue procedure, a
check is performed to avoid incrementing this index, if the queue is currently
empty. This check involves loading both the (`head` pointer, dequeue index) pair
as well as the (`tail` pointer, enqueue index) pair, which are stored in the two
most hotly contested shared memory objects involved in this algorithm.

In the case that the queue is **not** empty, the variable containing the former
pair is incremented immediately after performing. Hence, reordering the load of
`tail` before `head`, there is a greater possibility, the thread still has
ownership of `head`'s cache line when incrementing it. This assumes a x86-64
protocol for cache-coherency.

This reordering does not affect the algorithm's correctness, but it has a subtle
effect, nonetheless: There is a chance the executing thread has an outdated view
of the the `tail` variable after loading the `head` variable, whereas before it
was the other way around. When assessing the queue's state, it is possible that,
in the meantime, further elements have been enqueued in the tail node or even
that a new node has been appended and replaced it. Of course, it is also
possible that the state changes yet again in between loading `head` and
incrementing it, regardless of the order of the previous two operations.
The case of the queue becoming empty in between assessing its current state and
incrementing the dequeue index is accounted for either way. Reordering these two
loads merely alters the probabilities for observing different types of outdated
states.

This little change brings pretty drastic improvements of "raw" dequeue
performance, enabling the queue to have comparable dequeue performance to LCRQ,
which is able to avoid initial empty checks altogether.      

