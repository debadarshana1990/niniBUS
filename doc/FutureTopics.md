# niniBUS Future Topics

This document holds work intentionally deferred from the current
single-threaded, bounded, write-prioritized cursor bus.

## API Consistency

- Consider separate receive statuses for missing lane and missing subscriber
  only if callers need the distinction.

## Alternative Delivery Policies

The current policy always writes and may skip slow readers. Future policies
could include:

- reject write when full;
- block or wait for readers;
- timed write;
- drop newest instead of oldest unread data;
- disconnect a slow subscriber;
- prioritize selected subscribers;
- retain messages for a configured time;
- provide lossless lanes separately from latest-data lanes.

Policy must be chosen per lane or per bus explicitly. Changing the default
silently would break delivery expectations.

## Cursor Lifecycle

- Immediate head recomputation after unsubscribe.
- Semantics when the final cursor is removed.
- Optional registration at head for replay.
- Registration at a caller-provided sequence.
- Cursor reset or seek.
- Subscriber identity reuse.
- Maximum cursor count and bounded registration storage.

## Sequence Rollover

Logical sequences use a finite unsigned type. Work is needed to define:

- modular ordering;
- safe distance comparisons;
- pending and skipped calculations across rollover;
- rollover tests near the maximum value;
- whether reset is allowed when no cursors or messages remain.

Ordinary unsigned wrap alone is not a complete ordering policy.

## Concurrency

The current implementation is single-threaded. A concurrent version must
address:

- lane-map insertion versus lookup;
- writes versus reads on the circular buffer;
- cursor registration/removal versus reclaim;
- result visibility and message publication ordering;
- iterator/reference invalidation;
- lock granularity;
- deadlock avoidance;
- atomics and memory-order proofs;
- starvation and progress guarantees.

Start with a correct locked implementation and measured contention before
considering lock-free structures.

## Memory Predictability

- Caller-supplied allocator.
- Fixed storage with no post-construction allocation.
- Bounded or non-owning message types.
- Cursor registry without `unordered_map`.
- Allocation-failure behavior.
- Exact per-lane and per-subscriber footprint measurements.

## Observability

- Per-lane writes, reads, skips, and reclaim events.
- Current subscriber count.
- Per-cursor lag.
- High-water retained size.
- Last written and read sequence.
- Diagnostic callbacks that do not change delivery behavior.

Counters must define overflow and concurrency semantics before becoming API.

## Testing

- Model-based comparison against a simple reference implementation.
- Random operation sequences covering write/read/subscribe/unsubscribe.
- Capacity values 1, 2, and large boundaries.
- Multiple tied slow-cursor groups.
- Repeated unsubscribe/resubscribe cycles.
- Sequence rollover injection.
- Allocation-failure tests.
- Sanitizer and race-detector builds.
- Long-duration invariant checks.

## Persistence And IPC

Future transport work must define ownership, serialization, crash recovery,
process death, cursor persistence, compatibility, security, and cleanup.
The in-memory `cfifo` should not be assumed to map directly into shared memory
because standard containers and strings contain process-local state.

## Documentation

When a future topic is implemented, move it to `Milestone.md`, record the
decision in `DesignDecisions.md`, update `DESIGN.md` and `cfifo.md`, and add
the exact tests to `testReport.md`.
