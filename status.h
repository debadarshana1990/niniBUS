#pragma once

#include <cstdint>

using laneID_t = std::uint32_t;
using subscriberID_t = std::uint32_t;
using sequenceId_t = std::uint64_t;

enum class ReceiveStatus
{
    SUCCESS,
    NO_PENDING_MESSAGE,
    NO_CURSOR
};

struct PublishResult
{
    std::uint32_t credit;
    sequenceId_t sequenceId;
};

struct ReceiveResult
{
    ReceiveStatus status;
    std::uint32_t pendingMessages;
    sequenceId_t sequenceId;
    std::uint64_t skippedMessages;
};

enum class CreateLaneStatus
{
    Ok,
    LaneExists,
    InvalidCapacity
};
enum class SubscribeStatus
{
    Ok,
    LaneNotExist
};

struct SubscribeResult
{
    SubscribeStatus status;
    sequenceId_t nextSequenceId;
};
