# niniBUS

`niniBUS` is a small C++ in-process message bus.

It stores string messages in numeric lanes. A caller can publish messages to a
lane, subscribe to a lane, and receive queued messages from a lane.

## API

The public API is declared in `niniBUS.h`:

```cpp
using lane_t = uint32_t;

bool publish(lane_t laneID, std::string message);
bool receive(lane_t laneID, std::string& message);
bool subscribe(lane_t laneID);
```

## How It Works

- A lane is created lazily when it is first published to or subscribed to.
- Each lane stores messages in a FIFO `std::deque<std::string>`.
- `publish()` appends a message to a lane.
- `receive()` removes the oldest message from a lane.
- `subscribe()` ensures that a lane exists.

Each lane has one queue, so multiple receivers on the same lane compete for
messages. A received message is removed and cannot be received again.

## Repository Layout

- `niniBUS.h` - public API and `Lane` structure.
- `niniBUS.cpp` - message bus implementation.
- `Makefile` - builds the `niniBUS` static library.
- `example/main.cpp` - small example program.
- `example/Makefile` - builds and runs the example.
- `DESIGN.md` - deeper design notes and limitations.

## Build The Library

From the repository root:

```bash
make all
```

This creates:

```bash
libniniBUS.a
```

Generated `.o` and `.d` files are removed automatically.

You can remove the library with:

```bash
make clean
```

## Build The Example

From the example folder:

```bash
cd example
make all
```

This creates:

```bash
example/niniBUS_example
```

Generated `.o` and `.d` files are removed automatically.

Run the example:

```bash
cd example
make run
```

Clean the example:

```bash
cd example
make clean
```

## Example Behavior

`example/main.cpp`:

1. Creates a `niniBUS` instance.
2. Subscribes to lanes `1` and `2`.
3. Publishes two messages to lane `1`.
4. Receives from lane `1`.
5. Attempts to receive from lane `2`.
6. Receives from lane `1` again.

Lane `1` has two queued messages, so both receives from lane `1` succeed.
Lane `2` exists but has no queued messages, so receiving from lane `2` returns
`false` and leaves the output string empty.

## Important Limitations

- The bus is not thread-safe.
- Lanes are allocated with `new` and are not deleted by the current destructor.
- The implementation uses raw `Lane*` pointers in an `unordered_map`.
- `subscribe()` only creates a lane; it does not track subscriber count.
- `receive()` returns `false` for both missing lanes and empty lanes.

See `DESIGN.md` for more detail.
