#include <iostream>
#include "niniBUS.h"
#include "producer.h"
#include "consumer.h"
#include <thread>
#include <iostream>
#include <atomic>
#include <chrono>

using namespace std;

int main()
{
    cout << "Welcome to niniBUS" << endl;
    niniBUS bus;

    // Start two consumers (run 60s each, poll every 2s)
    bus.subscribe(1);
    thread cons1(consumer_thread, std::ref(bus), 1u, 1, 60, 2);
    thread cons2(consumer_thread, std::ref(bus), 1u, 2, 60, 2);

    // Give consumers a moment to start
    this_thread::sleep_for(chrono::milliseconds(100));

    // Start the producer (run 20s, produce every 1s)
    thread prod(producer_thread, std::ref(bus), 1u, 20, 1);

    // Wait for producer to finish (20s)
    prod.join();

    // Consumers will run for 60s total; wait for them
    cons1.join();
    cons2.join();

    return 0;
}


