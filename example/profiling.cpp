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
constexpr std::uint64_t DEFAULT_MESSAGE_COUNT = 10'000;
constexpr laneID_t LANE = 500;
constexpr subscriberID_t FIRST_SUBSCRIBER = 5000;
constexpr std::size_t SLOT_SIZE = 1024 * 1024;
const std::string PAYLOAD(SLOT_SIZE, 'x');

struct Timing
{
    double writeNanoseconds = 0.0;
    double readNanoseconds = 0.0;
    double reclaimNanoseconds = 0.0;
    std::uint64_t writes = 0;
    std::uint64_t reads = 0;
    std::uint64_t reclaims = 0;
    std::uint64_t skipped = 0;
};

double elapsed_nanoseconds(
    Clock::time_point start,
    Clock::time_point stop)
{
    return std::chrono::duration<double, std::nano>(
        stop - start).count();
}

void prepare_bus(
    niniBUS* bus,
    std::uint32_t subscriber_count)
{
    assert(bus->createLane(LANE, RING_SIZE) ==
           CreateLaneStatus::Ok);

    for (std::uint32_t subscriber = 0;
         subscriber < subscriber_count; ++subscriber)
    {
        assert(bus->subscribe(
            LANE, FIRST_SUBSCRIBER + subscriber).status ==
            SubscribeStatus::Ok);
    }
}

void timed_publish(
    niniBUS* bus,
    std::uint64_t message_count,
    Timing* timing)
{
    bool next_write_reclaims = false;

    for (std::uint64_t message = 0;
         message < message_count; ++message)
    {
        const auto start = Clock::now();
        const auto result = bus->publish(LANE, PAYLOAD);
        const auto stop = Clock::now();
        const double duration =
            elapsed_nanoseconds(start, stop);

        assert(result.sequenceId == message);

        if (next_write_reclaims)
        {
            timing->reclaimNanoseconds += duration;
            ++timing->reclaims;
        }
        else
        {
            timing->writeNanoseconds += duration;
            ++timing->writes;
        }

        // credit == 0 means this write filled the final free slot.
        // Therefore the following write must run reclamation first.
        next_write_reclaims = result.credit == 0;
    }
}

void read_one_message_per_subscriber(
    niniBUS* bus,
    std::uint32_t subscriber_count,
    std::vector<std::uint64_t>* next_sequences,
    Timing* timing)
{
    std::string payload;

    for (std::uint32_t subscriber = 0;
         subscriber < subscriber_count; ++subscriber)
    {
        const auto start = Clock::now();
        const auto result = bus->receive(
            LANE, FIRST_SUBSCRIBER + subscriber, payload);
        const auto stop = Clock::now();

        assert(result.status == ReceiveStatus::SUCCESS);
        assert(payload == PAYLOAD);

        auto& next_sequence = (*next_sequences)[subscriber];
        next_sequence += result.skippedMessages;
        assert(result.sequenceId == next_sequence);
        next_sequence = result.sequenceId + 1;

        timing->readNanoseconds +=
            elapsed_nanoseconds(start, stop);
        ++timing->reads;
        timing->skipped += result.skippedMessages;
    }
}

Timing run_without_threads(
    std::uint32_t subscriber_count,
    std::uint64_t message_count)
{
    niniBUS bus;
    Timing timing;
    std::vector<std::uint64_t> next_sequences(
        subscriber_count, 0);
    prepare_bus(&bus, subscriber_count);

    bool next_write_reclaims = false;
    for (std::uint64_t message = 0;
         message < message_count; ++message)
    {
        const auto start = Clock::now();
        const auto result = bus.publish(LANE, PAYLOAD);
        const auto stop = Clock::now();
        const double duration =
            elapsed_nanoseconds(start, stop);

        assert(result.sequenceId == message);
        if (next_write_reclaims)
        {
            timing.reclaimNanoseconds += duration;
            ++timing.reclaims;
        }
        else
        {
            timing.writeNanoseconds += duration;
            ++timing.writes;
        }
        next_write_reclaims = result.credit == 0;

        read_one_message_per_subscriber(
            &bus,
            subscriber_count,
            &next_sequences,
            &timing);
    }

    return timing;
}

bool all_subscribers_finished(
    const std::vector<std::uint64_t>& accounted,
    std::uint64_t message_count)
{
    for (const auto count : accounted)
    {
        if (count < message_count)
        {
            return false;
        }
    }
    return true;
}

void threaded_consumer(
    niniBUS* bus,
    std::uint32_t subscriber_count,
    std::uint64_t message_count,
    Timing* timing)
{
    std::vector<std::uint64_t> next_sequences(
        subscriber_count, 0);
    std::vector<std::uint64_t> accounted(
        subscriber_count, 0);
    std::string payload;

    while (!all_subscribers_finished(
        accounted, message_count))
    {
        for (std::uint32_t subscriber = 0;
             subscriber < subscriber_count; ++subscriber)
        {
            if (accounted[subscriber] >= message_count)
            {
                continue;
            }

            const auto start = Clock::now();
            const auto result = bus->receive(
                LANE, FIRST_SUBSCRIBER + subscriber, payload);
            const auto stop = Clock::now();

            if (result.status ==
                ReceiveStatus::NO_PENDING_MESSAGE)
            {
                continue;
            }

            assert(result.status == ReceiveStatus::SUCCESS);
            assert(payload == PAYLOAD);

            next_sequences[subscriber] +=
                result.skippedMessages;
            assert(result.sequenceId ==
                   next_sequences[subscriber]);
            next_sequences[subscriber] =
                result.sequenceId + 1;

            timing->readNanoseconds +=
                elapsed_nanoseconds(start, stop);
            ++timing->reads;
            timing->skipped += result.skippedMessages;
            accounted[subscriber] +=
                result.skippedMessages + 1;
        }

        std::this_thread::yield();
    }
}

Timing run_with_threads(
    std::uint32_t subscriber_count,
    std::uint64_t message_count)
{
    niniBUS bus;
    Timing producer_timing;
    Timing consumer_timing;
    prepare_bus(&bus, subscriber_count);

    std::thread consumer(
        threaded_consumer,
        &bus,
        subscriber_count,
        message_count,
        &consumer_timing);
    std::thread producer(
        timed_publish,
        &bus,
        message_count,
        &producer_timing);

    producer.join();
    consumer.join();

    producer_timing.readNanoseconds =
        consumer_timing.readNanoseconds;
    producer_timing.reads = consumer_timing.reads;
    producer_timing.skipped = consumer_timing.skipped;
    return producer_timing;
}

double average(double total, std::uint64_t count)
{
    return count == 0 ? 0.0 : total / count;
}

void print_comparison(
    const char* operation,
    double uncontended,
    double contended)
{
    const double extra = contended - uncontended;
    const double slowdown =
        uncontended == 0.0 ? 0.0 : contended / uncontended;

    std::cout << "    " << std::left << std::setw(12) << operation
              << std::right << std::setw(18) << uncontended
              << std::setw(18) << contended
              << std::setw(16) << extra
              << std::setw(14) << slowdown << "x\n";
}

void print_reclaim_comparison(
    std::uint32_t subscriber_count,
    const Timing& uncontended,
    const Timing& contended)
{
    const double uncontended_ns = average(
        uncontended.reclaimNanoseconds, uncontended.reclaims);
    const double contended_ns = average(
        contended.reclaimNanoseconds, contended.reclaims);
    const double extra = contended_ns - uncontended_ns;
    const double slowdown =
        uncontended_ns == 0.0
            ? 0.0
            : contended_ns / uncontended_ns;

    std::cout << "    " << std::setw(11) << subscriber_count
              << std::setw(18) << uncontended_ns
              << std::setw(18) << contended_ns
              << std::setw(16) << extra
              << std::setw(14) << slowdown << "x"
              << std::setw(16) << contended.skipped << '\n';
}

bool parse_message_count(
    const char* argument,
    std::uint64_t* message_count)
{
    char* end = nullptr;
    const auto value = std::strtoull(argument, &end, 10);
    if (argument == end || *end != '\0' || value == 0)
    {
        return false;
    }
    *message_count = value;
    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    std::uint64_t message_count = DEFAULT_MESSAGE_COUNT;
    if (argc > 2 ||
        (argc == 2 &&
         !parse_message_count(argv[1], &message_count)))
    {
        std::cerr << "Usage: " << argv[0]
                  << " [positive-message-count]\n";
        return EXIT_FAILURE;
    }

    const std::vector<std::uint32_t> subscriber_counts = {
        1, 10, 100, 200, 500
    };

    std::cout << std::fixed << std::setprecision(2)
              << "niniBUS mutex-contention profiling\n"
              << RING_SIZE << "-slot lane, "
              << PAYLOAD.size() << "-byte payload, "
              << message_count << " published messages\n"
              << "Uncontended: calls from one thread; the mutex still "
                 "locks and unlocks.\n"
              << "Contended: one producer thread and one consumer "
                 "thread use the same bus.\n"
              << "Extra ns includes mutex waiting and thread scheduling, "
                 "not only mutex instructions.\n";

    const Timing one_subscriber_without_threads =
        run_without_threads(1, message_count);
    const Timing one_subscriber_with_threads =
        run_with_threads(1, message_count);

    std::cout
        << "\n1. Normal read and write (one subscriber)\n"
        << "   Reclaiming writes are excluded from Write ns.\n"
        << "    " << std::left << std::setw(12) << "Operation"
        << std::right << std::setw(18) << "Uncontended ns"
        << std::setw(18) << "Contended ns"
        << std::setw(16) << "Extra ns"
        << std::setw(15) << "Slowdown" << '\n';
    print_comparison(
        "Write",
        average(
            one_subscriber_without_threads.writeNanoseconds,
            one_subscriber_without_threads.writes),
        average(
            one_subscriber_with_threads.writeNanoseconds,
            one_subscriber_with_threads.writes));
    print_comparison(
        "Read",
        average(
            one_subscriber_without_threads.readNanoseconds,
            one_subscriber_without_threads.reads),
        average(
            one_subscriber_with_threads.readNanoseconds,
            one_subscriber_with_threads.reads));

    std::cout
        << "\n2. Reclaim with one subscriber\n"
        << "   Reclaim ns is the write that found the ring full.\n"
        << "    " << std::setw(11) << "Subscribers"
        << std::setw(18) << "Uncontended ns"
        << std::setw(18) << "Contended ns"
        << std::setw(16) << "Extra ns"
        << std::setw(15) << "Slowdown"
        << std::setw(16) << "Thread skips" << '\n';
    print_reclaim_comparison(
        1,
        one_subscriber_without_threads,
        one_subscriber_with_threads);

    std::cout
        << "\n3. Multiple subscribers with reclaim\n"
        << "   One consumer thread reads every subscriber cursor.\n"
        << "    " << std::setw(11) << "Subscribers"
        << std::setw(18) << "Uncontended ns"
        << std::setw(18) << "Contended ns"
        << std::setw(16) << "Extra ns"
        << std::setw(15) << "Slowdown"
        << std::setw(16) << "Thread skips" << '\n';

    for (std::size_t index = 1;
         index < subscriber_counts.size(); ++index)
    {
        const auto subscriber_count = subscriber_counts[index];
        const Timing without_threads = run_without_threads(
            subscriber_count, message_count);
        const Timing with_threads = run_with_threads(
            subscriber_count, message_count);
        print_reclaim_comparison(
            subscriber_count,
            without_threads,
            with_threads);
    }

    return EXIT_SUCCESS;
}
