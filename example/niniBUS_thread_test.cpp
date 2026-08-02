#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

#include "niniBUS.h"

namespace
{

struct ConsumerResult
{
    std::uint64_t received = 0;
    std::uint64_t skipped = 0;
};

void print_pass(const std::string& test_name)
{
    std::cout << "[PASS] " << test_name << '\n';
}

void publish_numbered_messages(
    niniBUS* bus,
    laneID_t lane,
    std::uint64_t message_count)
{
    for (std::uint64_t number = 0; number < message_count; ++number)
    {
        const auto result = bus->publish(lane, std::to_string(number));
        assert(result.sequenceId == number);
    }
}

void consume_numbered_messages(
    niniBUS* bus,
    laneID_t lane,
    subscriberID_t subscriber,
    std::uint64_t message_count,
    ConsumerResult* result)
{
    std::string message;

    while (result->received < message_count)
    {
        const auto received = bus->receive(lane, subscriber, message);
        if (received.status == ReceiveStatus::NO_PENDING_MESSAGE)
        {
            std::this_thread::yield();
            continue;
        }

        assert(received.status == ReceiveStatus::SUCCESS);
        assert(received.skippedMessages == 0);
        assert(received.sequenceId == result->received);
        assert(message == std::to_string(result->received));
        ++result->received;
    }
}

void publish_fixed_messages(
    niniBUS* bus,
    laneID_t lane,
    std::uint64_t message_count,
    const std::string* payload)
{
    for (std::uint64_t number = 0; number < message_count; ++number)
    {
        bus->publish(lane, *payload);
    }
}

void consume_messages_with_reclamation(
    niniBUS* bus,
    laneID_t lane,
    subscriberID_t subscriber,
    std::uint64_t message_count,
    const std::string* payload,
    ConsumerResult* result)
{
    std::string message;
    std::uint64_t next_sequence = 0;

    while (result->received + result->skipped < message_count)
    {
        const auto received = bus->receive(lane, subscriber, message);
        if (received.status == ReceiveStatus::NO_PENDING_MESSAGE)
        {
            std::this_thread::yield();
            continue;
        }

        assert(received.status == ReceiveStatus::SUCCESS);

        next_sequence += received.skippedMessages;
        assert(received.sequenceId == next_sequence);
        assert(message == *payload);

        ++result->received;
        result->skipped += received.skippedMessages;
        next_sequence = received.sequenceId + 1;
    }
}

void consume_for_two_subscribers(
    niniBUS* bus,
    laneID_t lane,
    subscriberID_t first_subscriber,
    subscriberID_t second_subscriber,
    std::uint64_t message_count,
    ConsumerResult* first_result,
    ConsumerResult* second_result)
{
    std::string first_message;
    std::string second_message;

    while (first_result->received < message_count ||
           second_result->received < message_count)
    {
        if (first_result->received < message_count)
        {
            const auto received = bus->receive(
                lane, first_subscriber, first_message);
            if (received.status == ReceiveStatus::SUCCESS)
            {
                assert(received.skippedMessages == 0);
                assert(received.sequenceId == first_result->received);
                assert(first_message ==
                       std::to_string(first_result->received));
                ++first_result->received;
            }
            else
            {
                assert(received.status ==
                       ReceiveStatus::NO_PENDING_MESSAGE);
            }
        }

        if (second_result->received < message_count)
        {
            const auto received = bus->receive(
                lane, second_subscriber, second_message);
            if (received.status == ReceiveStatus::SUCCESS)
            {
                assert(received.skippedMessages == 0);
                assert(received.sequenceId == second_result->received);
                assert(second_message ==
                       std::to_string(second_result->received));
                ++second_result->received;
            }
            else
            {
                assert(received.status ==
                       ReceiveStatus::NO_PENDING_MESSAGE);
            }
        }

        std::this_thread::yield();
    }
}

void publish_to_two_lanes(
    niniBUS* bus,
    laneID_t first_lane,
    laneID_t second_lane,
    std::uint64_t message_count)
{
    for (std::uint64_t number = 0; number < message_count; ++number)
    {
        const auto first = bus->publish(
            first_lane, "first-" + std::to_string(number));
        const auto second = bus->publish(
            second_lane, "second-" + std::to_string(number));
        assert(first.sequenceId == number);
        assert(second.sequenceId == number);
    }
}

void consume_from_two_lanes(
    niniBUS* bus,
    laneID_t first_lane,
    laneID_t second_lane,
    subscriberID_t subscriber,
    std::uint64_t message_count,
    ConsumerResult* first_result,
    ConsumerResult* second_result)
{
    std::string message;

    while (first_result->received < message_count ||
           second_result->received < message_count)
    {
        if (first_result->received < message_count)
        {
            const auto received =
                bus->receive(first_lane, subscriber, message);
            if (received.status == ReceiveStatus::SUCCESS)
            {
                assert(received.sequenceId == first_result->received);
                assert(received.skippedMessages == 0);
                assert(message == "first-" +
                    std::to_string(first_result->received));
                ++first_result->received;
            }
            else
            {
                assert(received.status ==
                       ReceiveStatus::NO_PENDING_MESSAGE);
            }
        }

        if (second_result->received < message_count)
        {
            const auto received =
                bus->receive(second_lane, subscriber, message);
            if (received.status == ReceiveStatus::SUCCESS)
            {
                assert(received.sequenceId == second_result->received);
                assert(received.skippedMessages == 0);
                assert(message == "second-" +
                    std::to_string(second_result->received));
                ++second_result->received;
            }
            else
            {
                assert(received.status ==
                       ReceiveStatus::NO_PENDING_MESSAGE);
            }
        }

        std::this_thread::yield();
    }
}

void test_concurrent_delivery_without_reclamation()
{
    constexpr laneID_t LANE = 200;
    constexpr subscriberID_t SUBSCRIBER = 2000;
    constexpr std::uint64_t MESSAGE_COUNT = 50'000;

    niniBUS bus;
    ConsumerResult result;

    // The lane can hold every message, so the consumer must receive all of
    // them in sequence without skipping.
    assert(bus.createLane(LANE, MESSAGE_COUNT) == CreateLaneStatus::Ok);
    assert(bus.subscribe(LANE, SUBSCRIBER).status == SubscribeStatus::Ok);

    std::thread consumer(
        consume_numbered_messages,
        &bus,
        LANE,
        SUBSCRIBER,
        MESSAGE_COUNT,
        &result);

    std::thread producer(
        publish_numbered_messages,
        &bus,
        LANE,
        MESSAGE_COUNT);

    producer.join();
    consumer.join();

    assert(result.received == MESSAGE_COUNT);
    assert(result.skipped == 0);
    print_pass("two threads deliver every message in order");
}

void test_concurrent_delivery_with_reclamation()
{
    constexpr laneID_t LANE = 201;
    constexpr subscriberID_t SUBSCRIBER = 2010;
    constexpr std::uint32_t CAPACITY = 64;
    constexpr std::uint64_t MESSAGE_COUNT = 200'000;
    const std::string payload(256, 'x');

    niniBUS bus;
    ConsumerResult result;

    // The producer is expected to outrun this small ring. Every published
    // message must be accounted for as either received or skipped.
    assert(bus.createLane(LANE, CAPACITY) == CreateLaneStatus::Ok);
    assert(bus.subscribe(LANE, SUBSCRIBER).status == SubscribeStatus::Ok);

    std::thread consumer(
        consume_messages_with_reclamation,
        &bus,
        LANE,
        SUBSCRIBER,
        MESSAGE_COUNT,
        &payload,
        &result);

    std::thread producer(
        publish_fixed_messages,
        &bus,
        LANE,
        MESSAGE_COUNT,
        &payload);

    producer.join();
    consumer.join();

    assert(result.received + result.skipped == MESSAGE_COUNT);
    print_pass("two threads account for received and skipped messages");
}

void test_concurrent_1024_slot_ring()
{
    constexpr laneID_t LANE = 202;
    constexpr subscriberID_t SUBSCRIBER = 2020;
    constexpr std::uint32_t CAPACITY = 1024;
    constexpr std::uint64_t MESSAGE_COUNT = 100'000;
    const std::string payload(256, 'x');

    niniBUS bus;
    ConsumerResult result;

    // Publishing far more than 1024 messages forces the ring to wrap and
    // reclaim repeatedly while the consumer is reading.
    assert(bus.createLane(LANE, CAPACITY) == CreateLaneStatus::Ok);
    assert(bus.subscribe(LANE, SUBSCRIBER).status == SubscribeStatus::Ok);

    std::thread consumer(
        consume_messages_with_reclamation,
        &bus,
        LANE,
        SUBSCRIBER,
        MESSAGE_COUNT,
        &payload,
        &result);

    std::thread producer(
        publish_fixed_messages,
        &bus,
        LANE,
        MESSAGE_COUNT,
        &payload);

    producer.join();
    consumer.join();

    assert(result.received + result.skipped == MESSAGE_COUNT);
    print_pass("1024-slot ring wraps safely with two threads");
}

void test_concurrent_empty_messages()
{
    constexpr laneID_t LANE = 203;
    constexpr subscriberID_t SUBSCRIBER = 2030;
    constexpr std::uint64_t MESSAGE_COUNT = 20'000;
    const std::string empty_payload;

    niniBUS bus;
    ConsumerResult result;
    assert(bus.createLane(LANE, MESSAGE_COUNT) == CreateLaneStatus::Ok);
    assert(bus.subscribe(LANE, SUBSCRIBER).status == SubscribeStatus::Ok);

    std::thread consumer(
        consume_messages_with_reclamation,
        &bus,
        LANE,
        SUBSCRIBER,
        MESSAGE_COUNT,
        &empty_payload,
        &result);
    std::thread producer(
        publish_fixed_messages,
        &bus,
        LANE,
        MESSAGE_COUNT,
        &empty_payload);

    producer.join();
    consumer.join();

    assert(result.received == MESSAGE_COUNT);
    assert(result.skipped == 0);
    print_pass("empty messages remain valid with two threads");
}

void test_concurrent_capacity_one_reclamation()
{
    constexpr laneID_t LANE = 204;
    constexpr subscriberID_t SUBSCRIBER = 2040;
    constexpr std::uint64_t MESSAGE_COUNT = 50'000;
    const std::string payload(256, 'x');

    niniBUS bus;
    ConsumerResult result;
    assert(bus.createLane(LANE, 1) == CreateLaneStatus::Ok);
    assert(bus.subscribe(LANE, SUBSCRIBER).status == SubscribeStatus::Ok);

    std::thread consumer(
        consume_messages_with_reclamation,
        &bus,
        LANE,
        SUBSCRIBER,
        MESSAGE_COUNT,
        &payload,
        &result);
    std::thread producer(
        publish_fixed_messages,
        &bus,
        LANE,
        MESSAGE_COUNT,
        &payload);

    producer.join();
    consumer.join();

    assert(result.received + result.skipped == MESSAGE_COUNT);
    print_pass("capacity-one lane reports reclaimed messages");
}

void test_one_consumer_reads_two_subscribers()
{
    constexpr laneID_t LANE = 205;
    constexpr subscriberID_t FIRST_SUBSCRIBER = 2050;
    constexpr subscriberID_t SECOND_SUBSCRIBER = 2051;
    constexpr std::uint64_t MESSAGE_COUNT = 20'000;

    niniBUS bus;
    ConsumerResult first_result;
    ConsumerResult second_result;
    assert(bus.createLane(LANE, MESSAGE_COUNT) == CreateLaneStatus::Ok);
    assert(bus.subscribe(LANE, FIRST_SUBSCRIBER).status ==
           SubscribeStatus::Ok);
    assert(bus.subscribe(LANE, SECOND_SUBSCRIBER).status ==
           SubscribeStatus::Ok);

    std::thread consumer(
        consume_for_two_subscribers,
        &bus,
        LANE,
        FIRST_SUBSCRIBER,
        SECOND_SUBSCRIBER,
        MESSAGE_COUNT,
        &first_result,
        &second_result);
    std::thread producer(
        publish_numbered_messages,
        &bus,
        LANE,
        MESSAGE_COUNT);

    producer.join();
    consumer.join();

    assert(first_result.received == MESSAGE_COUNT);
    assert(second_result.received == MESSAGE_COUNT);
    print_pass("one consumer thread reads two subscriber cursors");
}

void test_one_producer_and_consumer_use_two_lanes()
{
    constexpr laneID_t FIRST_LANE = 206;
    constexpr laneID_t SECOND_LANE = 207;
    constexpr subscriberID_t SUBSCRIBER = 2060;
    constexpr std::uint64_t MESSAGE_COUNT = 20'000;

    niniBUS bus;
    ConsumerResult first_result;
    ConsumerResult second_result;
    assert(bus.createLane(FIRST_LANE, MESSAGE_COUNT) ==
           CreateLaneStatus::Ok);
    assert(bus.createLane(SECOND_LANE, MESSAGE_COUNT) ==
           CreateLaneStatus::Ok);
    assert(bus.subscribe(FIRST_LANE, SUBSCRIBER).status ==
           SubscribeStatus::Ok);
    assert(bus.subscribe(SECOND_LANE, SUBSCRIBER).status ==
           SubscribeStatus::Ok);

    std::thread consumer(
        consume_from_two_lanes,
        &bus,
        FIRST_LANE,
        SECOND_LANE,
        SUBSCRIBER,
        MESSAGE_COUNT,
        &first_result,
        &second_result);
    std::thread producer(
        publish_to_two_lanes,
        &bus,
        FIRST_LANE,
        SECOND_LANE,
        MESSAGE_COUNT);

    producer.join();
    consumer.join();

    assert(first_result.received == MESSAGE_COUNT);
    assert(second_result.received == MESSAGE_COUNT);
    print_pass("one producer and consumer use two independent lanes");
}

} // namespace

int main()
{
    test_concurrent_delivery_without_reclamation();
    test_concurrent_delivery_with_reclamation();
    test_concurrent_1024_slot_ring();
    test_concurrent_empty_messages();
    test_concurrent_capacity_one_reclamation();
    test_one_consumer_reads_two_subscribers();
    test_one_producer_and_consumer_use_two_lanes();

    std::cout << "All two-thread tests passed.\n";
    return 0;
}
