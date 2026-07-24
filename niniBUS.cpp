#include "niniBUS.h"

CreateLaneStatus niniBUS::createLane(
    laneID_t laneID,
    std::uint32_t capacity)
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
    const auto lane = lanes_.try_emplace(laneID);
    return lane.first->second.push(message);
}

SubscribeResult niniBUS::subscribe(
    laneID_t laneID,
    subscriberID_t subscriberID)
{
    auto it = lanes_.find(laneID);
    if (it == lanes_.end())
    {
        return {SubscribeStatus::LaneNotExist, 0};
    }
    return it->second.subscribe(subscriberID);
}

ReceiveResult niniBUS::receive(
    laneID_t laneID,
    subscriberID_t subscriberID,
    std::string& message)
{
    message.clear();

    auto it = lanes_.find(laneID);
    if (it == lanes_.end())
    {
        return {ReceiveStatus::NO_CURSOR, 0, 0, 0};
    }
    return it->second.pop(subscriberID, message);
}

bool niniBUS::unsubscribe(
    laneID_t laneID,
    subscriberID_t subscriberID)
{
    auto it = lanes_.find(laneID);
    if (it == lanes_.end())
    {
        return false;
    }
    return it->second.unsubscribe(subscriberID);
}
