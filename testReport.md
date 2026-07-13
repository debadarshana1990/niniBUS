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
| FIFO ordering through bus receive | Publish `first`, `second`, `third`; receive three times; receive once more from empty lane. | `test_fifo_ordering()` | Pass | Verifies FIFO order and `ReceiveStatus::LaneEmpty` after queue drain. |
| Multiple lanes stay independent | Publish to lane 1, lane 2, lane 1; receive from lane 1, lane 2, lane 1. | `test_multiple_lanes_do_not_interfere()` | Pass | Confirms each lane has independent queue state. |
| Publish lazily creates missing lane | Publish to lane 20; receive from lane 20. | `test_publish_to_non_existing_lane()` | Pass | Confirms publish creates a missing lane and stores the message. |
| Receive lazily creates missing lane | Receive from missing lane 30; receive again; publish; receive. | `test_receive_lazily_creates_missing_lane()` | Pass | Confirms `ReceiveStatus::LazyLaneCreated`, output clearing, and later publish/receive behavior. |
| Lane credit decreases and recovers | Publish two messages; check credit; receive one; publish again; check credit. | `test_lane_capacity_and_credit()` | Pass | Verifies credit decreases on publish and reflects freed space after receive. |
| Full lane rejects publish and preserves FIFO order | Fill lane to `DEFAULT_LANE_CAPACITY`; attempt overflow publish; receive one; retry publish; drain lane. | `test_lane_full_with_capacity_10()` | Pass | Verifies `PublishStatus::LaneFull`, retry after space is freed, and FIFO order after recovery. |
| Receive clears output before empty pop | Receive a real message; set output to stale data; receive from empty lane. | `test_receive_clears_output_on_empty_lane()` | Pass | Confirms empty receive clears old output data and returns `ReceiveStatus::LaneEmpty`. |
| Direct FIFO push, pop, and front behavior | Create FIFO; check empty state; pop empty; push two; inspect `front()`; pop until empty. | `test_direct_fifo_push_pop_front_status()` | Pass | Covers direct `niniFIFO` `push_back()`, `pop_front()`, `front()`, `isEmpty()`, `size()`, and status behavior. |
| Direct FIFO full status | Push `DEFAULT_LANE_CAPACITY` items; check full state; attempt overflow push. | `test_direct_fifo_full_status()` | Pass | Confirms `FIFOStatus::FULL` and that failed overflow does not increase size. |

## Expected Output

Successful test output includes:

```text
[PASS] FIFO ordering through bus receive
[PASS] multiple lanes stay independent
[PASS] publish lazily creates missing lane
[PASS] receive lazily creates missing lane
[PASS] lane credit decreases after publish and recovers after receive
[PASS] full lane rejects publish and preserves FIFO order after retry
[PASS] receive clears output before empty pop
[PASS] direct FIFO push_back pop_front and front behavior
[PASS] direct FIFO reports full status
All niniBUS example tests passed.
```

## Current Coverage Summary

Covered:

- Bus publish success path.
- Bus receive success path.
- Lazy lane creation from publish.
- Lazy lane creation from receive.
- Empty-lane receive behavior.
- Output clearing before receive.
- Multiple independent lanes.
- Lane capacity and credit.
- Full-lane rejection.
- FIFO order after full-lane recovery.
- Direct FIFO `push_back()`, `pop_front()`, `front()`, `isEmpty()`, `isFull()`,
  `size()`, and `getCapacity()` behavior.

Not covered yet:

- Thread safety.
- IPC.
- Runtime capacity configuration.
- Multi-producer or multi-consumer concurrency.
- Exception behavior from calling `front()` on an empty FIFO.
