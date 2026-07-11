# niniBUS Learning Notes

This file records small implementation lessons learned while building
`niniBUS`.

## `unordered_map::operator[]` And `Lane`

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
Lane& laneObj = it->second;
```

### What Happened

`lane_map_` is currently:

```cpp
std::unordered_map<uint32_t, Lane> lane_map_;
```

When code calls:

```cpp
lane_map_[laneID]
```

`std::unordered_map::operator[]` does two things:

1. If `laneID` already exists, it returns a reference to the existing `Lane`.
2. If `laneID` does not exist, it inserts a new value for that key first.

For a map value type of `Lane`, inserting through `operator[]` requires the map
to be able to default-construct a `Lane`.

That is why `Lane` keeps a default constructor:

```cpp
Lane(uint32_t cap = DEFAULT_LANE_CAPACITY);
```

Without a default constructor, `operator[]` cannot create a missing `Lane`, and
the compiler error appears inside standard-library template code. The error can
look noisy because it is reporting where `unordered_map::operator[]` was
instantiated, not only the exact user-level mistake.

`Lane` no longer stores `laneID`; the lane ID lives as the map key. The
constructor argument is capacity, not lane identity.

### Why `it->second` Is Different

This code:

```cpp
auto it = lane_map_.find(laneID);
Lane& laneObj = it->second;
```

uses an iterator returned by `find()`.

`it->second` accesses an existing map value. It does not create a new `Lane`.
Because it does not insert anything, it does not need `Lane` to be
default-constructible.

This is why `it->second` is a better fit after the code has already checked that
the lane exists.

### Current Code Pattern

Current `publish()` does this:

```cpp
auto it = lane_map_.find(laneID);
if (it != lane_map_.end())
{
    auto& laneobj = it->second;
    return laneobj.push(message);
}
```

This keeps the bus code boring: after lookup, it delegates to `Lane::push()`.
The lane owns the capacity check, queue mutation, credit calculation, and
publish status.

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
auto [it, inserted] = lane_map_.emplace(laneID, Lane());
return it->second.push(message);
```

This avoids direct access to lane internals. If `Lane::push()` changes later,
the bus code can stay mostly unchanged.

### Key Lesson

Use `operator[]` only when default-inserting a missing value is intentional.

Use `find()` plus `it->second` when the code only wants to access an existing
value.

Use `emplace()` or `try_emplace()` when the code wants to create a value with a
specific constructor.

Keep queue behavior inside `Lane::push()` and `Lane::pop()`. `niniBUS` should
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
error: redefinition of class Lane
```

### Example Scenario

If you have:

- `status.h` - defines `PublishStatus`, `ReceiveStatus`, `PublishResult`
- `Lane.h` - includes `status.h` and defines `Lane`
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
Lane& laneObj = it->second;
return laneObj.pop(message);
```

### Why The Reference Matters

`it->second` is the `Lane` object stored inside `lane_map_`.

Using `auto&` or `Lane&` creates a reference to that stored object. This means
`push()` and `pop()` mutate the real lane inside the map.

If the code used `auto` without `&`:

```cpp
auto laneobj = it->second;
laneobj.push(message);
```

then `laneobj` would be a copy of the stored `Lane`. The push would happen on
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
