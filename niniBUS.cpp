#include "niniBUS.h"

// Define static member

PublishResult niniBUS::publish(lane_t laneID, const std::string& message)
{
    ///check if present in the map
    auto it = lane_map_.find(laneID);
    if(it != lane_map_.end())
    {
        auto& laneobj = it->second;
        return laneobj.push(message);
    }
    //if its not present need to create the object and push the message
    Lane newLane;
    lane_map_[laneID] = newLane;
    return lane_map_[laneID].push(message);
}
/* subscribe*/
bool niniBUS::subscribe(lane_t laneID)
{
    // Check if the lane ID exists
    auto it = lane_map_.find(laneID);
    if (it == lane_map_.end())
    {
        Lane newLane;
        lane_map_[laneID] = newLane;
    }

    return true;
}

/* pull message is interesting */
ReceiveStatus niniBUS::receive(lane_t laneID, std::string& message)
{
    message.clear();
    //check if the laneID is present
    auto it = lane_map_.find(laneID);
    if (it == lane_map_.end())
    {
        std::cerr << "Lane ID not found. Subscribing for future messages." << std::endl;
        subscribe(laneID);
        return ReceiveStatus::LazyLaneCreated;
    }

    // Get reference to the lane in the map and check if it has content
    Lane& laneObj = it->second;
    return laneObj.pop(message);
}
