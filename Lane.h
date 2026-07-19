#pragma once
#include <string>
#include "niniCFIFO.h"
#include "status.h"


#define DEFAULT_LANE_CAPACITY (uint32_t)10 // Default capacity for lazily created lanes.

class lane_t
{

    // Bounded message storage for this lane.
    niniCFIFO<std::string> content_;
    uint32_t credit() const
    {
        return content_.capacity() - content_.size(); //credit is for the global queue
    }
    uint32_t getPendingMessage(uint32_t subscriber_id) const
    {
        return content_.pending(subscriber_id);
    }
    
public:

    explicit lane_t(uint32_t capacity = DEFAULT_LANE_CAPACITY) : content_(capacity) {}


    PublishResult push(const std::string& message);
    ReceiveResult pop(uint32_t subscriber_id, std::string& message);
    SubscribeStatus addSubscriber(uint32_t subscriber_id);

};
