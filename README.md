# niniBUS

`niniBUS` is a small C++ in-process message bus.

It stores string messages in numeric lanes. A caller can publish messages to a
lane, subscribe to a lane, and receive queued messages from a lane.

The current implementation is intentionally minimal and single-process. It is
best read as a learning/demo project for message queue behavior in C++.

## Current API

The public API is declared in `niniBUS.h`:

```cpp
bool publish(lane_t laneID, std::string message);
bool receive(lane_t laneID, std::string& message);
bool subscribe(lane_t laneID);
```

`lane_t` is an alias for `uint32_t`.

## How It Works

- A lane is created lazily the first time it is published to or subscribed to.
- Each lane stores messages in a FIFO `std::deque<std::string>`.
- `publish()` pushes a message to the back of the lane queue.
- `receive()` reads the oldest message from the front of the lane queue and
  removes it.
- If `receive()` is called for a missing lane, the bus creates the lane for
  future messages and returns `false`.
- If `receive()` is called for an empty lane, it returns `false`.

The bus currently behaves like a competing-consumer queue: each message can be
received once. It does not broadcast every message to every subscriber.

## Repository Layout

- `niniBUS.h` - public API and `Lane` data structure.
- `niniBUS.cpp` - implementation of publish, subscribe, and receive.
- `main.cpp` - small example program that creates lanes, publishes messages,
  and receives them.
- `DESIGN.md` - detailed design notes, current limitations, and recommended
  improvements.
- `Makefile` - build, run, debug, and clean targets.

## Build

Build the executable and keep intermediate `.o` and `.d` files:

```bash
make all
```

Or build the executable and then remove intermediate files:

```bash
make niniBUS
```

The executable is named `niniBUS`.

## Run

```bash
./niniBUS
```

You can also build and run in one step:

```bash
make run
```

## Example Behavior

`main.cpp` currently does the following:

1. Creates a `niniBUS` instance.
2. Subscribes to lanes `1` and `2`.
3. Publishes two messages to lane `1`.
4. Receives from lane `1`.
5. Attempts to receive from lane `2`.
6. Receives from lane `1` again.

Lane `1` has two queued messages, so both receives from lane `1` succeed.
Lane `2` exists but has no queued messages, so receiving from lane `2` returns
`false` and leaves the output string empty.

## Clean

Remove the executable and generated build files:

```bash
make clean
```

## Debug Build

Build with debug flags:

```bash
make debug
```

## Important Limitations

- The bus is not thread-safe.
- Lanes are allocated with `new` and are not deleted by the current destructor.
- `lanes_idx_` is static, which can cause invalid indexing if multiple
  `niniBUS` instances are used.
- `subscribe()` tracks `num_receivers` inconsistently, and that count does not
  affect message delivery.
- Public methods return `bool`, so callers get limited error details.

See `DESIGN.md` for a deeper explanation of these limitations and possible next
steps.

