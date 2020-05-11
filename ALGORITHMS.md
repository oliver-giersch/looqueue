# 1. Types & Constants

```
struct Node<T>:
..[Atomic<T*>; N] slots
..Atomic<Node*>   next
..ControlBlock    ctrl

struct ControlBlock:
..Atomic<(u16, u16)> head_mask
..Atomic<(u16, u16)> tail_mask
..Atomic<u8>         reclaim

type TagPtr<T> = (T* ptr, u16 idx)

enum AdvHead { QUEUE_EMPTY, ADVANCED }
enum AdvTail { ADV_AND_INSRT, ADV_ONLY }

const RESUME   = 0b001
const WRITER   = 0b010
const READER   = 0b100
const CONSUMED = READER | WRITER

const SLOT = 0b001
const DEQ  = 0b010
const ENQ  = 0b100
```

# 2. Enqueue

## 2.1 Fast Path

```
# method of Queue<T>
 E1 fn enqueue(T* el) -> void:
 E2 loop:
 E3 ..[t,i] = tail.fetch_add(1)
 E4 ..if i < N:
 E5 ....prev = t->slots[i].fetch_add(
 E6 ........(u64) el) | WRITER)
 E7 ....if prev <= RESUME: return
 E8 ....if prev == (READER | RESUME):
 E9 ......t->try_reclaim(i+1)
E10 ....continue
E11 ..else:
E12 ....switch (adv_tail(el, t)):
E13 ......case ADV_AND_INSRT: return
E14 ......case ADV_ONLY: continue
```

## 2.2 Slow Path (Advance Tail)

```
# method of Queue<T>
 T1 fn adv_tail(T* el, Node<T>* t) -> AdvTail:
 T2 loop:
 T3 ..curr = tail.load()
 T4 ..if t != curr.ptr:
 T5 ....t->incr_enq_count()
 T6 ....return ADV_ONLY
 T7 ..next = t->next.load()
 T8 ..if next == NULL:
 T9 ....node = alloc_node(el)
T10 ....if t->next.cas(NULL,node):
T11 ......while !tail.cas(curr, [node,1]):
T12 ........if curr.ptr != t:
T13 ..........t->incr_enq_count()
T14 ..........return ADV_AND_INSRT
T15 ......t->incr_enq_count(curr.idx-N)
T16 ......return ADV_AND_INSRT
T17 ....else:
T18 ......dealloc_node(node)
T19 ......continue
T20 ..else:
T21 ....while !tail.cas(curr, [next,1]):
T22 ......if curr.ptr != t:
T23 ........t->incr_enq_count()
T24 ........return ADV_ONLY
T25 ....t->incr_enq_count(curr.idx-N)
T26 ....return ADV_ONLY
```

# 3. Dequeue

## 3.1 Fast Path

```
# method of Queue<T>
 D1 fn dequeue() -> T*:
 D2 loop:
 D3 ..curr = head.load()
 D4 ..[h,i] = curr
 D5 ..[t,ti] = tail.load()
 D6 ..if (i >= N || i >= ti) && h == t:
 D7 ....return NULL
 D8 ..[h,i] = head.fetch_add(1)
 D9 ..if i < N:
D10 ....prev = h->slots[i].fetch_add(READER)
D11 ....if i == N-1:
D12 ......h->try_reclaim(0)
D13 ....if prev & WRITER != 0:
D14 ......if prev & RESUME != 0:
D15 ........h->try_reclaim(i+1)
D16 ......return (T*) (prev & PTR_MASK)
D17 ....continue
D18 ..else:
D19 ....switch (adv_head(curr, h, t)):
D20 ......case ADVANCED: continue
D21 ......case QUEUE_EMPTY: return NULL
```

## 3.2 Slow Path (Advance Head)

```
# method of Queue<T>
 H1 fn adv_head(TagPtr<T> curr, Node<T>* h, Node<T>* t) -> AdvHead
 H2 ..next = h->next.load()
 H3 ..if next == NULL || t == h:
 H4 ....h->incr_deq_count()
 H5 ....return QUEUE_EMPTY
 H6 ..curr.idx += 1
 H7 ..while !head.cas(curr, [next,0]):
 H8 ....if curr.ptr != h:
 H9 ......h->incr_deq_count()
H10 ......return ADVANCED
H11 ..h->incr_deq_count(curr.idx-N)
H12 ..return ADVANCED
```

## 4. Memory Management

Both functions are methods of `Node<T>`.

### Iterate all slots

```
R1 fn try_reclaim(u16 start) -> void:
R2 for i in (start..N):
R3 ..s = slots[i]
R4 ..if (s.load() & CONSUMED) != CONSUMED:
R5 ....if (s.faa(RESUME) & CONSUMED) != CONSUMED:
R6 ......return
R7 if ctrl.reclaim.fetch_add(SLOT) == (ENQ | DEQ):
R8 ..dealloc_node(this)
```

### Increase count of finished slow path operations (similar for dequeue)

```
 C1 fn incr_enq_count(u16 final_cnt = 0) -> void:
 C2 ..if final_count == 0:
 C3 ....m = ctrl.tail_mask.fetch_add(1)
 C4 ....final_cnt = m >> 16
 C5 ....curr_cnt = 1 + (m & 0xFFFF)
 C6 ..else:
 C7 ....v = 1 + (final_cnt << 16)
 C8 ....m = ctrl.tail_mask.fetch_add(v)
 C9 ....curr_cnt = 1 + (m & 0xFFFF)
C10 ..if curr_cnt == final_cnt:
C11 ....prev = ctrl.reclaim.fetch_add(ENQ)
C11 ....if prev == (DEQ | SLOTS):
C12 ......dealloc_node(this)
```