#include <cassert>
#include <iostream>
#include <string>

#include "niniBUS.h"

void test_fifo_ordering()
{
    niniBUS bus;
    std::string message;

    assert(bus.publish(1, "first") == PublishResult::Ok);
    assert(bus.publish(1, "second") == PublishResult::Ok);
    assert(bus.publish(1, "third") == PublishResult::Ok);

    assert(bus.receive(1, message) == ReceiveResult::Ok);
    assert(message == "first");

    assert(bus.receive(1, message) == ReceiveResult::Ok);
    assert(message == "second");

    assert(bus.receive(1, message) == ReceiveResult::Ok);
    assert(message == "third");

    assert(bus.receive(1, message) == ReceiveResult::LaneEmpty);
    assert(message.empty());
}

void test_multiple_lanes_do_not_interfere()
{
    niniBUS bus;
    std::string message;

    assert(bus.publish(1, "lane-1-a") == PublishResult::Ok);
    assert(bus.publish(2, "lane-2-a") == PublishResult::Ok);
    assert(bus.publish(1, "lane-1-b") == PublishResult::Ok);

    assert(bus.receive(1, message) == ReceiveResult::Ok);
    assert(message == "lane-1-a");

    assert(bus.receive(2, message) == ReceiveResult::Ok);
    assert(message == "lane-2-a");

    assert(bus.receive(1, message) == ReceiveResult::Ok);
    assert(message == "lane-1-b");
}

void test_receive_from_empty_lane()
{
    niniBUS bus;
    std::string message = "unchanged";

    assert(bus.subscribe(10));
    assert(bus.receive(10, message) == ReceiveResult::LaneEmpty);
    assert(message.empty());
}

void test_publish_to_non_existing_lane()
{
    niniBUS bus;
    std::string message;

    assert(bus.publish(20, "created-by-publish") == PublishResult::Ok);
    assert(bus.receive(20, message) == ReceiveResult::Ok);
    assert(message == "created-by-publish");
}

void test_subscribe_behavior()
{
    niniBUS bus;
    std::string message;

    assert(bus.subscribe(30));
    assert(bus.receive(30, message) == ReceiveResult::LaneEmpty);
    assert(message.empty());

    assert(bus.subscribe(30));
    assert(bus.publish(30, "after-subscribe") == PublishResult::Ok);
    assert(bus.receive(30, message) == ReceiveResult::Ok);
    assert(message == "after-subscribe");
}

int main()
{
    test_fifo_ordering();
    test_multiple_lanes_do_not_interfere();
    test_receive_from_empty_lane();
    test_publish_to_non_existing_lane();
    test_subscribe_behavior();

    std::cout << "All niniBUS example tests passed." << std::endl;
    return 0;
}
