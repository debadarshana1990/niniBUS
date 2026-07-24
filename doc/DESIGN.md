# niniBUS Design

## Purpose

niniBUS provides bounded, in-process broadcast delivery. A publisher writes a
message once to a lane. Every registered subscriber on that lane can read the
same message through an independent cursor.

The design is intentionally single-threaded. No method is safe to call
concurrently without external synchronization.

## Architecture

```text
niniBUS
└── unordered_map<laneID_t, lane>
    └── lane
        └── nbus::cfifo<string>
            ├── bounded shared message storage
            └── cursor map
                ├── subscriber A -> read sequence
                └── subscriber B -> read sequence
```

### `niniBUS`

Owns lane topology and translates lane lookup results into public bus result
types. It is responsible for explicit and publish-side lazy lane creation.

### `lane`

Adapts subscriber IDs and string messages to `nbus::cfifo<std::string>`.
It does not maintain a second queue or duplicate messages per subscriber.

### `nbus::cfifo<T>`

Owns bounded storage, logical sequence numbers, cursor registration, cursor
progress, skip accounting, and reclamation. See `cfifo.md` for its complete
contract.

## Data Model

Lane and subscriber IDs are `uint32_t`. Message sequence IDs are `uint64_t`.
Physical buffer sizes and indexes use `nbus::SizeType`; logical positions use
`nbus::SequenceType`.

The retained logical range is:

```text
[head_sequence_, tail_sequence_)
```

`tail_sequence_` is the next sequence to write. Each cursor's read sequence is
the next message that subscriber would read.

## Operation Flows

### Explicit lane creation

1. Validate that capacity is non-zero.
2. Insert the lane with `try_emplace`.
3. Preserve an existing lane if the ID is already present.

### Publish

1. Look up the lane.
2. If absent, create it with `DEFAULT_LANE_CAPACITY`.
3. Delegate to the lane and then to `cfifo::write`.
4. If storage is full, reclaim according to the write-priority policy.
5. Store the message, assign a sequence ID, and return remaining credit.

Publishing is the only operation that lazily creates a lane.
This is a deliberate convenience policy: callers can publish immediately when
the default capacity is suitable, or call `createLane()` first to configure a
specific capacity.

### Subscribe

1. Look up the lane.
2. If absent, return `LaneNotExist`.
3. Create a cursor at the current tail.
4. If the cursor already exists, leave its position unchanged.

Because registration begins at the tail, a subscriber observes future
messages only.

### Receive

1. Clear the caller's output string.
2. Look up the lane without inserting into the lane map.
3. Ask the existing lane to read using the subscriber ID.
4. Return `NO_CURSOR` for either a missing lane or missing subscriber.
5. Return `NO_PENDING_MESSAGE` if that cursor is caught up.
6. On success, copy the message and advance only that cursor.

Receive is topology-preserving: it never creates a lane or subscriber.
Successful receive necessarily mutates the requesting cursor's delivery
progress; other cursors are unchanged.

### Unsubscribe

1. Look up the lane without creating it.
2. Remove the requested cursor if it exists.
3. Return whether a cursor was actually removed.

Unsubscribe does not delete an empty lane.

## Write-Priority Reclamation

Storage is bounded but writes are accepted. When full:

- With no cursors, all retained messages can be discarded.
- If all cursors are caught up, all retained messages can be discarded.
- Otherwise, every cursor tied at the oldest read sequence is moved to the
  current tail.
- The number bypassed is accumulated in each moved cursor's skip counter.
- Head and size are recomputed from the new oldest cursor.
- The write then proceeds.

Moving every cursor tied at the minimum is required. Moving only one tied
cursor would leave another cursor at the old minimum, so no capacity would be
released.

The trade-off is deliberate: publishers do not wait or fail because of a slow
reader. Readers must inspect `SkippedMessages` if loss matters.

## Important Invariants

- One lane owns exactly one shared message buffer.
- A message is stored once regardless of subscriber count.
- Every registered subscriber has at most one cursor per lane.
- Duplicate registration does not rewind a cursor.
- Only the requesting cursor advances on a normal successful read.
- Reclamation may advance slow cursors as a policy action.
- `size() <= capacity()` after every public operation.
- `credit() == capacity() - size()`.
- Physical indexing uses `sequence % capacity`.
- A missing receive target never changes the lane map.

## Ownership And Lifetime

The bus owns lanes by value. A lane owns its `cfifo` by value. `cfifo` owns its
storage and cursor map. Standard-library RAII handles destruction; custom
destructors are unnecessary.

References or iterators into the unordered lane map must not be kept across
operations that can insert lanes, because rehashing can invalidate them.

## Error Model

Expected control-flow outcomes use status/result values:

- invalid or duplicate lane creation;
- missing lane during subscription;
- missing cursor or no pending message during receive;
- unsuccessful unsubscribe.

Construction with capacity zero throws `std::invalid_argument`, because a
zero-sized circular store cannot satisfy indexing invariants.

## Non-Goals

The current design does not provide concurrency, durability, IPC, delivery
acknowledgement, retry, ordering across lanes, subscriber fairness, or
lossless backpressure.
