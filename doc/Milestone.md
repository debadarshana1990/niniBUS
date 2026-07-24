# niniBUS Milestone Roadmap

> **A deterministic, embedded-friendly message bus for C++ systems.**

`niniBUS` is an in-process publish/subscribe library for systems programming,
embedded software, simulators, and platform software where predictable
behavior and explicit design matter more than feature count.

The project evolves incrementally. Each release focuses on one fundamental
engineering problem before the next layer of complexity is introduced.

---

## Philosophy

`niniBUS` follows these principles:

- Build from first principles.
- Keep APIs small and explicit.
- Favor deterministic and observable behavior.
- Separate message transport from message execution.
- Stabilize one design problem before adding another.
- Record limitations instead of hiding them behind optimistic APIs.

The bus moves messages. It does not own worker threads, schedule application
work, or execute subscriber callbacks.

---

## Current Status

| Version | Status | Theme |
|---|---|---|
| v1.0.0 | Released | Single publisher, single subscriber |
| v2.0.0 | In Progress | Cursor-based broadcast messaging |
| v3.0.0 | Planned | Thread-safe messaging |
| v4.0.0 | Planned | Deterministic memory |
| v5.0.0 | Planned | Inter-process communication |

---

## v1.0.0 — Foundation

### Theme

> Establish a small, bounded, in-process message bus.

The first release established the core message-bus model.

### Delivered

- Lane abstraction.
- Publish and receive operations.
- Bounded message queues.
- Explicit lane capacity.
- Default capacity for publish-created lanes.
- Credit-based capacity reporting.
- Status-oriented result types.
- Single-threaded behavior.
- A static library and example-based tests.

### Engineering Outcome

v1 proved the basic ownership model:

```text
niniBUS
└── Lane Map
    └── One bounded queue per lane
```

The original destructive FIFO behavior was sufficient for one consumer, but it
could not allow several subscribers to consume the same message independently.
That limitation motivated v2.

---

## v2.0.0 — Broadcast Messaging

### Theme

> One message. Multiple independent readers.

v2 introduces broadcast delivery without creating a separate message queue for
every subscriber.

### Architecture

Each lane owns one shared `nbus::cfifo<std::string>`. The lane also maintains an
independent cursor for every registered subscriber:

```text
Lane
├── Shared Message Storage
└── Subscriber Cursor Map
    ├── Subscriber A -> next read sequence
    ├── Subscriber B -> next read sequence
    └── Subscriber C -> next read sequence
```

A message is stored once. Every subscriber reads it through its own cursor.
One subscriber reading a message does not consume it for another subscriber.

### Implemented Work

- Define subscriber IDs.
- Add `subscribe()` and `unsubscribe()`.
- Maintain one shared cursor FIFO per lane.
- Maintain subscriber-to-cursor state.
- Initialize new cursors at the current tail.
- Accept subscriber ID in `receive()`.
- Advance only the requesting cursor during a normal successful read.
- Allow multiple subscribers to receive the same message.
- Return logical sequence IDs from publish and successful receive.
- Report messages still pending for the requesting subscriber.
- Detect and report messages skipped by reclamation.
- Add deterministic bounded reclamation.
- Add single-threaded bus and `cfifo` functional tests.

### Subscription Semantics

A new subscriber starts at the current tail:

```text
messages published before registration -> not delivered
messages published after registration  -> eligible for delivery
```

Duplicate subscription does not reset or move the existing cursor.

Unsubscribe removes the cursor from future delivery and reclaim decisions.
Subscribing again creates a new cursor at the then-current tail; it does not
restore the old cursor position.

### Lane-Creation Semantics

- `createLane()` explicitly creates a lane with caller-selected capacity.
- Capacity zero is rejected.
- Creating an existing lane does not replace or resize it.
- `publish()` deliberately lazily creates a missing lane using
  `DEFAULT_LANE_CAPACITY`; callers use `createLane()` first when they require a
  custom capacity.
- `subscribe()` requires the lane to exist.
- `receive()` never creates a missing lane or subscriber.
- `unsubscribe()` never creates a missing lane.

The receive-path decision is deliberate. Receiving may advance an existing
subscriber cursor, but it must not silently mutate bus topology. A wrong lane
or subscriber ID therefore remains an observable `NO_CURSOR` result.

### Write-Prioritization Policy

v2 currently prioritizes writers:

> A write will be accepted, but a slow reader may miss retained messages.

When the shared queue is full, `cfifo` deterministically reclaims space:

1. If no cursors are registered, retained history can be discarded.
2. If every cursor is caught up, retained storage can be reset logically.
3. Otherwise, find the oldest cursor position.
4. Advance every cursor tied at that oldest position to the current tail.
5. Accumulate the number skipped for each moved cursor.
6. Recompute the retained head and size.
7. Write the new message.

Moving every cursor tied at the oldest position is necessary. Moving only one
would leave another cursor at the same minimum, so no space would be released.

### Reader Consequences

This policy keeps memory bounded and prevents slow subscribers from blocking
publishers, but it is intentionally lossy:

- Slow subscribers can miss messages.
- Loss is reported through `skippedMessages`.
- Skip counts accumulate across reclaim operations.
- The accumulated count is returned with the subscriber's next successful
  receive and then reset.
- Applications requiring lossless delivery need a future backpressure policy.

### Core Invariants

```text
retained size <= configured capacity
credit == capacity - retained size
one physical message copy per lane
one independent cursor per registered subscriber
```

Logical sequence numbers are separate from physical buffer indexes:

```text
physical index = logical sequence % capacity
```

This preserves message identity across circular-buffer wraparound.

### Current Test Coverage

The v2 single-threaded tests cover:

- lane creation and invalid capacity;
- publish-side lazy lane creation;
- missing lane and missing subscriber receives;
- registration and duplicate registration;
- future-only subscriber registration;
- multiple independent subscribers;
- sequence IDs and pending counts;
- capacity-one behavior;
- physical wraparound;
- reclaim with no subscribers;
- reclaim after all subscribers catch up;
- tied and uneven slow subscribers;
- skipped-message accumulation and one-time reporting;
- unsubscribe, repeated unsubscribe, and missing targets;
- receive after unsubscribe;
- resubscription at the current tail;
- bounded size and credit after repeated reclamation.

### Remaining v2 Work

Before v2.0.0 is released:

- Complete API naming and result-type consistency review.
- Resolve the intended return contract of `cfifo::create_cursor()`.
- Define behavior for logical sequence-number rollover.
- Expand randomized and model-based reclaim testing.
- Keep all public, design, learning, and test documentation synchronized.
- Perform a clean build and run both example test suites.

### v2 Definition of Done

- Broadcast behavior is documented as a stable contract.
- Multiple subscribers independently receive shared messages.
- Subscriber lifecycle behavior is fully tested.
- Reclaim behavior is deterministic and bounded.
- Skipped reads are observable.
- Receive never lazily creates topology.
- Publish lazily creates a missing lane with `DEFAULT_LANE_CAPACITY`.
- Public API types and their documented meanings agree.
- All single-threaded tests pass from a clean build.

---

## v3.0.0 — Concurrency

### Theme

> Safe communication between threads.

Concurrency will be added only after the single-threaded broadcast model and
message-lifetime policy are stable.

### Phase 1

- One publisher thread.
- Multiple subscriber threads.
- Safe subscriber cursor progress.
- Safe subscription and unsubscribe lifecycle.

### Phase 2

- Multiple publisher threads.
- Multiple subscriber threads.
- Safe lane creation and lookup.
- Defined ordering and progress guarantees.

### Research Areas

- Lock ownership and granularity.
- Mutex-based reference implementation.
- Atomic cursor or sequence state.
- Memory ordering.
- Subscriber removal during receive or reclaim.
- Starvation and fairness.
- Race-detector and stress testing.

Correctness comes first. Mutexes, atomics, lock-free structures, or a hybrid
will be selected from measured architectural needs rather than chosen in
advance.

### v3 Definition of Done

- Supported concurrency patterns are stated precisely.
- Unsupported patterns fail review rather than remaining ambiguous.
- Data-race-free behavior is verified with appropriate tooling.
- Ordering, visibility, and progress guarantees are documented.
- Single-threaded behavior remains compatible unless a breaking change is
  explicitly announced.

---

## v4.0.0 — Deterministic Memory

### Theme

> Predictable allocation for embedded and systems software.

Possible work:

- Custom allocator interface.
- Fixed memory pools.
- Caller-provided storage.
- Bounded cursor registry.
- Bounded message representation.
- Zero-copy or reduced-copy publish.
- No allocation after initialization.
- Exact per-lane and per-subscriber memory measurements.
- Defined allocation-failure behavior.

### v4 Definition of Done

- Allocation points are known and documented.
- Embedded configurations can bound memory consumption.
- Failure behavior is explicit.
- Performance and footprint claims are supported by measurements.

---

## v5.0.0 — Inter-Process Communication

### Theme

> Extend the programming model beyond a single process.

The goal is to preserve the recognizable publish/subscribe model while allowing
communication across process boundaries.

### Possible Work

- Transport abstraction.
- Shared-memory or socket-based transport.
- Serialization and schema versioning.
- Process discovery and lifecycle.
- Persistent sequence and cursor state.
- Crash recovery.
- Delivery acknowledgement and retry.
- Security and access control.

The in-memory `cfifo` cannot simply be placed in shared memory because standard
containers and strings contain process-local state. IPC requires an explicit
storage and ownership design.

### v5 Definition of Done

- Transport semantics are independent from application execution.
- Process failure and recovery behavior are documented.
- Serialization compatibility is defined.
- Resource ownership and cleanup are deterministic.

---

## Project Structure

```text
niniBUS
├── Core Message Bus
├── Lane Management
├── Publish / Subscribe
└── cfifo
    └── Shared cursor-based FIFO
```

`niniBUS` intentionally does not own worker threads or invoke subscriber
callbacks. This separation allows applications to choose their own execution
model:

- embedded firmware;
- RTOS tasks;
- standard C++ threads;
- simulators;
- event loops;
- platform services.

Execution policy belongs to the application. Message transport belongs to the
bus.

---

## Long-Term Vision

The broader goal is a family of small systems-software components that remain
independently useful:

```text
cfifo
  │
  ▼
niniBUS
  │
  ▼
niniMem
  │
  ▼
Future components
```

Each component should have one clear responsibility, a small explicit API, and
behavior that can be understood without reading an entire framework.

---

## Release Rule

Every milestone follows the same sequence:

1. State the engineering problem.
2. Define the policy and invariants.
3. Implement the smallest complete solution.
4. Add positive, negative, boundary, and regression tests.
5. Update public and internal documentation.
6. Build from a clean state and run all tests.
7. Release only when implementation, tests, and documentation agree.

---

## License

This project is released under the MIT License.
