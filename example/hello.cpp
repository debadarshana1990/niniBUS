#include <cassert>
#include <iostream>
#include <string>

#include "niniBUS.h"

void test_fifo_ordering()
{
    niniBUS bus;
    std::string message;

    assert(bus.publish(1, "first").Status == PublishStatus::Ok);
    assert(bus.publish(1, "second").Status == PublishStatus::Ok);
    assert(bus.publish(1, "third").Status == PublishStatus::Ok);

    assert(bus.receive(1, message) == ReceiveStatus::Ok);
    assert(message == "first");

    assert(bus.receive(1, message) == ReceiveStatus::Ok);
    assert(message == "second");

    assert(bus.receive(1, message) == ReceiveStatus::Ok);
    assert(message == "third");

    assert(bus.receive(1, message) == ReceiveStatus::LaneEmpty);
    assert(message.empty());
}

void test_multiple_lanes_do_not_interfere()
{
    niniBUS bus;
    std::string message;

    assert(bus.publish(1, "lane-1-a").Status == PublishStatus::Ok);
    assert(bus.publish(2, "lane-2-a").Status == PublishStatus::Ok);
    assert(bus.publish(1, "lane-1-b").Status == PublishStatus::Ok);

    assert(bus.receive(1, message) == ReceiveStatus::Ok);
    assert(message == "lane-1-a");

    assert(bus.receive(2, message) == ReceiveStatus::Ok);
    assert(message == "lane-2-a");

    assert(bus.receive(1, message) == ReceiveStatus::Ok);
    assert(message == "lane-1-b");
}

void test_receive_from_empty_lane()
{
    niniBUS bus;
    std::string message = "unchanged";

    assert(bus.subscribe(10));
    assert(bus.receive(10, message) == ReceiveStatus::LaneEmpty);
    assert(message.empty());
}

void test_publish_to_non_existing_lane()
{
    niniBUS bus;
    std::string message;

    assert(bus.publish(20, "created-by-publish").Status == PublishStatus::Ok);
    assert(bus.receive(20, message) == ReceiveStatus::Ok);
    assert(message == "created-by-publish");
}

void test_subscribe_behavior()
{
    niniBUS bus;
    std::string message;

    assert(bus.subscribe(30));
    assert(bus.receive(30, message) == ReceiveStatus::LaneEmpty);
    assert(message.empty());

    assert(bus.subscribe(30));
    assert(bus.publish(30, "after-subscribe").Status == PublishStatus::Ok);
    assert(bus.receive(30, message) == ReceiveStatus::Ok);
    assert(message == "after-subscribe");
}

void test_lane_capacity_and_credit()
{
    niniBUS bus;
    std::string message;

    // Publish a message and check remaining credit
    PublishResult result1 = bus.publish(40, "msg1");
    assert(result1.Status == PublishStatus::Ok);
    assert(result1.Credit == DEFAULT_LANE_CAPACITY - 1);  // 9 messages left

    // Publish another and verify credit decreases
    PublishResult result2 = bus.publish(40, "msg2");
    assert(result2.Status == PublishStatus::Ok);
    assert(result2.Credit == DEFAULT_LANE_CAPACITY - 2);  // 8 messages left

    // Receive one message and verify credit increases
    assert(bus.receive(40, message) == ReceiveStatus::Ok);
    assert(message == "msg1");
}

void test_lane_full_with_capacity_10()
{
    niniBUS bus;
    std::string message;

    // Publish exactly 10 messages (capacity is 10)
    for (int i = 1; i <= 10; i++)
    {
        PublishResult result = bus.publish(50, "msg" + std::to_string(i));
        assert(result.Status == PublishStatus::Ok);
        assert(result.Credit == DEFAULT_LANE_CAPACITY - i);  // Remaining credit
    }

    // Try to publish the 11th message - should fail (lane full)
    PublishResult fullResult = bus.publish(50, "msg11");
    assert(fullResult.Status == PublishStatus::LaneFull);
    assert(fullResult.Credit == 0);  // No credit left

    // Receive one message to free up space
    assert(bus.receive(50, message) == ReceiveStatus::Ok);
    assert(message == "msg1");

    // Now we should be able to publish again
    PublishResult retryResult = bus.publish(50, "msg11_retry");
    assert(retryResult.Status == PublishStatus::Ok);
    assert(retryResult.Credit == 0);  // 9 messages in queue, 1 credit left = 0 after publish
}

int main()
{
    test_fifo_ordering();
    test_multiple_lanes_do_not_interfere();
    test_receive_from_empty_lane();
    test_publish_to_non_existing_lane();
    test_subscribe_behavior();
    test_lane_capacity_and_credit();
    std::cout<<"******credit test******"<<std::endl;
    test_lane_full_with_capacity_10();

    std::cout << "All niniBUS example tests passed." << std::endl;
    return 0;
}
