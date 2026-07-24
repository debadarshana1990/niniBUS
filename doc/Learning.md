# niniBUS Learning Notes

These notes capture C++ and design lessons from the current cursor-broadcast
implementation.

## `explicit` Prevents Accidental Construction

A one-argument constructor can otherwise act as an implicit conversion:

```cpp
class Buffer
{
public:
    Buffer(uint32_t capacity);
};

void inspect(Buffer buffer);

Buffer a = 10;  // silently constructs Buffer(10)
inspect(20);    // silently constructs Buffer(20)
```

Capacity is configuration, not a value naturally convertible into a buffer.
Marking the constructor `explicit` makes that intent visible:

```cpp
explicit Buffer(uint32_t capacity);

Buffer a{10};
inspect(Buffer{20});
```

It prevents surprising temporary allocations, makes call sites readable, and
turns accidental conversions into compiler errors.

## Validate Before Member Allocation

C++ initializes members before entering the constructor body. This is too late
if invalid capacity must be rejected before storage allocation:

```cpp
cfifo(SizeType capacity) : content_(capacity)
{
    if (capacity == 0) { /* already initialized content_ */ }
}
```

Use a validation helper in the initializer expression so validation happens
before the vector is constructed.

## `try_emplace` Avoids Unwanted Default Construction

`map[key]` inserts a default-constructed mapped value when the key is absent.
That is useful only when insertion is intended and the value is default
constructible.

```cpp
auto [it, inserted] = cursor_map_.try_emplace(id, tail_sequence_);
```

This constructs cursor state only when the ID is new. Duplicate registration
leaves the existing cursor unchanged. For pure lookup, use `find()` or
`contains()`; neither mutates the map.

## Read Paths And Topology Mutation

“Receive does not mutate state” needs precision. A successful read advances
the requesting cursor—that is the purpose of the operation. The important
decision is that receive does not create topology:

- it does not insert a missing lane;
- it does not register a missing subscriber;
- an invalid ID remains visible as `NO_CURSOR`.

This prevents innocent-looking reads from growing maps.

## Shared Empty Is Not Cursor Empty

`cfifo::empty()` answers whether the shared retained range is empty. It does
not answer whether one cursor is caught up.

A queue can be non-empty while a fast cursor has no pending messages because a
slower cursor still needs retained data. Use the read result for cursor-specific
availability.

## Logical Sequence Versus Physical Index

A circular slot is reused, so it cannot be a permanent message identity.

```cpp
const SizeType read_index =
    static_cast<SizeType>(read_sequence % capacity_);
```

- `SequenceType` represents logical time/order.
- `SizeType` represents capacity, counts, and physical indexes.
- Modulo converts a logical sequence into a physical slot.

Explicit casts on bounded differences document the invariant:

```cpp
return static_cast<SizeType>(tail_sequence_ - read_sequence);
```

## Use Half-Open Ranges

The retained sequence range is `[head, tail)`. Therefore:

```text
size = tail - head
pending = tail - cursor.read
```

The next write receives sequence `tail`, then tail increments. The next read
uses the cursor's current sequence, then the cursor increments. Adding one to
these distances usually indicates mixed inclusive/exclusive reasoning.

## Reclaim Tied Minimum Cursors Together

When full, moving one slow cursor may release nothing if another cursor has
the same minimum sequence. Move every cursor tied at that minimum, then find
the new minimum and recompute:

```text
head = minimum remaining cursor read sequence
size = tail - head
```

Special cases:

- no cursors: head can move to tail;
- all cursors at tail: size becomes zero;
- some cursors behind: only the oldest tied group is forced forward.

## Writer Priority Must Be Observable

Always accepting writes is simple for publishers but can lose unread data.
The loss must not be silent. Each forced cursor records how many messages were
skipped, and a later successful read reports that count.

This makes the policy visible; it does not make it lossless.

## Result Types Beat Ambiguous Sentinels

Read needs to report several independent facts: status, pending count, message
sequence, and skipped count. Write reports sequence and credit. Structured
results keep these related values together and allow APIs to grow without
overloading one integer with several meanings.

Every failure path should initialize every result field deterministically.

## Duplicate Subscription Is Not A Reset

Registration should be idempotent. If the cursor exists, keep its current
position. Rewinding or fast-forwarding on repeated setup would cause duplicate
delivery or unexpected loss.

Reset, seek, and replay—if ever added—deserve explicit APIs.

## Unsubscribe Is More Than Erase

Removing a cursor changes future delivery and which cursor can constrain
retention. Negative cases matter:

- missing lane;
- missing subscriber;
- repeated unsubscribe;
- receive after unsubscribe;
- resubscribe, which starts at the then-current tail;
- removal of the slowest or final cursor.

## Rule Of Zero

The bus, lane, and cursor FIFO own standard-library values. Their members
already manage lifetime correctly, so custom empty destructors and hand-written
copy logic add risk without value.

## Tests Should Attack Invariants

Happy paths alone would not have found reclaim bugs. High-value tests repeatedly
assert:

```text
size <= capacity
credit == capacity - size
successful read sequence == corresponding write sequence
only the requesting cursor advances normally
missing receive targets do not become registered
```

Use capacity one, wraparound, tied slow cursors, uneven cursors, no cursors,
unsubscribe, and repeated reclaim cycles. Boundary tests are where circular
math stops being polite and starts telling the truth.
