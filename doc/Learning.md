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
lane_map_[laneID].content.push_back(message);
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

That is why `Lane` currently has:

```cpp
Lane() : laneID(0) {}
```

Without a default constructor, `operator[]` cannot create a missing `Lane`, and
the compiler error appears inside standard-library template code. The error can
look noisy because it is reporting where `unordered_map::operator[]` was
instantiated, not only the exact user-level mistake.

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
    lane_map_[laneID].content.push_back(message);
    return PublishResult::Ok;
}
```

This works, but it performs an unnecessary second map lookup. The code already
has the iterator, so it can use:

```cpp
it->second.content.push_back(message);
```

That avoids calling `operator[]` after `find()`.

### Safer Pattern

When the lane already exists:

```cpp
auto it = lane_map_.find(laneID);
if (it != lane_map_.end())
{
    it->second.content.push_back(message);
    return PublishResult::Ok;
}
```

When creating a lane:

```cpp
auto [it, inserted] = lane_map_.emplace(laneID, Lane(laneID));
it->second.content.push_back(message);
```

This avoids default-constructing a placeholder `Lane` with ID `0` and then
assigning over it.

### Key Lesson

Use `operator[]` only when default-inserting a missing value is intentional.

Use `find()` plus `it->second` when the code only wants to access an existing
value.

Use `emplace()` or `try_emplace()` when the code wants to create a value with a
specific constructor.

