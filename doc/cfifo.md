# `nbus::cfifo` — Cursor FIFO

> I am not perfect, and neither is my code. Please be gentle with both of us.
> I do, however, come with tests, documentation, and a sincere desire not to
> lose your messages unnecessarily.

## Overview

`nbus::cfifo` means **Cursor FIFO**. It is a bounded, cursor-based message data
structure that stores each message once in a shared circular buffer while
allowing multiple registered subscribers to read the same message
independently.

Think of it as one bookshelf with several readers, each using a separate
bookmark. This saves us from buying every reader an identical bookshelf,
although the slowest readers may occasionally discover that the librarian
needed the space back.

```text
nbus::cfifo<T>
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
slow-subscriber policy when a write reaches a full queue. That sentence is less
cheerful than the bookshelf analogy, but regrettably more important.

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

Two verbs. No committee meeting required.

It does not expose `push()`, `push_back()`, `front()`, or `pop_front()` because
those names suggest a destructive, single-consumer FIFO model.

## Header And Public Types

Include the header:

```cpp
#include "cfifo.h"
```

All Cursor FIFO public types are declared inside `namespace nbus`. Including
the header does not add these names to the global namespace.

The global namespace already has enough furniture. `cfifo` keeps its shoes in
the `nbus` cupboard.

```cpp
namespace nbus
{
// SizeType, SequenceType, CFIFOReadStatus, result types, and cfifo<T>.
}
```

Namespace-level aliases:

```cpp
namespace nbus
{
using SizeType = std::uint32_t;
using SequenceType = std::uint64_t;
}
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

Applications should normally use qualified names such as
`nbus::cfifo<std::string>` and `nbus::CFIFOReadStatus`. A local `using`
declaration is also valid when shorter names are desirable:

```cpp
using nbus::cfifo;
using nbus::CFIFOReadStatus;
```

The subscriber ID and the sequence ID are different concepts:

| Concept | Type | Meaning |
|---|---|---|
| Subscriber ID | `subscriber_type` | Identifies one registered reader. |
| Sequence ID | `SequenceType` | Identifies one position in the global message stream. |
| Bounded size or count | `SizeType` | Capacity, retained size, credit, or current pending count. |
| Accumulated skipped count | `SequenceType` | Total cursor movement accumulated across reclaims before the next successful read. |

The per-subscriber cursor representation is a private nested implementation
type. It is not part of the public `nbus` API.

Subscriber IDs do not need to resemble sequence IDs. For example, subscriber
`5000` may read message sequence `2`.

This is intentional. Asking subscriber 5000 to wait for sequence 5000 would be
a remarkably patient but incorrect API.

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

An empty string is still a message. Quiet messages deserve representation too.

## Construction

```cpp
explicit cfifo(SizeType capacity);
```

Example:

```cpp
nbus::cfifo<std::string> queue(16);
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
    nbus::cfifo<int> invalid(0);
}
catch (const std::invalid_argument&)
{
    // Expected.
}
```

Capacity validation occurs before the backing vector is constructed. This
keeps invalid capacity handling in the member-initialization path.

The constructor is `explicit`, so an integer cannot be silently converted into
a `cfifo` object:

This prevents `8` from waking up one morning and discovering it has become a
message queue.

```cpp
void consume(nbus::cfifo<int> queue);

// consume(8);           // Rejected: no implicit conversion.
consume(nbus::cfifo<int>(8));  // Explicit construction.
```

## Sequence Model

`cfifo` uses monotonically increasing logical sequence numbers and maps them to
physical circular-buffer slots.

### Tail sequence

The tail sequence identifies the **next write position**. If the tail is `5`,
the next successfully written message receives sequence ID `5`, and the tail
then advances to `6`.

The tail is always preparing the next seat. It never sits down itself.

### Cursor read sequence

A subscriber cursor identifies that subscriber's **next unread message**. If a
cursor is `3`, its next successful read returns sequence `3`, after which its
cursor advances to `4`.

It is a bookmark, not a time machine. Moving it forward is easy; asking for
yesterday's page after reclamation is less successful.

### Head sequence

The head sequence identifies the oldest sequence still retained by the current
storage policy. It advances during reclamation, not on each read.

The head moves only when storage pressure becomes persuasive. Until then, it
enjoys a stable administrative position.

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

Off-by-one errors were invited to this section and politely shown the door.

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

The physical slots go in circles; the logical sequence keeps walking straight.
This is healthier than it sounds.

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

That is the entire public menu. The chef refuses to serve `pop_front()`.

## Cursor Registration

### `create_cursor()`

```cpp
SequenceType create_cursor(subscriber_type id);
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
nbus::cfifo<std::string> queue(4);

queue.write("old");          // Sequence 0.
queue.create_cursor(100);    // Cursor begins at tail sequence 1.
queue.write("new");          // Sequence 1.

std::string message;
auto result = queue.read(100, message);

assert(result.status == CFIFOReadStatus::SUCCESS);
assert(result.sequence_id == 1);
assert(message == "new");
```

The return value is:

| Return | Meaning |
|---|---|
| `1` | A new cursor was inserted. |
| `0` | The ID was already registered. |

The declared return type is `SequenceType`, but the current implementation returns the boolean `try_emplace(...).second` converted to that type. Therefore this value is an insertion flag, not the cursor position or tail sequence. Duplicate registration does not reset or change the existing cursor position.

Calling twice is not a subscription renewal ceremony. The first bookmark stays
exactly where it was.

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

The check is courteous, not compulsory—like knocking before entering an empty
conference room.

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
- Discards that subscriber's position and any pending `skipped_messages` report.

If the same ID is registered again later, it is a new cursor at the then-current
tail. Its previous position is not restored.

The re-registered subscriber gets a fresh start, not a season recap.

## Writing

```cpp
CFIFOWriteResult write(const T& value);
```

### Write result

```cpp
struct CFIFOWriteResult
{
    SequenceType sequence_id{0};
    SizeType credit{0};
};
```

Both fields default to zero if an application default-constructs the result.

`write()` has no status field because the current policy always creates space
when the queue is full. Its result fields are:

| Field | Meaning |
|---|---|
| `sequence_id` | The global sequence assigned to the written message. |
| `credit` | The number of unused slots after the write. |

The returned sequence ID is the tail value before the tail advances. The first
write returns sequence `0`, the second returns sequence `1`, and so on.
Reclamation and physical-buffer wraparound do not reset the sequence.

The numbers remain loyal even while physical slots are recycled with the
enthusiasm of a bottle depot.

`credit` is:

```cpp
credit = capacity - size
```

### Write behavior when space exists

When the queue is not full:

1. Calculate `tail_sequence % capacity`.
2. Assign the value to that physical slot.
3. Save the current tail as the written message's `sequence_id`.
4. Increment the tail sequence.
5. Increment retained size.
6. Return the written sequence and remaining credit.

Six steps on paper; one tiny adventure in the buffer.

### Write behavior when full

When `full()` is true, `write()` calls the private reclamation policy before
writing. Current reclamation always creates space in the supported states:

- No registered cursors: discard all retained history.
- Every cursor caught up: discard all retained history.
- One or more cursors behind: advance every cursor tied at the minimum
  sequence, then retain only the range still needed by more advanced cursors.

The new message is written after reclamation and receives the old tail sequence
as its `sequence_id`.

The write always gets a seat. The seating arrangement for slow readers is where
the plot becomes interesting.

## Reading

```cpp
CFIFOReadResult read(subscriber_type id, T& output);
```

### Read result

```cpp
struct CFIFOReadResult
{
    CFIFOReadStatus status{CFIFOReadStatus::NO_PENDING_MESSAGE};
    SizeType pending_messages{0};
    SequenceType sequence_id{0};
    SequenceType skipped_messages{0};
};
```

The result is default-safe: its status defaults to
`NO_PENDING_MESSAGE`, and all numeric fields default to zero.

Declared statuses:

```cpp
enum class CFIFOReadStatus
{
    SUCCESS,
    NO_PENDING_MESSAGE,
    NO_CURSOR
};
```

| Status | Meaning |
|---|---|
| `SUCCESS` | One message was assigned to `output`, and the subscriber cursor advanced. |
| `NO_PENDING_MESSAGE` | The cursor equals the tail and is caught up. |
| `NO_CURSOR` | The subscriber ID is not registered. |

No cursor, no message. The queue is organized, not psychic.

### Successful read

For a successful read:

- `status` is `SUCCESS`.
- `output` contains the message at the cursor's previous sequence.
- `sequence_id` is that previous cursor sequence.
- The cursor advances by exactly one.
- `pending_messages` is calculated after the cursor advances.
- `skipped_messages` reports the cursor's most recent reclamation skip, if any.

Example:

```text
Before read:
cursor = 4
tail   = 7

Read returns sequence_id = 4
Cursor advances to      = 5
pending_messages          = 7 - 5 = 2
```

The arithmetic is intentionally dull. Dull arithmetic is dependable
arithmetic.

### `sequence_id`

`sequence_id` identifies the logical message sequence returned by this specific
read. It is captured before the cursor increments.

It is independent of:

- Subscriber ID.
- Physical buffer index.
- Pending-message count.
- Number of times the circular buffer has wrapped.

Different subscribers reading the same shared message receive the same
`sequence_id`.

Shared truth, independent bookmarks, fewer duplicate objects—everyone wins
except the memory allocator, which gets less attention.

### `pending_messages`

`pending_messages` is cursor-specific. It reports how many currently available
messages remain for that subscriber after the successful read:

```cpp
pending_messages = tail_sequence - advanced_cursor_sequence;
```

It does not report global `size()` or global `credit()`.

`pending_messages` answers “How far behind am I?”, not “How crowded is the
building?” Those are different anxieties.

### `skipped_messages`

`skipped_messages` reports how many unread messages reclamation skipped when it advanced
this cursor directly to the tail:

```cpp
skipped_messages = tail_sequence - old_cursor_sequence;
```

The value is attached to the cursor until its next successful read. That read
returns the value and clears the stored report. Later reads return zero unless
another reclamation moves the cursor again.

If multiple reclamations move the cursor before it successfully reads again,
the skipped counts accumulate. `SequenceType` is used so cumulative logical
movement is not restricted to the 32-bit bounded-capacity count type.

Example:

```text
Before reclaim:
cursor = 1
tail   = 3

Skipped sequences: 1 and 2
skipped_messages:           3 - 1 = 2
```

After reclamation, the cursor is placed at sequence `3`. If a new write stores
sequence `3`, the subscriber's next successful read returns that message with
`sequence_id == 3` and `skipped_messages == 2`.

`skipped_messages` counts skipped messages, not messages previously read
normally. It is an apology counter, not a productivity score.

### Unsuccessful read behavior

For `NO_CURSOR` and `NO_PENDING_MESSAGE`:

```text
pending_messages = 0
sequence_id     = 0
skipped_messages        = 0
```

The output argument is not modified.

`sequence_id` and `skipped_messages` are meaningful only when `status` is
`SUCCESS`. Sequence zero is a valid successful sequence, so always inspect
`status` before interpreting the numeric fields.

Zero is doing two jobs here. Checking `status` is how we ask which hat it is
wearing.

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

Capacity is a promise, not a suggestion.

### `size()`

Returns the number of retained shared slots according to the current head and
tail policy. A read advances one cursor but does not immediately decrement
global size.

Reading alone does not make `size()` smaller. The queue cleans lazily and feels
no shame about it.

### `credit()`

Returns:

```cpp
capacity() - size()
```

Credit changes after writes and reclamation. It does not increase merely
because one subscriber reads a message.

One reader finishing a chapter does not make the shared bookshelf physically
larger.

### `empty()`

Returns whether global retained size is zero.

This is not a cursor-specific caught-up check. The queue may be non-empty while
a particular cursor has no pending messages.

The room can contain books even when Alice has finished reading all of hers.

### `full()`

Returns whether:

```cpp
size() == capacity()
```

The queue invariant requires `size()` never to exceed `capacity()`. Equality
expresses that invariant directly and makes an accidental over-capacity state
visible during testing instead of treating it as an ordinary full state.

If `size()` somehow exceeds capacity, the queue has not become ambitious; an
invariant has been broken.

## Reclamation Policy

`cfifo` describes its reclaim policy this way:

> I will write for you every time, but I hope you will not mind if you miss
> something while reading.

In less emotional and more technical language: writes are prioritized, storage
is bounded, and the slowest subscribers may skip unread messages when the queue
needs room. `skipped_messages` tells them exactly how much disappointment
occurred.

### When reclamation runs

Reclamation is lazy. It runs only when a write begins while the queue is full.
It does not leap into action merely because somebody read a message; it waits
until space is actually needed, much like a sensible person postponing garage
cleaning.

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
`skipped_messages`. The queue cannot return the missed messages, but at least it
does not pretend nothing happened.

### Case 1: no registered cursors

No subscriber can reference retained history, so all retained messages are
reclaimed. Nobody was listening, so the queue tidies the room without filing an
apology:

```text
head = tail
size = 0
```

The pending write then stores one new message:

```text
size = 1
credit = capacity - 1
```

The write completes and returns the new message's sequence ID and remaining
credit. The global sequence does not reset. Only retained storage is cleared.

### Case 2: every cursor is at the tail

The minimum cursor sequence equals the tail. Because it is the minimum, every
other cursor must also be at the tail. No active subscriber needs an old
message, so reclamation can clean everything with a clear conscience:

```text
head = tail
size = 0
```

The pending write then adds one retained message.

### Case 3: one or more cursors are behind

This is where feelings may be bruised. Reclamation performs these steps:

1. Find a subscriber whose cursor has the smallest read sequence.
2. Read that minimum sequence value.
3. Find every cursor with that same minimum sequence.
4. For each tied slow cursor:
   - Record `tail - cursor` in `skipped_messages`.
   - Move the cursor to the current tail.
   - Mark the skip for one-time reporting.
5. Find the new smallest cursor sequence.
6. Move the head to that new minimum.
7. Recalculate retained size as `tail - head`.
8. Write the pending value and increment size.

All cursors tied at the slowest position are moved in the same reclaim call.
The unordered map's iteration order does not change the result because the
policy matches and advances the entire tied group.

Nobody can avoid reclamation by hiding in a tie. The policy has counted
everyone.

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

Rechecking the minimum is the queue's version of looking around before moving
the furniture.

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
skipped_messages = 2 - 1 = 1  // B is skipped
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
sequence_id     = 2
skipped_messages        = 1
pending_messages = 0
```

Message B is gone, message C has arrived, and `skipped_messages` delivers the
awkward but honest explanation.

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
A.skipped_messages = 3 and A moves to 3.
B.skipped_messages = 3 and B moves to 3.
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
report `skipped_messages == 3`.

Cursor C kept up with the reading group. A and B receive the new chapter plus a
brief incident report.

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

Both subscribers next receive sequence `3` with `skipped_messages == 3`.

Nobody was singled out. This particular disappointment was distributed
equally.

### Capacity-one behavior

With capacity `1`, a full-queue write replaces the retained history according
to the same policy:

```text
Write old    → sequence 0, size 1
Write latest → reclaim sequence 0, write sequence 1
```

A subscriber that had not read sequence `0` next receives `latest` with
`skipped_messages == 1`.

A capacity-one queue is less a library and more a sticky note.

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

This rule is worth repeating whenever an ID such as `5000` starts looking
suspiciously mathematical.

## Complete Example

```cpp
#include <cassert>
#include <iostream>
#include <string>

#include "cfifo.h"

int main()
{
    nbus::cfifo<std::string> queue(3);

    constexpr nbus::cfifo<std::string>::subscriber_type alice = 100;
    constexpr nbus::cfifo<std::string>::subscriber_type bob = 200;

    assert(queue.create_cursor(alice));
    assert(queue.create_cursor(bob));

    auto firstWrite = queue.write("first");
    auto secondWrite = queue.write("second");

    assert(firstWrite.sequence_id == 0);
    assert(firstWrite.credit == 2);
    assert(secondWrite.sequence_id == 1);
    assert(secondWrite.credit == 1);

    std::string message;

    auto aliceFirst = queue.read(alice, message);
    assert(aliceFirst.status == nbus::CFIFOReadStatus::SUCCESS);
    assert(aliceFirst.sequence_id == 0);
    assert(aliceFirst.pending_messages == 1);
    assert(aliceFirst.skipped_messages == 0);
    assert(message == "first");

    // Bob independently reads the same shared message and sequence.
    auto bobFirst = queue.read(bob, message);
    assert(bobFirst.status == nbus::CFIFOReadStatus::SUCCESS);
    assert(bobFirst.sequence_id == 0);
    assert(message == "first");

    auto aliceSecond = queue.read(alice, message);
    assert(aliceSecond.status == nbus::CFIFOReadStatus::SUCCESS);
    assert(aliceSecond.sequence_id == 1);
    assert(message == "second");

    auto aliceCaughtUp = queue.read(alice, message);
    assert(aliceCaughtUp.status ==
           nbus::CFIFOReadStatus::NO_PENDING_MESSAGE);

    std::cout << "Cursor FIFO example passed.\n";
    return 0;
}
```

Compile from the repository root:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -I. example/cfifo_test.cpp -o cfifo_test
./cfifo_test
```

If the example passes, the queue is pleased. It remains emotionally neutral
about compiler optimization levels.

Or use the example Makefile:

```bash
cd example
make cfifo-test
```

## Status-Handling Example

```cpp
std::string message;
nbus::CFIFOReadResult result = queue.read(alice, message);

switch (result.status)
{
case nbus::CFIFOReadStatus::SUCCESS:
    std::cout << "sequence=" << result.sequence_id
              << " skipped=" << result.skipped_messages
              << " pending=" << result.pending_messages
              << " message=" << message << '\n';
    break;

case nbus::CFIFOReadStatus::NO_PENDING_MESSAGE:
    std::cout << "subscriber is caught up\n";
    break;

case nbus::CFIFOReadStatus::NO_CURSOR:
    std::cout << "subscriber is not registered\n";
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
| `remove_cursor()` | Expected `O(1)` | One key-based erase. |
| State accessors | `O(1)` | Direct arithmetic or member access. |

The shared message-storage cost is `O(capacity)`. Cursor metadata costs `O(S)`.
Messages are not duplicated per subscriber.

The `O(1)` unordered-map operations are average/expected complexity; worst-case
hash-table behavior can be linear.

Complexity notation is where the jokes become asymptotically less frequent.

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

In short: bring a mutex. Optimism is not a synchronization primitive.

## Error And Exception Behavior

- Zero capacity throws `std::invalid_argument`.
- Memory allocation failures from `std::vector` or `std::unordered_map`
  propagate as standard exceptions.
- Exceptions thrown by `T` construction or assignment propagate to the caller.
- Reading an unknown subscriber returns `NO_CURSOR`; it does not throw.
- Reading a caught-up subscriber returns `NO_PENDING_MESSAGE`; it does not
  throw.
- Duplicate cursor registration returns numeric `0`; it does not throw merely
  because the ID already exists. A new registration returns numeric `1`.
- Removing an unknown cursor returns `false`.

The API does not currently define a general exception-safety guarantee for
user-defined `T` types whose assignment operations throw.

If `T` throws during assignment, `cfifo` will not attempt couples counseling
between your type and the standard library.

## Current Limitations And Small Disappointments

Every data structure has dreams larger than its current implementation.
`cfifo` is no exception. Please lower your expectations gently:

- It is not thread-safe yet. Multiple threads must bring their own lock and,
  ideally, agree about who is holding it.
- Reclamation is lossy for the slowest subscribers. The queue favors fresh
  writes and sends an honest `skipped_messages` count with the next successful
  read.
- There is no lossless backpressure mode. The queue will not ask a publisher
  to sit quietly and reconsider its life choices.
- There is no blocking read, blocking write, or timeout API. It is a data
  structure, not a waiting room.
- There is no cursor-specific `pending(id)` inspection API. For now, a read
  result is the source of truth.
- There is no retained-history registration mode. New cursors begin at the
  current tail, so late arrivals do not receive a dramatic recap.
- Removing a cursor does not reclaim immediately. Cleanup waits until a later
  full-queue write actually needs space.
- Subscriber IDs cannot currently use a configurable type.
- Allocator customization is not exposed; the standard allocator has the job.
- Move-only or non-default-constructible message types are not supported by the
  current vector-and-assignment storage model.
- `create_cursor()` currently declares `SequenceType` but returns a numeric insertion flag (`1` or `0`); this should be normalized to an intentional API type in a future cleanup.
- Sequence-number overflow behavior is not defined. A 64-bit sequence provides
  a very long runway, but infinity was outside the milestone.
- Serialization and persistence are not provided. Restarting the process is
  not a memory test the queue can pass.
- Iterators and STL container compatibility are intentionally absent. `cfifo`
  admires the STL without trying to impersonate it.

## Test Coverage

The example test executable currently covers:

- Constructor state and zero-capacity rejection.
- Cursor registration, lookup, and duplicate detection.
- Unknown-cursor reads.
- Future-only late registration.
- Multiple subscribers reading the same shared messages.
- Normal sequence IDs across independent cursors.
- Sequence IDs returned by writes before and after reclamation.
- Full-queue reclamation without subscribers.
- Cursor removal and duplicate removal.
- Empty strings as valid messages.
- Bounded size and credit after reclamation.
- One-time `skipped_messages` reporting.
- Accumulation across multiple reclaims before a successful read.
- Sequence IDs after reclamation.
- Preservation of a more advanced cursor.
- Reclamation when every cursor is caught up.
- Repeated reclamation and physical wraparound.
- Logical write sequence IDs across physical buffer wraparound.
- Capacity-one reclamation.
- Removal of the slowest cursor before reclamation.
- Removal and future-only re-registration at the current tail.
- Separation of subscriber IDs and sequence IDs.
- Reclamation of every cursor tied at the head.
- Preservation of an advanced cursor while tied slow cursors are moved.

Run the tests with:

```bash
cd example
make cfifo-test
```

The tests are not proof of perfection—the opening paragraph already confessed
to that—but they are substantially better than hopeful staring.

## Policy Summary

The defining behavior of the current `cfifo` is:

> I will store each message once and let every registered subscriber keep its
> own place. I will welcome late subscribers at the current tail. I will keep
> writing when the buffer fills, although the slowest readers may miss a few
> chapters. If that happens, I will report the exact number through
> `skipped_messages`. I may be bounded and imperfect, but at least I am honest.
