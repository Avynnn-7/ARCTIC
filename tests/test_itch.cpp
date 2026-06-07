#include <gtest/gtest.h>
#include "itch_parser.hpp"
#include "replay_engine.hpp"
#include <cstring>
#include <fstream>

using namespace arctic;
using namespace arctic::itch;

// ═══════════════════════════════════════════════════════════════════════
// ITCH Parser Tests
// ═══════════════════════════════════════════════════════════════════════

TEST(ITCHParserTest, ByteSwap) {
    // Verify big-endian conversion
    uint16_t val16 = ntoh16(0x0100);  // 256 in big-endian = 1 in little-endian
    EXPECT_EQ(val16, 1);

    uint32_t val32 = ntoh32(0x01000000);
    EXPECT_EQ(val32, 1);
}

TEST(ITCHParserTest, Timestamp48) {
    uint8_t ts[6] = {0x00, 0x00, 0x00, 0x00, 0x01, 0x00}; // 256 ns
    EXPECT_EQ(ntoh48(ts), 256ULL);

    // 09:30:00.000 = 34200000000000 ns
    uint64_t ns = 34200000000000ULL;
    uint8_t ts2[6];
    ts2[0] = static_cast<uint8_t>((ns >> 40) & 0xFF);
    ts2[1] = static_cast<uint8_t>((ns >> 32) & 0xFF);
    ts2[2] = static_cast<uint8_t>((ns >> 24) & 0xFF);
    ts2[3] = static_cast<uint8_t>((ns >> 16) & 0xFF);
    ts2[4] = static_cast<uint8_t>((ns >> 8) & 0xFF);
    ts2[5] = static_cast<uint8_t>(ns & 0xFF);
    EXPECT_EQ(ntoh48(ts2), ns);
}

TEST(ITCHParserTest, MessageSizeLookup) {
    EXPECT_EQ(message_size('A'), 36);
    EXPECT_EQ(message_size('F'), 40);
    EXPECT_EQ(message_size('E'), 31);
    EXPECT_EQ(message_size('D'), 19);
    EXPECT_EQ(message_size('U'), 35);
    EXPECT_EQ(message_size('P'), 44);
    EXPECT_EQ(message_size('S'), 12);
    EXPECT_EQ(message_size('?'), -1);
}

// Counter handler for testing parse_buffer dispatch
struct CountingHandler : public MessageHandler {
    int adds = 0, execs = 0, cancels = 0, deletes = 0, trades = 0, sys = 0;
    void on_add_order(const AddOrderMsg&) override { adds++; }
    void on_add_order_mpid(const AddOrderMPIDMsg&) override { adds++; }
    void on_order_executed(const OrderExecutedMsg&) override { execs++; }
    void on_order_cancel(const OrderCancelMsg&) override { cancels++; }
    void on_order_delete(const OrderDeleteMsg&) override { deletes++; }
    void on_trade(const TradeMsg&) override { trades++; }
    void on_system_event(const SystemEventMsg&) override { sys++; }
};

TEST(ITCHParserTest, ParseSingleAddOrder) {
    // Construct a valid Add Order message with 2-byte length prefix
    uint8_t buf[2 + 36] = {};

    // Length prefix (big-endian 36)
    buf[0] = 0; buf[1] = 36;

    // Message type
    buf[2] = 'A';

    // Stock locate = 1 (big-endian)
    buf[3] = 0; buf[4] = 1;

    // Order ref = 42 (big-endian uint64)
    buf[10] = 0; buf[11] = 0; buf[12] = 0; buf[13] = 0;
    buf[14] = 0; buf[15] = 0; buf[16] = 0; buf[17] = 42;

    // Side = Buy
    buf[18] = 'B';

    // Shares = 100 (big-endian uint32)
    buf[19] = 0; buf[20] = 0; buf[21] = 0; buf[22] = 100;

    // Price = 100.0000 = 1000000 (big-endian uint32)
    uint32_t price_be = ntoh32(1000000);
    std::memcpy(&buf[31], &price_be, 4);

    CountingHandler handler;
    auto stats = parse_buffer(buf, sizeof(buf), handler);

    EXPECT_EQ(stats.messages_parsed, 1u);
    EXPECT_EQ(stats.add_orders, 1u);
    EXPECT_EQ(handler.adds, 1);
}

// ═══════════════════════════════════════════════════════════════════════
// Synthetic Generator + Replay Integration Test
// ═══════════════════════════════════════════════════════════════════════

TEST(ReplayEngineTest, GenerateAndReplay) {
    std::string test_file = "test_synthetic.bin";

    // Generate 1000 messages
    ASSERT_TRUE(ITCHGenerator::generate(test_file, 1000, 100.0, 12345));

    // Verify file exists and has content
    std::ifstream f(test_file, std::ios::binary | std::ios::ate);
    ASSERT_TRUE(f.is_open());
    EXPECT_GT(f.tellg(), 0);
    f.close();

    // Replay through a fresh OrderBook (heap allocated to prevent stack overflow)
    auto book = std::make_unique<OrderBook>(0.01, 100.0);
    auto stats = ReplayEngine::replay(test_file, *book, 0);

    EXPECT_GT(stats.parse_stats.messages_parsed, 0u);
    EXPECT_GT(stats.parse_stats.add_orders, 0u);
    EXPECT_GT(stats.total_elapsed_ms, 0.0);
    EXPECT_GT(stats.msgs_per_second, 0.0);

    // Cleanup
    std::remove(test_file.c_str());
}

TEST(ReplayEngineTest, MappedFileNonexistent) {
    MappedFile mf;
    EXPECT_FALSE(mf.open("this_file_does_not_exist.bin"));
    EXPECT_FALSE(mf.is_open());
    EXPECT_EQ(mf.data(), nullptr);
    EXPECT_EQ(mf.size(), 0u);
}
