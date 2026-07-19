#include "Lane.h"

PublishResult lane_t::push(const std::string& message)
{
    if(content_.push(message) == CFIFOStatus::SUCCESS)
    {
        return { credit(), PublishStatus::Ok };
    }
    return { 0, PublishStatus::LaneFull };
}

ReceiveResult lane_t::pop(uint32_t subscriber_id, std::string& message)
{
    message.clear(); //clear the message string if any stray data
    if (content_.read(subscriber_id, message) == CFIFOStatus::SUCCESS)
    {
        return { getPendingMessage(subscriber_id),ReceiveStatus::Ok };
    }
    return { 0, ReceiveStatus::LaneEmpty };
}

SubscribeStatus lane_t::addSubscriber(uint32_t subscriber_id)
{
    // Add the subscriber to the lane's subscriber list
   if(content_.addCursor(subscriber_id))
       return SubscribeStatus::Ok;
   return SubscribeStatus::AlreadySubscribed;
}
