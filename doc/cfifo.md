# `cfifo` — Cursor FIFO

`cfifo` means **Cursor FIFO**. It is a bounded, cursor-based FIFO container for
delivering messages from one shared queue to multiple independent readers.

Unlike a traditional FIFO, reading does not immediately remove the message from
the shared queue. Each reader is identified by a cursor ID, and every cursor
tracks its own position in the message sequence.

```text
cfifo<T>
├── One bounded shared buffer
├── One global write sequence
└── Cursor map
    ├── Cursor 10 → read sequence
    ├── Cursor 20 → read sequence
    └── Cursor 30 → read sequence
```

This lets multiple cursors read the same stored message without creating a
separate message queue for every reader.

## Header And Type

Include the header:

```cpp
#include "cfifo.h"
```

Create a queue by specifying its message type and capacity:

```cpp
cfifo<std::string> messages(16);
```

Capacity should be greater than zero.

## Purpose And Design Direction

`cfifo` is a purpose-built data structure for cursor-based FIFO delivery. Its
API is intentionally centered on two data operations:

```cpp
write(value);
read(cursor, message);
```

The design borrows familiar C++ container conventions such as `value_type`,
`size_type`, `size()`, `capacity()`, and `empty()`. This makes the type easier
for C++ developers to understand, but `cfifo` is **not intended to become an STL
container**.

Current type aliases are:

```cpp
using size_type = std::uint32_t; // Namespace scope.

// Inside cfifo<T>:
using value_type = T;
using sequence_type = std::uint64_t;
using cursor_type = std::uint32_t;
```

```cpp
bool empty() const;
bool full() const;
size_type size() const;
size_type capacity() const;
```

It deliberately does not expose `push()`, `push_back()`, `front()`, or
`pop_front()`. Those names suggest conventional destructive queue behavior,
while Cursor FIFO has one shared message store and independent reader progress.
The public data-operation vocabulary will remain `write()` and `read()`.

## Primary Use Case

Use `cfifo<T>` when:

- One producer-side message store should be shared by multiple readers.
- Each reader is identified by a numeric cursor ID.
- Every cursor must progress independently.
- Multiple cursors may read the same stored message.
- Message storage should not be duplicated per reader.
- Queue capacity and write credit must remain bounded and observable.

The intention is to provide a dedicated Cursor FIFO abstraction for one-to-many
message delivery: write each message once, retain it in one bounded shared
buffer, and let multiple registered cursors read it independently. Familiar C++
container naming is used only where it makes the API easier to understand.
`cfifo` is not intended as a drop-in replacement for `std::queue`, `std::deque`,
or another STL container, and STL interface compatibility is not a design goal.

## Status And Result Types

### Write status

```cpp
enum class CFIFOWriteStatus
{
    SUCCESS,
    Q_FULL,
    FAILED
};
```

| Status | Meaning |
|---|---|
| `SUCCESS` | The message was written. |
| `Q_FULL` | The bounded shared queue has no free slot. |
| `FAILED` | Reserved for a future failure mode. |

`write()` returns:

```cpp
struct CFIFOWriteResult
{
    CFIFOWriteStatus status;
    size_type credit;
};
```

`credit` reports the number of unused shared-buffer slots after the write
attempt.

### Read status

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
| `SUCCESS` | The next message for the cursor was returned. |
| `NO_PENDING_MESSAGE` | The cursor has caught up with the writer. |
| `NO_CURSOR` | The cursor ID was not registered. |
| `FAILED` | Reserved for a future failure mode. |

`read()` returns:

```cpp
struct CFIFOReadResult
{
    CFIFOReadStatus status;
    size_type pendingMessage;
};
```

`pendingMessage` reports how many messages remain unread by that cursor after a
successful read.

## Public API

### Constructor

```cpp
explicit cfifo(size_type capacity);
```

Constructs a bounded Cursor FIFO with the requested shared-buffer capacity. A
capacity of zero throws `std::invalid_argument`. The constructor is `explicit`,
so an integer cannot be implicitly converted into a `cfifo` object.

### Write a message

```cpp
CFIFOWriteResult write(const T& value);
```

Writes one message at the global tail when capacity is available.

```cpp
auto result = messages.write("hello");

if (result.status == CFIFOWriteStatus::SUCCESS)
{
    std::cout << "remaining credit: " << result.credit << '\n';
}
else if (result.status == CFIFOWriteStatus::Q_FULL)
{
    std::cout << "queue is full\n";
}
```

### Register a cursor

```cpp
bool add_cursor(cursor_type id);
```

Registers a cursor ID at the current global tail sequence:

```cpp
auto [it, inserted] = cursor_map_.try_emplace(id, tailSeq_);
```

Therefore, a newly registered cursor is initially caught up. It receives
messages written **after registration** and does not read messages that were
already retained before registration.

```cpp
messages.add_cursor(100);
messages.add_cursor(200);
```

`add_cursor()` returns `true` when a new cursor is registered. It returns
`false` when the cursor ID already exists, and the existing cursor position
remains unchanged because `try_emplace()` does not replace an existing mapped
value.

### Check cursor registration

```cpp
bool contains_cursor(cursor_type id) const;
```

Returns `true` when the cursor ID exists.

```cpp
if (!messages.contains_cursor(100))
{
    messages.add_cursor(100);
}
```

### Read for one cursor

```cpp
CFIFOReadResult read(cursor_type id, T& message);
```

Reads the next message for one registered cursor. Only that cursor advances.
Other cursors remain unchanged and can independently read the same shared
message.

```cpp
std::string message;
auto result = messages.read(100, message);

switch (result.status)
{
case CFIFOReadStatus::SUCCESS:
    std::cout << "message: " << message << '\n';
    std::cout << "pending: " << result.pendingMessage << '\n';
    break;

case CFIFOReadStatus::NO_PENDING_MESSAGE:
    std::cout << "cursor is caught up\n";
    break;

case CFIFOReadStatus::NO_CURSOR:
    std::cout << "register the cursor first\n";
    break;

case CFIFOReadStatus::FAILED:
    std::cout << "read failed\n";
    break;
}
```

### Inspect shared queue state

```cpp
bool empty() const;
bool full() const;
size_type credit() const;
size_type size() const;
size_type capacity() const;
```

These functions describe the **shared queue**, not an individual cursor:

- `empty()` reports whether the shared queue contains no retained messages.
- `full()` reports whether all shared-buffer slots are occupied.
- `credit()` reports unused shared-buffer slots.
- `size()` reports occupied shared-buffer slots.
- `capacity()` reports the configured maximum slot count.

Queue emptiness and cursor catch-up are different states. A queue may be
non-empty even when a particular cursor has no pending messages. Use the
`CFIFOReadResult` returned by `read()`—specifically
`CFIFOReadStatus::NO_PENDING_MESSAGE`—or a future cursor-specific pending API to
determine whether that cursor is caught up.

## Complete Example

```cpp
#include <cassert>
#include <iostream>
#include <string>

#include "cfifo.h"

int main()
{
    cfifo<std::string> queue(4);

    constexpr cfifo<std::string>::cursor_type alice = 1;
    constexpr cfifo<std::string>::cursor_type bob = 2;

    queue.add_cursor(alice);
    queue.add_cursor(bob);

    auto firstWrite = queue.write("first");
    auto secondWrite = queue.write("second");

    assert(firstWrite.status == CFIFOWriteStatus::SUCCESS);
    assert(secondWrite.status == CFIFOWriteStatus::SUCCESS);
    assert(queue.size() == 2);
    assert(queue.credit() == 2);

    std::string message;

    auto aliceFirst = queue.read(alice, message);
    assert(aliceFirst.status == CFIFOReadStatus::SUCCESS);
    assert(aliceFirst.pendingMessage == 1);
    assert(message == "first");

    auto aliceSecond = queue.read(alice, message);
    assert(aliceSecond.status == CFIFOReadStatus::SUCCESS);
    assert(aliceSecond.pendingMessage == 0);
    assert(message == "second");

    auto aliceCaughtUp = queue.read(alice, message);
    assert(aliceCaughtUp.status == CFIFOReadStatus::NO_PENDING_MESSAGE);

    // Bob still has an independent cursor and can read the same messages.
    auto bobFirst = queue.read(bob, message);
    assert(bobFirst.status == CFIFOReadStatus::SUCCESS);
    assert(message == "first");

    std::cout << "Cursor FIFO example passed.\n";
}
```

Compile from the repository root:

```bash
g++ -std=c++17 -Wall -Wextra -I. example.cpp -o example
```

## Message Lifetime And Current Limitations

Cursor reads currently do not reclaim shared-buffer slots. A message must
eventually remain available until every relevant subscriber has moved past it,
but that reclamation policy is not implemented yet.

Current consequences:

- Reading does not reduce the shared queue `size()`.
- Reading does not restore `credit()`.
- Once the queue becomes full, later writes return `Q_FULL`.
- Cursor removal is not available yet.
- Slow-subscriber and reclamation policies are not defined yet.
- Late cursors start at the current tail and receive only future writes.
- The class is single-threaded; concurrent calls require external
  synchronization.
- The sequence-number overflow policy is not defined yet.

The reclamation invariant for future work is:

> A message may be reclaimed only when no active subscriber cursor can still
> reference it.

## API Direction

Future work should strengthen Cursor FIFO semantics rather than move toward STL
conformance. The core data API remains:

```cpp
write(value);
read(cursor, message);
```

Expected future work includes cursor removal, shared-slot reclamation,
optional retained-history registration modes, sequence overflow handling, and
synchronization.
These changes must preserve the defining model: one shared message queue with
independent cursor progress and no per-reader message duplication.
