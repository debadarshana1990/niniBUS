#pragma once
#include <iostream>
#include <string>
#include <unordered_map>
#include "Lane.h"
#include "status.h"

using laneID_t = uint32_t;

class niniBUS
{
private:
    std::unordered_map<laneID_t, lane_t> lane_map_; // map for msg iD, its idx

public:
    PublishResult publish(laneID_t, const std::string& message);
    ReceiveStatus receive(laneID_t, std::string& message);

};
