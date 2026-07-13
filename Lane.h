#pragma once
#include <iostream>
#include <string>
#include "niniFIFO.h"
#include "status.h"

#define DEFAULT_LANE_CAPACITY (uint32_t)10

class lane_t
{

    // Message structure definition
    niniFIFO_t<std::string, DEFAULT_LANE_CAPACITY> content;
    uint32_t qsize() const
    {
        return content.size();
    }
    uint32_t getCapacity() const
    {
        return DEFAULT_LANE_CAPACITY;
    }
    uint32_t getCredit() const 
    { 
        return DEFAULT_LANE_CAPACITY - content.size();
    }
    
public:
    lane_t() = default;
    lane_t(const lane_t& other) = default;

    PublishResult push(const std::string& message);
    ReceiveStatus pop(std::string& message);
    
};
