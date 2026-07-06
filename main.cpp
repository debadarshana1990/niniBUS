#include <iostream>
#include "niniBUS.h"

using namespace std;

int main()
{
    std::cout << "Welcome to niniBUS" << std::endl;
    niniBUS bus;

    // Start two consumers (run 60s each, poll every 2s)
    bus.subscribe(1);
    bus.subscribe(2);
    bus.publish(1, "Hello from producer 1");
    bus.publish(1, "Hello Again from producer 1");
    std::string message;
    bus.receive(1, message);
    std::cout << "Received on lane 1: " << message << std::endl;
    bus.receive(2, message);
    std::cout << "Received on lane 2: " << message << std::endl;
    bus.receive(1, message);
    std::cout << "Received on lane 1: " << message << std::endl;

    return 0;
}


