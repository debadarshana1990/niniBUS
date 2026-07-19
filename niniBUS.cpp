#include "niniBUS.h"

CreateLaneStatus niniBUS::createLane(laneID_t laneID, uint32_t capacity)
{
    if (capacity == 0)
    {
        return CreateLaneStatus::InvalidCapacity;
    }

    auto [it, inserted] = lane_map_.try_emplace(laneID, capacity);
    return inserted ? CreateLaneStatus::Ok : CreateLaneStatus::LaneExists;
}
SubscribeStatus niniBUS::subscribe(laneID_t laneID, uint32_t subscriber_id)
{
    auto [it, _] = lane_map_.try_emplace(laneID);

    lane_t& laneObj = it->second;
    return laneObj.addSubscriber(subscriber_id);
}

PublishResult niniBUS::publish(laneID_t laneID, const std::string& message)
{
    // Check if lane exists, if not create it with default capacity
    auto [it, _] = lane_map_.try_emplace(laneID);

    return it->second.push(message);
}

ReceiveResult niniBUS::receive(laneID_t laneID, uint32_t subscriber_id, std::string& message)
{
    message.clear();
    // Check if the laneID is present and if subscribe is done
    auto [it, inserted] = lane_map_.try_emplace(laneID);
    if (inserted)
    {
        // Lane was just created, add subscriber
        it->second.addSubscriber(subscriber_id);
        return { 0, ReceiveStatus::LazyLaneCreated };
    }

    // The lane read finds or creates the subscriber cursor in one lookup.
    lane_t& laneObj = it->second;
    return laneObj.pop(subscriber_id, message);
}
