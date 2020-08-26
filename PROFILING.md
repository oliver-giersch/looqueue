# Performance Profiling

## Using `perf record`

- `$QUEUE = [loo|faa|lcr|msc]`
- `$BENCH = [pairs|bursts|rand]`

```bash
$ perf record -F 1000 --call-graph dwarf -o $OUT_FILE \
    ./bench_throughput $QUEUE $BENCH 1M 10
```

## Using `perf stat`

```bash
$ perf stat -e \
cs,branches,branch-misses,cache-misses,cache-references,cycles,instructions,\
L1-dcache-load-misses,L1-dcache-loads,L1-dcache-stores,L1-icache-load-misses,\
LLC-load-misses,LLC-loads,LLC-store-misses,LLC-stores,branch-load-misses,\
branch-loads,dTLB-load-misses,dTLB-loads,dTLB-store-misses,dTLB-stores,\
iTLB-load-misses,iTLB-loads,node-load-misses,node-loads,node-store-misses,\
node-stores \
    ./bench_throughput $QUEUE $BENCH 1M 10
```

## Performance Analysis

- `rand` benchmark, 1M operations, 10 runs

### enqueue (75%)

#### `loo::queue`

| operation          | instruction | samples | samples (normalized) |
| ------------------ | ----------- | ------- | -------------------- |
| FAA (1) [tail+idx] | lock xadd   | 186.9k  | 62.3k                |
| FAA (OR) slot      | lock xadd   | 14.3k   | 4.8k                 |
| SUM                |             | 201.2k  | 67.1k                |

#### `faa::queue`:

| operation       | instruction  | samples | samples (normalized) |
| ----------------| ------------ | ------- | -------------------- |
| HP acquire      | xchg         | 14.7k   | 4.9k                 |
| FAA (1) enq_idx | lock xadd    | 168.7k  | 56.2k                |
| CXCHG slot      | lock cmpxchg | 22.7k   | 7.6k                 |
| HP release      | mov          | 1.3k    | 0.4k                 |
| SUM             |              | 207.4k  | 69.1k                |

### dequeue (25%)

#### `loo::queue`

| operation          | instruction | samples |
| ------------------ | ----------- | ------- |
| LOAD [tail+idx]    | mov         | 57.1k   |
| FAA (0) [head+idx] | lock xadd   | 4.2k    |
| FAA (1) [head+idx] | lock xadd   | 5.5k    |
| FAA (OR) slot      | lock xadd   | 7.4k    |
| SUM                |             | 74.2k   |

#### `faa::queue`

| operation       | instruction | samples |
| --------------- | ----------- | ------- |
| HP acquire      | xchg        | 1.5k    |
| LOAD deq_idx    | mov         | 11.6k   |
| LOAD enq_idx    | mov         | 0.8k    |
| FAA (1) deq_idx | lock xadd   | 8.8k    |
| XCHG slot       | xchg        | 16.7k   |
| HP release      | mov         | 0.5k    |
| SUM             |             | 39.6k   | 

### Reasoning

The dequeue performance `loo::queue` is significantly worse (by a factor of 2),
because it has to perform a read on the tail pointer. This read takes up more
than half of the time required by each dequeue operation in this scenario,
because the tail pointer variable is especially heavily contested in this
specific benchmark, whereas `faa::queue` does not have to touch the tail pointer
variable at all.
This property is highly valuable in this scenario since it is highly unlikely
that head and tail will ever point at the same node or that queue is empty, as
most operations are enqueues.

