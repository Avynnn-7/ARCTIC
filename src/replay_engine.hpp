#pragma once
/**
 * @file replay_engine.hpp
 * @brief Memory-Mapped ITCH Replay Engine
 *
 * Maps an ITCH binary file into virtual memory and replays it through
 * the LOB at configurable speed. Measures per-message processing latency
 * and reports throughput statistics.
 *
 * Architecture:
 *   File on disk
 *     -> CreateFileMapping / mmap (zero-copy into address space)
 *       -> itch::parse_buffer (zero-copy reinterpret_cast over mapped pages)
 *         -> OrderBook::add_order / cancel_order_by_index (O(1) flat array ops)
 *
 * No heap allocations in the hot path. No copies. No parsing overhead.
 * This is how firms like Jump and Citadel replay historical data.
 */

#include "itch_parser.hpp"
#include "order_book.hpp"
#include "tsc_clock.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

namespace arctic {

// ═══════════════════════════════════════════════════════════════════════
// Memory-Mapped File Wrapper (Windows + POSIX)
// ═══════════════════════════════════════════════════════════════════════

class MappedFile {
public:
    MappedFile() = default;
    ~MappedFile();

    // Non-copyable
    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;

    // Move
    MappedFile(MappedFile&& other) noexcept;
    MappedFile& operator=(MappedFile&& other) noexcept;

    bool open(const std::string& path);
    void close();

    const uint8_t* data() const { return data_; }
    size_t size() const { return size_; }
    bool is_open() const { return data_ != nullptr; }

private:
    uint8_t* data_ = nullptr;
    size_t size_ = 0;

#ifdef _WIN32
    void* file_handle_ = nullptr;  // HANDLE
    void* map_handle_  = nullptr;  // HANDLE
#else
    int fd_ = -1;
#endif
};

// ═══════════════════════════════════════════════════════════════════════
// Replay Statistics
// ═══════════════════════════════════════════════════════════════════════

struct ReplayStats {
    itch::ParseStats parse_stats;

    // Timing
    double total_elapsed_ms   = 0.0;
    double msgs_per_second    = 0.0;
    double bytes_per_second   = 0.0;

    // LOB state at end of replay
    double best_bid           = 0.0;
    double best_ask           = 0.0;
    double spread             = 0.0;
    size_t total_orders       = 0;

    // Per-message latency percentiles (nanoseconds)
    double p50_ns = 0.0;
    double p90_ns = 0.0;
    double p99_ns = 0.0;
    double p999_ns = 0.0;
    double min_ns = 0.0;
    double max_ns = 0.0;
};

// ═══════════════════════════════════════════════════════════════════════
// LOB Message Handler — bridges ITCH messages to the OrderBook
// ═══════════════════════════════════════════════════════════════════════

class LOBHandler : public itch::MessageHandler {
public:
    explicit LOBHandler(OrderBook* book);

    void on_add_order(const itch::AddOrderMsg& msg) override;
    void on_add_order_mpid(const itch::AddOrderMPIDMsg& msg) override;
    void on_order_executed(const itch::OrderExecutedMsg& msg) override;
    void on_order_executed_price(const itch::OrderExecutedPriceMsg& msg) override;
    void on_order_cancel(const itch::OrderCancelMsg& msg) override;
    void on_order_delete(const itch::OrderDeleteMsg& msg) override;
    void on_order_replace(const itch::OrderReplaceMsg& msg) override;

    // Filter to a single stock (by locate code). 0 = accept all.
    void set_stock_filter(uint16_t locate) { stock_filter_ = locate; }

    size_t orders_applied() const { return orders_applied_; }
    size_t orders_rejected() const { return orders_rejected_; }

private:
    OrderBook* book_;
    uint16_t stock_filter_ = 0;
    size_t orders_applied_ = 0;
    size_t orders_rejected_ = 0;

    // Map ITCH order_ref -> pool index in our OrderBook
    std::unordered_map<uint64_t, int32_t> ref_to_index_;

    void handle_add(uint64_t ref, bool is_buy, double price, int32_t shares, uint16_t locate);
    void handle_cancel(uint64_t ref, int32_t shares);
    void handle_delete(uint64_t ref);
};

// ═══════════════════════════════════════════════════════════════════════
// Replay Engine
// ═══════════════════════════════════════════════════════════════════════

class ReplayEngine {
public:
    /**
     * Replay an ITCH binary file through the given OrderBook.
     *
     * @param path        Path to the .itch or .bin file
     * @param book        Target OrderBook
     * @param locate      Stock locate filter (0 = all stocks)
     * @return            Replay statistics
     */
    static ReplayStats replay(const std::string& path, OrderBook& book, uint16_t locate = 0);
};

// ═══════════════════════════════════════════════════════════════════════
// Synthetic ITCH Data Generator (for testing without real exchange data)
// ═══════════════════════════════════════════════════════════════════════

class ITCHGenerator {
public:
    /**
     * Generate a synthetic ITCH binary file with realistic order flow.
     *
     * @param path        Output file path
     * @param num_msgs    Number of messages to generate
     * @param ref_price   Reference mid price (e.g. 100.0)
     * @param seed        RNG seed for reproducibility
     */
    static bool generate(const std::string& path, int num_msgs, 
                         double ref_price = 100.0, uint64_t seed = 42);
};

} // namespace arctic
