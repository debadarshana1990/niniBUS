#pragma once
#include <iostream>
#include <string>
#include <deque>
#include "status.h"

#define MAX_LANE_CAPACITY (uint32_t)10

class Lane
{

    // Message structure definition
    uint32_t capacity;
    std::deque<std::string> content;
    
public:
   // Lane() : capacity(0) {}
    Lane( uint32_t cap = MAX_LANE_CAPACITY) : capacity(cap) {};
    Lane(const Lane& other) = default;
    
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
    ~Lane(){}
    PublishResult push(std::string message);
    ReceiveStatus pop(std::string& message);
    
};