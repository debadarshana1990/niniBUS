# niniBUS Test Report

This report tracks the current example-based tests in `example/hello.cpp`.

## How To Run

From the repository root:

```bash
make
make -C example test
```

The example prints one `[PASS]` line for each successful test. A failed
assertion stops the program.

## Current Test Cases

| Test Case | Steps | Function | Status | Comment |
|---|---|---|---|---|
| FIFO ordering through bus receive | Publish `first`, `second`, `third`; receive three times; receive once more from empty lane. | `test_fifo_ordering()` | Pass | Verifies FIFO order, `ReceiveResult::PendingMessages`, and `ReceiveStatus::LaneEmpty` after queue drain. |
| Multiple lanes stay independent | Publish to lane 1, lane 2, lane 1; receive from lane 1, lane 2, lane 1. | `test_multiple_lanes_do_not_interfere()` | Pass | Confirms each lane has independent queue state and independent pending counts. |
| Publish lazily creates missing lane | Publish to lane 20; receive from lane 20. | `test_publish_to_non_existing_lane()` | Pass | Confirms publish creates a missing lane, stores the message, and receive reports zero pending after drain. |
| Explicit lane uses requested capacity | Create lane 21 with capacity 3; fill it; reject overflow; drain it. | `test_create_lane_uses_requested_capacity()` | Pass | Verifies custom capacity, credit, full status, FIFO order, and pending counts. |
| Duplicate creation preserves lane | Create lane 22 with capacity 2; publish; request the same ID with capacity 5; fill and overflow. | `test_create_lane_does_not_replace_existing_lane()` | Pass | Confirms `LaneExist` and that the original lane capacity and queued state remain intact. |
| Publish-created lane uses default capacity | Publish to missing lane 23; attempt explicit recreation with a larger capacity; fill to the default and overflow. | `test_publish_created_lane_uses_default_capacity()` | Pass | Confirms lazy publish creation uses `DEFAULT_LANE_CAPACITY` and cannot be resized by `createLane()`. |
| Zero capacity is rejected | Request capacity 0, then create the same lane ID with capacity 1. | `test_create_lane_rejects_zero_capacity()` | Pass | Confirms `InvalidCapacity` does not reserve or create the lane. |
| Capacity-one lane works | Fill a one-slot lane, reject overflow, receive, republish, and receive again. | `test_capacity_one_lane()` | Pass | Covers the smallest valid circular buffer and reuse when head and tail coincide. |
| Custom-capacity FIFO wraps | Fill a capacity-3 FIFO, pop two, push two, and drain in order. | `test_custom_capacity_fifo_wraparound()` | Pass | Verifies modulo arithmetic uses runtime capacity and preserves FIFO order. |
| Receive lazily creates missing lane | Receive from missing lane 30; receive again; publish; receive. | `test_receive_lazily_creates_missing_lane()` | Pass | Confirms `ReceiveStatus::LazyLaneCreated`, output clearing, zero pending count, and later publish/receive behavior. |
| Lane credit decreases and recovers | Publish two messages; check credit; receive one; publish again; check credit. | `test_lane_capacity_and_credit()` | Pass | Verifies credit decreases on publish, receive reports pending messages, and freed space allows publish. |
| Full lane rejects publish and preserves FIFO order | Fill lane to `DEFAULT_LANE_CAPACITY`; attempt overflow publish; receive one; retry publish; drain lane. | `test_lane_full_with_capacity_10()` | Pass | Verifies `PublishStatus::LaneFull`, retry after space is freed, FIFO order after recovery, and receive pending counts. |
| Receive clears output before empty pop | Receive a real message; set output to stale data; receive from empty lane. | `test_receive_clears_output_on_empty_lane()` | Pass | Confirms empty receive clears old output data and returns `ReceiveStatus::LaneEmpty` with zero pending messages. |
| Receive reports pending message count | Publish four messages; receive each message; check pending counts from 3 to 0; receive once more from empty lane. | `test_receive_reports_pending_messages()` | Pass | Confirms `ReceiveResult::PendingMessages` is measured after a successful pop. |
| Direct FIFO push, pop, and front behavior | Create FIFO; check empty state; pop empty; push two; inspect `front()`; pop until empty. | `test_direct_fifo_push_pop_front_status()` | Pass | Covers direct `niniFIFO` `push_back()`, `pop_front()`, `front()`, `empty()`, `size()`, and status behavior. |
| Direct FIFO full status | Push `DEFAULT_LANE_CAPACITY` items; check full state; attempt overflow push. | `test_direct_fifo_full_status()` | Pass | Confirms `FIFOStatus::FULL` and that failed overflow does not increase size. |
| Direct FIFO wraparound | Fill FIFO with `A0`-`A9`; pop five; push `B0`-`B4`; drain remaining `A` values; drain wrapped `B` values. | `test_fifo_wraparound()` | Pass | Proves tail wrap, head wrap, order preservation across wraparound, and queue reuse. |
| Direct FIFO empty negative paths | Pop from empty FIFO twice; call `front()` on empty FIFO and catch `std::runtime_error`. | `test_fifo_empty_negative_paths()` | Pass | Confirms empty pop returns `FIFOStatus::EMPTY` repeatedly and empty `front()` throws. |
| Direct FIFO overflow does not corrupt data | Fill FIFO; attempt two overflow pushes; drain original values. | `test_fifo_overflow_does_not_corrupt_order()` | Pass | Confirms rejected overflow writes do not change size or corrupt queued order. |
| Bus rejected publish is not received | Fill a bus lane; publish one rejected message; drain accepted messages; receive empty. | `test_bus_rejected_publish_is_not_received()` | Pass | Confirms `PublishStatus::LaneFull` does not enqueue the rejected message. |

## Expected Output

Successful test output includes:

```text
[PASS] FIFO ordering through bus receive
[PASS] multiple lanes stay independent
[PASS] publish lazily creates missing lane
[PASS] createLane uses the requested capacity
[PASS] createLane preserves an existing lane and its capacity
[PASS] publish-created lane uses the default capacity
[PASS] createLane rejects zero capacity without creating a lane
[PASS] capacity-one lane supports full receive and reuse
[PASS] custom-capacity FIFO wraparound preserves order
[PASS] receive lazily creates missing lane
[PASS] lane credit decreases after publish and recovers after receive
[PASS] full lane rejects publish and preserves FIFO order after retry
[PASS] receive clears output before empty pop
[PASS] receive reports pending message count
[PASS] direct FIFO push_back pop_front and front behavior
[PASS] direct FIFO reports full status
[PASS] direct FIFO wraparound preserves order
[PASS] direct FIFO empty pop and front negative paths
[PASS] direct FIFO overflow does not corrupt queued data
[PASS] bus rejected publish is not received later
All niniBUS example tests passed.
```

## Current Coverage Summary

Covered:

- Bus publish success path.
- Bus receive success path.
- Receive result status and pending-message count.
- Lazy lane creation from publish.
- Lazy lane creation from receive.
- Explicit lane creation with runtime capacity.
- Duplicate lane creation and capacity preservation.
- Default capacity for publish-created lanes.
- Empty-lane receive behavior.
- Output clearing before receive.
- Multiple independent lanes.
- Lane capacity and credit.
- Full-lane rejection.
- FIFO order after full-lane recovery.
- Direct FIFO wraparound of both head and tail indexes.
- Direct FIFO negative behavior for empty pop, empty front, and overflow.
- Bus-level negative behavior for rejected publish.
- Direct FIFO `push_back()`, `pop_front()`, `front()`, `empty()`, `full()`,
  `size()`, and `capacity()` behavior.

Not covered yet:

- Thread safety.
- IPC.
- Multi-producer or multi-consumer concurrency.
