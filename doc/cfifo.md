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

## STL-Inspired Design

`cfifo` is intended to feel familiar to users of the C++ Standard Library. It
provides container-style type aliases and inspection functions:

```cpp
using value_type = T;
using size_type = std::uint32_t;
using sequence_type = std::uint64_t;
using cursor_type = std::uint32_t;
```

```cpp
bool empty() const;
bool full() const;
size_type size() const;
size_type capacity() const;
```

It is currently **STL-inspired**, not a fully STL-conforming container. It does
not provide iterators, allocator support, standard container concepts, or the
usual `push_back()`/`front()`/`pop_front()` interface. Cursor-based reads have
different semantics from destructive standard FIFO operations.

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
    uint32_t credit;
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
    uint32_t PendingMessage;
};
```

`PendingMessage` reports how many messages remain unread by that cursor after a
successful read.

## Public API

### Constructor

```cpp
explicit cfifo(size_type capacity);
```

Constructs a bounded Cursor FIFO with the requested shared-buffer capacity.
The constructor is `explicit`, so an integer cannot be implicitly converted
into a `cfifo` object.

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

Registers a cursor ID. In the current implementation, a cursor starts at the
current global tail sequence:

```cpp
cursor_map_[id] = tailSeq_;
```

Therefore, the cursor receives messages written **after registration**. It does
not read messages that were already retained before it was registered.

```cpp
messages.add_cursor(100);
messages.add_cursor(200);
```

Calling `add_cursor()` with an existing ID currently resets that cursor to the
latest tail sequence and returns `true`. Callers should check
`contains_cursor()` first when resetting an existing cursor is not intended.

### Check cursor registration

```cpp
bool contains_cursor(cursor_type id);
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
    std::cout << "pending: " << result.PendingMessage << '\n';
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
uint32_t credit() const;
size_type size() const;
size_type capacity() const;
```

These functions describe the **shared queue**, not an individual cursor:

- `empty()` reports whether the shared queue contains no retained messages.
- `full()` reports whether all shared-buffer slots are occupied.
- `credit()` reports unused shared-buffer slots.
- `size()` reports occupied shared-buffer slots.
- `capacity()` reports the configured maximum slot count.

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
    assert(aliceFirst.PendingMessage == 1);
    assert(message == "first");

    auto aliceSecond = queue.read(alice, message);
    assert(aliceSecond.status == CFIFOReadStatus::SUCCESS);
    assert(aliceSecond.PendingMessage == 0);
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

### Constructor implementation note

The current header initializes `buffer_` with `capacity_` before `capacity_` is
initialized, because C++ initializes members in declaration order:

```cpp
buffer_(capacity_)
```

The backing vector should be initialized from the constructor parameter instead:

```cpp
buffer_(capacity)
```

This implementation issue should be corrected before relying on the runnable
examples above.

Cursor reads currently do not reclaim shared-buffer slots. A message must
eventually remain available until every relevant subscriber has moved past it,
but that reclamation policy is not implemented yet.

Current consequences:

- Reading does not reduce the shared queue `size()`.
- Reading does not restore `credit()`.
- Once the queue becomes full, later writes return `Q_FULL`.
- Cursor removal is not available yet.
- Slow-subscriber and reclamation policies are not defined yet.
- The class is single-threaded; concurrent calls require external
  synchronization.
- Copy and move overloads, iterators, allocators, and standard range support are
  not implemented yet.

The reclamation invariant for future work is:

> A message may be reclaimed only when no active subscriber cursor can still
> reference it.

## Possible STL-Style Refinements

Future API refinements may include:

- `push()` or `push_back()` naming in addition to `write()`.
- `emplace()` for in-place message construction.
- An rvalue overload such as `write(T&&)`.
- `max_size()` and allocator-aware construction.
- Standardized lowercase result member names such as `pending_messages`.
- `const` on `contains_cursor()`.
- Cursor registration that reports whether insertion actually occurred.
- Cursor removal and safe shared-slot reclamation.

These refinements should preserve the defining Cursor FIFO behavior: one shared
message queue with independent reader progress.
