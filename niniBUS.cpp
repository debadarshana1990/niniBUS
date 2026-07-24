#include "niniBUS.h"

CreateLaneStatus niniBUS::createLane(laneID_t laneID, uint32_t capacity)
{
    if (capacity == 0)
    {
        return CreateLaneStatus::InvalidCapacity;
    }

    auto [it, inserted] = lanes_.try_emplace(laneID, capacity);
    return inserted ? CreateLaneStatus::Ok : CreateLaneStatus::LaneExists;
}

PublishResult niniBUS::publish(laneID_t laneID, const std::string& message)
{
    // Check if lane exists, if not create it 
    auto [it, _] = lanes_.try_emplace(laneID);

    return it->second.push(message);
}

SubscribeResult niniBUS::subscribe(laneID_t laneID,subscribeID_t subscribeID)
{
    //check if lane Exist ,if not return lane not exist
    auto it = lanes_.find(laneID);
    if(it == lanes_.end())
        return {SubscribeStatus::LaneNotExist,0};
    auto result = it->second.subscribe(subscribeID);
    return result;
}




ReceiveResult niniBUS::receive(laneID_t laneID,subscribeID_t subscribeID ,std::string& message)
{
    message.clear();
    //No lazy lane creation any more. Receiver should not mutate anything 
    
    auto it = lanes_.find(laneID);
    if(it == lanes_.end())
    {
        return { ReceiveStatus::NO_CURSOR,0,0,0 };
    }
    // Get reference to the lane in the map and check if it has content
    lane& laneObj = it->second;
    return laneObj.pop(subscribeID,message);
}
bool niniBUS::unsubscribe(laneID_t laneID, subscribeID_t subscribeID)
{
    auto it = lanes_.find(laneID);
    if(it == lanes_.end())
        return false;
    lane& laneObj = it->second;
    return laneObj.unsubscribe(subscribeID);
}