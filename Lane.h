#pragma once
#include <iostream>
#include <string>
#include "niniFIFO.h"
#include "status.h"



class lane_t
{

    // Message structure definition
    niniFIFO<std::string> content;
    uint32_t qsize() const
    {
        return content.size();
    }
    uint32_t getCapacity() const
    {
        return content.getCapacity();
    }
    uint32_t getCredit() const
    {
        return content.getCapacity() - content.size();
    }
    
public:
    lane_t() = default;

    PublishResult push(const std::string& message);
    ReceiveStatus pop(std::string& message);
    
};
