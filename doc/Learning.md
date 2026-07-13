# niniBUS Learning Notes

This file records small implementation lessons learned while building
`niniBUS`.

## `unordered_map::operator[]` And `lane_t`

### Error / Compiler Note

The compiler reported a note like:

```text
niniBUS.cpp:12:18: note: in instantiation of member function
'std::unordered_map<unsigned int, Lane>::operator[]' requested here
```

The line involved was:

```cpp
lane_map_[laneID].push(message);
```

Related receive code used:

```cpp
lane_t& laneObj = it->second;
```

### What Happened

`lane_map_` is currently:

```cpp
std::unordered_map<laneID_t, lane_t> lane_map_;
```

When code calls:

```cpp
lane_map_[laneID]
```

`std::unordered_map::operator[]` does two things:

1. If `laneID` already exists, it returns a reference to the existing `lane_t`.
2. If `laneID` does not exist, it inserts a new value for that key first.

For a map value type of `lane_t`, inserting through `operator[]` requires the
map to be able to default-construct a `lane_t`.

That is why `lane_t` keeps a default constructor:

```cpp
lane_t() = default;
```

Without a default constructor, `operator[]` cannot create a missing `lane_t`,
and the compiler error appears inside standard-library template code. The error
can look noisy because it is reporting where `unordered_map::operator[]` was
instantiated, not only the exact user-level mistake.

`lane_t` no longer stores `laneID`; the lane ID lives as the map key. The
capacity now comes from the FIFO member. `niniFIFO` owns the default value and
stores the active capacity in its runtime `capacity_` member.

### Why `it->second` Is Different

This code:

```cpp
auto it = lane_map_.find(laneID);
lane_t& laneObj = it->second;
```

uses an iterator returned by `find()`.

`it->second` accesses an existing map value. It does not create a new `lane_t`.
Because it does not insert anything, it does not need `lane_t` to be
default-constructible.

This is why `it->second` is a better fit after the code has already checked that
the lane exists.

### Current Code Pattern

Current `publish()` does this:

```cpp
auto [it, inserted] = lane_map_.try_emplace(laneID);
return it->second.push(message);
```

This keeps the bus code boring: find or create the lane, then delegate to
`lane_t::push()`. The lane owns the capacity check, queue mutation, credit
calculation, and publish status.

`try_emplace()` returns an iterator to the lane either way:

1. If the lane already exists, the iterator points to the existing lane.
2. If the lane does not exist, the map default-constructs it in place and
   returns an iterator to the new lane.

The `inserted` flag says whether a new lane was created. `publish()` does not
currently need that flag, but it is available if later code wants to treat a
new lane differently.

### Why `try_emplace()` Is Better Here

The older code did this:

```cpp
auto it = lane_map_.find(laneID);
if (it != lane_map_.end())
{
    auto& laneobj = it->second;
    return laneobj.push(message);
}

lane_t newLane;
lane_map_[laneID] = newLane;
return lane_map_[laneID].push(message);
```

That has two problems.

First, it can create a double lookup. `find(laneID)` searches the map once. If
the lane is missing, `lane_map_[laneID]` searches again to insert or access the
entry. If the code then uses `lane_map_[laneID]` again to push, that is another
map access. The code already knows which key it wants, but it keeps asking the
map to look it up again.

Second, `operator[]` default-inserts a value when the key is missing, then the
assignment writes `newLane` into that value. That means the code may construct a
temporary lane and also default-construct the lane inside the map before
assignment. It works, but it is more work than the operation needs.

`try_emplace()` expresses the real intent directly:

```cpp
auto [it, inserted] = lane_map_.try_emplace(laneID);
return it->second.push(message);
```

It looks for the key once, creates the lane only if it is missing, and gives the
code an iterator to the stored lane. Because no constructor arguments are passed
after `laneID`, the map uses `lane_t`'s default constructor directly in the map.
After that, `it->second.push(message)` mutates the actual lane inside the map.

The code could also be written directly as:

```cpp
return it->second.push(message);
```

That avoids calling `operator[]` after `find()`.

### Safer Pattern

When the lane already exists:

```cpp
auto it = lane_map_.find(laneID);
if (it != lane_map_.end())
{
    return it->second.push(message);
}
```

When creating a lane:

```cpp
auto [it, inserted] = lane_map_.try_emplace(laneID);
return it->second.push(message);
```

This avoids direct access to lane internals. If `lane_t::push()` changes later,
the bus code can stay mostly unchanged.

### Key Lesson

Use `operator[]` only when default-inserting a missing value is intentional.

Use `find()` plus `it->second` when the code only wants to access an existing
value.

Use `try_emplace()` when the code wants to get an existing value or create a
missing value in one map operation.

Keep queue behavior inside `lane_t::push()` and `lane_t::pop()`. `niniBUS` should
only find/create the lane and delegate.

## `#pragma once` - Header Include Guard

### What Is It?

`#pragma once` is a preprocessor directive that tells the compiler to include a
header file only once during compilation, even if multiple source files try to
include it.

### Problem It Solves

Without `#pragma once`, the same header can be processed multiple times,
resulting in compiler errors like:

```text
error: redefinition of class lane_t
```

### Example Scenario

If you have:

- `status.h` - defines `PublishStatus`, `ReceiveStatus`, `PublishResult`
- `Lane.h` - includes `status.h` and defines `lane_t`
- `niniBUS.h` - includes both `Lane.h` and `status.h`
- `niniBUS.cpp` - includes `niniBUS.h`

Then `status.h` gets included twice:
1. Via `niniBUS.h` → `Lane.h` → `status.h`
2. Via `niniBUS.h` → `status.h`

With `#pragma once`, the second inclusion is automatically skipped.

### Usage

Place at the very top of every header file:

```cpp
#pragma once

// Rest of header file
```

### Alternative (Older Method)

Before `#pragma once` was widely supported, include guards were used:

```cpp
#ifndef LANE_H_INCLUDED
#define LANE_H_INCLUDED

// Header content here

#endif  // LANE_H_INCLUDED
```

This still works, but `#pragma once` is simpler and more readable.

### Key Lesson

Always add `#pragma once` to the top of header files to prevent multiple
inclusion errors. It's a modern C++ best practice and supported by all major
compilers.

## `auto& laneobj = it->second` - Reference Versus Copy

### Code

Current `publish()` gets an iterator from `try_emplace()` and mutates the stored
lane through `it->second`:

```cpp
auto [it, _] = lane_map_.try_emplace(laneID);
return it->second.push(message);
```

Current `receive()` uses the same idea:

```cpp
auto [it, inserted] = lane_map_.try_emplace(laneID);
if (inserted)
{
    return ReceiveStatus::LazyLaneCreated;
}
return it->second.pop(message);
```

### Why The Reference Matters

`it->second` is the `lane_t` object stored inside `lane_map_`.

Using `it->second`, `auto&`, or `lane_t&` accesses the stored object. This means
`push()` and `pop()` mutate the real lane inside the map.

If the code used `auto` without `&`:

```cpp
auto laneobj = it->second;
laneobj.push(message);
```

then `laneobj` would be a copy of the stored `lane_t`. The push would happen on
the copied lane, not on the lane inside `lane_map_`. After the function returns,
the copy would be destroyed and the bus would lose that change.

For `receive()`, the same copy bug would be even more confusing: `pop()` would
remove a message from the copied lane, while the original lane in the map would
still contain the message.

### Key Lesson

Use a reference when mutating a map value:

```cpp
auto& laneobj = it->second;
```

Use a copy only when you intentionally want a separate snapshot of the value.

## Removed Empty Destructors

### What Changed

The explicit destructors were removed from `niniBUS` and `lane_t`.

Earlier code had a `niniBUS` destructor that only contained commented-out
messages, and `lane_t` had an empty destructor:

```cpp
~lane_t() {}
```

At the moment, those destructors are not required.

### Why They Are Not Needed

`niniBUS` stores lanes by value in an `std::unordered_map`:

```cpp
std::unordered_map<laneID_t, lane_t> lane_map_;
```

Each `lane_t` stores messages in value-type members such as
`niniFIFO<std::string>`, which contains an `std::vector<std::string>`.

These standard-library members clean themselves up automatically when their
owning object is destroyed. That means the compiler-generated destructor already
does the correct thing:

1. Destroy the `niniBUS` object.
2. Destroy `lane_map_`.
3. Destroy each stored `lane_t`.
4. Destroy each lane's FIFO, vector, and strings.

There is no raw `new`/`delete`, file handle, socket, thread, mutex handle, or
other manual resource that needs custom destructor code today.

### Why Removing Them Is Better

An empty destructor adds code without adding behavior. A destructor that only
prints messages also makes object destruction noisy, which is not a good default
for a library.

Removing those destructors follows the C++ rule of zero: if a class does not
directly manage a resource, let the compiler generate the destructor, copy, and
move behavior.

### Key Lesson

Only write a destructor when the class has real cleanup work to do.

For the current bus design, standard-library containers already own the cleanup,
so explicit destructors and copy constructors can stay removed until a future
feature introduces a resource that needs manual lifetime management.

## Compilation Stages For `try_emplace()` And `niniFIFO`

### Stage 1 - `try_emplace(laneID)` Needed A Default-Constructible Lane

The code changed lane creation to:

```cpp
auto [it, inserted] = lane_map_.try_emplace(laneID);
```

This asks `std::unordered_map` to create the mapped value with no constructor
arguments when `laneID` is missing. Because the mapped value is `lane_t`, this
requires:

```cpp
lane_t() = default;
```

The compiler error appeared deep inside libc++ `unordered_map` / `__hash_table`
code, near a note like:

```text
note: in instantiation of function template specialization
'std::unordered_map<unsigned int, lane_t>::try_emplace<>' requested here
```

The useful clue was the call site:

```text
niniBUS.cpp:8:37: note: ... try_emplace(laneID)
```

### Fix

Make sure `lane_t` can be default-constructed. The current version does that:

```cpp
class lane_t
{
    niniFIFO<std::string> content;

public:
    lane_t() = default;
};
```

This lets `try_emplace(laneID)` default-construct the lane directly inside the
map.

### Stage 2 - `std::array<T, capacity>` Cannot Use A Runtime Member

During the FIFO work, the compiler reported:

```text
error: invalid use of non-static data member 'capacity'
```

The problematic idea was using a runtime data member as the size of
`std::array`:

```cpp
std::array<T, capacity> buffer;
```

`std::array` needs its size as a compile-time constant. A normal member variable
such as `capacity` is only known at runtime, so it cannot be used as the array
size.

### Intermediate Fix

Make FIFO capacity a non-type template parameter:

```cpp
template <typename T, uint32_t CAPACITY>
class niniFIFO_t
{
    std::array<T, CAPACITY> buffer;
};
```

That made `lane_t` choose the capacity at compile time:

```cpp
niniFIFO_t<std::string, DEFAULT_LANE_CAPACITY> content;
```

That version compiled further, but it made capacity part of the type. The
current design moved away from that because capacity should be runtime FIFO
state.

### Current Fix

The current design uses `std::vector` and a runtime `capacity_` member instead:

```cpp
template <typename T>
class niniFIFO
{
    std::vector<T> buffer_;
    uint32_t capacity_;
    uint32_t head_;
    uint32_t tail_;
    uint32_t currSize_;
};
```

`std::vector` still stores elements contiguously, like `std::array`, but it does
not require capacity to be a template argument. That makes it a better fit for a
FIFO that may later support runtime capacity changes.

`DEFAULT_LANE_CAPACITY` moved into `niniFIFO.h` because the default queue depth
belongs to the FIFO, not to lane identity or bus routing.

### Stage 3 - Template Methods Stay In The Header

The next compiler errors looked like:

```text
error: use of class template 'niniFIFO_t' requires template arguments
error: unknown type name 'T'
```

This happened because template method definitions were written in
`niniFIFO.cpp` as if `niniFIFO_t` and `T` were ordinary concrete names.

For templates, the compiler must see the full method definitions when it
instantiates a concrete type such as:

```cpp
niniFIFO_t<std::string, DEFAULT_LANE_CAPACITY>
```

### Fix

Keep the `niniFIFO` method definitions in `niniFIFO.h`:

```cpp
template <typename T>
class niniFIFO
{
public:
    FIFOStatus push_back(const T& message)
    {
        // implementation in header
    }
};
```

The current build no longer compiles `niniFIFO.cpp`; the FIFO template is
header-only.

### Stage 4 - State Naming Must Match The Constructor

Another compile error was:

```text
error: member initializer 'count' does not name a non-static data member
```

That happened after the FIFO state member was renamed. The constructor still
initialized `count`, but the class member was no longer named `count`.

### Fix

Use one state name consistently:

```cpp
uint32_t currSize;

niniFIFO() : capacity_(DEFAULT_LANE_CAPACITY), head_(0), tail_(0), currSize_(0)
{
    buffer_.resize(capacity_);
}
```

### Stage 5 - `receive()` Can Use `try_emplace()` Too

Older receive code used `find()` first, then called `subscribe()` to create a
missing lane. That split lane creation across two functions.

Current `receive()` uses:

```cpp
auto [it, inserted] = lane_map_.try_emplace(laneID);
if (inserted)
{
    return ReceiveStatus::LazyLaneCreated;
}

lane_t& laneObj = it->second;
return laneObj.pop(message);
```

This is better because:

1. The map is queried once.
2. The returned iterator is reused.
3. The `inserted` flag directly tells whether lazy lane creation happened.
4. A separate `subscribe()` function is not needed unless the project later
   tracks real subscribers.

### Stage 6 - FIFO Mutations Return Status

`push_back()` and `pop_front()` both mutate FIFO state, so both now return
`FIFOStatus`:

```cpp
FIFOStatus push_back(const T& message);
FIFOStatus pop_front();
```

This keeps FIFO mutation APIs aligned: callers can check `SUCCESS`, `FULL`, or
`EMPTY` instead of relying on one operation returning status and another using a
different failure style.

`front()` is kept as a separate STL-style accessor. That mirrors the common
container pattern: read the front element with `front()`, then remove it with
`pop_front()`.

### Current Successful Build Stages

The current root build now completes these stages:

```text
g++ ... -c niniBUS.cpp -o niniBUS.o
g++ ... -c Lane.cpp -o Lane.o
ar rcs libniniBUS.a niniBUS.o Lane.o
rm -f niniBUS.o Lane.o niniBUS.d Lane.d
```

### Key Lessons

`try_emplace(laneID)` default-constructs the map value when the key is missing,
so the mapped type must have a usable default constructor.

`std::array<T, N>` needs `N` to be known at compile time. Use `std::vector`
when capacity needs to live in runtime state.

Template class method definitions usually belong in the header unless explicit
template instantiation is being used deliberately.

Use the rule of zero when classes only own standard-library members. Avoid
empty destructors and defaulted copy constructors unless there is a real reason
to declare them.
