#include "niniBUS.h"

// Define static member

bool niniBUS::publish(lane_t laneID, std::string message)
{
    ///check if present in the map
    auto it = lane_map_.find(laneID);
    if(it != lane_map_.end())
    {
        //get the laneid
        Lane* laneObj = it->second;
        laneObj->content.push_back(message);
        return true;
    }
    //if its not present need to create the object and push the message
    Lane* newLane = new Lane(laneID);
    lane_map_[laneID] = newLane;
    newLane->content.push_back(message);
    return true;
}
/* subscribe*/
bool niniBUS::subscribe(lane_t laneID)
{
    // Check if the lane ID exists
    auto it = lane_map_.find(laneID);
    if (it == lane_map_.end())
    {
        Lane *newLane = new Lane(laneID);
        lane_map_[laneID] = newLane;
    }

    return true;
}

/* pull message is interesting */
bool niniBUS::receive(lane_t laneID, std::string& message)
{
    message.clear();
    //check if the laneID is present
    auto it = lane_map_.find(laneID);
    if (it == lane_map_.end())
    {
        std::cerr << "Lane ID not found. Subscribing for future messages." << std::endl;
        subscribe(laneID);
        return false;
    }

    // Get the message object
    Lane* laneObj = it->second;
    if (laneObj->content.empty())
    {
        std::cerr << "No content available for lane ID " << laneID << std::endl;
        return false;
    }

    // Pull the latest content
    message = laneObj->content.front();
    laneObj->content.pop_front();

    return true;
}
