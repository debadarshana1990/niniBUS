#include "consumer.h"
#include <iostream>
#include <chrono>
#include <thread>

void consumer_thread(niniBUS& bus, uint32_t msgID, int consumer_id, int run_seconds, int interval_seconds)
{
    msg message{msgID, ""};
    int elapsed = 0;
    while (elapsed < run_seconds) {
        if (bus.pull_msg(message)) {
            std::cout << "[consumer] Consumer " << consumer_id << " received: " << message.content << std::endl;
        } else {
            std::cout << "[consumer] No content for msgID " << msgID << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::seconds(interval_seconds));
        elapsed += interval_seconds;
    }
    std::cout << "[consumer] Consumer " << consumer_id << " completed after " << run_seconds << " seconds" << std::endl;
}
