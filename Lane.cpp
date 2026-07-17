#include "Lane.h"

PublishResult lane_t::push(const std::string& message)
{
    if(content_.push_back(message) == FIFOStatus::SUCCESS)
    {
        return { credit(), PublishStatus::Ok };
    }
    return { 0, PublishStatus::LaneFull };
}

ReceiveResult lane_t::pop(std::string& message)
{
    message.clear(); //clear the message string if any stray data
    if (!content_.empty())
    {
        message = content_.front();
        if (content_.pop_front() == FIFOStatus::SUCCESS)
        {
            return { getPendingMessage(),ReceiveStatus::Ok };
        }
    }
    return { 0, ReceiveStatus::LaneEmpty };
}
