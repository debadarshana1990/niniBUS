#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "niniBUS.h"

namespace
{

using Clock = std::chrono::steady_clock;

constexpr std::uint32_t RING_SIZE = 1024;
constexpr std::uint64_t DEFAULT_MESSAGES_PER_LANE = 1'000'000;
const std::string PAYLOAD(256, 'x');

struct ProfileCase
{
    const char* name;
    std::uint32_t laneCount;
    std::uint32_t subscribersPerLane;
};

struct CursorResult
{
    std::uint64_t received = 0;
    std::uint64_t skipped = 0;
    std::uint64_t nextSequence = 0;
};

void publish_messages(
    niniBUS* bus,
    const std::vector<laneID_t>* lanes,
    std::uint64_t messages_per_lane)
{
    for (std::uint64_t message = 0;
         message < messages_per_lane; ++message)
    {
        for (const auto lane : *lanes)
        {
            const auto result = bus->publish(lane, PAYLOAD);
            assert(result.sequenceId == message);
        }
    }
}

bool all_messages_accounted_for(
    const std::vector<CursorResult>& results,
    std::uint64_t messages_per_lane)
{
    for (const auto& result : results)
    {
        if (result.received + result.skipped < messages_per_lane)
        {
            return false;
        }
    }
    return true;
}

void consume_messages(
    niniBUS* bus,
    const std::vector<laneID_t>* lanes,
    std::uint32_t subscribers_per_lane,
    std::uint64_t messages_per_lane,
    std::vector<CursorResult>* results)
{
    std::string message;

    while (!all_messages_accounted_for(
        *results, messages_per_lane))
    {
        for (std::uint32_t lane_index = 0;
             lane_index < lanes->size(); ++lane_index)
        {
            const laneID_t lane = (*lanes)[lane_index];

            for (std::uint32_t subscriber = 0;
                 subscriber < subscribers_per_lane; ++subscriber)
            {
                const std::uint32_t result_index =
                    lane_index * subscribers_per_lane + subscriber;
                auto& cursor = (*results)[result_index];

                if (cursor.received + cursor.skipped >=
                    messages_per_lane)
                {
                    continue;
                }

                const subscriberID_t subscriber_id =
                    lane * 1000 + subscriber;
                const auto received =
                    bus->receive(lane, subscriber_id, message);

                if (received.status ==
                    ReceiveStatus::NO_PENDING_MESSAGE)
                {
                    continue;
                }

                assert(received.status == ReceiveStatus::SUCCESS);
                cursor.nextSequence += received.skippedMessages;
                assert(received.sequenceId == cursor.nextSequence);
                assert(message == PAYLOAD);

                ++cursor.received;
                cursor.skipped += received.skippedMessages;
                cursor.nextSequence = received.sequenceId + 1;
            }
        }

        std::this_thread::yield();
    }
}

void run_profile_case(
    const ProfileCase& profile,
    std::uint64_t messages_per_lane)
{
    niniBUS bus;
    std::vector<laneID_t> lanes;

    for (std::uint32_t lane_index = 0;
         lane_index < profile.laneCount; ++lane_index)
    {
        const laneID_t lane = 300 + lane_index;
        lanes.push_back(lane);
        assert(bus.createLane(lane, RING_SIZE) ==
               CreateLaneStatus::Ok);

        for (std::uint32_t subscriber = 0;
             subscriber < profile.subscribersPerLane; ++subscriber)
        {
            const subscriberID_t subscriber_id =
                lane * 1000 + subscriber;
            assert(bus.subscribe(lane, subscriber_id).status ==
                   SubscribeStatus::Ok);
        }
    }

    std::vector<CursorResult> results(
        profile.laneCount * profile.subscribersPerLane);

    const auto start = Clock::now();

    std::thread consumer(
        consume_messages,
        &bus,
        &lanes,
        profile.subscribersPerLane,
        messages_per_lane,
        &results);

    std::thread producer(
        publish_messages,
        &bus,
        &lanes,
        messages_per_lane);

    producer.join();
    consumer.join();

    const auto stop = Clock::now();
    const std::chrono::duration<double> elapsed = stop - start;

    std::uint64_t total_received = 0;
    std::uint64_t total_skipped = 0;
    for (const auto& result : results)
    {
        assert(result.received + result.skipped ==
               messages_per_lane);
        total_received += result.received;
        total_skipped += result.skipped;
    }

    const std::uint64_t total_published =
        messages_per_lane * profile.laneCount;
    const double publishes_per_second =
        total_published / elapsed.count();

    std::cout << std::left << std::setw(26) << profile.name
              << std::right << std::setw(8) << profile.laneCount
              << std::setw(10) << profile.subscribersPerLane
              << std::setw(12) << std::fixed << std::setprecision(3)
              << elapsed.count()
              << std::setw(16) << std::fixed << std::setprecision(0)
              << publishes_per_second
              << std::setw(14) << total_received
              << std::setw(14) << total_skipped << '\n';
}

bool parse_message_count(
    const char* argument,
    std::uint64_t& message_count)
{
    char* end = nullptr;
    const auto value = std::strtoull(argument, &end, 10);
    if (argument == end || *end != '\0' || value == 0)
    {
        return false;
    }
    message_count = value;
    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    std::uint64_t messages_per_lane =
        DEFAULT_MESSAGES_PER_LANE;
    if (argc > 2 ||
        (argc == 2 &&
         !parse_message_count(argv[1], messages_per_lane)))
    {
        std::cerr << "Usage: " << argv[0]
                  << " [positive-messages-per-lane]\n";
        return EXIT_FAILURE;
    }

    const std::vector<ProfileCase> profiles = {
        {"one lane, one subscriber", 1, 1},
        {"one lane, 8 subscribers", 1, 8},
        {"4 lanes, one subscriber", 4, 1},
        {"4 lanes, 8 subscribers", 4, 8},
    };

    std::cout
        << "Threaded niniBUS profiling\n"
        << "One producer thread, one consumer thread, "
        << RING_SIZE << "-slot lanes, " << PAYLOAD.size()
        << "-byte payload\n"
        << messages_per_lane << " messages published per lane\n\n"
        << std::left << std::setw(26) << "Case"
        << std::right << std::setw(8) << "Lanes"
        << std::setw(10) << "Subs/lane"
        << std::setw(12) << "Time(s)"
        << std::setw(16) << "Publishes/s"
        << std::setw(14) << "Received"
        << std::setw(14) << "Skipped" << '\n'
        << std::string(100, '-') << '\n';

    for (const auto& profile : profiles)
    {
        run_profile_case(profile, messages_per_lane);
    }

    return EXIT_SUCCESS;
}
