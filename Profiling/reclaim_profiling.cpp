#include "cfifo.h"

#include <benchmark/benchmark.h>
#include <chrono>
#include <cstdint>
#include <string>

namespace
{

constexpr nbus::SizeType fifo_capacity = 1024;
constexpr std::size_t message_size = 256;
constexpr std::uint32_t maximum_subscribers = 128;

const std::string message(message_size, 'x');
nbus::cfifo<std::string> fifo(fifo_capacity);

enum class CursorLayout
{
    ONE_HEAD_REST_TAIL,
    ONE_MID_REST_TAIL,
    ONE_TAIL_MINUS_1_REST_TAIL,
    ALL_HEAD,
    ALL_MID,
    ALL_TAIL_MINUS_1,
    ALL_TAIL,
    HEAD_MID_TAIL
};

void advance_cursor(nbus::cfifo<std::string>::subscriber_type id,
                    nbus::SizeType reads)
{
    std::string received;
    for (nbus::SizeType i = 0; i < reads; ++i)
        benchmark::DoNotOptimize(fifo.read(id, received));
}

void prepare_full_fifo(std::uint32_t subscribers, CursorLayout layout)
{
    // Remove cursor state left by the preceding benchmark iteration.
    for (std::uint32_t id = 1; id <= maximum_subscribers; ++id)
        fifo.remove_cursor(id);

    // Normalize retained storage to one unreferenced message.
    while (!fifo.full())
        benchmark::DoNotOptimize(fifo.write(message));
    benchmark::DoNotOptimize(fifo.write(message));

    if (subscribers == 0)
    {
        while (!fifo.full())
            benchmark::DoNotOptimize(fifo.write(message));
        return;
    }

    for (std::uint32_t id = 1; id <= subscribers; ++id)
        fifo.create_cursor(id);

    // One slot contains history from before cursor registration. The other
    // slots form the subscriber-visible full-queue window.
    constexpr nbus::SizeType visible_messages = fifo_capacity - 1;
    for (nbus::SizeType i = 0; i < visible_messages; ++i)
        benchmark::DoNotOptimize(fifo.write(message));

    const nbus::SizeType middle = visible_messages / 2;
    const nbus::SizeType tail_minus_1 = visible_messages - 1;

    for (std::uint32_t id = 1; id <= subscribers; ++id)
    {
        nbus::SizeType reads = 0;
        switch (layout)
        {
            case CursorLayout::ONE_HEAD_REST_TAIL:
                reads = id == 1 ? 0 : visible_messages;
                break;
            case CursorLayout::ONE_MID_REST_TAIL:
                reads = id == 1 ? middle : visible_messages;
                break;
            case CursorLayout::ONE_TAIL_MINUS_1_REST_TAIL:
                reads = id == 1 ? tail_minus_1 : visible_messages;
                break;
            case CursorLayout::ALL_HEAD:
                reads = 0;
                break;
            case CursorLayout::ALL_MID:
                reads = middle;
                break;
            case CursorLayout::ALL_TAIL_MINUS_1:
                reads = tail_minus_1;
                break;
            case CursorLayout::ALL_TAIL:
                reads = visible_messages;
                break;
            case CursorLayout::HEAD_MID_TAIL:
                reads = id == 1 ? 0 :
                    (id == 2 ? middle : visible_messages);
                break;
        }
        advance_cursor(id, reads);
    }
}

void run_reclaim_benchmark(benchmark::State& state,
                           std::uint32_t subscribers,
                           CursorLayout layout)
{
    for (auto _ : state)
    {
        prepare_full_fifo(subscribers, layout);

        // The FIFO is full, so this write invokes reclaim().
        const auto start = std::chrono::steady_clock::now();
        benchmark::DoNotOptimize(fifo.write(message));
        const auto end = std::chrono::steady_clock::now();
        state.SetIterationTime(
            std::chrono::duration<double>(end - start).count());
    }

    state.SetItemsProcessed(state.iterations());
    state.counters["time_per_reclaiming_write"] = benchmark::Counter(
        1,
        benchmark::Counter::kIsIterationInvariantRate |
            benchmark::Counter::kInvert);
}

static void BM_reclaim(benchmark::State& state,
                       std::uint32_t subscribers,
                       CursorLayout layout)
{
    run_reclaim_benchmark(state, subscribers, layout);
}

static void BM_reclaim_subscriber_scaling(benchmark::State& state)
{
    const auto subscribers = static_cast<std::uint32_t>(state.range(0));
    run_reclaim_benchmark(
        state, subscribers, CursorLayout::ALL_HEAD);
    state.SetComplexityN(subscribers);
}

BENCHMARK_CAPTURE(
    BM_reclaim, no_subscribers, 0, CursorLayout::ALL_HEAD)
    ->Iterations(1000)
    ->UseManualTime();

BENCHMARK_CAPTURE(
    BM_reclaim, one_head, 1, CursorLayout::ONE_HEAD_REST_TAIL)
    ->Iterations(1000)
    ->UseManualTime();
BENCHMARK_CAPTURE(
    BM_reclaim, one_mid, 1, CursorLayout::ONE_MID_REST_TAIL)
    ->Iterations(1000)
    ->UseManualTime();
BENCHMARK_CAPTURE(
    BM_reclaim, one_tail_minus_1, 1,
    CursorLayout::ONE_TAIL_MINUS_1_REST_TAIL)
    ->Iterations(1000)
    ->UseManualTime();
BENCHMARK_CAPTURE(
    BM_reclaim, one_tail, 1, CursorLayout::ALL_TAIL)
    ->Iterations(1000)
    ->UseManualTime();

BENCHMARK_CAPTURE(
    BM_reclaim, ten_one_at_head, 10,
    CursorLayout::ONE_HEAD_REST_TAIL)
    ->Iterations(1000)
    ->UseManualTime();
BENCHMARK_CAPTURE(
    BM_reclaim, ten_one_at_mid, 10,
    CursorLayout::ONE_MID_REST_TAIL)
    ->Iterations(1000)
    ->UseManualTime();
BENCHMARK_CAPTURE(
    BM_reclaim, ten_one_at_tail_minus_1, 10,
    CursorLayout::ONE_TAIL_MINUS_1_REST_TAIL)
    ->Iterations(1000)
    ->UseManualTime();
BENCHMARK_CAPTURE(
    BM_reclaim, ten_all_at_tail, 10, CursorLayout::ALL_TAIL)
    ->Iterations(1000)
    ->UseManualTime();

BENCHMARK_CAPTURE(
    BM_reclaim, ten_all_at_head, 10, CursorLayout::ALL_HEAD)
    ->Iterations(1000)
    ->UseManualTime();
BENCHMARK_CAPTURE(
    BM_reclaim, ten_all_at_mid, 10, CursorLayout::ALL_MID)
    ->Iterations(1000)
    ->UseManualTime();
BENCHMARK_CAPTURE(
    BM_reclaim, ten_head_mid_tail, 10,
    CursorLayout::HEAD_MID_TAIL)
    ->Iterations(1000)
    ->UseManualTime();

BENCHMARK(BM_reclaim_subscriber_scaling)
    ->ArgName("subscribers")
    ->RangeMultiplier(2)
    ->Range(1, maximum_subscribers)
    ->Iterations(1000)
    ->UseManualTime()
    ->Complexity(benchmark::oN);

}  // namespace

BENCHMARK_MAIN();
