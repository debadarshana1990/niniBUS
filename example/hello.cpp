#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>

#include "niniBUS.h"
#include "niniFIFO.h"

void print_pass(const std::string& test_name)
{
    std::cout << "[PASS] " << test_name << std::endl;
}

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

    print_pass("FIFO ordering through bus receive");
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

    print_pass("multiple lanes stay independent");
}

void test_publish_to_non_existing_lane()
{
    niniBUS bus;
    std::string message;

    assert(bus.publish(20, "created-by-publish").Status == PublishStatus::Ok);
    assert(bus.receive(20, message) == ReceiveStatus::Ok);
    assert(message == "created-by-publish");

    print_pass("publish lazily creates missing lane");
}

void test_receive_lazily_creates_missing_lane()
{
    niniBUS bus;
    std::string message = "old-data";

    assert(bus.receive(30, message) == ReceiveStatus::LazyLaneCreated);
    assert(message.empty());

    assert(bus.receive(30, message) == ReceiveStatus::LaneEmpty);
    assert(message.empty());

    assert(bus.publish(30, "created-after-receive").Status == PublishStatus::Ok);
    assert(bus.receive(30, message) == ReceiveStatus::Ok);
    assert(message == "created-after-receive");

    print_pass("receive lazily creates missing lane");
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

    PublishResult result3 = bus.publish(40, "msg3");
    assert(result3.Status == PublishStatus::Ok);
    assert(result3.Credit == DEFAULT_LANE_CAPACITY - 2);

    print_pass("lane credit decreases after publish and recovers after receive");
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

    for (int i = 2; i <= 10; i++)
    {
        assert(bus.receive(50, message) == ReceiveStatus::Ok);
        assert(message == "msg" + std::to_string(i));
    }

    assert(bus.receive(50, message) == ReceiveStatus::Ok);
    assert(message == "msg11_retry");

    assert(bus.receive(50, message) == ReceiveStatus::LaneEmpty);
    assert(message.empty());

    print_pass("full lane rejects publish and preserves FIFO order after retry");
}

void test_receive_clears_output_on_empty_lane()
{
    niniBUS bus;
    std::string message;

    assert(bus.publish(60, "one").Status == PublishStatus::Ok);
    assert(bus.receive(60, message) == ReceiveStatus::Ok);
    assert(message == "one");

    message = "stale";
    assert(bus.receive(60, message) == ReceiveStatus::LaneEmpty);
    assert(message.empty());

    print_pass("receive clears output before empty pop");
}

void test_direct_fifo_push_pop_front_status()
{
    niniFIFO<std::string> fifo;

    assert(fifo.isEmpty());
    assert(!fifo.isFull());
    assert(fifo.size() == 0);
    assert(fifo.getCapacity() == DEFAULT_LANE_CAPACITY);
    assert(fifo.pop_front() == FIFOStatus::EMPTY);

    assert(fifo.push_back("first") == FIFOStatus::SUCCESS);
    assert(fifo.push_back("second") == FIFOStatus::SUCCESS);
    assert(fifo.size() == 2);
    assert(fifo.front() == "first");

    assert(fifo.pop_front() == FIFOStatus::SUCCESS);
    assert(fifo.front() == "second");
    assert(fifo.pop_front() == FIFOStatus::SUCCESS);
    assert(fifo.pop_front() == FIFOStatus::EMPTY);
    assert(fifo.isEmpty());

    print_pass("direct FIFO push_back pop_front and front behavior");
}

void test_direct_fifo_full_status()
{
    niniFIFO<std::string> fifo;

    for (uint32_t i = 0; i < DEFAULT_LANE_CAPACITY; i++)
    {
        assert(fifo.push_back("item-" + std::to_string(i)) == FIFOStatus::SUCCESS);
    }

    assert(fifo.isFull());
    assert(fifo.size() == DEFAULT_LANE_CAPACITY);
    assert(fifo.push_back("overflow") == FIFOStatus::FULL);
    assert(fifo.size() == DEFAULT_LANE_CAPACITY);

    print_pass("direct FIFO reports full status");
}

void test_fifo_wraparound()
{
    niniFIFO<std::string> fifo;

    for (int i = 0; i < 10; ++i)
    {
        assert(fifo.push_back("A" + std::to_string(i)) == FIFOStatus::SUCCESS);
    }

    for (int i = 0; i < 5; ++i)
    {
        assert(fifo.front() == "A" + std::to_string(i));
        assert(fifo.pop_front() == FIFOStatus::SUCCESS);
    }

    for (int i = 0; i < 5; ++i)
    {
        assert(fifo.push_back("B" + std::to_string(i)) == FIFOStatus::SUCCESS);
    }

    for (int i = 5; i < 10; ++i)
    {
        assert(fifo.front() == "A" + std::to_string(i));
        assert(fifo.pop_front() == FIFOStatus::SUCCESS);
    }

    for (int i = 0; i < 5; ++i)
    {
        assert(fifo.front() == "B" + std::to_string(i));
        assert(fifo.pop_front() == FIFOStatus::SUCCESS);
    }

    assert(fifo.isEmpty());

    print_pass("direct FIFO wraparound preserves order");
}

void test_fifo_empty_negative_paths()
{
    niniFIFO<std::string> fifo;
    bool threw = false;

    assert(fifo.pop_front() == FIFOStatus::EMPTY);
    assert(fifo.pop_front() == FIFOStatus::EMPTY);
    assert(fifo.isEmpty());

    try
    {
        fifo.front();
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }

    assert(threw);

    print_pass("direct FIFO empty pop and front negative paths");
}

void test_fifo_overflow_does_not_corrupt_order()
{
    niniFIFO<std::string> fifo;

    for (uint32_t i = 0; i < DEFAULT_LANE_CAPACITY; i++)
    {
        assert(fifo.push_back("keep-" + std::to_string(i)) == FIFOStatus::SUCCESS);
    }

    assert(fifo.push_back("drop-1") == FIFOStatus::FULL);
    assert(fifo.push_back("drop-2") == FIFOStatus::FULL);
    assert(fifo.size() == DEFAULT_LANE_CAPACITY);

    for (uint32_t i = 0; i < DEFAULT_LANE_CAPACITY; i++)
    {
        assert(fifo.front() == "keep-" + std::to_string(i));
        assert(fifo.pop_front() == FIFOStatus::SUCCESS);
    }

    assert(fifo.isEmpty());

    print_pass("direct FIFO overflow does not corrupt queued data");
}

void test_bus_rejected_publish_is_not_received()
{
    niniBUS bus;
    std::string message;

    for (uint32_t i = 0; i < DEFAULT_LANE_CAPACITY; i++)
    {
        assert(bus.publish(70, "accepted-" + std::to_string(i)).Status == PublishStatus::Ok);
    }

    assert(bus.publish(70, "rejected").Status == PublishStatus::LaneFull);

    for (uint32_t i = 0; i < DEFAULT_LANE_CAPACITY; i++)
    {
        assert(bus.receive(70, message) == ReceiveStatus::Ok);
        assert(message == "accepted-" + std::to_string(i));
    }

    assert(bus.receive(70, message) == ReceiveStatus::LaneEmpty);
    assert(message.empty());

    print_pass("bus rejected publish is not received later");
}

int main()
{
    test_fifo_ordering();
    test_multiple_lanes_do_not_interfere();
    test_publish_to_non_existing_lane();
    test_receive_lazily_creates_missing_lane();
    test_lane_capacity_and_credit();
    test_lane_full_with_capacity_10();
    test_receive_clears_output_on_empty_lane();
    test_direct_fifo_push_pop_front_status();
    test_direct_fifo_full_status();
    test_fifo_wraparound();
    test_fifo_empty_negative_paths();
    test_fifo_overflow_does_not_corrupt_order();
    test_bus_rejected_publish_is_not_received();

    std::cout << "All niniBUS example tests passed." << std::endl;
    return 0;
}
