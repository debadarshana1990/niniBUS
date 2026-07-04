# niniBUS — Design summary

## Purpose

This document briefly explains the current implementation in this repository and the runtime test wiring added so far. It is intentionally short and focused on the existing code and runtime behaviour.

## High-level architecture

- `niniBUS` (core): an in-process message bus that stores messages by `msgID`.
  - Public API (used by the example): `push_msg(msg)`, `pull_msg(msg&)`, `subscribe(msgID)`.
  - Implementation lives in `niniBUS.h` / `niniBUS.cpp` and is unchanged by the example code modifications.
- Example wiring (producer / consumer): separate modules that exercise the bus concurrently.
  - `producer.{h,cpp}` — produces messages periodically and calls `push_msg`.
  - `consumer.{h,cpp}` — polls `pull_msg` periodically and prints received messages.
  - `main.cpp` — composes threads: starts consumers first, then the producer per the test scenario.

## Data shapes

- `msg` (C struct)
  - `uint32_t msgID`
  - `std::string content`

- `niniMSG` (internal)
  - `uint32_t msgID`
  - `std::deque<std::string> content`
  - `uint32_t num_receivers`

## Threading model (example)

- The example creates threads **outside** `niniBUS`:
  - 2 consumer threads: both subscribe to the same `msgID` and poll every 2s for up to 60 seconds.
  - 1 producer thread: writes `heelo1`, `heelo2`, ... every 1s and runs for 20 seconds.
- The example intentionally does not change `niniBUS`. As a result, `niniBUS` has no internal synchronization primitives (no mutex/condition_variable). The example uses non-blocking polling: consumers call `pull_msg` and sleep between polls.

## Behaviour observed in the test

- Producer pushes messages at 1s cadence for 20s. Consumers poll at 2s cadence for 60s.
- Because `niniBUS` does not provide synchronization, timing determines which consumer gets which message. The example demonstrates message flow but not safe concurrent semantics.

## Files (where to look)

- `niniBUS.h` / `niniBUS.cpp` — message bus implementation (core).
- `producer.h` / `producer.cpp` — producer loop, message naming (`heeloN`), durations.
- `consumer.h` / `consumer.cpp` — consumer loop, polling intervals and runtime duration.
- `main.cpp` — starts the threads and orchestrates the test.
- `Makefile` — build rules. Notable targets:
  - `make all` (or `make build`) — builds and keeps object (`.o`) and dependency (`.d`) files.
  - `make niniBUS` — builds and then removes `.o` and `.d`, leaving only the executable.

## Current limitations and risks

- Thread safety: `niniBUS` is not thread-safe. Concurrent `push_msg` / `pull_msg` can cause races or inconsistent behavior in real workloads.
- Non-blocking polling: consumers may miss messages if they poll slower than producers or if producers finish early.
- No logging structure: logs are simple console prints prefixed with `[producer]` / `[consumer]`.

## Suggested next steps

1. Make `niniBUS` thread-safe: add `std::mutex` to protect internal structures and a `std::condition_variable` to allow blocking `pull`.
2. Decide message distribution semantics: broadcast (all consumers get all messages) vs round-robin vs queue-per-consumer.
3. Add unit tests for `niniBUS` (happy path + concurrent push/pull).
4. Improve logging (timestamps, optional file logging, structured JSON logs).

## Quick run

Build and run the example:

```bash
make all
./niniBUS
```

The producer runs for 20s and prints `[producer] heeloN` messages; two consumers run for 60s and print `[consumer] Consumer X received: heeloN` when they pull messages.

---

This is a concise design doc capturing the current state. Ask if you want a deeper design (sequence diagrams, data-race analysis, or an RFC for thread-safety changes).

## Core implementation details (niniBUS)

This section documents the internal implementation of `niniBUS` as present in `niniBUS.h` and `niniBUS.cpp`.

Data structures
- `vector<niniMSG*> dataStruct` — dynamic array of pointers to `niniMSG` objects; each `niniMSG` holds a deque of message contents for a specific `msgID`.
- `static uint32_t dataStruct_idx` — monotonic index allocator used when creating new `niniMSG` entries.
- `unordered_map<uint32_t,uint32_t> dataStruct_map` — maps `msgID` to an index into `dataStruct`.

Core algorithms
- push_msg(msg message)
  - Lookup `msgID` in `dataStruct_map`.
  - If not present, allocate `new niniMSG(msgID)`, set `dataStruct_map[msgID] = dataStruct_idx++`, and `push_back` the pointer into `dataStruct`.
  - Append `message.content` to `dataStruct[idx]->content` (a `deque<string>`).
  - Returns true always (no failure modes currently).

- subscribe(uint32_t msgID)
  - If `msgID` not present in `dataStruct_map`, create a `niniMSG` object for it and increment `num_receivers` on the newly-created `niniMSG`.
  - Returns true always.

- pull_msg(msg& message)
  - Looks up `message.msgID` in `dataStruct_map`.
  - If not present: logs an error and calls `subscribe(message.msgID)` to create the slot for future messages, then returns false (no data now).
  - If present but the `niniMSG->content` deque is empty: logs "No content" and returns false.
  - Otherwise takes `front()` from deque, assigns it to `message.content`, pops it from the deque, and returns true.

Invariants and contracts
- The `dataStruct_map`->index points to a valid entry in `dataStruct` for all stored IDs.
- `dataStruct_idx` increases monotonically and is used as the insertion index when new `niniMSG` entries are appended.
- `push_msg` and `pull_msg` assume single-threaded access; no locking is present.

Complexity
- push_msg: average O(1) map lookup + amortized O(1) push_back + O(1) deque push_back.
- pull_msg: average O(1) map lookup + O(1) deque front/pop.

Error and edge cases
- No capacity or memory checks when creating new `niniMSG` — out-of-memory will throw.
- No bounds checking if `dataStruct_map` becomes inconsistent with `dataStruct` (e.g., if indices were removed); current code never removes entries.
- `pull_msg` mutates the internal deque; if multiple consumers call `pull_msg` concurrently for the same `msgID` data races will occur.

Recommended fixes (practical)
1. Add a `std::mutex` (e.g., `std::mutex mu;`) to `niniBUS` to guard `dataStruct`, `dataStruct_map`, and `dataStruct_idx` updates. Lock in `push_msg`, `subscribe`, and `pull_msg` for the minimal critical sections.
2. For blocking consumers: add a `std::condition_variable` per `niniMSG` or a global condition keyed by `msgID` so `pull_msg` can optionally wait until `content` is non-empty. Provide both `pull_msg_nonblocking` and `pull_msg_blocking(timeout)` variants.
3. Decide message distribution semantics:
   - Broadcast: store a counter of how many receivers still need to receive each message (use `num_receivers`). Consumers should pull copies until count reaches 0.
   - Round-robin / Work queue: keep a single deque per `msgID` and let consumers pop messages; this is the current behavior but requires locking.
4. Add unit tests (use multiple threads) to validate thread-safety and distribution semantics.

Security and robustness
- Avoid logging message contents in production; sanitize or redact sensitive strings.
- If high-throughput is required, consider lock-free or sharded data structures.

Notes
- The example code placed producer/consumer threads outside `niniBUS` for clarity. A production design may prefer that `niniBUS` provide subscription callbacks or a thread-safe queue abstraction to attach consumers.

