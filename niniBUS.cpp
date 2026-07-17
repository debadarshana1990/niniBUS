#include "niniBUS.h"

// Define static member
CreateLaneStatus niniBUS::CreateLane(laneID_t laneID,uint32_t capacity)
{
    auto[it,inserted] = lane_map_.try_emplace(laneID, capacity);
    if(inserted)
        return CreateLaneStatus::ok;
    return CreateLaneStatus::LaneExist;
}

PublishResult niniBUS::publish(laneID_t laneID, const std::string& message)
{
    // Check if lane exists, if not create it 
    auto [it, _] = lane_map_.try_emplace(laneID);

    return it->second.push(message);
}

ReceiveResult niniBUS::receive(laneID_t laneID, std::string& message)
{
    message.clear();
    //check if the laneID is present
    auto [it, inserted] = lane_map_.try_emplace(laneID);
    if(inserted)
    {
        return { 0,ReceiveStatus::LazyLaneCreated };
    }

    // Get reference to the lane in the map and check if it has content
    lane_t& laneObj = it->second;
    return laneObj.pop(message);
}
