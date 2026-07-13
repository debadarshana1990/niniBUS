#include "Lane.h"

PublishResult lane_t::push(const std::string& message)
{
    if(content.push_back(message) == FIFOStatus::SUCCESS)
    {
        return { getCredit(), PublishStatus::Ok };
    }
    return { 0, PublishStatus::LaneFull };
}

ReceiveStatus lane_t::pop(std::string& message)
{
    message.clear(); //clear the message string if any stray data
    if (!content.isEmpty())
    {
        message = content.front();
        if (content.pop_front() == FIFOStatus::SUCCESS)
        {
            return ReceiveStatus::Ok;
        }
    }
    return ReceiveStatus::LaneEmpty;
}
