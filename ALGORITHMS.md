# 1. Types & Constants

```
type TagPtr<T> = (T* ptr, u16 idx)

struct Queue<T>:
..TagPtr<T> head
..TagPtr<T> tail

struct Node<T>:
..[Atomic<u64>; N] slots
..Atomic<Node*>   next
..Atomic<Control> ctrl

struct Control:
..u64 flags      : 4
..u64 head_count : 15
..u64 head_total : 15
..u64 tail_count : 15
..u64 tail_total : 15

enum HeadRes { EMPTY, ADVANCED }
enum TailRes { INSERTED, ADVANCED }
enum Result { EMPTY, ADV, INSRT }

const RESUME   = 0b001
const WRITER   = 0b010
const READER   = 0b100
const CONSUMED = READER | WRITER

const SLOTS = 0b001
```

# 2. Enqueue

## 2.1 Fast Path

```
# method of Queue<T>
    fn enqueue(T* it) -> void:
 E1 loop:
 E2 ..[t,i] = tail.fetch_add(1)
 E3 ..if i < N:
 E4 ....slot = (u64) it | WRITER
 E5 ....prev = t->slots[i].fetch_add(slot)
 E6 ....if prev <= RESUME: return
 E7 ....if prev == (READER | RESUME):
 E8 ......t->try_reclaim(i+1)
 E9 ....continue
E10 ..else:
E11 ....switch (adv_tail([t,i+1], t, it)):
E12 ......case INSRT: return
E13 ......case ADV:   continue
```

## 2.2 Slow Path (Advance Tail)

```
fn adv_tail(
    TagPtr<T> tag_tail,
    Node<T>*  t,
    T*        it
) -> Result:
 T1 ..total = 0
 T2 ..node = alloc_node(it)
 T3 ..next = NULL
 T4 ..if t->next.cas(&next,node):
 T5 ....next = node
 T6 ....res = INSRT
 T7 ..else:
 T8 ....free_node(node)
 T9 ....res = ADV
T10 ..while !cas_tail(&tag_tail, [next, 1]):
T11 ....if tag_tail.ptr != t: goto out
T12 ..total = tag_tail.idx-N
T13 out:
T14 ..t->incr_enq_count(total)
T15 ..return res
```

# 3. Dequeue

## 3.1 Fast Path

```
    fn dequeue() -> T*:
 D1 loop:
 D2 ..tag_head = head.load()
 D3 ..[h,di] = tag_head
 D4 ..[t,ei] = tail.load()
 D5 ..if h == t && (di >= N || ei <= di):
 D6 ....return NULL
 D7 ..[h,i] = head.fetch_add(1)
 D8 ..if i < N:
 D9 ....prev = h->slots[i].fetch_add(READER)
D10 ....if prev & WRITER != 0:
D11 ......if prev & RESUME != 0:
D12 ........h->try_reclaim(i+1)
D13 ......return (T*) (prev & PTR_MASK)
D14 ....continue
D15 ..else:
D16 ....switch (adv_head([h,i+1], h, t)):
D17 ......case EMPTY: return NULL
D18 ......case ADV:   continue

```

## 3.2 Slow Path (Advance Head)

```
    fn adv_head(
      TagPtr<T> curr,
      Node<T>* h,
      Node<T>* t
    ) -> HeadRes
 H1 ..[t,_] = tail.load()
 H2 ..if h == t:
 H3 ....res = EMPTY
 H4 ....goto out
 H5 ..next = h->next.load()
 H6 ..while !cas_head(&tag_head, [next,0]):
 H7 ....if tag_head.ptr != h: goto out
 H8 ..total = tag_head.idx-N
 H9 ..res = ADVANCED
H10 out:
H11 ..h->incr_deq_count(total)
H12 ..return res
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