#pragma once
#include <iostream>
#include <string>
#include <unordered_map>
#include "Lane.h"
#include "status.h"

using lane_t = uint32_t;

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
