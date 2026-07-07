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

## V0 - Core Bus

Objective: build the smallest working in-process message bus.

Current status: mostly complete.

TODO:

- [x] Define `lane_t`.
- [x] Define `Lane`.
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
- [x] Make the example check `ReceiveResult` before printing received data.
- [x] Decide whether the destructor should print messages or stay quiet. : Its humor, let it be 
- [x] Remove unused includes such as `<vector>` and `<queue>`.

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

- [ ] Remove stale comments.
- [ ] Remove unused headers.
- [ ] Make API naming and parameter names consistent.
- [ ] Decide whether `subscribe()` should return `bool` or a result enum.
- [ ] Decide whether unused enum values should be implemented or removed.
- [ ] Add unit tests for publish/receive FIFO behavior.
- [ ] Add unit tests for multiple independent lanes.
- [ ] Add unit tests for receiving from an empty lane.
- [ ] Add unit tests for receiving from a missing lane.
- [ ] Add unit tests for publish-before-subscribe.
- [ ] Add a test Makefile target or document the test command.
- [ ] Make the example handle return values explicitly.
- [ ] Keep `README.md` and `DESIGN.md` updated with every API change.

Definition of done:

- Code has no obvious stale comments or unused includes.
- Basic tests pass.
- Example handles error/result values correctly.
- Public API behavior is documented.
- No known ownership issues remain.

## V1 - Smarter Bus

Objective: improve the single-threaded bus without changing its core
architecture.

TODO:

- [ ] Decide whether lanes should have bounded queues.
- [ ] Add optional queue capacity per lane.
- [ ] Implement `PublishResult::LaneFull` if bounded queues are added.
- [ ] Decide whether `PublishResult::LaneNotFound` is needed.
- [ ] Add queue size/statistics accessors.
- [ ] Add lane existence/query helpers if useful.
- [ ] Add tests for queue capacity.
- [ ] Add tests for publish result statuses.
- [ ] Document back-pressure behavior.

Possible API shape:

```cpp
PublishResult result = bus.publish(lane, msg);

if (result == PublishResult::LaneFull)
{
    // Handle back pressure.
}
```

Definition of done:

- Queue capacity behavior is clear.
- Publish result enums are either fully implemented or simplified.
- Queue statistics are tested and documented.

## V2 - Embedded Optimization

Objective: reduce memory footprint and improve predictability for constrained
systems.

TODO:

- [ ] Measure memory used by one `Lane`.
- [ ] Measure overhead of `std::unordered_map<uint32_t, Lane>`.
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
