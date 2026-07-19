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
| Explicit subscription | Create a lane; register two subscribers; repeat one registration. | `test_create_lane_and_subscribe()` | Pass | Verifies subscriber registration and duplicate detection. |
| Subscribe lazily creates lane | Subscribe on a missing lane, then try explicit lane creation. | `test_subscribe_lazily_creates_lane()` | Pass | Confirms subscribe creates a default-capacity lane. |
| Receive lazily creates lane and subscriber | Receive on a missing lane and verify later duplicate subscription. | `test_receive_lazily_creates_lane_and_subscriber()` | Pass | Verifies `LazyLaneCreated`, output clearing, and cursor creation. |
| Direct cursor registration | Add and query two CFIFO subscriber IDs. | `test_direct_cfifo_cursor_registration()` | Pass | Covers direct cursor registration and duplicate rejection. |
| Receive automatically subscribes | Receive on an existing empty lane, then receive a retained message with another new subscriber. | `test_receive_auto_subscribes_existing_lane()` | Pass | Confirms read-time automatic registration on empty and nonempty lanes. |
| Multiple subscribers read same message | Publish once and receive independently with two subscriber IDs. | `test_multiple_subscribers_read_same_message()` | Pass | Confirms one shared stored message can be read by multiple subscribers. |
| Independent catch-up and pending counts | Publish A and B; let two subscribers independently read A, B, then catch up. | `test_subscribers_advance_independently_and_catch_up()` | Pass | Verifies FIFO order, subscriber-specific pending counts, independent advancement, and `LaneEmpty` after catch-up. |
| Capacity-one catch-up | Publish one message, read it, then receive again. | `test_capacity_one_subscriber_does_not_repeat_message()` | Pass | Confirms a wrapped physical index does not repeat a consumed message. |
| Full-capacity cursor wrap | Fill a capacity-three lane, read all messages, receive after catch-up, then attempt another publish. | `test_full_capacity_cursor_wrap_and_pending_count()` | Pass | Detects modulo-cursor ambiguity, pending-count underflow, message repetition, and confirms eviction remains deferred. |
| Different subscriber speeds | Publish A, B, C and interleave reads from a fast and slow subscriber. | `test_subscribers_can_read_at_different_speeds()` | Pass | Confirms one subscriber cannot advance or change another subscriber's pending count. |
| Late subscriber retained history | Publish two messages before the subscriber's first receive, then read both. | `test_late_subscriber_reads_retained_history()` | Pass | Documents and verifies that new subscribers begin at the oldest retained message. |
| Empty string message | Publish an empty string, receive it successfully, then receive after catch-up. | `test_empty_string_message_is_distinct_from_empty_lane()` | Pass | Confirms status distinguishes a valid empty payload from an unavailable message. |

## Expected Output

Successful test output includes:

```text
[PASS] create lane and register independent subscribers
[PASS] subscribe lazily creates a default-capacity lane
[PASS] receive lazily creates a lane and subscriber cursor
[PASS] direct CFIFO cursor registration
[PASS] receive auto-subscribes on an existing lane
[PASS] multiple subscribers read the same shared message
[PASS] subscriber cursors advance independently and stop at the tail
[PASS] capacity-one subscriber does not repeat a consumed message
[PASS] full-capacity cursor wrap preserves pending counts
[PASS] subscribers consume independently at different speeds
[PASS] late subscriber reads retained history from the beginning
[PASS] empty string message is distinct from an empty lane
All currently supported niniBUS example tests passed.
```

## Current Coverage Summary

Covered:

- Explicit and automatic subscriber registration.
- Lazy lane creation through subscribe and receive.
- Multiple subscribers reading one retained message.
- Independent subscriber FIFO order and catch-up.
- Subscriber-specific pending-message counts.
- Empty receive after subscriber catch-up.
- Capacity-one physical-index wrap without message repetition.
- Full-capacity wrap with correct pending countdown and catch-up.
- Different subscriber consumption speeds.
- Late-subscriber retained-history semantics.
- Empty string payload status handling.
- Direct CFIFO cursor registration.

Not covered yet:

- Thread safety.
- IPC.
- Multi-producer or multi-consumer concurrency.
- Queue-slot eviction and restored publisher credit.
