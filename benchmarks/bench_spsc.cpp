/**
 * SPSC Ring Buffer Microbenchmarks
 *
 * Measures push/pop latency with percentile distribution (p50, p90, p99, p99.9).
 * Uses RDTSCP for sub-nanosecond timing resolution.
 *
 * Build with: cmake -DARCTIC_BUILD_BENCHMARKS=ON ..
 * Run with:   ./bench_spsc --benchmark_format=console
 */

#include <benchmark/benchmark.h>
#include "spsc_buffer.hpp"
#include "tsc_clock.hpp"
#include "order_book.hpp"
#include "arena_allocator.hpp"
#include <vector>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <thread>
#include <atomic>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#endif

void pin_thread_to_core(int core_id) {
#ifdef _WIN32
    DWORD_PTR mask = (static_cast<DWORD_PTR>(1) << core_id);
    SetThreadAffinityMask(GetCurrentThread(), mask);
#else
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif
}

// ═══════════════════════════════════════════════════════════════════════
// SPSC Push Latency (single-threaded, measures push overhead in isolation)
// ═══════════════════════════════════════════════════════════════════════

static void BM_SPSC_Push(benchmark::State& state) {
    arctic::SPSCBuffer<double> buf(1024);
    double val = 42.0;
    // Keep popping to prevent full buffer
    for (auto _ : state) {
        buf.push(val);
        double out;
        buf.pop(out);
        benchmark::DoNotOptimize(out);
    }
}
BENCHMARK(BM_SPSC_Push)->Iterations(1000000);

// ═══════════════════════════════════════════════════════════════════════
// SPSC Push/Pop Round-Trip with TSC Percentile Measurement
// ═══════════════════════════════════════════════════════════════════════

static void BM_SPSC_Latency_Distribution(benchmark::State& state) {
    arctic::SPSCBuffer<double> buf(1024);
    arctic::TscClock tsc;
    
    constexpr int NUM_SAMPLES = 100000;
    std::vector<double> latencies_ns;
    latencies_ns.reserve(NUM_SAMPLES);
    
    for (auto _ : state) {
        latencies_ns.clear();
        
        for (int i = 0; i < NUM_SAMPLES; ++i) {
            double val = static_cast<double>(i);
            
            uint64_t t0 = arctic::TscClock::rdtscp();
            buf.push(val);
            double out;
            buf.pop(out);
            uint64_t t1 = arctic::TscClock::rdtscp();
            
            benchmark::DoNotOptimize(out);
            latencies_ns.push_back(tsc.ticks_to_ns(t1 - t0));
        }
        
        std::sort(latencies_ns.begin(), latencies_ns.end());
        
        size_t n = latencies_ns.size();
        state.counters["p50_ns"]   = latencies_ns[n * 50 / 100];
        state.counters["p90_ns"]   = latencies_ns[n * 90 / 100];
        state.counters["p99_ns"]   = latencies_ns[n * 99 / 100];
        state.counters["p99.9_ns"] = latencies_ns[n * 999 / 1000];
        state.counters["min_ns"]   = latencies_ns[0];
        state.counters["max_ns"]   = latencies_ns[n - 1];
    }
}
BENCHMARK(BM_SPSC_Latency_Distribution)->Iterations(1)->Unit(benchmark::kNanosecond);

// ═══════════════════════════════════════════════════════════════════════
// SPSC Cross-Thread Latency (producer/consumer on separate threads)
// ═══════════════════════════════════════════════════════════════════════

static void BM_SPSC_CrossThread(benchmark::State& state) {
    arctic::SPSCBuffer<uint64_t> buf(1024);
    arctic::TscClock tsc;
    std::atomic<bool> running{true};
    
    constexpr int NUM_SAMPLES = 50000;
    std::vector<double> latencies_ns;
    latencies_ns.reserve(NUM_SAMPLES);
    
    for (auto _ : state) {
        latencies_ns.clear();
        running.store(true);
        
        // Consumer thread: pop and measure latency
        std::thread consumer([&]() {
            pin_thread_to_core(1); // Pin consumer to core 1
            for (int i = 0; i < NUM_SAMPLES; ++i) {
                uint64_t tsc_sent;
                while (!buf.pop(tsc_sent)) {
                    // Spin-wait (realistic for latency-sensitive consumer)
                }
                uint64_t tsc_recv = arctic::TscClock::rdtscp();
                latencies_ns.push_back(tsc.ticks_to_ns(tsc_recv - tsc_sent));
            }
        });
        
        // Producer: push TSC timestamp
        pin_thread_to_core(0); // Pin producer to core 0
        for (int i = 0; i < NUM_SAMPLES; ++i) {
            uint64_t ts = arctic::TscClock::rdtscp();
            while (!buf.push(ts)) {
                // Spin if full
            }
        }
        
        consumer.join();
        
        std::sort(latencies_ns.begin(), latencies_ns.end());
        size_t n = latencies_ns.size();
        state.counters["p50_ns"]   = latencies_ns[n * 50 / 100];
        state.counters["p90_ns"]   = latencies_ns[n * 90 / 100];
        state.counters["p99_ns"]   = latencies_ns[n * 99 / 100];
        state.counters["p99.9_ns"] = latencies_ns[n * 999 / 1000];
    }
}
BENCHMARK(BM_SPSC_CrossThread)->Iterations(1)->Unit(benchmark::kNanosecond);

// ═══════════════════════════════════════════════════════════════════════
// Order Book: Add Order Latency
// ═══════════════════════════════════════════════════════════════════════

static void BM_OrderBook_AddOrder(benchmark::State& state) {
    arctic::TscClock tsc;
    
    constexpr int NUM_SAMPLES = 50000; // Must fit within LOB_MAX_ORDERS (65536)
    std::vector<double> latencies_ns;
    latencies_ns.reserve(NUM_SAMPLES);
    
    for (auto _ : state) {
        latencies_ns.clear();
        // Fresh book each iteration (heap-allocated — OrderBook is ~5MB due to pool)
        auto fresh_book = std::make_unique<arctic::OrderBook>(0.01, 100.0);
        
        for (int i = 0; i < NUM_SAMPLES; ++i) {
            double price = 99.50 + (i % 100) * 0.01;
            bool is_buy = (i % 2 == 0);
            
            uint64_t t0 = arctic::TscClock::rdtscp();
            auto id = fresh_book->add_order(is_buy, price, 100);
            uint64_t t1 = arctic::TscClock::rdtscp();
            
            benchmark::DoNotOptimize(id);
            latencies_ns.push_back(tsc.ticks_to_ns(t1 - t0));
        }
        
        std::sort(latencies_ns.begin(), latencies_ns.end());
        size_t n = latencies_ns.size();
        state.counters["p50_ns"]   = latencies_ns[n * 50 / 100];
        state.counters["p90_ns"]   = latencies_ns[n * 90 / 100];
        state.counters["p99_ns"]   = latencies_ns[n * 99 / 100];
        state.counters["p99.9_ns"] = latencies_ns[n * 999 / 1000];
    }
}
BENCHMARK(BM_OrderBook_AddOrder)->Iterations(1)->Unit(benchmark::kNanosecond);

// ═══════════════════════════════════════════════════════════════════════
// Order Book: Match Market Order Latency
// ═══════════════════════════════════════════════════════════════════════

static void BM_OrderBook_Match(benchmark::State& state) {
    arctic::TscClock tsc;
    
    constexpr int NUM_SAMPLES = 1000; // Each creates a fresh heap-allocated book
    std::vector<double> latencies_ns;
    latencies_ns.reserve(NUM_SAMPLES);
    
    for (auto _ : state) {
        latencies_ns.clear();
        
        for (int i = 0; i < NUM_SAMPLES; ++i) {
            // Seed a fresh book with liquidity (heap-allocated)
            auto book = std::make_unique<arctic::OrderBook>(0.01, 100.0);
            book->seed_liquidity(100.0, 0.05, 10, 100);
            
            uint64_t t0 = arctic::TscClock::rdtscp();
            int fills = book->match_market_order(true, 50);
            uint64_t t1 = arctic::TscClock::rdtscp();
            
            benchmark::DoNotOptimize(fills);
            latencies_ns.push_back(tsc.ticks_to_ns(t1 - t0));
        }
        
        std::sort(latencies_ns.begin(), latencies_ns.end());
        size_t n = latencies_ns.size();
        state.counters["p50_ns"]   = latencies_ns[n * 50 / 100];
        state.counters["p90_ns"]   = latencies_ns[n * 90 / 100];
        state.counters["p99_ns"]   = latencies_ns[n * 99 / 100];
        state.counters["p99.9_ns"] = latencies_ns[n * 999 / 1000];
    }
}
BENCHMARK(BM_OrderBook_Match)->Iterations(1)->Unit(benchmark::kNanosecond);

// ═══════════════════════════════════════════════════════════════════════
// Arena Allocator: Allocate/Reset cycle
// ═══════════════════════════════════════════════════════════════════════

static void BM_ArenaAllocator(benchmark::State& state) {
    arctic::ArenaAllocator arena(1024 * 1024); // 1MB
    
    for (auto _ : state) {
        double* buf = arena.allocate<double>(1000);
        buf[0] = 42.0;
        benchmark::DoNotOptimize(buf);
        arena.reset();
    }
}
BENCHMARK(BM_ArenaAllocator)->Iterations(1000000);

// ═══════════════════════════════════════════════════════════════════════
// Baseline: std::vector allocation (for comparison with arena)
// ═══════════════════════════════════════════════════════════════════════

static void BM_StdVectorAlloc(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<double> v(1000);
        v[0] = 42.0;
        benchmark::DoNotOptimize(v.data());
    }
}
BENCHMARK(BM_StdVectorAlloc)->Iterations(1000000);

// ═══════════════════════════════════════════════════════════════════════
// TSC Overhead: Cost of reading the timestamp counter
// ═══════════════════════════════════════════════════════════════════════

static void BM_TSC_Overhead(benchmark::State& state) {
    for (auto _ : state) {
        uint64_t ts = arctic::TscClock::rdtscp();
        benchmark::DoNotOptimize(ts);
    }
}
BENCHMARK(BM_TSC_Overhead)->Iterations(10000000);

BENCHMARK_MAIN();
