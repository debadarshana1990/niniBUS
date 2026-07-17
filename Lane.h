#pragma once
#include <string>
#include "niniFIFO.h"
#include "status.h"


#define DEFAULT_LANE_CAPACITY (uint32_t)10 // Default capacity for lazily created lanes.

class lane_t
{

    // Bounded message storage for this lane.
    niniFIFO<std::string> content_;
    uint32_t credit() const
    {
        return content_.capacity() - content_.size();
    }
    uint32_t getPendingMessage() const
    {
        return content_.size();
    }
    
public:

    explicit lane_t(uint32_t capacity = DEFAULT_LANE_CAPACITY) : content_(capacity) {}


    PublishResult push(const std::string& message);
    ReceiveResult pop(std::string& message);
    
};
