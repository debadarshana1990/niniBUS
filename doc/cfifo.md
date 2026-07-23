# `cfifo` — Cursor FIFO

## Overview

`cfifo` means **Cursor FIFO**. It is a bounded, cursor-based message data
structure that stores each message once in a shared circular buffer while
allowing multiple registered subscribers to read the same message
independently.

```text
cfifo<T>
├── Bounded circular buffer<T>
├── Global head sequence
├── Global tail sequence
└── Subscriber cursor map
    ├── Subscriber 10 → next read sequence
    ├── Subscriber 20 → next read sequence
    └── Subscriber 30 → next read sequence
```

A conventional FIFO normally removes its front element when it is read.
`cfifo` cannot do that because another subscriber may still need the same
message. Instead, each subscriber owns a cursor that identifies its next unread
sequence.

The implementation is currently single-threaded and uses a bounded, lossy
slow-subscriber policy when a write reaches a full queue.

## Design Intention

`cfifo` is intended to be a dedicated Cursor FIFO data structure for
one-to-many delivery:

- A message is written once.
- Message storage is shared by all subscribers.
- Every registered subscriber progresses independently.
- Subscriber queues are not duplicated.
- Storage remains bounded.
- A slow subscriber may be advanced when new data needs space.
- The subscriber is told how many messages reclamation skipped.

The design is inspired by familiar C++ container conventions where those
conventions improve readability. It exposes names such as `value_type`,
`size()`, `capacity()`, and `empty()`.

It is **not** intended to be an STL container, satisfy STL container
requirements, or act as a drop-in replacement for `std::queue`, `std::deque`,
or another standard container.

The public data-operation vocabulary is deliberately:

```cpp
write(value);
read(subscriber, output);
```

It does not expose `push()`, `push_back()`, `front()`, or `pop_front()` because
those names suggest a destructive, single-consumer FIFO model.

## Header And Public Types

Include the header:

```cpp
#include "cfifo.h"
```

Namespace-scope types:

```cpp
using SizeType = std::uint32_t;
using SequenceType = std::uint64_t;
```

Class aliases:

```cpp
template <typename T>
class cfifo
{
public:
    using value_type = T;
    using subscriber_type = std::uint32_t;
};
```

The subscriber ID and the sequence ID are different concepts:

| Concept | Type | Meaning |
|---|---|---|
| Subscriber ID | `subscriber_type` | Identifies one registered reader. |
| Sequence ID | `SequenceType` | Identifies one position in the global message stream. |
| Size or count | `SizeType` | Capacity, retained size, credit, pending count, or skipped count. |

Subscriber IDs do not need to resemble sequence IDs. For example, subscriber
`5000` may read message sequence `2`.

## Required Properties Of `T`

The current storage is created with:

```cpp
std::vector<T> buffer_(capacity);
```

Consequently, the current implementation requires `T` to be default
constructible. Writing and reading use assignment:

```cpp
buffer_[index] = value;
output = buffer_[index];
```

Therefore, the operations used in this implementation also require `T` to be
copy-assignable. Empty strings and other values equal to their default value
are valid messages; status values, not message contents, indicate whether a
read succeeded.

## Construction

```cpp
explicit cfifo(SizeType capacity);
```

Example:

```cpp
cfifo<std::string> queue(16);
```

Construction establishes:

```text
capacity() = requested capacity
size()     = 0
credit()   = capacity()
empty()    = true
full()     = false
head       = 0
tail       = 0
```

A zero capacity throws `std::invalid_argument`:

```cpp
try
{
    cfifo<int> invalid(0);
}
catch (const std::invalid_argument&)
{
    // Expected.
}
```

The constructor is `explicit`, so an integer cannot be silently converted into
a `cfifo` object:

```cpp
void consume(cfifo<int> queue);

// consume(8);           // Rejected: no implicit conversion.
consume(cfifo<int>(8));  // Explicit construction.
```

## Sequence Model

`cfifo` uses monotonically increasing logical sequence numbers and maps them to
physical circular-buffer slots.

### Tail sequence

The tail sequence identifies the **next write position**. If the tail is `5`,
the next successfully written message receives sequence ID `5`, and the tail
then advances to `6`.

### Cursor read sequence

A subscriber cursor identifies that subscriber's **next unread message**. If a
cursor is `3`, its next successful read returns sequence `3`, after which its
cursor advances to `4`.

### Head sequence

The head sequence identifies the oldest sequence still retained by the current
storage policy. It advances during reclamation, not on each read.

### Half-open sequence ranges

Pending and skipped ranges use the standard half-open form:

```text
[cursor.read_sequence, tail_sequence)
```

The number of messages in that range is:

```cpp
tail_sequence - cursor.read_sequence
```

There is no `+1` because `tail_sequence` is the next empty write position, not
an existing message.

Example:

```text
cursor = 1
tail   = 3

Unread sequences: 1, 2
Count:            3 - 1 = 2
```

### Physical buffer index

A logical sequence maps to a physical slot with modulo arithmetic:

```cpp
index = sequence % capacity;
```

For capacity `3`:

| Sequence | Physical slot |
|---:|---:|
| 0 | 0 |
| 1 | 1 |
| 2 | 2 |
| 3 | 0 |
| 4 | 1 |

Sequence IDs continue increasing even when physical storage wraps around.

## Public API Summary

| API | Purpose |
|---|---|
| `cfifo(capacity)` | Construct a bounded shared buffer. |
| `write(value)` | Write at the global tail, reclaiming first when full. |
| `read(id, output)` | Read and advance only subscriber `id`. |
| `create_cursor(id)` | Register `id` at the current tail. |
| `contains_cursor(id)` | Check whether `id` is registered. |
| `remove_cursor(id)` | Remove a registered subscriber cursor. |
| `empty()` | Report whether retained shared storage is empty. |
| `full()` | Report whether retained shared storage reached capacity. |
| `credit()` | Report unused shared-buffer slots. |
| `size()` | Report retained shared-buffer slots. |
| `capacity()` | Report configured capacity. |

## Cursor Registration

### `create_cursor()`

```cpp
bool create_cursor(subscriber_type id);
```

A new cursor is initialized at the current tail:

```cpp
cursor_map_.try_emplace(id, tail_sequence_);
```

This means registration is **future-only**:

- Messages written before registration are not delivered to the new cursor.
- Messages written after registration are delivered in sequence order unless
  later skipped by reclamation.
- A newly registered cursor is initially caught up.

Example:

```cpp
cfifo<std::string> queue(4);

queue.write("old");          // Sequence 0.
queue.create_cursor(100);    // Cursor begins at tail sequence 1.
queue.write("new");          // Sequence 1.

std::string message;
auto result = queue.read(100, message);

assert(result.status == CFIFOReadStatus::SUCCESS);
assert(result.sequenceID == 1);
assert(message == "new");
```

The return value is:

| Return | Meaning |
|---|---|
| `true` | A new cursor was inserted. |
| `false` | The ID was already registered. |

Duplicate registration does not reset or change the existing cursor position.

### `contains_cursor()`

```cpp
bool contains_cursor(subscriber_type id) const;
```

Returns whether a cursor is registered:

```cpp
if (!queue.contains_cursor(100))
{
    queue.create_cursor(100);
}
```

This check is useful when a caller wants to avoid attempting duplicate
registration. `create_cursor()` itself is already duplicate-safe.

### `remove_cursor()`

```cpp
bool remove_cursor(subscriber_type id);
```

The return value is:

| Return | Meaning |
|---|---|
| `true` | The cursor existed and was removed. |
| `false` | The ID was not registered. |

Removing a cursor:

- Immediately prevents further reads using that subscriber ID.
- Does not immediately change `head_sequence_`, `size()`, or `credit()`.
- Excludes the cursor from the next reclamation decision.
- Discards that subscriber's position and any pending `movedBy` report.

If the same ID is registered again later, it is a new cursor at the then-current
tail. Its previous position is not restored.

## Writing

```cpp
CFIFOWriteResult write(const T& value);
```

### Write result

```cpp
struct CFIFOWriteResult
{
    CFIFOWriteStatus status;
    SizeType credit;
};
```

Declared statuses:

```cpp
enum class CFIFOWriteStatus
{
    SUCCESS,
    Q_EMPTY,
    Q_FULL,
    FAILED
};
```

Current behavior:

| Status | Current meaning |
|---|---|
| `SUCCESS` | The message was stored and the tail advanced. |
| `Q_EMPTY` | Returned only by the defensive branch where a full queue cannot be reclaimed. Current `reclaim()` paths return `true`, so this is not normally produced. |
| `Q_FULL` | Declared but not currently returned by `write()`. In particular, a full queue with no registered cursors does **not** return `Q_FULL`; it reclaims all retained history and accepts the new value. |
| `FAILED` | Reserved and not currently returned. |

`credit` reports unused slots after the operation:

```cpp
credit = capacity - size
```

### Write behavior when space exists

When the queue is not full:

1. Calculate `tail_sequence % capacity`.
2. Assign the value to that physical slot.
3. Increment the tail sequence.
4. Increment retained size.
5. Return `SUCCESS` and the remaining credit.

### Write behavior when full

When `full()` is true, `write()` calls the private reclamation policy before
writing. Current reclamation always creates space in the supported states:

- No registered cursors: discard all retained history.
- Every cursor caught up: discard all retained history.
- One or more cursors behind: advance every cursor tied at the minimum
  sequence, then retain only the range still needed by more advanced cursors.

The new message is written after reclamation and receives the old tail sequence
as its `sequenceID`.

## Reading

```cpp
CFIFOReadResult read(subscriber_type id, T& output);
```

### Read result

```cpp
struct CFIFOReadResult
{
    CFIFOReadStatus status;
    SizeType pendingMessage;
    SequenceType sequenceID;
    SizeType movedBy;
};
```

Declared statuses:

```cpp
enum class CFIFOReadStatus
{
    SUCCESS,
    NO_PENDING_MESSAGE,
    NO_CURSOR,
    FAILED
};
```

| Status | Meaning |
|---|---|
| `SUCCESS` | One message was assigned to `output`, and the subscriber cursor advanced. |
| `NO_PENDING_MESSAGE` | The cursor equals the tail and is caught up. |
| `NO_CURSOR` | The subscriber ID is not registered. |
| `FAILED` | Reserved and not currently returned. |

### Successful read

For a successful read:

- `status` is `SUCCESS`.
- `output` contains the message at the cursor's previous sequence.
- `sequenceID` is that previous cursor sequence.
- The cursor advances by exactly one.
- `pendingMessage` is calculated after the cursor advances.
- `movedBy` reports the cursor's most recent reclamation skip, if any.

Example:

```text
Before read:
cursor = 4
tail   = 7

Read returns sequenceID = 4
Cursor advances to      = 5
pendingMessage          = 7 - 5 = 2
```

### `sequenceID`

`sequenceID` identifies the logical message sequence returned by this specific
read. It is captured before the cursor increments.

It is independent of:

- Subscriber ID.
- Physical buffer index.
- Pending-message count.
- Number of times the circular buffer has wrapped.

Different subscribers reading the same shared message receive the same
`sequenceID`.

### `pendingMessage`

`pendingMessage` is cursor-specific. It reports how many currently available
messages remain for that subscriber after the successful read:

```cpp
pendingMessage = tail_sequence - advanced_cursor_sequence;
```

It does not report global `size()` or global `credit()`.

### `movedBy`

`movedBy` reports how many unread messages reclamation skipped when it advanced
this cursor directly to the tail:

```cpp
movedBy = tail_sequence - old_cursor_sequence;
```

The value is attached to the cursor until its next successful read. That read
returns the value and clears the stored report. Later reads return zero unless
another reclamation moves the cursor again.

Example:

```text
Before reclaim:
cursor = 1
tail   = 3

Skipped sequences: 1 and 2
movedBy:           3 - 1 = 2
```

After reclamation, the cursor is placed at sequence `3`. If a new write stores
sequence `3`, the subscriber's next successful read returns that message with
`sequenceID == 3` and `movedBy == 2`.

`movedBy` counts skipped messages, not messages previously read normally.

### Unsuccessful read behavior

For `NO_CURSOR` and `NO_PENDING_MESSAGE`:

```text
pendingMessage = 0
sequenceID     = 0
movedBy        = 0
```

The output argument is not modified.

Sequence zero is a valid successful sequence. Always check `status` before
interpreting `sequenceID`.

## Shared Queue State

```cpp
bool empty() const;
bool full() const;
SizeType credit() const;
SizeType size() const;
SizeType capacity() const;
```

These functions describe retained shared storage, not one subscriber's
progress.

### `capacity()`

Returns the fixed number of physical buffer slots supplied to the constructor.

### `size()`

Returns the number of retained shared slots according to the current head and
tail policy. A read advances one cursor but does not immediately decrement
global size.

### `credit()`

Returns:

```cpp
capacity() - size()
```

Credit changes after writes and reclamation. It does not increase merely
because one subscriber reads a message.

### `empty()`

Returns whether global retained size is zero.

This is not a cursor-specific caught-up check. The queue may be non-empty while
a particular cursor has no pending messages.

### `full()`

Returns whether:

```cpp
size() >= capacity()
```

In a valid state, size does not exceed capacity. The `>=` comparison is
defensive: if bookkeeping were ever corrupted, it prevents an over-capacity
state from appearing writable.

## Reclamation Policy

### When reclamation runs

Reclamation is lazy. It runs only when a write begins while the queue is full.

It does not run immediately when:

- A subscriber reads.
- A cursor catches up.
- A cursor is removed.
- A new cursor is registered.

This means already-consumed physical history can remain counted in `size()`
until a later full-queue write triggers reclamation.

### Policy goal

The current policy prioritizes accepting recent writes while keeping storage
bounded. It is a deliberate eviction policy, not a lossless slow-subscriber
policy.

When necessary, the slowest subscribers are advanced to the current tail and
lose their unread backlog. Their next successful read reports the loss through
`movedBy`.

### Case 1: no registered cursors

No subscriber can reference retained history, so all retained messages are
reclaimed:

```text
head = tail
size = 0
```

The pending write then stores one new message:

```text
size = 1
credit = capacity - 1
```

The write returns `CFIFOWriteStatus::SUCCESS`; it does not return `Q_FULL`.
The global sequence does not reset. Only retained storage is cleared.

### Case 2: every cursor is at the tail

The minimum cursor sequence equals the tail. Because it is the minimum, every
other cursor must also be at the tail. No active subscriber needs an old
message, so reclamation sets:

```text
head = tail
size = 0
```

The pending write then adds one retained message.

### Case 3: one or more cursors are behind

Reclamation performs these steps:

1. Find a subscriber whose cursor has the smallest read sequence.
2. Read that minimum sequence value.
3. Find every cursor with that same minimum sequence.
4. For each tied slow cursor:
   - Record `tail - cursor` in `movedBy`.
   - Move the cursor to the current tail.
   - Mark the skip for one-time reporting.
5. Find the new smallest cursor sequence.
6. Move the head to that new minimum.
7. Recalculate retained size as `tail - head`.
8. Write the pending value and increment size.

All cursors tied at the slowest position are moved in the same reclaim call.
The unordered map's iteration order does not change the result because the
policy matches and advances the entire tied group.

### Why the minimum is found again

After moving the old slowest group, another subscriber may determine the new
oldest retained sequence.

```text
Before reclaim:
tail = 3
A = 0
B = 0
C = 1

Move tied slow cursors A and B:
A = 3
B = 3
C = 1

New minimum = 1
head = 1
retained size before new write = 3 - 1 = 2
```

Setting the head directly to the tail would incorrectly discard sequences `1`
and `2`, which cursor C still needs.

### Worked example: partially advanced subscriber

Capacity is `2`:

```text
Write A → sequence 0
Write B → sequence 1
tail = 2
size = 2 (full)

Subscriber reads A:
cursor = 1
size remains 2

Write C triggers reclaim:
movedBy = 2 - 1 = 1  // B is skipped
cursor = 2
head = 2
size before C = 0

Write C → sequence 2
tail = 3
size = 1
credit = 1
```

The subscriber's next read returns:

```text
message        = C
sequenceID     = 2
movedBy        = 1
pendingMessage = 0
```

### Worked example: tied slow cursors and one faster cursor

Capacity is `3`:

```text
Write A, B, C → sequences 0, 1, 2
tail = 3

Cursor A = 0
Cursor B = 0
Cursor C = 1
```

Writing D triggers reclamation:

```text
A and B are tied at the minimum sequence 0.
A.movedBy = 3 and A moves to 3.
B.movedBy = 3 and B moves to 3.
C remains at 1.

New head = 1
Retained size before D = 3 - 1 = 2
```

D is written at sequence `3`:

```text
tail = 4
size = 3
credit = 0
```

Cursor C can still read B and C before D. Cursors A and B next read D and each
report `movedBy == 3`.

### Worked example: all cursors tied at the head

```text
capacity = 3
tail = 3
A = 0
B = 0
```

Both cursors are moved to tail `3`. The new minimum is `3`, so:

```text
head = 3
size before new write = 0
```

After writing sequence `3`:

```text
size = 1
credit = 2
```

Both subscribers next receive sequence `3` with `movedBy == 3`.

### Capacity-one behavior

With capacity `1`, a full-queue write replaces the retained history according
to the same policy:

```text
Write old    → sequence 0, size 1
Write latest → reclaim sequence 0, write sequence 1
```

A subscriber that had not read sequence `0` next receives `latest` with
`movedBy == 1`.

## Behavioral Invariants

Assuming no sequence-number overflow and a valid internal state:

```text
0 <= size <= capacity
credit = capacity - size
head_sequence <= tail_sequence
head_sequence <= every active cursor sequence <= tail_sequence
```

After reclamation and before the pending write:

```text
head_sequence = minimum active cursor sequence
size = tail_sequence - head_sequence
```

When there are no active cursors, or every cursor is at the tail:

```text
head_sequence = tail_sequence
size = 0
```

After every successful write:

```text
new message sequence = previous tail_sequence
tail_sequence advances by one
size advances by one
```

Subscriber IDs never participate in sequence arithmetic.

## Complete Example

```cpp
#include <cassert>
#include <iostream>
#include <string>

#include "cfifo.h"

int main()
{
    cfifo<std::string> queue(3);

    constexpr cfifo<std::string>::subscriber_type alice = 100;
    constexpr cfifo<std::string>::subscriber_type bob = 200;

    assert(queue.create_cursor(alice));
    assert(queue.create_cursor(bob));

    auto firstWrite = queue.write("first");
    auto secondWrite = queue.write("second");

    assert(firstWrite.status == CFIFOWriteStatus::SUCCESS);
    assert(firstWrite.credit == 2);
    assert(secondWrite.status == CFIFOWriteStatus::SUCCESS);
    assert(secondWrite.credit == 1);

    std::string message;

    auto aliceFirst = queue.read(alice, message);
    assert(aliceFirst.status == CFIFOReadStatus::SUCCESS);
    assert(aliceFirst.sequenceID == 0);
    assert(aliceFirst.pendingMessage == 1);
    assert(aliceFirst.movedBy == 0);
    assert(message == "first");

    // Bob independently reads the same shared message and sequence.
    auto bobFirst = queue.read(bob, message);
    assert(bobFirst.status == CFIFOReadStatus::SUCCESS);
    assert(bobFirst.sequenceID == 0);
    assert(message == "first");

    auto aliceSecond = queue.read(alice, message);
    assert(aliceSecond.status == CFIFOReadStatus::SUCCESS);
    assert(aliceSecond.sequenceID == 1);
    assert(message == "second");

    auto aliceCaughtUp = queue.read(alice, message);
    assert(aliceCaughtUp.status == CFIFOReadStatus::NO_PENDING_MESSAGE);

    std::cout << "Cursor FIFO example passed.\n";
    return 0;
}
```

Compile from the repository root:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -I. example/cfifo_test.cpp -o cfifo_test
./cfifo_test
```

Or use the example Makefile:

```bash
cd example
make cfifo-test
```

## Status-Handling Example

```cpp
std::string message;
CFIFOReadResult result = queue.read(alice, message);

switch (result.status)
{
case CFIFOReadStatus::SUCCESS:
    std::cout << "sequence=" << result.sequenceID
              << " skipped=" << result.movedBy
              << " pending=" << result.pendingMessage
              << " message=" << message << '\n';
    break;

case CFIFOReadStatus::NO_PENDING_MESSAGE:
    std::cout << "subscriber is caught up\n";
    break;

case CFIFOReadStatus::NO_CURSOR:
    std::cout << "subscriber is not registered\n";
    break;

case CFIFOReadStatus::FAILED:
    std::cout << "reserved read failure\n";
    break;
}
```

## Complexity

Let `S` be the number of registered subscribers.

| Operation | Expected time | Notes |
|---|---:|---|
| Construction | `O(capacity)` | The vector default-constructs every `T`. |
| `write()` without reclaim | `O(1)` | One modulo operation and assignment. |
| `write()` with reclaim | `O(S)` | Several linear scans of the cursor map. |
| `read()` | Expected `O(1)` | One unordered-map lookup and one assignment. |
| `create_cursor()` | Expected `O(1)` | May rehash the unordered map. |
| `contains_cursor()` | Expected `O(1)` | Unordered-map lookup. |
| `remove_cursor()` | Expected `O(1)` | Lookup followed by erase. |
| State accessors | `O(1)` | Direct arithmetic or member access. |

The shared message-storage cost is `O(capacity)`. Cursor metadata costs `O(S)`.
Messages are not duplicated per subscriber.

The `O(1)` unordered-map operations are average/expected complexity; worst-case
hash-table behavior can be linear.

## Thread Safety

`cfifo` is currently single-threaded.

The implementation contains mutable shared state:

- Circular-buffer elements.
- Head and tail sequences.
- Retained size.
- Subscriber cursor positions.
- Reclamation metadata.

Concurrent reads or writes without external synchronization cause data races.
This includes two subscribers reading concurrently, because both calls access
the same cursor map and shared object state.

Use external locking if a `cfifo` instance must be shared across threads. No
internal locks, atomics, memory-ordering guarantees, or lock-free behavior are
currently provided.

## Error And Exception Behavior

- Zero capacity throws `std::invalid_argument`.
- Memory allocation failures from `std::vector` or `std::unordered_map`
  propagate as standard exceptions.
- Exceptions thrown by `T` construction or assignment propagate to the caller.
- Reading an unknown subscriber returns `NO_CURSOR`; it does not throw.
- Reading a caught-up subscriber returns `NO_PENDING_MESSAGE`; it does not
  throw.
- Duplicate cursor registration returns `false`; it does not throw merely
  because the ID already exists.
- Removing an unknown cursor returns `false`.

The API does not currently define a general exception-safety guarantee for
user-defined `T` types whose assignment operations throw.

## Current Limitations And Explicit Non-Goals

- The implementation is not thread-safe.
- Reclamation is lossy for the slowest subscribers.
- There is no lossless backpressure mode.
- There is no blocking read or blocking write.
- There is no timeout API.
- There is no cursor-specific `pending(id)` inspection API.
- There is no retained-history registration mode; new cursors always start at
  the current tail.
- Cursor removal does not trigger immediate reclamation.
- Subscriber IDs cannot currently be configured to another type.
- Allocator customization is not exposed.
- Move-only, non-default-constructible message types are not supported by the
  current storage and assignment approach.
- Sequence-number overflow behavior is not defined.
- Serialization and persistence are not provided.
- The class does not provide iterators or STL container compatibility.
- `Q_FULL` and `FAILED` write statuses are declared but not currently emitted.
- `FAILED` read status is declared but not currently emitted.
- `Q_EMPTY` is present in the write status enum and is only associated with a
  defensive reclaim-failure branch; normal current reclaim paths succeed.

## Test Coverage

The example test executable currently covers:

- Constructor state and zero-capacity rejection.
- Cursor registration, lookup, and duplicate detection.
- Unknown-cursor reads.
- Future-only late registration.
- Multiple subscribers reading the same shared messages.
- Normal sequence IDs across independent cursors.
- Full-queue reclamation without subscribers.
- Cursor removal and duplicate removal.
- Empty strings as valid messages.
- Bounded size and credit after reclamation.
- One-time `movedBy` reporting.
- Sequence IDs after reclamation.
- Preservation of a more advanced cursor.
- Reclamation when every cursor is caught up.
- Repeated reclamation and physical wraparound.
- Capacity-one reclamation.
- Removal of the slowest cursor before reclamation.
- Separation of subscriber IDs and sequence IDs.
- Reclamation of every cursor tied at the head.
- Preservation of an advanced cursor while tied slow cursors are moved.

Run the tests with:

```bash
cd example
make cfifo-test
```

## Policy Summary

The defining behavior of the current `cfifo` is:

> Store each message once in a bounded shared circular buffer. Let registered
> subscribers advance independently through global sequence IDs. Register new
> subscribers at the current tail. When a full queue needs space, discard all
> history if nobody needs it; otherwise advance every subscriber tied at the
> oldest cursor to the tail, report its skipped count on its next successful
> read, preserve any history still needed by more advanced subscribers, and
> write the newest message.
