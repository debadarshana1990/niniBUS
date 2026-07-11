#pragma once
#include <cstdint>

enum class PublishStatus {
    Ok,
    LaneNotFound,
    LaneFull
};
enum class ReceiveStatus {
    Ok,
    LaneNotFound,
    LaneEmpty,
    LazyLaneCreated
};
struct PublishResult 
{
    uint32_t Credit;
    PublishStatus Status;
};