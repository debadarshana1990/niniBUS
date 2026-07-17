#pragma once
#include <string>
#include <unordered_map>
#include "Lane.h"
#include "status.h"

using laneID_t = uint32_t;

class niniBUS
{
private:
    std::unordered_map<laneID_t, lane_t> lane_map_; // map for lane id and lane object

public:
    PublishResult publish(laneID_t, const std::string& message);
    ReceiveResult receive(laneID_t, std::string& message);
    CreateLaneStatus createLane(laneID_t laneID, uint32_t capacity);

};
