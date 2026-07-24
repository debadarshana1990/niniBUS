#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "Lane.h"
#include "status.h"

class niniBUS
{
public:
    PublishResult publish(
        laneID_t laneID,
        const std::string& message);
    ReceiveResult receive(
        laneID_t laneID,
        subscriberID_t subscriberID,
        std::string& message);
    CreateLaneStatus createLane(
        laneID_t laneID,
        std::uint32_t capacity);
    SubscribeResult subscribe(
        laneID_t laneID,
        subscriberID_t subscriberID);
    bool unsubscribe(
        laneID_t laneID,
        subscriberID_t subscriberID);

private:
    std::unordered_map<laneID_t, Lane> lanes_;
};
