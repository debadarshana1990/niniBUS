#include "niniBUS.h"

// Define static member

PublishResult niniBUS::publish(lane_t laneID, std::string message)
{
    ///check if present in the map
    auto it = lane_map_.find(laneID);
    if(it != lane_map_.end())
    {
        // Lane exists, push message directly to the map entry
        lane_map_[laneID].content.push_back(message);
        return PublishResult::Ok;
    }
    //if its not present need to create the object and push the message
    Lane newLane(laneID);
    lane_map_[laneID] = newLane;
    lane_map_[laneID].content.push_back(message);
    return PublishResult::Ok;
}
/* subscribe*/
bool niniBUS::subscribe(lane_t laneID)
{
    // Check if the lane ID exists
    auto it = lane_map_.find(laneID);
    if (it == lane_map_.end())
    {
        Lane newLane(laneID);
        lane_map_[laneID] = newLane;
    }

    return true;
}

/* pull message is interesting */
ReceiveResult niniBUS::receive(lane_t laneID, std::string& message)
{
    message.clear();
    //check if the laneID is present
    auto it = lane_map_.find(laneID);
    if (it == lane_map_.end())
    {
        std::cerr << "Lane ID not found. Subscribing for future messages." << std::endl;
        subscribe(laneID);
        return ReceiveResult::LazyLaneCreated;
    }

    // Get reference to the lane in the map and check if it has content
    Lane& laneObj = it->second;
    if (laneObj.content.empty())
    {
        std::cerr << "No content available for lane ID " << laneID << std::endl;
        return ReceiveResult::LaneEmpty;
    }

    // Pull the latest content
    message = laneObj.content.front();
    laneObj.content.pop_front();

    return ReceiveResult::Ok;
}
