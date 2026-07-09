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
public:
    // Message structure definition
    lane_t laneID;
    uint32_t capacity;
    std::deque<std::string> content;

    Lane() : laneID(0), capacity(0) {}
    Lane(lane_t id, uint32_t cap = MAX_LANE_CAPACITY) : laneID(id), capacity(cap) {};
    Lane(const Lane& other) = default;
    uint32_t getCredit() const 
    { 
        return capacity - content.size(); 
    }
    ~Lane(){}
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
