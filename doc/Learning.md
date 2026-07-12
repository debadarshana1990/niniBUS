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
lane_t(uint32_t cap = DEFAULT_LANE_CAPACITY);
```

Without a default constructor, `operator[]` cannot create a missing `lane_t`,
and the compiler error appears inside standard-library template code. The error
can look noisy because it is reporting where `unordered_map::operator[]` was
instantiated, not only the exact user-level mistake.

`lane_t` no longer stores `laneID`; the lane ID lives as the map key. The
constructor argument is capacity, not lane identity.

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
auto [it, inserted] = lane_map_.try_emplace(laneID, lane_t());
return it->second.push(message);
```

This keeps the bus code boring: find or create the lane, then delegate to
`lane_t::push()`. The lane owns the capacity check, queue mutation, credit
calculation, and publish status.

`try_emplace()` returns an iterator to the lane either way:

1. If the lane already exists, the iterator points to the existing lane.
2. If the lane does not exist, the map creates it and returns an iterator to the
   new lane.

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
auto [it, inserted] = lane_map_.try_emplace(laneID, lane_t());
return it->second.push(message);
```

It looks for the key once, creates the lane only if it is missing, and gives the
code an iterator to the stored lane. After that, `it->second.push(message)`
mutates the actual lane inside the map.

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
auto [it, inserted] = lane_map_.try_emplace(laneID, lane_t());
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

Current `publish()` uses:

```cpp
auto& laneobj = it->second;
return laneobj.push(message);
```

Current `receive()` uses the same idea:

```cpp
lane_t& laneObj = it->second;
return laneObj.pop(message);
```

### Why The Reference Matters

`it->second` is the `lane_t` object stored inside `lane_map_`.

Using `auto&` or `lane_t&` creates a reference to that stored object. This means
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

Each `lane_t` stores messages in standard-library objects such as
`std::deque<std::string>`.

These standard-library members clean themselves up automatically when their
owning object is destroyed. That means the compiler-generated destructor already
does the correct thing:

1. Destroy the `niniBUS` object.
2. Destroy `lane_map_`.
3. Destroy each stored `lane_t`.
4. Destroy each lane's queue and strings.

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
so the explicit destructors can stay removed until a future feature introduces a
resource that needs manual lifetime management.
