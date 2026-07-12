#include "niniBUS.h"

// Define static member

PublishResult niniBUS::publish(laneID_t laneID, const std::string& message)
{
    // Check if lane exists, if not create it
    auto [it, inserted] = lane_map_.try_emplace(laneID, lane_t());

    return it->second.push(message);
}
/* subscribe*/
bool niniBUS::subscribe(laneID_t laneID)
{
    // Check if the lane ID exists, if not create it
    lane_map_.try_emplace(laneID, lane_t());
    
    // Return true as long as the subscription is successful (whether new or existing)
    return true;
}

/* pull message is interesting */
ReceiveStatus niniBUS::receive(laneID_t laneID, std::string& message)
{
    message.clear();
    //check if the laneID is present
    auto it = lane_map_.find(laneID);
    if (it == lane_map_.end())
    {
        std::cerr << "Lane ID not found. Subscribing for future messages." << std::endl;
        if(subscribe(laneID))
        {
            return ReceiveStatus::LazyLaneCreated;
        }
        return ReceiveStatus::LaneNotFound;
    }

    // Get reference to the lane in the map and check if it has content
    lane_t& laneObj = it->second;
    return laneObj.pop(message);
}
