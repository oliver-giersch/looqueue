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

(todo)

## 3. Lock Freedom

(todo)
