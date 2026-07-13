# niniBUS Future Topics

This document is the parking lot for ideas that are intentionally outside the
active milestone.

The purpose is to prevent scope creep while keeping good ideas visible. A topic
listed here is not approved for immediate implementation. Move it into
`doc/Milestone.md` only when it becomes part of the active roadmap.

## Rules

- Capture ideas here instead of expanding the current milestone.
- Do not implement a future topic until its milestone is active.
- Prefer measurements before optimization work.
- Promote a topic only when the problem is real, documented, and worth the
  added complexity.

## Promotion Checklist

Before moving a topic from this document into a milestone:

- [ ] The problem is clearly stated.
- [ ] The expected user or developer benefit is clear.
- [ ] The added complexity is acceptable.
- [ ] The required tests are understood.
- [ ] The documentation impact is understood.
- [ ] The topic fits the active milestone.

## V1 Candidates - Smarter Single-Threaded Bus

These ideas improve the current single-threaded design without adding
multi-threading or IPC.

### FIFO Evolution

Current state: V1 now uses a project-owned fixed-size FIFO:

```cpp
niniFIFO_t<std::string, DEFAULT_LANE_CAPACITY>
```

This section now tracks possible future FIFO improvements rather than the
initial replacement of `std::deque`.

Topics:

- Optional dynamic capacity.
- Queue statistics.
- Configurable queue size.
- Clear overflow behavior.

Questions:

- Should capacity be per lane or global?
- Should queue capacity be compile-time or runtime configuration?
- Should the current template-capacity FIFO stay, or should runtime capacity be
  introduced?
- Should FIFO errors use return statuses, exceptions, or both?

Promotion trigger:

- Move to a future milestone when runtime capacity, richer statistics, or a
  different overflow policy becomes a real requirement.

### Back Pressure

Idea: define what happens when a lane cannot accept more messages.

Possible policies:

- Reject the new message.
- Drop the oldest message.
- Drop the newest message.
- Block the publisher, if blocking APIs exist later.
- Use a configurable per-lane policy.

Possible API:

```cpp
enum class PublishStatus {
    Ok,
    LaneFull,
    LaneNotFound
};

struct PublishResult {
    uint32_t Credit;
    PublishStatus Status;
};
```

Questions:

- Should the current fixed lane capacity remain global or become configurable?
- Should full lanes reject new messages, drop old messages, or use a
  configurable policy?
- Should `LaneFull` remain simple, or should it carry richer back-pressure
  information?
- Should dropped-message counts be tracked?

Promotion trigger:

- Move to V1 when the current `LaneFull` behavior is not enough or when slow
  consumers become a real use case.

### Lane Statistics

Idea: expose lightweight information about lane state.

Possible metrics:

- Current queue depth.
- High-water mark.
- Total messages published.
- Total messages received.
- Total messages dropped.
- Lane creation count.

Questions:

- Should statistics be always enabled?
- Should statistics be optional for embedded builds?
- Should stats be per lane, global, or both?

Promotion trigger:

- Move to V1 when queue capacity or observability becomes part of the public
  API.

## V2 Candidates - Embedded Optimization

These ideas are about memory layout, predictability, and performance after the
basic behavior is correct.

### Memory Footprint Measurement

Idea: measure before changing data structures.

Measurements:

- Size of `lane_t`.
- Size of `niniFIFO_t<std::string, DEFAULT_LANE_CAPACITY>`.
- Size and overhead of `std::unordered_map<laneID_t, lane_t>`.
- Per-message allocation behavior from `std::string`.
- Per-lane allocation behavior.
- Allocation count during publish/receive.

Questions:

- How much memory does one lane consume?
- How much overhead does `unordered_map` add?
- Is dynamic allocation acceptable for the target environment?
- What are the real memory limits?

Promotion trigger:

- Move to V2 when target memory constraints are known.

### Storage Strategy Alternatives

Ideas to compare:

- `std::unordered_map<laneID_t, lane_t>`.
- Sorted vector of lanes.
- Fixed-size lane table.
- Open-addressed static hash table.
- Pool-allocated lanes.

Questions:

- Is lane lookup speed more important than memory predictability?
- Is the maximum lane count known?
- Should lane IDs be dense or sparse?

Promotion trigger:

- Move to V2 after memory and timing measurements exist.

### Embedded Configuration

Possible compile-time configuration:

- Maximum lane count.
- Maximum queue depth.
- Maximum message size.
- Static memory allocation mode.
- Optional statistics.
- Optional logging.

Questions:

- Should configuration be compile-time, runtime, or both?
- Should defaults optimize for readability or embedded constraints?
- How should invalid configuration be reported?

Promotion trigger:

- Move to V2 when hard resource limits are defined.

## V3 Candidates - Thread Safety And Concurrency

These ideas are deferred until the single-threaded bus is stable and tested.

### Thread-Safe Bus

Topics:

- Producer synchronization.
- Consumer synchronization.
- Mutex ownership.
- Lane-level locking.
- Global bus locking.
- Blocking receive.
- Timeout receive.
- Read/write contention.
- Performance measurement.

Questions:

- Should all methods be thread-safe?
- Should users opt into thread safety?
- Should receive block or stay non-blocking?
- Is ordering guaranteed across threads?

Promotion trigger:

- Move to V3 when multi-threaded usage is an explicit requirement.

### Lock-Free Data Structures

Study topics:

- SPSC queue.
- MPSC queue.
- MPMC queue.
- Ring buffers.
- CAS.
- ABA problem.
- Acquire/release semantics.
- False sharing.
- Cache-line alignment.

Questions:

- Is lock-free behavior needed, or would mutexes be enough?
- Which producer/consumer shape matters most?
- How will correctness be tested?

Promotion trigger:

- Move out of study only after mutex-based concurrency has been implemented and
  measured.

## V4 Candidates - IPC

These ideas are for communication across processes.

Possible transports:

- Unix domain sockets.
- Shared memory.
- Zero-copy shared buffers.

Topics:

- Transport abstraction.
- Serialization format.
- Connection management.
- Process lifetime.
- Error handling across process boundaries.
- Transport-specific examples.

Questions:

- Should IPC live inside core `niniBUS` or in a separate transport layer?
- Is zero-copy required?
- What message format crosses process boundaries?
- How are lane IDs shared between processes?

Promotion trigger:

- Move to V4 only after the in-process API is stable.

## V5 Candidates - Production Features

These ideas should wait until core behavior is stable, tested, and documented.

Possible features:

- Structured logging.
- Quiet logging controls.
- Metrics hooks.
- Profiling hooks.
- Configuration object or builder.
- Install target.
- Packaging.
- CI workflow.
- Release checklist.
- API versioning policy.

Questions:

- Which features help real users?
- Which features can remain optional?
- Which features increase the maintenance burden?

Promotion trigger:

- Move to V5 after core, tests, and examples are stable.

## Research Topics

These are learning/research notes, not implementation commitments.

### Memory Internals

- `memcpy` implementation.
- `memmove` implementation.
- glibc internals.
- musl internals.
- Alignment.
- SIMD optimization.
- Cache optimization.
- Copy avoidance.

### Scheduling And Event Dispatch

- Event loops.
- Dispatch scheduling.
- Fair scheduling.
- Priority scheduling.
- Work stealing.
- Runtime scheduling.
- SystemC kernel scheduler.
- FreeRTOS scheduler.
- Linux scheduler.
- `epoll` internals.
- `io_uring`.

### Zero Copy

- Shared payloads.
- Reference counting.
- Scatter/gather IO.
- DMA-friendly buffers.
- Buffer pools.

## Future Documentation

Possible future documents:

- Architecture Guide.
- API Guide.
- Memory Model.
- Threading Model.
- Performance Guide.
- Benchmark Report.
- Release Guide.

## Blog Ideas

Possible future articles:

- Why I built `niniBUS`.
- Designing the lane abstraction.
- Building a bounded FIFO.
- Back pressure in embedded systems.
- Memory optimization journey.
- Building a thread-safe message bus.
- Designing for embedded constraints.

## Never Forget

Interesting ideas are captured, not implemented immediately.

The current milestone always has priority.
