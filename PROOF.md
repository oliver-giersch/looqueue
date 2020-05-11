# Correctness Proof

This document presents the relevant correctness proofs for the lock-free
unbounded MPMC FIFO queue implemented in this library called `loo::queue`.

## 1. Linearizability

Informally, *linearizability* as defined by Herlihy requires for every data
structure operation to appear to take effect instantaneously to an external
observer at some specific point in between being initiated and concluding.
For this to be the case, every operation must have a *linearization point* (LP)
at which its effects become observable.

**Lemma 1.1.** Every enqueue operation *E(v)* takes effect atomically at one
statement.

**Proof.** The LP for an enqueue operation is the instant in which the
inserted element *v* becomes observable by other threads and can be dequeued.

There are two separate opportunities for linearization during the course of
the enqueue algorithm.
The first potential LP is the atomic FAA that performs a bitwise write of *v*
into the reserved slot.
However, linearization only occurs at this point and the element *v* becomes
observable to a dequeuer, if the slot did **not** already contain the `READER`
bit *before* the FAA completed, which could have been set by a competing FAA on
the same slot by the corresponding dequeue operation.
In this case, linearization occurs either at some subsequent retry at this point
or the second potential LP.
The second opportunity for linearization is at the bounded CAS loop after a
*successful* CAS that appends a newly allocated node with *v* tentatively
inserted in the first slot to the current tail node by exchanging its next
pointer.
The CAS that eventually becomes the LP does not necessarily have to succeed and
it is sufficient to determine, that the tail pointer has been advanced.
In either case, the new tail node will have become visible to dequeueing threads
and the write of *v* into the first slot is finalized.

**Lemma 1.2** Every dequeue operation *D()* takes effect atomically at one
statement.

**Proof.** The LP for a dequeue operation is the instant in which the queue is
either (a) determined to be empty or (b) an element in a slot is consumed so
that it can be returned and also not be dequeued by any other other thread.

There are two potential LPs for determining a queue to be empty and another
potential LP for consuming an element.
The first opportunity for determining an empty queue is the tail pointer load
before incrementing the dequeue index.
The second opportunity is at the load of the current head node's next pointer
in the slow path.
The potential LP at which an element can actually be dequeued is the FAA that
sets the `READER` bit in the reserved slot and returns the previous slot's
state.
Linearization occurs at this point, only if the previous state contains some
non-zero bit pattern in the bits reserved for storing the element itself, which
indicates an enqueue operation has linearized at this slot earlier and inserted
an element.
Otherwise, the operation will linearize at some subsequent retry at either this
point, or at one of the two empty checks, if the queue has become empty in the
meantime due to concurrent dequeue operations.

Because every dequeue operation has to *first* atomically increment the dequeue
index (and subsequently use its previous value), it follows that every slot can
only be accessed by exactly one dequeue operation and therefore every element
can be dequeued at most once.

## 2. Overflow Guarantees

Both enqueue and dequeue operations are required to atomically increment their
associated index value, which is stored in the same memory word as the pointer to the respective node.
Given the two constants $N$ (the array size of each node) and $B$ (the number of available tag bits), the maximum extent of the index value for any distinct $(pointer, index)$ pair can assume before the pointer is advanced and the index is reset can be bound by the number of (preemptable) threads accessing the same
queue.
We are thereby able to prove that an overflow of the index bits into the pointer bits is impossible as long as the number of threads stays below a precise threshold.

**Definition 2.1** Let $P$ be number of *all* producer threads accessing the same queue and $2^B - 1$ the largest possible integer value that fits into the tag bits of the composed $(tail, index)$ pair without overflowing.

**Lemma 2.1** The largest value the enqueue index can possibly assume is $P + N$ and it follows, that with any $P \leq 2^B - N - 1$ the enqueue index can never overflow.

**Proof.** Assume that the current value of the enqueue index is $N$, which means that there have been $N$ previous increments which resulted in fast path operations that can unconditionally enqueue another element or retry after concluding (e.g., due to having to abandon the reserved slot).
Subsequently, there can hence be at most $P$ further concurrent enqueue attempts (one per producer thread), all of which must necessarily enter the slow path.
Within that path there are four sub-paths, none of which permit a thread to exit it **and** then potentially observe and increment the previous $(tail, index)$ pair again.
The sub-path beginning at line T4 can only be taken if the tail pointer has already been updated, in which case the enqueue index has also been reset and can not be incremented again.
The sub-paths at lines T11 and T21 can likewise only result in an executing thread leaving the procedure if one of the CAS at either line has succeeded in updating the $(tail, index)$ pair.
The sub-path at line T18 can never leave the procedure at all and can also be taken at most once per thread.
In total, the index value can thus never exceed $P + N$.
By equating this hard upper bound with the largest *possible* index value ($2^B - 1$), it can be concluded that with $P \leq 2^B - N - 1$ distinct producer threads the enqueue index can never overflow.

**Definition 2.2** Let $C$ be number of *all* consumer threads accessing the same queue and $2^B - 1$ the largest possible integer value that fits into the tag bits of the composed $(head, index)$ pair without overflowing.

**Lemma 2.2** The largest possible value the dequeue index can possibly assume is $2 \cdot C + N - 1$ and it follows, that with any $C \leq \frac{2^B - N - 1}{2}$ the dequeue index can never overflow.

**Proof.** Assume that the current value of the dequeue index is $N - 1$, which means that there have been $N - 1$ previous increments which resulted in fast path operations, i.e., those that unconditionally may attempt to dequeue another element or retry.
Additionally, the queue's head and tail nodes currently point at the same node and the enqueue index is some value $E \ge N$, i.e., all slots have already been either written to or abandoned.
Under any other conditions, the dequeue index can only grow up to $C + N$.

Further assume that there are exactly $C$ subsequent concurrent dequeue attempts, all of which pass the *empty* check at line D6 before the first operation increments the dequeue index (otherwise all follow-on operations would not pass the check).
All $C$ threads necessarily enter the slow path and perform a second *empty* check at line H3.
Assuming that either no enqueue operation has appended a new node or that a new node has been appended but the tail pointer not yet been swung forward, all $C$ threads will assess the queue to be empty.
The value of the dequeue index is now $C + N$, but all $C$ consumers may yet initiate another dequeue operation.
However, none may pass the first *empty* check and increment the dequeue index until the queue's tail pointer has been updated by a corresponding enqueue operation.
Since the tail pointer is exclusively updated only *after* a new node has already been appended (see lines T10, T11 and T21), when any of the $C$ additional operations are permitted to move past the *empty* check at line D6, it is guaranteed that the current head's next pointer is no longer `NULL` and hence, none of these threads can ever fail the second *empty* check at line H3 again for the same $(head, index)$ pair.
Consequently, all $C$ threads will remain in the slow path until the queue's head pointer is updated and the dequeue index is reset, in which case the previous value can't be incremented any further.
Therefore, the dequeue index can not exceed $2 \cdot C + N$.
As before, it can be concluded that with $C \leq \frac{2^{B} - N - 1}{2}$ consumer threads the dequeue index can never overflow.

## 3. Lock Freedom

Our queue has full non-blocking and lock-free progress guarantees.
For the fast path and in the general case, *all* threads can make progress by simply inserting into or consuming from their reserved array slot.
However, this can not be guaranteed, since it is possible for a slot having to be abandoned and both corresponding operations having to retry.
This can, in fact, theoretically result in a temporary live-lock situation, in which all threads continuously have to abandon one slot after another.
However, this live-lock can last at most for $N$ steps, after which the current node is drained and all dequeue operations have to conclude that the queue is empty, whereas at least one enqueue operation is guaranteed to be able to insert its element through the CAS at line T10, so at least one thread is able to make progress in a bounded number of steps.

Threads entering the slow path, on the other hand, may not leave it again, until the desired (path-local) progress has been made by some thread, i.e., when the respective operation's node has been advanced.
Therefore, threads may be required to loop inside this path in order to guarantee this property.
However, the number of steps required to make progress is bound by the total number of producer or consumer threads:
The only possibility for a thread in the slow path to fail at either effecting or observing progress in advancing the respective node is another thread interfering by incrementing the index value (Lines E3 and D8).
Since a thread in the slow path can not leave it and increment the same index value again, this provides a "soak-off" effect:
The probability for further interfering FAA increments decreases and eventually drops to zero, once all producer or consumer threads have entered the slow path and are "trapped" in it, at which point progress by one of the threads in that path can be guaranteed.
This progress will eventually be observed by the other trapped threads, allowing them to proceed as well.