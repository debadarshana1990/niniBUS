#pragma once
#include <cstdint>

enum class PublishStatus {
    Ok,
    LaneFull
};
enum class ReceiveStatus {
    Ok,
    LaneEmpty,
    LazyLaneCreated
};
struct PublishResult 
{
    uint32_t Credit;
    PublishStatus Status;
};
struct ReceiveResult 
{
    uint32_t PendingMessages;
    ReceiveStatus Status;
};

enum class CreateLaneStatus
{
    ok,
    LaneExist,
    not_ok
};
