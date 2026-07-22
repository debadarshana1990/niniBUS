# `cfifo` — Cursor FIFO

`cfifo` means **Cursor FIFO**. It is a bounded data structure for delivering
messages from one shared circular buffer to multiple independent readers.

Each reader is identified by a subscriber ID and owns a cursor containing its
position in the global message sequence. Reading advances only the requesting
subscriber. It does not directly remove the message for other subscribers.

```text
cfifo<T>
├── One bounded shared circular buffer
├── One global write sequence
└── Subscriber cursor map
    ├── Subscriber 10 → read sequence
    ├── Subscriber 20 → read sequence
    └── Subscriber 30 → read sequence
```

Messages are stored once, rather than copied into a separate queue for each
subscriber.

## Intention

`cfifo` is a purpose-built Cursor FIFO data structure. Its primary operations
are intentionally named:

```cpp
write(value);
read(subscriber, message);
```

The interface uses a few familiar C++ container ideas, including `value_type`,
`size()`, `capacity()`, and `empty()`, to make it easy for C++ developers to
understand. It is only inspired by those conventions. It is not intended to be
an STL container or a drop-in replacement for `std::queue` or `std::deque`, and
STL interface compatibility is not a design goal.

In particular, `cfifo` does not expose `push()`, `push_back()`, `front()`, or
`pop_front()`. Those names imply destructive single-reader queue behavior,
which does not describe a shared queue with independent cursors.

## Header And Types

Include the header:

```cpp
#include "cfifo.h"
```

The current public types are:

```cpp
using SizeType = std::uint32_t;
using SequenceType = std::uint64_t;

// Inside cfifo<T>:
using value_type = T;
using subscriber_type = std::uint32_t;
```

Create a queue by specifying its value type and capacity:

```cpp
cfifo<std::string> messages(16);
```

The capacity must be greater than zero. A zero capacity throws
`std::invalid_argument`.

## Public API

### Constructor

```cpp
explicit cfifo(SizeType capacity);
```

Constructs a bounded Cursor FIFO. The constructor is `explicit`, preventing an
integer from being implicitly converted into a `cfifo` object.

### Register a subscriber cursor

```cpp
bool create_cursor(subscriber_type id);
```

Registers a new cursor at the current global tail. The subscriber is initially
caught up and can read messages written after registration. It does not receive
messages already written before registration.

`create_cursor()` returns `true` when it inserts a new ID. It returns `false`
when the ID already exists, and the existing cursor position remains unchanged.
Internally, this duplicate-safe behavior is provided by `try_emplace()`.

```cpp
assert(messages.create_cursor(100));
assert(!messages.create_cursor(100));
```

### Check subscriber registration

```cpp
bool contains_cursor(subscriber_type id) const;
```

Returns `true` when the subscriber ID has a registered cursor.

```cpp
if (!messages.contains_cursor(100))
{
    messages.create_cursor(100);
}
```

### Remove a subscriber cursor

```cpp
bool remove_cursor(subscriber_type id);
```

Removes the registered cursor. It returns `true` when the cursor existed and
was removed, or `false` when the ID was not registered. Removal does not
immediately reclaim storage; a later full-queue write runs reclamation using
the remaining active cursors.

### Write a message

```cpp
CFIFOWriteResult write(const T& value);
```

Writes one message at the global tail. When the queue is full and subscribers
exist, `write()` invokes the reclaim policy described below before storing the
new message.

```cpp
struct CFIFOWriteResult
{
    CFIFOWriteStatus status;
    SizeType credit;
};
```

The statuses are:

| Status | Meaning |
|---|---|
| `SUCCESS` | The value was written. |
| `Q_FULL` | The queue is full and cannot reclaim because no cursor exists. |
| `FAILED` | Reserved for a future failure mode. |

`credit` is the number of unused shared-buffer slots after the write attempt.

```cpp
auto result = messages.write("hello");
if (result.status == CFIFOWriteStatus::SUCCESS)
{
    std::cout << "remaining credit: " << result.credit << '\n';
}
```

### Read for one subscriber

```cpp
CFIFOReadResult read(subscriber_type id, T& message);
```

Reads the next available message and advances only the requested subscriber's
cursor.

```cpp
struct CFIFOReadResult
{
    CFIFOReadStatus status;
    SizeType movedBy;
    SizeType pendingMessage;
};
```

The statuses are:

| Status | Meaning |
|---|---|
| `SUCCESS` | A message was copied to the output argument. |
| `NO_PENDING_MESSAGE` | The subscriber cursor is caught up with the writer. |
| `NO_CURSOR` | The subscriber ID is not registered. |
| `FAILED` | Reserved for a future failure mode. |

On a successful read:

- `pendingMessage` is the number of messages still pending for that subscriber.
- `movedBy` reports messages skipped when reclamation moved that subscriber to
  the tail. It is reported on the subscriber's next successful read and is then
  cleared.

For `NO_CURSOR` and `NO_PENDING_MESSAGE`, both counts are zero and the output
message is not modified.

```cpp
std::string message;
auto result = messages.read(100, message);

if (result.status == CFIFOReadStatus::SUCCESS)
{
    std::cout << "message: " << message << '\n';
    std::cout << "skipped by reclaim: " << result.movedBy << '\n';
    std::cout << "still pending: " << result.pendingMessage << '\n';
}
```

### Inspect shared queue state

```cpp
bool empty() const;
bool full() const;
SizeType credit() const;
SizeType size() const;
SizeType capacity() const;
```

These functions describe the shared retained storage, not one subscriber's
cursor:

- `empty()` reports whether the shared queue retains no messages.
- `full()` reports whether retained storage has reached capacity.
- `credit()` reports the number of unused slots.
- `size()` reports the number of retained slots.
- `capacity()` reports the configured maximum number of slots.

A queue may be non-empty even when a particular subscriber has no pending
messages. Use the result of `read()` to determine whether that subscriber is
caught up. A future cursor-specific pending API may provide this information
without attempting a read.

## Reclamation And Slow Subscribers

Reading advances a cursor but does not immediately alter global `size()` or
`credit()`. Reclamation runs when a write finds the shared buffer full.

The current policy favors recent data:

1. Find a subscriber with the smallest read sequence.
2. Record how many unread messages that subscriber is skipping.
3. Advance that subscriber directly to the current tail.
4. Recalculate the oldest sequence still required by any active subscriber.
5. Repeat when tied slow cursors still leave the queue full.
6. Write the new message once at least one slot is available.

Consequences of this policy:

- A slow subscriber may lose unread messages when a new write needs space.
- The skipped count is returned through `movedBy` on its next successful read.
- Faster subscribers retain independently readable messages when their cursor
  still determines part of the retained range.
- When all active cursors are advanced to the tail, all old retained storage is
  reclaimed before the new message is written.
- `size()` never intentionally exceeds `capacity()`, and `credit()` remains in
  the range from zero through `capacity()`.
- With no registered cursor, a full queue cannot select a reclaim candidate, so
  `write()` returns `Q_FULL`.

If multiple subscribers are tied at the oldest sequence, their selection order
is unspecified because the cursor map is unordered. Reclamation may advance
multiple tied subscribers until a slot becomes available.

This is an eviction policy. It is intentionally different from a strict
reclamation rule that would retain every message until every active subscriber
had consumed it.

## Complete Example

```cpp
#include <cassert>
#include <string>

#include "cfifo.h"

int main()
{
    cfifo<std::string> queue(2);

    constexpr cfifo<std::string>::subscriber_type alice = 1;
    constexpr cfifo<std::string>::subscriber_type bob = 2;

    assert(queue.create_cursor(alice));
    assert(queue.create_cursor(bob));

    assert(queue.write("first").status == CFIFOWriteStatus::SUCCESS);
    assert(queue.write("second").status == CFIFOWriteStatus::SUCCESS);

    std::string message;

    auto aliceFirst = queue.read(alice, message);
    assert(aliceFirst.status == CFIFOReadStatus::SUCCESS);
    assert(aliceFirst.pendingMessage == 1);
    assert(message == "first");

    // Bob has an independent cursor and reads the same shared message.
    auto bobFirst = queue.read(bob, message);
    assert(bobFirst.status == CFIFOReadStatus::SUCCESS);
    assert(message == "first");

    // The full queue reclaims according to cursor progress before writing.
    auto thirdWrite = queue.write("third");
    assert(thirdWrite.status == CFIFOWriteStatus::SUCCESS);

    return 0;
}
```

Build and run the repository test example:

```bash
cd example
make cfifo-test
```

## Current Limitations

- The class is single-threaded. Concurrent calls require external
  synchronization.
- Reclamation deliberately allows slow subscribers to skip unread messages.
- Cursor tie-breaking order is unspecified.
- Cursor removal does not reclaim immediately.
- The sequence-number overflow policy is not defined.
- `FAILED` statuses are reserved and are not currently produced.

Future work should strengthen Cursor FIFO semantics without changing its core
identity: one bounded shared message buffer, independent subscriber cursors,
and the `write()`/`read()` data-operation vocabulary.
