#include "producer.h"
#include <thread>
#include <chrono>
#include <iostream>

void producer_thread(niniBUS& bus, uint32_t msgID, int run_seconds, int interval_seconds)
{
    int counter = 1;
    int elapsed = 0;
    while (elapsed < run_seconds) {
        msg message = { msgID, "heelo" + std::to_string(counter) };
        bus.push_msg(message);
        std::cout << "[producer] heelo" << counter << std::endl;
        ++counter;
        std::this_thread::sleep_for(std::chrono::seconds(interval_seconds));
        elapsed += interval_seconds;
    }
    std::cout << "[producer] completed after " << run_seconds << " seconds" << std::endl;
}
