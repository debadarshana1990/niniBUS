#pragma once
#include <cstdint>
#include "cfifo.h"


using laneID_t = uint32_t;
using subscribeID_t = std::uint32_t;
using sequenceId_t = std::uint64_t;

enum class ReceiveStatus {
    SUCCESS,
    NO_PENDING_MESSAGE,
    NO_CURSOR
};
struct PublishResult
{
    uint32_t Credit;
    sequenceId_t sequenceID;
};
struct ReceiveResult 
{
    ReceiveStatus Status;
    uint32_t PendingMessages;
    sequenceId_t sequenceID;
    uint64_t SkippedMessages;
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
    LaneNotExist,
};
struct SubscribeResult
{
    SubscribeStatus status;
    sequenceId_t startSequenceId;};
