# niniBUS — quick README

Overview
--------
This small demo implements a basic in-process message bus (`niniBUS`) and a tiny producer/consumer example. The goal was to exercise threading and simple message passing using an existing `niniBUS` API without modifying the bus implementation itself.

Requirements implemented (brief)
- One producer and two consumers (both consumers read from the same `msgID`).
- Producer sends messages named `heelo1`, `heelo2`, ... at a 1s cadence and runs for 20 seconds.
- Consumers poll the bus at 2s cadence (2x slower than the producer) and run for 60 seconds.
- Logging is prefixed with `[producer]` and `[consumer]` for easy terminal reading.
- Build: `make all` keeps object/dependency meta files; `make niniBUS` builds and removes meta files leaving only the executable.

Files of interest
- `main.cpp` — example wiring (starts 2 consumers, then producer; joins appropriately).
- `producer.h` / `producer.cpp` — producer loop (20s, 1s interval), sends `heeloN` messages.
- `consumer.h` / `consumer.cpp` — consumer loops (60s, 2s interval) and prints received messages.
- `niniBUS.h` / `niniBUS.cpp` — message bus (left unchanged by your request).

How to build and run
1. Build and keep meta files:
```bash
make all
```
2. Build and remove meta files (keep only binary):
```bash
make niniBUS
```
3. Run the executable:
```bash
./niniBUS
```
The producer will print `[producer] heeloN` messages for ~20s. Consumers print `[consumer] Consumer X received: heeloN` when they successfully pull a message; they run for 60s total.

Notes & next steps
- The `niniBUS` implementation currently has no synchronization primitives (mutex/condition_variable). That makes concurrent push/pull usage timing-dependent and unsafe in a real multi-threaded environment. I intentionally left `niniBUS` unchanged per your request.

