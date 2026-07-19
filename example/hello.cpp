#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>

#include "niniBUS.h"
#include "niniCFIFO.h"

void print_pass(const std::string& test_name)
{
    std::cout << "[PASS] " << test_name << std::endl;
}

void test_create_lane_and_subscribe()
{
    niniBUS bus;

    assert(bus.createLane(1, 3) == CreateLaneStatus::Ok);
    assert(bus.subscribe(1, 100) == SubscribeStatus::Ok);
    assert(bus.subscribe(1, 100) == SubscribeStatus::AlreadySubscribed);
    assert(bus.subscribe(1, 101) == SubscribeStatus::Ok);

    print_pass("create lane and register independent subscribers");
}

void test_subscribe_lazily_creates_lane()
{
    niniBUS bus;

    assert(bus.subscribe(2, 200) == SubscribeStatus::Ok);
    assert(bus.createLane(2, 5) == CreateLaneStatus::LaneExists);
    assert(bus.subscribe(2, 200) == SubscribeStatus::AlreadySubscribed);

    print_pass("subscribe lazily creates a default-capacity lane");
}

void test_receive_lazily_creates_lane_and_subscriber()
{
    niniBUS bus;
    std::string message = "stale";

    ReceiveResult result = bus.receive(3, 300, message);
    assert(result.Status == ReceiveStatus::LazyLaneCreated);
    assert(result.PendingMessages == 0);
    assert(message.empty());
    assert(bus.subscribe(3, 300) == SubscribeStatus::AlreadySubscribed);

    print_pass("receive lazily creates a lane and subscriber cursor");
}

void test_direct_cfifo_cursor_registration()
{
    niniCFIFO<std::string> fifo(3);

    assert(fifo.capacity() == 3);
    assert(fifo.addCursor(400));
    assert(fifo.hasCursor(400));
    assert(!fifo.addCursor(400));
    assert(fifo.addCursor(401));

    print_pass("direct CFIFO cursor registration");
}

void test_receive_auto_subscribes_existing_lane()
{
    niniBUS bus;
    std::string message = "stale";

    assert(bus.createLane(4, 3) == CreateLaneStatus::Ok);

    ReceiveResult emptyLaneResult = bus.receive(4, 499, message);
    assert(emptyLaneResult.Status == ReceiveStatus::LaneEmpty);
    assert(emptyLaneResult.PendingMessages == 0);
    assert(message.empty());
    assert(bus.subscribe(4, 499) == SubscribeStatus::AlreadySubscribed);

    assert(bus.publish(4, "shared-message").Status == PublishStatus::Ok);

    ReceiveResult queuedMessageResult = bus.receive(4, 400, message);
    assert(queuedMessageResult.Status == ReceiveStatus::Ok);
    assert(message == "shared-message");
    assert(bus.subscribe(4, 400) == SubscribeStatus::AlreadySubscribed);

    print_pass("receive auto-subscribes on an existing lane");
}

void test_multiple_subscribers_read_same_message()
{
    niniBUS bus;
    std::string firstMessage;
    std::string secondMessage;

    assert(bus.createLane(5, 3) == CreateLaneStatus::Ok);
    assert(bus.publish(5, "broadcast-message").Status == PublishStatus::Ok);

    ReceiveResult first = bus.receive(5, 500, firstMessage);
    ReceiveResult second = bus.receive(5, 501, secondMessage);

    assert(first.Status == ReceiveStatus::Ok);
    assert(second.Status == ReceiveStatus::Ok);
    assert(first.PendingMessages == 0);
    assert(second.PendingMessages == 0);
    assert(firstMessage == "broadcast-message");
    assert(secondMessage == "broadcast-message");
    assert(bus.subscribe(5, 500) == SubscribeStatus::AlreadySubscribed);
    assert(bus.subscribe(5, 501) == SubscribeStatus::AlreadySubscribed);

    print_pass("multiple subscribers read the same shared message");
}

void test_subscribers_advance_independently_and_catch_up()
{
    niniBUS bus;
    std::string message;

    assert(bus.createLane(6, 3) == CreateLaneStatus::Ok);
    assert(bus.publish(6, "A").Status == PublishStatus::Ok);
    assert(bus.publish(6, "B").Status == PublishStatus::Ok);

    ReceiveResult firstA = bus.receive(6, 600, message);
    assert(firstA.Status == ReceiveStatus::Ok);
    assert(firstA.PendingMessages == 1);
    assert(message == "A");

    ReceiveResult firstB = bus.receive(6, 600, message);
    assert(firstB.Status == ReceiveStatus::Ok);
    assert(firstB.PendingMessages == 0);
    assert(message == "B");

    ReceiveResult firstCaughtUp = bus.receive(6, 600, message);
    assert(firstCaughtUp.Status == ReceiveStatus::LaneEmpty);
    assert(firstCaughtUp.PendingMessages == 0);
    assert(message.empty());

    ReceiveResult secondA = bus.receive(6, 601, message);
    assert(secondA.Status == ReceiveStatus::Ok);
    assert(secondA.PendingMessages == 1);
    assert(message == "A");

    ReceiveResult secondB = bus.receive(6, 601, message);
    assert(secondB.Status == ReceiveStatus::Ok);
    assert(secondB.PendingMessages == 0);
    assert(message == "B");

    ReceiveResult secondCaughtUp = bus.receive(6, 601, message);
    assert(secondCaughtUp.Status == ReceiveStatus::LaneEmpty);
    assert(secondCaughtUp.PendingMessages == 0);
    assert(message.empty());

    print_pass("subscriber cursors advance independently and stop at the tail");
}

void test_capacity_one_subscriber_does_not_repeat_message()
{
    niniBUS bus;
    std::string message;

    assert(bus.createLane(7, 1) == CreateLaneStatus::Ok);
    assert(bus.publish(7, "only").Status == PublishStatus::Ok);

    ReceiveResult received = bus.receive(7, 700, message);
    assert(received.Status == ReceiveStatus::Ok);
    assert(received.PendingMessages == 0);
    assert(message == "only");

    ReceiveResult caughtUp = bus.receive(7, 700, message);
    assert(caughtUp.Status == ReceiveStatus::LaneEmpty);
    assert(caughtUp.PendingMessages == 0);
    assert(message.empty());

    print_pass("capacity-one subscriber does not repeat a consumed message");
}

void test_full_capacity_cursor_wrap_and_pending_count()
{
    niniBUS bus;
    std::string message;

    assert(bus.createLane(8, 3) == CreateLaneStatus::Ok);
    assert(bus.publish(8, "A").Status == PublishStatus::Ok);
    assert(bus.publish(8, "B").Status == PublishStatus::Ok);
    assert(bus.publish(8, "C").Status == PublishStatus::Ok);

    for (uint32_t i = 0; i < 3; ++i)
    {
        ReceiveResult result = bus.receive(8, 800, message);
        assert(result.Status == ReceiveStatus::Ok);
        assert(result.PendingMessages == 2 - i);
        assert(message == std::string(1, static_cast<char>('A' + i)));
    }

    ReceiveResult caughtUp = bus.receive(8, 800, message);
    assert(caughtUp.Status == ReceiveStatus::LaneEmpty);
    assert(caughtUp.PendingMessages == 0);
    assert(message.empty());

    // Eviction is not implemented, so subscriber reads do not restore credit.
    PublishResult stillFull = bus.publish(8, "D");
    assert(stillFull.Status == PublishStatus::LaneFull);
    assert(stillFull.Credit == 0);

    print_pass("full-capacity cursor wrap preserves pending counts");
}

void test_subscribers_can_read_at_different_speeds()
{
    niniBUS bus;
    std::string message;

    assert(bus.createLane(9, 3) == CreateLaneStatus::Ok);
    assert(bus.publish(9, "A").Status == PublishStatus::Ok);
    assert(bus.publish(9, "B").Status == PublishStatus::Ok);
    assert(bus.publish(9, "C").Status == PublishStatus::Ok);

    ReceiveResult fastA = bus.receive(9, 900, message);
    assert(fastA.PendingMessages == 2);
    assert(message == "A");
    ReceiveResult fastB = bus.receive(9, 900, message);
    assert(fastB.PendingMessages == 1);
    assert(message == "B");

    ReceiveResult slowA = bus.receive(9, 901, message);
    assert(slowA.Status == ReceiveStatus::Ok);
    assert(slowA.PendingMessages == 2);
    assert(message == "A");

    ReceiveResult fastC = bus.receive(9, 900, message);
    assert(fastC.PendingMessages == 0);
    assert(message == "C");

    ReceiveResult slowB = bus.receive(9, 901, message);
    assert(slowB.PendingMessages == 1);
    assert(message == "B");
    ReceiveResult slowC = bus.receive(9, 901, message);
    assert(slowC.PendingMessages == 0);
    assert(message == "C");

    assert(bus.receive(9, 900, message).Status == ReceiveStatus::LaneEmpty);
    assert(bus.receive(9, 901, message).Status == ReceiveStatus::LaneEmpty);

    print_pass("subscribers consume independently at different speeds");
}

void test_late_subscriber_reads_retained_history()
{
    niniBUS bus;
    std::string message;

    assert(bus.createLane(10, 3) == CreateLaneStatus::Ok);
    assert(bus.publish(10, "before-subscribe-1").Status == PublishStatus::Ok);
    assert(bus.publish(10, "before-subscribe-2").Status == PublishStatus::Ok);

    ReceiveResult first = bus.receive(10, 1000, message);
    assert(first.Status == ReceiveStatus::Ok);
    assert(first.PendingMessages == 1);
    assert(message == "before-subscribe-1");

    ReceiveResult second = bus.receive(10, 1000, message);
    assert(second.Status == ReceiveStatus::Ok);
    assert(second.PendingMessages == 0);
    assert(message == "before-subscribe-2");

    assert(bus.receive(10, 1000, message).Status == ReceiveStatus::LaneEmpty);

    print_pass("late subscriber reads retained history from the beginning");
}

void test_empty_string_message_is_distinct_from_empty_lane()
{
    niniBUS bus;
    std::string message = "stale";

    assert(bus.createLane(11, 2) == CreateLaneStatus::Ok);
    assert(bus.publish(11, "").Status == PublishStatus::Ok);

    ReceiveResult received = bus.receive(11, 1100, message);
    assert(received.Status == ReceiveStatus::Ok);
    assert(received.PendingMessages == 0);
    assert(message.empty());

    ReceiveResult caughtUp = bus.receive(11, 1100, message);
    assert(caughtUp.Status == ReceiveStatus::LaneEmpty);
    assert(caughtUp.PendingMessages == 0);
    assert(message.empty());

    print_pass("empty string message is distinct from an empty lane");
}

// The legacy tests below assume that receiving messages frees queue slots and
// eventually makes the queue empty. Keep these use cases for later, but do not
// run them yet because the niniCFIFO eviction policy is not implemented.
#if 0

ReceiveResult receive_and_expect(
    niniBUS& bus,
    laneID_t laneID,
    std::string& message,
    ReceiveStatus expectedStatus,
    uint32_t expectedPendingMessages)
{
    ReceiveResult result = bus.receive(laneID, message);
    assert(result.Status == expectedStatus);
    assert(result.PendingMessages == expectedPendingMessages);
    return result;
}

void test_fifo_ordering()
{
    niniBUS bus;
    std::string message;

    assert(bus.publish(1, "first").Status == PublishStatus::Ok);
    assert(bus.publish(1, "second").Status == PublishStatus::Ok);
    assert(bus.publish(1, "third").Status == PublishStatus::Ok);

    receive_and_expect(bus, 1, message, ReceiveStatus::Ok, 2);
    assert(message == "first");

    receive_and_expect(bus, 1, message, ReceiveStatus::Ok, 1);
    assert(message == "second");

    receive_and_expect(bus, 1, message, ReceiveStatus::Ok, 0);
    assert(message == "third");

    receive_and_expect(bus, 1, message, ReceiveStatus::LaneEmpty, 0);
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

    receive_and_expect(bus, 1, message, ReceiveStatus::Ok, 1);
    assert(message == "lane-1-a");

    receive_and_expect(bus, 2, message, ReceiveStatus::Ok, 0);
    assert(message == "lane-2-a");

    receive_and_expect(bus, 1, message, ReceiveStatus::Ok, 0);
    assert(message == "lane-1-b");

    print_pass("multiple lanes stay independent");
}

void test_publish_to_non_existing_lane()
{
    niniBUS bus;
    std::string message;

    assert(bus.publish(20, "created-by-publish").Status == PublishStatus::Ok);
    receive_and_expect(bus, 20, message, ReceiveStatus::Ok, 0);
    assert(message == "created-by-publish");

    print_pass("publish lazily creates missing lane");
}

void test_create_lane_uses_requested_capacity()
{
    niniBUS bus;
    std::string message;
    constexpr uint32_t capacity = 3;

    assert(bus.createLane(21, capacity) == CreateLaneStatus::Ok);

    for (uint32_t i = 0; i < capacity; ++i)
    {
        PublishResult result = bus.publish(21, "custom-" + std::to_string(i));
        assert(result.Status == PublishStatus::Ok);
        assert(result.Credit == capacity - i - 1);
    }

    PublishResult fullResult = bus.publish(21, "overflow");
    assert(fullResult.Status == PublishStatus::LaneFull);
    assert(fullResult.Credit == 0);

    for (uint32_t i = 0; i < capacity; ++i)
    {
        receive_and_expect(bus, 21, message, ReceiveStatus::Ok, capacity - i - 1);
        assert(message == "custom-" + std::to_string(i));
    }

    print_pass("createLane uses the requested capacity");
}

void test_create_lane_does_not_replace_existing_lane()
{
    niniBUS bus;

    assert(bus.createLane(22, 2) == CreateLaneStatus::Ok);
    assert(bus.publish(22, "first").Credit == 1);

    assert(bus.createLane(22, 5) == CreateLaneStatus::LaneExists);

    PublishResult second = bus.publish(22, "second");
    assert(second.Status == PublishStatus::Ok);
    assert(second.Credit == 0);

    PublishResult overflow = bus.publish(22, "overflow");
    assert(overflow.Status == PublishStatus::LaneFull);
    assert(overflow.Credit == 0);

    print_pass("createLane preserves an existing lane and its capacity");
}

void test_publish_created_lane_uses_default_capacity()
{
    niniBUS bus;

    PublishResult first = bus.publish(23, "default-0");
    assert(first.Status == PublishStatus::Ok);
    assert(first.Credit == DEFAULT_LANE_CAPACITY - 1);

    assert(bus.createLane(23, DEFAULT_LANE_CAPACITY + 5) == CreateLaneStatus::LaneExists);

    for (uint32_t i = 1; i < DEFAULT_LANE_CAPACITY; ++i)
    {
        PublishResult result = bus.publish(23, "default-" + std::to_string(i));
        assert(result.Status == PublishStatus::Ok);
        assert(result.Credit == DEFAULT_LANE_CAPACITY - i - 1);
    }

    PublishResult overflow = bus.publish(23, "overflow");
    assert(overflow.Status == PublishStatus::LaneFull);
    assert(overflow.Credit == 0);

    print_pass("publish-created lane uses the default capacity");
}

void test_create_lane_rejects_zero_capacity()
{
    niniBUS bus;

    assert(bus.createLane(24, 0) == CreateLaneStatus::InvalidCapacity);

    // A rejected creation must not reserve the lane ID.
    assert(bus.createLane(24, 1) == CreateLaneStatus::Ok);

    print_pass("createLane rejects zero capacity without creating a lane");
}

void test_capacity_one_lane()
{
    niniBUS bus;
    std::string message;

    assert(bus.createLane(25, 1) == CreateLaneStatus::Ok);

    PublishResult first = bus.publish(25, "one");
    assert(first.Status == PublishStatus::Ok);
    assert(first.Credit == 0);

    PublishResult full = bus.publish(25, "two");
    assert(full.Status == PublishStatus::LaneFull);
    assert(full.Credit == 0);

    receive_and_expect(bus, 25, message, ReceiveStatus::Ok, 0);
    assert(message == "one");

    PublishResult retry = bus.publish(25, "two");
    assert(retry.Status == PublishStatus::Ok);
    assert(retry.Credit == 0);

    receive_and_expect(bus, 25, message, ReceiveStatus::Ok, 0);
    assert(message == "two");

    print_pass("capacity-one lane supports full receive and reuse");
}

void test_custom_capacity_fifo_wraparound()
{
    niniFIFO<std::string> fifo(3);

    assert(fifo.push_back("A") == FIFOStatus::SUCCESS);
    assert(fifo.push_back("B") == FIFOStatus::SUCCESS);
    assert(fifo.push_back("C") == FIFOStatus::SUCCESS);

    assert(fifo.front() == "A");
    assert(fifo.pop_front() == FIFOStatus::SUCCESS);
    assert(fifo.front() == "B");
    assert(fifo.pop_front() == FIFOStatus::SUCCESS);

    assert(fifo.push_back("D") == FIFOStatus::SUCCESS);
    assert(fifo.push_back("E") == FIFOStatus::SUCCESS);
    assert(fifo.full());

    for (const std::string& expected : {"C", "D", "E"})
    {
        assert(fifo.front() == expected);
        assert(fifo.pop_front() == FIFOStatus::SUCCESS);
    }

    assert(fifo.empty());

    print_pass("custom-capacity FIFO wraparound preserves order");
}

void test_receive_lazily_creates_missing_lane()
{
    niniBUS bus;
    std::string message = "old-data";

    receive_and_expect(bus, 30, message, ReceiveStatus::LazyLaneCreated, 0);
    assert(message.empty());

    receive_and_expect(bus, 30, message, ReceiveStatus::LaneEmpty, 0);
    assert(message.empty());

    assert(bus.publish(30, "created-after-receive").Status == PublishStatus::Ok);
    receive_and_expect(bus, 30, message, ReceiveStatus::Ok, 0);
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
    receive_and_expect(bus, 40, message, ReceiveStatus::Ok, 1);
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
    receive_and_expect(bus, 50, message, ReceiveStatus::Ok, DEFAULT_LANE_CAPACITY - 1);
    assert(message == "msg1");

    // Now we should be able to publish again
    PublishResult retryResult = bus.publish(50, "msg11_retry");
    assert(retryResult.Status == PublishStatus::Ok);
    assert(retryResult.Credit == 0);  // 9 messages in queue, 1 credit left = 0 after publish

    for (int i = 2; i <= 10; i++)
    {
        receive_and_expect(
            bus,
            50,
            message,
            ReceiveStatus::Ok,
            DEFAULT_LANE_CAPACITY - i + 1);
        assert(message == "msg" + std::to_string(i));
    }

    receive_and_expect(bus, 50, message, ReceiveStatus::Ok, 0);
    assert(message == "msg11_retry");

    receive_and_expect(bus, 50, message, ReceiveStatus::LaneEmpty, 0);
    assert(message.empty());

    print_pass("full lane rejects publish and preserves FIFO order after retry");
}

void test_receive_clears_output_on_empty_lane()
{
    niniBUS bus;
    std::string message;

    assert(bus.publish(60, "one").Status == PublishStatus::Ok);
    receive_and_expect(bus, 60, message, ReceiveStatus::Ok, 0);
    assert(message == "one");

    message = "stale";
    receive_and_expect(bus, 60, message, ReceiveStatus::LaneEmpty, 0);
    assert(message.empty());

    print_pass("receive clears output before empty pop");
}

void test_receive_reports_pending_messages()
{
    niniBUS bus;
    std::string message;

    assert(bus.publish(80, "pending-1").Status == PublishStatus::Ok);
    assert(bus.publish(80, "pending-2").Status == PublishStatus::Ok);
    assert(bus.publish(80, "pending-3").Status == PublishStatus::Ok);
    assert(bus.publish(80, "pending-4").Status == PublishStatus::Ok);

    receive_and_expect(bus, 80, message, ReceiveStatus::Ok, 3);
    assert(message == "pending-1");

    receive_and_expect(bus, 80, message, ReceiveStatus::Ok, 2);
    assert(message == "pending-2");

    receive_and_expect(bus, 80, message, ReceiveStatus::Ok, 1);
    assert(message == "pending-3");

    receive_and_expect(bus, 80, message, ReceiveStatus::Ok, 0);
    assert(message == "pending-4");

    receive_and_expect(bus, 80, message, ReceiveStatus::LaneEmpty, 0);
    assert(message.empty());

    print_pass("receive reports pending message count");
}

void test_direct_fifo_push_pop_front_status()
{
    niniFIFO<std::string> fifo(DEFAULT_LANE_CAPACITY);

    assert(fifo.empty());
    assert(!fifo.full());
    assert(fifo.size() == 0);
    assert(fifo.capacity() == DEFAULT_LANE_CAPACITY);
    assert(fifo.pop_front() == FIFOStatus::EMPTY);

    assert(fifo.push_back("first") == FIFOStatus::SUCCESS);
    assert(fifo.push_back("second") == FIFOStatus::SUCCESS);
    assert(fifo.size() == 2);
    assert(fifo.front() == "first");

    assert(fifo.pop_front() == FIFOStatus::SUCCESS);
    assert(fifo.front() == "second");
    assert(fifo.pop_front() == FIFOStatus::SUCCESS);
    assert(fifo.pop_front() == FIFOStatus::EMPTY);
    assert(fifo.empty());

    print_pass("direct FIFO push_back pop_front and front behavior");
}

void test_direct_fifo_full_status()
{
    niniFIFO<std::string> fifo(DEFAULT_LANE_CAPACITY);

    for (uint32_t i = 0; i < DEFAULT_LANE_CAPACITY; i++)
    {
        assert(fifo.push_back("item-" + std::to_string(i)) == FIFOStatus::SUCCESS);
    }

    assert(fifo.full());
    assert(fifo.size() == DEFAULT_LANE_CAPACITY);
    assert(fifo.push_back("overflow") == FIFOStatus::FULL);
    assert(fifo.size() == DEFAULT_LANE_CAPACITY);

    print_pass("direct FIFO reports full status");
}

void test_fifo_wraparound()
{
    niniFIFO<std::string> fifo(DEFAULT_LANE_CAPACITY);

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

    assert(fifo.empty());

    print_pass("direct FIFO wraparound preserves order");
}

void test_fifo_empty_negative_paths()
{
    niniFIFO<std::string> fifo(DEFAULT_LANE_CAPACITY);
    bool threw = false;

    assert(fifo.pop_front() == FIFOStatus::EMPTY);
    assert(fifo.pop_front() == FIFOStatus::EMPTY);
    assert(fifo.empty());

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
    niniFIFO<std::string> fifo(DEFAULT_LANE_CAPACITY);

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

    assert(fifo.empty());

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
        receive_and_expect(
            bus,
            70,
            message,
            ReceiveStatus::Ok,
            DEFAULT_LANE_CAPACITY - i - 1);
        assert(message == "accepted-" + std::to_string(i));
    }

    receive_and_expect(bus, 70, message, ReceiveStatus::LaneEmpty, 0);
    assert(message.empty());

    print_pass("bus rejected publish is not received later");
}
#endif

int main()
{
    test_create_lane_and_subscribe();
    test_subscribe_lazily_creates_lane();
    test_receive_lazily_creates_lane_and_subscriber();
    test_direct_cfifo_cursor_registration();
    test_receive_auto_subscribes_existing_lane();
    test_multiple_subscribers_read_same_message();
    test_subscribers_advance_independently_and_catch_up();
    test_capacity_one_subscriber_does_not_repeat_message();
    test_full_capacity_cursor_wrap_and_pending_count();
    test_subscribers_can_read_at_different_speeds();
    test_late_subscriber_reads_retained_history();
    test_empty_string_message_is_distinct_from_empty_lane();

    std::cout << "All currently supported niniBUS example tests passed."
              << std::endl;
    return 0;
}
