#include "niniBUS.h"

// Define static member
uint32_t niniBUS::lanes_idx_ = 0;

bool niniBUS::publish(lane_t laneID, std::string message)
{
    ///check if present in the map
    auto it = lane_map_.find(laneID);
    if(it == lane_map_.end())
    {
        //we are processing a new LaneID. this is costly
        //need to create a new object for this LaneID
        Lane *newLane = new Lane(laneID);
        lane_map_[laneID] = lanes_idx_++;
        lanes_.push_back(newLane); //everything  is here
    }

        //we have already created the LaneID object. just push the content
    uint32_t idx = lane_map_[laneID];
    lanes_[idx]->content.push_back(message);

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
        lane_map_[laneID] = lanes_idx_++;
        lanes_.push_back(newLane); //everything  is here
        newLane->num_receivers++;
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
    Lane* laneObj = lanes_[it->second];
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