#pragma once
#include <string>
#include "status.h"
#include "cfifo.h"


#define DEFAULT_LANE_CAPACITY (uint32_t)10 // Default capacity for lazily created lanes.

class lane
{

    // Bounded message storage for this lane.
    nbus::cfifo<std::string> content_;
    uint32_t credit() const
    {
        return content_.capacity() - content_.size();
    }
    uint32_t getPendingMessage() const
    {
        return content_.size();
    }
    
public:

    explicit lane(uint32_t capacity = DEFAULT_LANE_CAPACITY) : content_(capacity) {}


    PublishResult push(const std::string& message);
    ReceiveResult pop(subscribeID_t subscribeID,std::string& message);
    SubscribeResult subscribe(subscribeID_t subscribeID);
    bool unsubscribe(subscribeID_t subscribeID);
   
    
};
