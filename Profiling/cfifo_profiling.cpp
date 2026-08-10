#include "cfifo.h"

#include <benchmark/benchmark.h>
#include <cstdint>
#include <memory>
#include <string>

constexpr nbus::SizeType fifo_size = 1024;
constexpr std::size_t message_size = 256;
constexpr nbus::cfifo<std::string>::subscriber_type subscriber_id = 1;
const std::string message(message_size, 'x');

void report_work(benchmark::State& state,
                 std::uint32_t messages,
                 const char* time_counter)
{
    state.SetItemsProcessed(state.iterations() * messages);
    state.SetBytesProcessed(state.iterations() * messages * message_size);
    state.counters[time_counter] = benchmark::Counter(
        messages,
        benchmark::Counter::kIsIterationInvariantRate |
            benchmark::Counter::kInvert);
}

static void BM_cfifo_write(benchmark::State& state)
{
    const auto writes = static_cast<std::uint32_t>(state.range(0));
    auto fifo = std::make_unique<nbus::cfifo<std::string>>(fifo_size);

    for (auto _ : state)
    {
        for (std::uint32_t i = 0; i < writes; ++i)
            benchmark::DoNotOptimize(fifo->write(message));
    }

    report_work(state, writes, "time_per_write");
    state.SetComplexityN(writes);
}

static void BM_cfifo_read(benchmark::State& state)
{
    const auto messages = static_cast<std::uint32_t>(state.range(0));
    auto fifo = std::make_unique<nbus::cfifo<std::string>>(fifo_size);
    std::string received;
    fifo->create_cursor(subscriber_id);

    for (auto _ : state)
    {
        for (std::uint32_t i = 0; i < messages; ++i)
        {
            benchmark::DoNotOptimize(fifo->write(message));
            benchmark::DoNotOptimize(
                fifo->read(subscriber_id, received));
        }
    }

    report_work(state, messages, "time_per_write_read");
    state.SetComplexityN(messages);
}

static void BM_cfifo_read_full_queue(benchmark::State& state)
{
    const auto capacity = static_cast<nbus::SizeType>(state.range(0));
    auto fifo = std::make_unique<nbus::cfifo<std::string>>(capacity);
    std::string received;
    fifo->create_cursor(subscriber_id);

    for (auto _ : state)
    {
        state.PauseTiming();
        for (nbus::SizeType i = 0; i < capacity; ++i)
            benchmark::DoNotOptimize(fifo->write(message));

        state.ResumeTiming();
        for (nbus::SizeType i = 0; i < capacity; ++i)
            benchmark::DoNotOptimize(
                fifo->read(subscriber_id, received));
    }

    report_work(state, capacity, "time_per_read");
    state.SetComplexityN(capacity);
}

BENCHMARK(BM_cfifo_write)
    ->ArgName("writes")
    ->Arg(1)
    ->Arg(10)
    ->Arg(100)
    // 256-byte messages: 1 KiB, 1 MiB, and 1 GiB of total payload.
    ->Arg((1 * 1024) / message_size)
    ->Arg((1 * 1024 * 1024) / message_size)
    ->Arg((1LL * 1024 * 1024 * 1024) / message_size)
    ->Complexity(benchmark::oN);

BENCHMARK(BM_cfifo_read)
    ->ArgName("messages")
    ->Arg(1)
    ->Arg(10)
    ->Arg(100)
    ->Arg((1 * 1024) / message_size)
    ->Arg((1 * 1024 * 1024) / message_size)
    ->Arg((1LL * 1024 * 1024 * 1024) / message_size)
    ->Complexity(benchmark::oN);

BENCHMARK(BM_cfifo_read_full_queue)
    ->ArgName("capacity")
    ->RangeMultiplier(8)
    ->Range(1, 32768)
    ->Complexity(benchmark::oN);

BENCHMARK_MAIN();
