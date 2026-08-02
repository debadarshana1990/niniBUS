#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "niniBUS.h"

namespace
{

using Clock = std::chrono::steady_clock;

constexpr std::uint32_t RING_SIZE = 1024;
constexpr std::uint32_t BUCKET_SIZE = 128;
constexpr std::uint32_t DEFAULT_TRIALS = 200;
constexpr laneID_t LANE_ID = 1;
constexpr subscriberID_t FIRST_SUBSCRIBER_ID = 1;

const std::string PAYLOAD(256, 'x');

double nanoseconds(Clock::time_point start, Clock::time_point stop)
{
    return std::chrono::duration<double, std::nano>(stop - start).count();
}

bool parse_trials(const char* argument, std::uint32_t& trials)
{
    char* end = nullptr;
    const auto value = std::strtoul(argument, &end, 10);
    if (argument == end || *end != '\0' || value == 0 ||
        value > 100'000)
    {
        return false;
    }
    trials = static_cast<std::uint32_t>(value);
    return true;
}

bool prepare_bus(
    niniBUS& bus,
    std::uint32_t subscriber_count = 1)
{
    if (bus.createLane(LANE_ID, RING_SIZE) != CreateLaneStatus::Ok)
    {
        return false;
    }

    for (std::uint32_t index = 0; index < subscriber_count; ++index)
    {
        if (bus.subscribe(
                LANE_ID, FIRST_SUBSCRIBER_ID + index).status !=
            SubscribeStatus::Ok)
        {
            return false;
        }
    }
    return true;
}

bool benchmark_read_and_write(std::uint32_t trials)
{
    double write_time = 0.0;
    double read_time = 0.0;
    std::uint64_t checksum = 0;

    for (std::uint32_t trial = 0; trial < trials; ++trial)
    {
        niniBUS bus;
        if (!prepare_bus(bus))
        {
            return false;
        }

        auto start = Clock::now();
        for (std::uint32_t slot = 0; slot < RING_SIZE; ++slot)
        {
            checksum += bus.publish(LANE_ID, PAYLOAD).sequenceId;
        }
        auto stop = Clock::now();
        write_time += nanoseconds(start, stop);

        std::string received;
        start = Clock::now();
        for (std::uint32_t slot = 0; slot < RING_SIZE; ++slot)
        {
            const auto result =
                bus.receive(LANE_ID, FIRST_SUBSCRIBER_ID, received);
            if (result.status != ReceiveStatus::SUCCESS ||
                received != PAYLOAD)
            {
                return false;
            }
            checksum += result.sequenceId;
        }
        stop = Clock::now();
        read_time += nanoseconds(start, stop);
    }

    const double operations =
        static_cast<double>(trials) * RING_SIZE;
    std::cout << "Average operation latency (no reclamation)\n"
              << "  One write: " << write_time / operations << " ns\n"
              << "  One read:  " << read_time / operations << " ns\n"
              << "  Checksum:  " << checksum << "\n\n";
    return true;
}

bool benchmark_fill_progression(std::uint32_t trials)
{
    std::array<double, RING_SIZE> slot_time{};
    double total_fill_time = 0.0;

    for (std::uint32_t trial = 0; trial < trials; ++trial)
    {
        niniBUS fill_bus;
        if (!prepare_bus(fill_bus))
        {
            return false;
        }

        const auto fill_start = Clock::now();
        for (std::uint32_t slot = 0; slot < RING_SIZE; ++slot)
        {
            fill_bus.publish(LANE_ID, PAYLOAD);
        }
        total_fill_time += nanoseconds(fill_start, Clock::now());

        // Use a separate bus because timing every slot adds clock overhead
        // and would distort the total-fill measurement above.
        niniBUS progression_bus;
        if (!prepare_bus(progression_bus))
        {
            return false;
        }
        for (std::uint32_t slot = 0; slot < RING_SIZE; ++slot)
        {
            const auto start = Clock::now();
            progression_bus.publish(LANE_ID, PAYLOAD);
            const auto stop = Clock::now();
            slot_time[slot] += nanoseconds(start, stop);
        }
    }

    std::cout << "Time to fill all " << RING_SIZE << " slots\n"
              << "  Average total: "
              << total_fill_time / trials / 1'000.0 << " us\n"
              << "  Write latency as the buffer fills "
                 "(includes clock-call overhead):\n"
              << "    Slots         Average ns/write\n";

    for (std::uint32_t first = 0; first < RING_SIZE;
         first += BUCKET_SIZE)
    {
        double bucket_total = 0.0;
        for (std::uint32_t slot = first;
             slot < first + BUCKET_SIZE; ++slot)
        {
            bucket_total += slot_time[slot];
        }
        const double bucket_average =
            bucket_total / trials / BUCKET_SIZE;
        std::cout << "    " << std::setw(4) << first + 1 << "-"
                  << std::setw(4) << first + BUCKET_SIZE << "    "
                  << std::setw(12) << bucket_average << '\n';
    }
    std::cout << '\n';
    return true;
}

struct LatencyStats
{
    double minimum;
    double median;
    double p95;
    double maximum;
    double average;
};

bool measure_reclaim(
    std::uint32_t subscriber_count,
    bool varied_cursors,
    std::uint32_t trials,
    LatencyStats& stats)
{
    std::vector<double> samples;
    samples.reserve(trials);
    std::string received;

    for (std::uint32_t trial = 0; trial < trials; ++trial)
    {
        niniBUS bus;
        if (!prepare_bus(bus, subscriber_count))
        {
            return false;
        }

        for (std::uint32_t slot = 0; slot < RING_SIZE; ++slot)
        {
            bus.publish(LANE_ID, PAYLOAD);
        }

        if (varied_cursors)
        {
            // Subscriber 0 remains at the head. The others are spread from
            // the head toward the tail, creating different cursor positions.
            for (std::uint32_t subscriber = 1;
                 subscriber < subscriber_count; ++subscriber)
            {
                const std::uint32_t reads =
                    RING_SIZE * subscriber / subscriber_count;
                for (std::uint32_t read = 0; read < reads; ++read)
                {
                    bus.receive(
                        LANE_ID,
                        FIRST_SUBSCRIBER_ID + subscriber,
                        received);
                }
            }
        }

        const auto start = Clock::now();
        const auto result = bus.publish(LANE_ID, PAYLOAD);
        const auto stop = Clock::now();
        if (result.sequenceId != RING_SIZE)
        {
            return false;
        }
        samples.push_back(nanoseconds(start, stop));

        // The slowest subscriber must be advanced to the new message.
        const auto slowest_result =
            bus.receive(LANE_ID, FIRST_SUBSCRIBER_ID, received);
        if (slowest_result.status != ReceiveStatus::SUCCESS ||
            slowest_result.sequenceId != RING_SIZE ||
            slowest_result.skippedMessages != RING_SIZE ||
            received != PAYLOAD)
        {
            return false;
        }

        if (varied_cursors && subscriber_count > 1)
        {
            // The next subscriber was ahead of the slowest cursor and must
            // remain at its original position rather than being reclaimed.
            const auto expected_sequence =
                RING_SIZE / subscriber_count;
            const auto preserved_result = bus.receive(
                LANE_ID, FIRST_SUBSCRIBER_ID + 1, received);
            if (preserved_result.status != ReceiveStatus::SUCCESS ||
                preserved_result.sequenceId != expected_sequence ||
                preserved_result.skippedMessages != 0 ||
                received != PAYLOAD)
            {
                return false;
            }
        }
        else
        {
            // Every tied subscriber should be advanced together.
            for (std::uint32_t subscriber = 1;
                 subscriber < subscriber_count; ++subscriber)
            {
                const auto tied_result = bus.receive(
                    LANE_ID,
                    FIRST_SUBSCRIBER_ID + subscriber,
                    received);
                if (tied_result.status != ReceiveStatus::SUCCESS ||
                    tied_result.sequenceId != RING_SIZE ||
                    tied_result.skippedMessages != RING_SIZE)
                {
                    return false;
                }
            }
        }
    }

    std::sort(samples.begin(), samples.end());
    double total = 0.0;
    for (const auto sample : samples)
    {
        total += sample;
    }

    stats.minimum = samples.front();
    stats.median = samples[samples.size() / 2];
    stats.p95 = samples[(samples.size() - 1) * 95 / 100];
    stats.maximum = samples.back();
    stats.average = total / samples.size();
    return true;
}

void print_reclaim_result(
    const char* scenario,
    std::uint32_t subscribers,
    const LatencyStats& stats,
    double baseline)
{
    std::cout << "    " << std::left << std::setw(27) << scenario
              << std::right << std::setw(8) << subscribers
              << std::setw(12) << stats.median
              << std::setw(12) << stats.p95
              << std::setw(14) << stats.median / baseline << "x\n";
}

bool benchmark_reclamation(std::uint32_t trials)
{
    std::cout << "Reclamation latency\n"
              << "  Timed operation is write #1025 into a full buffer.\n"
              << "  \"Typical\" is the median result. P95 means 95% of "
                 "writes completed within that time.\n"
              << "  Times are nanoseconds; 1000 ns = 1 microsecond.\n\n"
              << "    " << std::left << std::setw(27) << "Scenario"
              << std::right << std::setw(8) << "Subs"
              << std::setw(12) << "Typical"
              << std::setw(12) << "P95"
              << std::setw(14) << "vs. 1 sub" << '\n';

    LatencyStats stats{};
    if (!measure_reclaim(1, false, trials, stats))
    {
        return false;
    }
    const double baseline = std::max(1.0, stats.median);
    print_reclaim_result(
        "one slow subscriber", 1, stats, baseline);

    const std::vector<std::uint32_t> subscriber_counts = {
        2, 8, 32, 128, 256
    };
    for (const auto subscriber_count : subscriber_counts)
    {
        if (!measure_reclaim(
                subscriber_count, false, trials, stats))
        {
            return false;
        }
        print_reclaim_result(
            "multiple, same cursor",
            subscriber_count,
            stats,
            baseline);
    }

    for (const auto subscriber_count : subscriber_counts)
    {
        if (!measure_reclaim(
                subscriber_count, true, trials, stats))
        {
            return false;
        }
        print_reclaim_result(
            "multiple, varied cursors",
            subscriber_count,
            stats,
            baseline);
    }
    std::cout
        << "\n  Scenario meaning:\n"
        << "    one slow subscriber: reclaim advances that subscriber.\n"
        << "    same cursor:         reclaim advances every tied "
           "subscriber.\n"
        << "    varied cursors:      reclaim advances only the slowest; "
           "faster cursors stay unchanged.\n"
        << "\n  Ignore occasional minimum values of 0 ns from older "
           "output; a single operation can be\n"
        << "  shorter than the clock resolution. Median and P95 are more "
           "useful for comparison.\n\n";
    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    std::uint32_t trials = DEFAULT_TRIALS;
    if (argc > 2 || (argc == 2 && !parse_trials(argv[1], trials)))
    {
        std::cerr << "Usage: " << argv[0]
                  << " [trials (1-100000)]\n";
        return EXIT_FAILURE;
    }

    std::cout << std::fixed << std::setprecision(2)
              << "niniBUS 1024-slot performance test (" << trials
              << " trials, " << PAYLOAD.size() << "-byte payload)\n\n";

    if (!benchmark_read_and_write(trials) ||
        !benchmark_fill_progression(trials) ||
        !benchmark_reclamation(trials))
    {
        std::cerr << "Benchmark validation failed\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
