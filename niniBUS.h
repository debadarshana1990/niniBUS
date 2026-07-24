#pragma once
#include <string>
#include <unordered_map>
#include "Lane.h"
#include "status.h"




class niniBUS
{
private:
    std::unordered_map<laneID_t, lane> lanes_; // map for lane id and lane object

public:
    PublishResult publish(laneID_t, const std::string& message);
    ReceiveResult receive(laneID_t laneID,subscribeID_t subscribeID ,std::string& message);
    CreateLaneStatus createLane(laneID_t laneID, uint32_t capacity);
    SubscribeResult subscribe(laneID_t laneID, subscribeID_t subscribeID);
    bool unsubscribe(laneID_t laneID, subscribeID_t subscribeID);

};
