#pragma once
#include <iostream>
#include <vector>
#include <queue>
#include <string>
#include <stdint.h>
#include <unordered_map>
#include <deque>

using lane_t = uint32_t;

class Lane
{
public:
    // Message structure definition
    lane_t laneID;
    std::deque<std::string> content;
    // Add more fields as needed
    uint32_t num_receivers; // number of receivers

    Lane(lane_t id) : laneID(id), num_receivers(1)
    {
        //std::cout<<"lane: "<<laneID<<" created"<<std::endl;
        //std::cout<<"num_receivers: "<<num_receivers<<std::endl;
    };
    Lane(const Lane& other) = default;
    Lane& operator=(const Lane& other) = delete; //no copy or move or assignment allowed
    ~Lane()
    {
        //std::cout<<"lane: "<<laneID<<" destroyed"<<std::endl;
    }
};
class niniBUS
{
private:
    std::vector<Lane*> lanes_;
    static uint32_t lanes_idx_; //hold the next new publisher idx
    std::unordered_map<uint32_t, uint32_t > lane_map_; // map for msg iD, its idx

public:
    niniBUS() = default;
    ~niniBUS()
    {
        std::cout<<"Are You Sure You Want To Destroy The Message Bus?"<<std::endl;
        std::cout<<"All messages will be lost."<<std::endl;
        std::cout<<"Be a good Human. World is enough for everyone."<<std::endl;
    }
    bool publish(lane_t,std::string message);
    bool receive(lane_t,std::string& message);
    bool subscribe(lane_t LaneID);

};
