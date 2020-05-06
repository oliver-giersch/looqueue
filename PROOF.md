# Correctness Proof

This document presents the relevant correctness proofs for the lock-free unbounded MPMC FIFO queue implemented in this library called `loo::queue`.

## 1. Linearizability

Informally, *linearizability* as defined by Herlihy requires for every data structure operation to appear to take effect instantaneously to an external observer at some specific point in between being initiated and concluding.
For this to be the case, every operation must have a *linearization point* (LP), at which its effects become observable.

**Lemma 1.1.** Every enqueue operation *E(v)* takes effect atomically at one statement.

**Proof.** The LP for an enqueue operation is the instant in which the inserted element *v* becomes observable by other threads in the sense that it can be dequeued.
There are two atomic instructions in the enqueue algorithm that can have this effect.

There are two separate opportunities for liniearization during the course of the enqueue algorithm.
The first potential LP is the atomic FAA that performs a bitwise write of *v* into the reserved slot.
However, linearization only occurs at this point and the element *v* becomes observable to a dequeuer, if the slot did **not** already contain the `READ` bit *before* the FAA completed, which could have been set by a competing FAA on the same slot by the corresponding dequeue operation.
In this case, linearization occurs at some subsequent retry at this point or the second potential LP.
The second opportunity for linearization is at the bounded CAS loop after a *successful* CAS that appends a newly allocated node with *v* tentatively inserted in the first slot to the current tail node by exchanging its next pointer.
The CAS that eventually becomes the LP does not necessarily have to succeed, it suffices to determine that the tail pointer has been advanced.

**Lemma 1.2** Every dequeue operation *D()* takes effect atomically at one statement.

**Proof.** ...

## 2. Overflow Guarantees

## 3. Lock Freedom