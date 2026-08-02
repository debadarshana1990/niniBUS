#pragma once

#include <cstdint>
#include <string>
#include <mutex>


#include "cfifo.h"
#include "status.h"

inline constexpr std::uint32_t DEFAULT_LANE_CAPACITY = 10;

class Lane
{
public:
    explicit Lane(std::uint32_t capacity = DEFAULT_LANE_CAPACITY)
        : content_(capacity)
    {
    }

    PublishResult push(const std::string& message);
    ReceiveResult pop(
        subscriberID_t subscriberID,
        std::string& message);
    SubscribeResult subscribe(subscriberID_t subscriberID);
    bool unsubscribe(subscriberID_t subscriberID);

private:
    nbus::cfifo<std::string> content_;
    std::mutex mutex_;
};
