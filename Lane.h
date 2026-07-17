#pragma once
#include <string>
#include "niniFIFO.h"
#include "status.h"


#define DEFAULT_LANE_CAPACITY (uint32_t)10 //niniFIFO should holds the default value of the queue capacity not the lane

class lane_t
{

    // Message structure definition
    niniFIFO<std::string> content;
    uint32_t getCredit() const
    {
        return content.getCapacity() - content.size();
    }
    uint32_t getPendingMessage() const
    {
        return content.size();
    }
    
public:

    lane_t(uint32_t capacity = DEFAULT_LANE_CAPACITY) : content(capacity) {} // Initialize the FIFO with the specified capacity


    PublishResult push(const std::string& message);
    ReceiveResult pop(std::string& message);
    
};
