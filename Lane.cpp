#include "Lane.h"

PublishResult lane_t::push(const std::string& message)
{
    if (qsize() < getCapacity())
    {
        content.push_back(message);
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
        content.pop_front();
        return ReceiveStatus::Ok;
    }
    return ReceiveStatus::LaneEmpty;
}
