# niniBUS Milestone Roadmap

This roadmap keeps the project focused. Each version should answer one
engineering question and should not grow just because the next topic looks
interesting.

Current active milestone: V1.2 - V1 Cleanup and Completion.

## V0 - Basic Message Bus

Status: complete.

### Objective

Build the simplest working in-process message bus.

### Features

- Lane creation.
- Publish.
- Receive.
- Lazy lane creation.
- Basic status handling.
- Initial example application.
- Initial documentation.
- Tagged release: `v0.0.0`.

### Engineering Question

Can I build a working in-process message bus with a simple API?

## V1 - Bounded and Predictable Message Bus

### V1.0 - Lane Separation and Basic Back Pressure

Status: complete.

### Objective

Separate lane behavior from bus routing and make each lane bounded.

### Features

- Move lane implementation into `Lane.h` and `Lane.cpp`.
- Keep `niniBUS` responsible only for lane lookup, lazy lane creation, routing,
  and delegation.
- Add bounded lane capacity.
- Add `PublishResult`.
- Add `PublishStatus::LaneFull`.
- Add publisher credit.
- Keep credit derived from:

```text
capacity - current size
```

- Remove unused publish statuses.
- Keep lane size, capacity, and credit helpers private.
- Add tests for lane capacity, publisher credit, full-lane rejection, publish
  retry after receive, and lane independence.

### Definition Of Done

- Bus routing remains simple.
- Lane owns queue behavior.
- Full lanes reject new messages.
- Credit reflects remaining queue space.
- Full-lane behavior is tested.
- Documentation is updated.

### V1.1 - Custom Circular FIFO

Status: complete.

### Objective

Replace the lane's direct STL queue usage with a project-owned circular FIFO.

### Features

- Add `niniFIFO<T>`.
- Use `std::vector<T>` as internal storage.
- Maintain `head_`, `tail_`, `currSize_`, and `capacity_`.
- Implement STL-like APIs:
  - `push_back()`
  - `pop_front()`
  - `front()`
  - `isEmpty()`
  - `isFull()`
  - `size()`
  - `getCapacity()`
- Add `FIFOStatus`:
  - `SUCCESS`
  - `FULL`
  - `EMPTY`
- Integrate `niniFIFO<std::string>` into `lane_t`.
- Keep template implementation in the header.
- Preserve STL-like separation between reading with `front()` and removing with
  `pop_front()`.

### Tests

- Empty FIFO.
- FIFO ordering.
- Full FIFO.
- Overflow rejection.
- Pop from empty FIFO.
- FIFO reuse after becoming empty.
- Wraparound.
- Repeated wraparound.
- FIFO order after full-lane recovery.
- Size correctness during push and pop.

### Definition Of Done

- Circular FIFO behavior is correct.
- Wraparound is directly tested.
- Full and empty behavior is tested.
- Lane uses `niniFIFO`.
- No `const_cast`.
- No unnecessary `.cpp` file for the template.
- Tests pass after a clean build.

### V1.2 - V1 Cleanup and Completion

Status: completed.

### Objective

Close the fixed-capacity portion of V1 before adding explicit lane creation.

### Tasks

The following list records the fixed-capacity V1.2 scope that was completed
before V1.3 introduced runtime capacity:

- Remove duplicate full checking from `lane_t::push()`.
- Let `niniFIFO::push_back()` own full detection.
- Translate `FIFOStatus::FULL` into `PublishStatus::LaneFull`.
- Keep the current STL-like `front()` and `pop_front()` interface.
- Keep the current fixed default capacity behavior.
- Do not add runtime capacity configuration yet.
- Remove unused includes.
- Remove stale comments.
- Clean naming and documentation.
- Add a dedicated wraparound test.
- Update the test report.
- Mark the current V1 implementation complete.
- Run:

```bash
make clean
make

make -C example clean
make -C example test
```

### Explicitly Deferred To V1.3

- User-configurable runtime capacity.
- Capacity constructor.
- Capacity-one testing.
- Zero-capacity validation.
- Revisiting `front()` empty behavior.
- Public naming cleanup for `size()` and `capacity()`.

### Definition Of Done

- Existing V1 behavior is complete.
- No known FIFO correctness issue remains.
- Current API is stable.
- Tests and documentation match the implementation.
- V1.2 is committed and tagged.

### V1.3 - Runtime Capacity and FIFO API Refinement

Status: in progress.

### Objective

Complete the deferred FIFO API work before starting concurrency.

This is the final part of V1.

### Features

#### Runtime Capacity

Allow FIFO capacity to be selected during construction.

Possible API:

```cpp
explicit niniFIFO(uint32_t capacity = DEFAULT_LANE_CAPACITY);
```

Current bus API:

```cpp
CreateLaneStatus CreateLane(laneID_t laneID, uint32_t capacity);
```

`CreateLane()` now creates a lane with the requested capacity. Publishing or
receiving on an unknown lane still creates it with `DEFAULT_LANE_CAPACITY`.
Calling `CreateLane()` for an existing ID returns `LaneExist` and preserves the
existing lane.

#### Capacity Validation

Define behavior for invalid capacity.

Questions:

- Should capacity zero throw?
- Should it assert?
- Should construction fail through another mechanism?

#### Capacity Edge Cases

Add tests for:

- Capacity = 1.
- Small capacities.
- Large configured capacities.
- Zero capacity.
- Repeated wraparound with custom capacity.

#### Public API Naming

Refine the inspection API.

Current APIs:

```cpp
size()
getCapacity()
```

Potential final APIs:

```cpp
size()
capacity()
```

The final naming should be consistent with STL conventions where appropriate.

#### `front()` Behavior Review

Keep the STL-like interface:

```cpp
front()
pop_front()
```

Review only the empty-access policy:

- Runtime exception.
- Assertion.
- Documented precondition.
- Another embedded-friendly policy.

Do not replace it with a combined read-and-pop API unless a concrete
requirement appears.

#### Lane Capacity Configuration

Decide whether `Lane` should:

- Always use the default FIFO capacity.
- Accept capacity during construction.
- Obtain capacity from bus configuration.

### Tests

- Runtime capacity constructor. (covered)
- Capacity = 1.
- Zero-capacity policy.
- Custom-capacity full condition. (covered through `CreateLane()`)
- Custom-capacity wraparound.
- Size and capacity APIs.
- Lane behavior with configured capacity. (covered)

### Definition Of Done

- Runtime capacity is supported.
- Invalid capacity behavior is defined.
- Capacity = 1 is tested.
- FIFO API names are finalized.
- Empty `front()` behavior is documented.
- Lane capacity configuration is clear.
- V1 API is considered stable.

## V1 Final State

At the end of V1, `niniBUS` has:

- Bounded lanes.
- Custom circular FIFO.
- Back pressure.
- Publisher credit.
- Full and empty status handling.
- Runtime-configurable FIFO capacity.
- Stable FIFO inspection APIs.
- Documented empty-access behavior.
- Direct FIFO tests.
- Bus integration tests.
- Wraparound tests.
- Edge-case tests.
- Updated design documentation.
- Tagged V1 release.

### Engineering Question

Can I make the single-threaded bus bounded, predictable, reusable, and fully
tested?

## V2 - Competing Messages / Broadcast

### Objective

Support multiple subscribers per lane with message competition or broadcast modes.

### Features

- Multi-subscriber support per lane.
- Competing message model (one subscriber gets the message).
- Broadcast message model (all subscribers get the message).
- Lane subscription management.
- Subscriber lifecycle.
- Message distribution strategy.
- Backpressure with multiple subscribers.

### V2.1 - Design Exploration

Status: not started.

### Objective

Explore design patterns for multi-subscriber message delivery.

### Questions

- Competing vs broadcast - which modes?
- Per-lane or per-subscriber buffers?
- How to handle backpressure from slow subscribers?
- Subscriber identification.
- Subscription/unsubscription mechanism.
- Message ownership and lifetime.
- Resource management with multiple subscribers.

### V2.2 - TBD

Status: not started.

### Objective

Complete design decisions from V2.1 exploration.

### Tasks

- [ ] Decide on multi-subscriber modes
- [ ] Design subscriber interface
- [ ] Design buffer strategy
- [ ] Design backpressure handling
- [ ] Define API

### Definition Of Done

- Multi-subscriber design is complete and documented.

### Engineering Question

Can `niniBUS` support multiple subscribers with predictable message delivery?

## V3 - Thread-Safe Message Bus

### Objective

Support safe concurrent publishing and receiving.

### Features

- Mutex-based synchronization.
- Thread-safe publish.
- Thread-safe receive.
- Multiple producers.
- Multiple consumers.
- Per-lane synchronization.
- Bus-map synchronization.
- Lock ownership documentation.
- Lock ordering rules.
- Contention measurements.
- Thread-safety tests.

### Questions

- Global bus lock or per-lane lock?
- When is the map lock required?
- Can lane operations proceed independently?
- How are lazy creation races handled?
- What is the ownership model?
- What happens when multiple consumers read one lane?

### Tests

- Multiple producers on one lane.
- Multiple consumers on one lane.
- Producers and consumers together.
- Multiple independent lanes.
- Lane creation races.
- Full queue under contention.
- Empty queue under contention.
- Long-running stress test.

### Definition Of Done

- No data races under supported usage.
- Synchronization contract is documented.
- Mutex implementation is measured.
- Contention bottlenecks are understood.
- Correctness comes before optimization.

### Engineering Question

Can multiple threads use `niniBUS` safely?

## V4 - Lock-Free FIFO Research

### Objective

Explore whether a lock-free queue meaningfully improves the system.

### Features

- Lock-free bounded FIFO prototype.
- Atomic indexes or sequence counters.
- Memory-ordering study.
- Single-producer/single-consumer prototype.
- Possible multi-producer/multi-consumer exploration.
- Cache-line and false-sharing investigation.
- ABA analysis where relevant.
- Comparison with mutex implementation.
- Correctness stress tests.
- Throughput and latency benchmarks.

### Important Rule

The lock-free implementation is experimental until it is proven correct and
performs better for a measured workload.

### Questions

- Is SPSC enough for a useful mode?
- Is MPMC complexity justified?
- Which memory ordering is required?
- Does lock-free improve latency?
- Does it improve throughput?
- Does it make the code harder to maintain?
- Is the mutex baseline already sufficient?

### Definition Of Done

- Lock-free prototype is documented.
- Correctness assumptions are explicit.
- Mutex and lock-free versions are compared.
- Performance claims are backed by measurements.
- A decision is made to adopt, retain as experimental, or reject it.

### Engineering Question

Can locks be removed without sacrificing correctness or maintainability?

## V5 - Storage and Memory Management

### Objective

Separate FIFO behavior from memory ownership and make memory use predictable.

### V5.0 - `niniStorage`

### Features

- Introduce `niniStorage`.
- Move storage responsibility out of `niniFIFO`.
- Initially use `std::vector` internally.
- Expose only operations required by FIFO.
- Keep FIFO algorithm independent of storage implementation.
- Support runtime capacity through storage.

### Goal

FIFO should know how to manage queue state, not where the memory comes from.

### V5.1 - `niniAllocator`

### Features

- Preallocated memory pool.
- Block allocation.
- Free and reclaim.
- Contiguous storage allocation.
- Fragmentation measurement.
- Coalescing strategy if required.
- Allocation failure behavior.
- Memory ownership rules.
- Leak detection tests.

### Questions

- Fixed-size blocks or variable-size blocks?
- How are blocks reclaimed?
- Is contiguous allocation required?
- How much fragmentation occurs?
- Should every lane own its storage?
- Should the bus own a shared pool?

### Measurements

- Allocation latency.
- Deallocation latency.
- Memory overhead.
- Fragmentation.
- Lane memory footprint.
- FIFO memory footprint.
- Comparison with `std::vector`.

### Definition Of Done

- FIFO is independent of concrete storage.
- Storage ownership is documented.
- Allocation and reclamation are tested.
- Memory usage is measured.
- Failure behavior is defined.

### Engineering Question

Can `niniBUS` control and predict its own memory usage?

## V5 - Inter-Process Communication

### Objective

Allow message exchange between processes.

### Candidate Transports

- Unix domain sockets.
- TCP sockets.
- Shared memory.

### Features

- Transport abstraction.
- Message framing.
- Serialization.
- Deserialization.
- Connection lifecycle.
- Sender and receiver process examples.
- Failure detection.
- Reconnection policy.
- Partial-read handling.
- Partial-write handling.
- Back pressure across transport boundaries.
- IPC benchmarks.

### Questions

- Which transport is the initial baseline?
- How are messages framed?
- How are lane IDs serialized?
- How are disconnected peers handled?
- How are full remote queues represented?
- Can shared memory reuse `niniFIFO`?
- Where does copying occur?

### Measurements

- End-to-end latency.
- Messages per second.
- CPU usage.
- Copy count.
- Memory usage.
- Socket versus shared-memory performance.

### Definition Of Done

- Two processes can exchange messages reliably.
- Framing is correct.
- Failure paths are tested.
- Transport decisions are documented.
- Performance is measured.

### Engineering Question

Can `niniBUS` cross a process boundary while preserving its semantics?

## Future Topics

These are intentionally outside the main roadmap until a real need appears.

- Priority lanes.
- Broadcast.
- Multiple subscribers.
- Consumer queue-depth feedback.
- Message filtering.
- Wildcard subscription.
- QoS.
- Persistence.
- Zero-copy messaging.
- Shared-memory optimizations.
- Monitoring.
- Tracing.
- Metrics.
- Distributed operation.
- Network discovery.
- Security.
- Authentication.
- Versioned message schemas.

## Release Rule

A version is complete only when it is:

- Implemented.
- Tested.
- Documented.
- Measured where relevant.
- Cleanly committed.
- Tagged.
- Explainable in a video.

Do not start the next version because the next topic looks exciting.

Start it only after the current version is complete.

## Project Philosophy

```text
Make it boring.
Make it correct.
Make it complete.
Measure it.
Document it.
Then move forward.
```
