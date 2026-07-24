# niniBUS Test Report

The example directory contains two single-threaded, assert-based suites:

- `niniBUS_test.cpp` checks the public bus API.
- `cfifo_test.cpp` checks the cursor FIFO data structure directly.

## Run

```sh
cd example
make clean
make test
```

## niniBUS Coverage

The bus suite covers:

- default construction;
- explicit lane creation;
- invalid zero capacity;
- duplicate lane creation without replacement;
- deliberate publish-side lazy lane creation with `DEFAULT_LANE_CAPACITY`;
- publishing and receiving with sequence correlation;
- subscriber registration;
- subscription failure for missing lane;
- duplicate subscription without cursor reset;
- receive on missing lane;
- receive with missing subscriber;
- no pending message for a caught-up subscriber;
- multiple subscribers reading the same message;
- independent cursor progress;
- future-only delivery after subscription;
- bounded write-priority behavior;
- skipped-message reporting;
- unsubscribe success;
- unsubscribe of missing lane or subscriber;
- repeated unsubscribe;
- receive after unsubscribe;
- resubscribe beginning at the new tail.

These tests protect both topology policies: publish conveniently creates a
missing default-capacity lane, while subscribe fails and receive creates
neither lanes nor subscribers when topology is missing.

## cfifo Coverage

The data-structure suite covers:

- constructor state and invalid capacity;
- cursor registration, returned next-read position, duplicate idempotence, and
  containment;
- cursor removal and negative removal;
- read without registered cursor;
- cursor start at current tail;
- read with no pending message;
- independent reads by multiple cursors;
- write/read logical sequence IDs;
- pending-message counts;
- shared `empty`, `full`, `size`, `capacity`, and `credit`;
- wraparound;
- capacity-one operation;
- reclaim with no cursors;
- reclaim after all cursors catch up;
- reclaim with one slow cursor;
- tied slow cursors;
- minimum and maximum subscriber IDs;
- uneven cursor progress;
- skipped-message accumulation and reporting;
- repeated reclaim and wraparound;
- size and credit remaining bounded.

## Core Assertions

Tests should continue to enforce:

```text
queue.size() <= queue.capacity()
queue.credit() == queue.capacity() - queue.size()
```

For every successful read:

- output equals the message written at `sequenceId`;
- pending count is relative to that cursor;
- normal read advances no other cursor;
- skipped count reflects reclaim-driven movement since the previous successful
  read.

## Negative And Boundary Cases

Negative cases are first-class coverage, not optional cleanup:

- capacity zero;
- duplicate lane and cursor creation;
- unknown lane;
- unknown subscriber;
- empty/caught-up read;
- removal of unknown cursor;
- repeated unsubscribe;
- read after unsubscribe.

Boundary cases include capacity one, exact capacity, first reclaim, repeated
reclaim, physical wraparound, all cursors tied, and no registered cursor.

## Not Yet Covered

- Concurrent use or data races.
- Logical sequence rollover.
- Allocation failure.
- Exception behavior of copying a message.
- Very large capacities and cursor populations.
- Fuzz/model-based randomized operation sequences.
- Sanitizer runs as part of an automated CI pipeline.

Those gaps correspond to future milestones and must not be interpreted as
current guarantees.
