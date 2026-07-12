#pragma once
#include <iostream>
#include <string>
#include <deque>
#include "status.h"

#define DEFAULT_LANE_CAPACITY (uint32_t)10

class lane_t
{

    // Message structure definition
    uint32_t capacity;
    std::deque<std::string> content;
    uint32_t qsize() const
    {
        return content.size();
    }
    uint32_t getCapacity() const
    {
        return capacity;
    }
    uint32_t getCredit() const 
    { 
        return capacity - content.size(); 
    }
    
public:
   // lane_t() : capacity(0) {}
    lane_t( uint32_t cap = DEFAULT_LANE_CAPACITY) : capacity(cap) {};
    lane_t(const lane_t& other) = default;

    PublishResult push(const std::string& message);
    ReceiveStatus pop(std::string& message);
    
};