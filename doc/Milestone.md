# niniBUS Milestones

This roadmap keeps the project focused. Each feature should belong to one
milestone. Ideas that do not fit the active milestone should move to a future
ideas document instead of expanding the current scope.

## Philosophy

- Correctness first.
- Simplicity before optimization.
- One milestone at a time.
- Measure before optimizing.
- Keep examples and documentation aligned with the code.

Current active milestone: V1 - Smarter Bus.

## V0 - Core Bus

Objective: build the smallest working in-process message bus.

Current status: complete.

TODO:

- [x] Define `laneID_t`.
- [x] Define `lane_t`.
- [x] Store messages per lane.
- [x] Create lanes lazily.
- [x] Implement `publish()`.
- [x] Implement `receive()`.
- [x] Implement `subscribe()`.
- [x] Preserve FIFO ordering within each lane.
- [x] Keep lanes independent from each other.
- [x] Store lanes without raw owning pointers.
- [x] Build the bus as a static library.
- [x] Move the example into `example/`.
- [x] Give the example its own Makefile.
- [x] Make `make all` remove `.o` and `.d` metadata files.
- [x] Update `README.md`.
- [x] Update `DESIGN.md`.
- [x] Make the example check `ReceiveStatus` before using received data.
- [x] Document that custom destructors are not required right now.
- [x] Remove unused includes such as `<vector>` and `<queue>`.
- [x] Split lane implementation into `Lane.h` and `Lane.cpp`.
- [x] Move queue push/pop behavior into `lane_t`.
- [x] Keep lane ID as the map key instead of storing it inside `lane_t`.
- [x] Keep `niniBUS::publish()` and `niniBUS::receive()` as lookup/delegation
      code.

Out of scope:

- Multi-threading.
- IPC.
- Back pressure.
- Custom FIFO implementation.
- Lock-free structures.
- Performance optimization.

Definition of done:

- Multiple lanes can be created.
- Messages can be published and received.
- Messages are received in FIFO order.
- Empty-lane receive behavior is documented.
- Missing-lane receive behavior is documented.
- Example builds and runs.
- Documentation matches the implementation.

## V0.1 - Engineering Cleanup

Objective: make the prototype easier to maintain and safer to extend.

TODO:

- [x] Remove stale comments.
- [x] Remove unused headers.
- [x] Make API naming and parameter names consistent.
- [x] Decide whether `subscribe()` should return `bool` or a result enum.
- [x] Decide whether unused enum values should be implemented or removed.
- [x] Make lane size, capacity, and credit helpers private.
- [x] Add unit tests for publish/receive FIFO behavior.
- [x] Add unit tests for multiple independent lanes.
- [x] Add unit tests for receiving from an empty lane.
- [x] Add unit tests for receiving from a missing lane.
- [x] Add unit tests for publish-before-subscribe.
- [x] Add a test Makefile target or document the test command.
- [x] Make the example handle return values explicitly.
- [x] Keep `README.md` and `DESIGN.md` updated with every API change.

Definition of done:

- Code has no obvious stale comments or unused includes.
- Basic tests pass.
- Example handles error/result values correctly.
- Public API behavior is documented.
- No known ownership issues remain.

## V1 - Smarter Bus

Objective: improve the single-threaded bus without changing its core
architecture.

Current status: started.

Implemented so far:

- Lanes are bounded by `DEFAULT_LANE_CAPACITY`.
- `lane_t::push()` returns `PublishStatus::LaneFull` when the lane has no
  remaining credit.
- `PublishResult::Credit` gives publisher feedback after each publish attempt.
- Example tests cover lane capacity, lane credit, and full-lane behavior.

TODO:

- [x] Start V1 officially.
- [x] Split lane implementation into separate `Lane.h` and `Lane.cpp` files.
- [x] Keep `niniBUS::publish()` and `niniBUS::receive()` as boring
      lookup/delegation functions.
- [x] Move publish/receive queue behavior into `lane_t::push()` and
      `lane_t::pop()`.
- [x] Document that queue behavior changes should stay isolated in `lane_t`.
- [x] Decide whether lanes should have bounded queues.
- [x] Add default bounded lane capacity.
- [x] Implement `PublishStatus::LaneFull`.
- [x] Add lane credit through `PublishResult::Credit`.
- [x] Add publisher feedback for accepted and rejected messages.
- [ ] Decide whether `PublishStatus::LaneNotFound` is needed.
- [x] Make lane size, capacity, and credit helpers private.
- [x] Decide whether public queue size/statistics accessors are needed.
- [x] Add tests for queue capacity.
- [x] Add tests for publish result statuses.
- [x] Document basic back-pressure behavior.

Possible API shape:

```cpp
PublishResult result = bus.publish(lane, msg);

if (result.Status == PublishStatus::LaneFull)
{
    // Handle back pressure.
}
```

Definition of done:

- Queue capacity behavior is clear.
- Publish result statuses used by the implementation are tested.
- Lane credit and `LaneFull` behavior are documented.
- Any unused result statuses are either implemented, removed, or explicitly
  documented as reserved.

## V2 - Embedded Optimization

Objective: reduce memory footprint and improve predictability for constrained
systems.

TODO:

- [ ] Measure memory used by one `lane_t`.
- [ ] Measure overhead of `std::unordered_map<laneID_t, lane_t>`.
- [ ] Compare `unordered_map` with a fixed-size lane table.
- [ ] Decide whether dynamic allocation from STL containers is acceptable.
- [ ] Investigate fixed maximum lane count.
- [ ] Investigate fixed maximum queue depth.
- [ ] Investigate custom allocator or pool allocator.
- [ ] Benchmark publish and receive costs.
- [ ] Document memory/performance tradeoffs.

Questions to answer:

- How much memory does one lane consume?
- Is `unordered_map` the right storage strategy?
- Should the bus support compile-time limits?
- Can memory be reduced without sacrificing readability?

Definition of done:

- Memory and timing measurements exist.
- Storage strategy is chosen based on measurements.
- Any embedded constraints are documented.

## V3 - Concurrency

Objective: support multi-threaded applications safely.

TODO:

- [ ] Define the thread-safety contract.
- [ ] Add mutex protection for `lane_map_`.
- [ ] Add synchronization for each lane queue.
- [ ] Decide whether operations should block or remain non-blocking.
- [ ] Consider `receive_blocking()`.
- [ ] Consider timeout-based receive.
- [ ] Add multi-threaded tests.
- [ ] Measure contention.
- [ ] Document concurrency guarantees.

Possible future topics:

- Lock-free queues.
- Memory ordering.
- ABA problem.
- False sharing.

Definition of done:

- Concurrent publish/receive is safe.
- Threaded tests pass repeatedly.
- Performance impact is measured.
- Documentation explains the concurrency model.

## V4 - IPC

Objective: support communication across multiple processes.

TODO:

- [ ] Define IPC requirements.
- [ ] Decide whether IPC belongs in core `niniBUS` or a separate transport.
- [ ] Evaluate Unix domain sockets.
- [ ] Evaluate shared memory.
- [ ] Evaluate serialization format.
- [ ] Decide whether zero-copy transport is necessary.
- [ ] Prototype one transport.
- [ ] Add IPC example.
- [ ] Document transport limitations.

Possible transports:

- Unix domain socket.
- Shared memory.
- Zero-copy shared buffer.

Definition of done:

- One IPC transport works in an example.
- Transport lifecycle is documented.
- IPC design does not complicate the core in-process bus unnecessarily.

## V5 - Production Features

Objective: add operational features only after the core behavior is stable.

TODO:

- [ ] Add structured logging or quiet logging controls.
- [ ] Add metrics hooks.
- [ ] Add profiling hooks.
- [ ] Add configuration object or builder.
- [ ] Add versioning policy.
- [ ] Add API stability notes.
- [ ] Add packaging/install target if needed.
- [ ] Add CI build/test workflow.
- [ ] Add release checklist.

Possible features:

- Monitoring.
- Metrics.
- Profiling.
- Logging.
- Configuration.
- Transport plugins.
- Persistence, only if justified.

Definition of done:

- Production features are optional and documented.
- Core API remains understandable.
- Build/test/release workflow is repeatable.

## Rules

1. Never optimize before measuring.
2. Correctness before performance.
3. One milestone at a time.
4. Any idea outside the current milestone should be moved out of the active
   milestone scope.
5. Finish the current milestone before unlocking the next one.
