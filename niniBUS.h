#pragma once
#include <iostream>
#include <string>
#include <stdint.h>
#include <unordered_map>
#include <deque>

#define MAX_LANE_CAPACITY (uint32_t)10
using lane_t = uint32_t;


enum class PublishStatus {
    Ok,
    LaneNotFound,
    LaneFull
};
enum class ReceiveStatus {
    Ok,
    LaneNotFound,
    LaneEmpty,
    LazyLaneCreated
};
struct PublishResult 
{
    uint32_t Credit;
    PublishStatus Status;
};



class Lane
{

    // Message structure definition
    uint32_t capacity;
    std::deque<std::string> content;
public:
   // Lane() : capacity(0) {}
    Lane( uint32_t cap = MAX_LANE_CAPACITY) : capacity(cap) {};
    Lane(const Lane& other) = default;
    uint32_t getCredit() const 
    { 
        return capacity - content.size(); 
    }
    ~Lane(){}
    void push(std::string message)
    {
        content.push_back(message);
    };
    std::string pop()
    {
        std::string msg = content.front();
        content.pop_front();
        return msg;
    }
    uint32_t qsize() const
    {
        return content.size();
    }
    uint32_t getCapacity() const
    {
        return capacity;
    }
};
class niniBUS
{
private:
    std::unordered_map<uint32_t, Lane> lane_map_; // map for msg iD, its idx

public:
    niniBUS() = default;
    ~niniBUS()
    {
       // std::cout<<"Are You Sure You Want To Destroy The Message Bus?"<<std::endl;
       // std::cout<<"All messages will be lost."<<std::endl;
      //  std::cout<<"Be a good Human. World is enough for everyone."<<std::endl;
    }
    PublishResult publish(lane_t,std::string message);
    ReceiveStatus receive(lane_t,std::string& message);
    bool subscribe(lane_t LaneID);

};
