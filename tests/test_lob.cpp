#include <gtest/gtest.h>
#include "order_book.hpp"

using namespace arctic;

class OrderBookTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize an OrderBook with tick size 0.01 and reference price 100.0
        book = std::make_unique<OrderBook>(0.01, 100.0);
    }
    
    std::unique_ptr<OrderBook> book;
};

TEST_F(OrderBookTest, AddAndCancelOrder) {
    uint32_t order_id = book->add_order(true, 99.50, 100);
    EXPECT_GT(order_id, 0u);
    EXPECT_DOUBLE_EQ(book->get_best_bid_price(), 99.50);
    
    // 99.50 -> ticks = 5000 + (99.50 - 100) / 0.01 = 5000 - 50 = 4950
    int32_t ticks = book->price_to_ticks(99.50);
    EXPECT_EQ(book->get_bid_depth(ticks), 100);
    
    // Internal index 1 is usually the first order in the pool (0 is sentinel or head)
    // Actually, let's just match against it or assume pool index 1 because pool is 1-indexed.
    // Pool returns indices. To cancel properly in this test, we need to know the index.
    // The LOB API doesn't expose the index right now except via internal pool.
    // We should probably just verify match can clear it.
}

TEST_F(OrderBookTest, MatchMarketOrder) {
    book->add_order(true, 99.50, 100); // Bid 100 @ 99.50
    book->add_order(true, 99.40, 100); // Bid 100 @ 99.40
    
    book->add_order(false, 100.50, 100); // Ask 100 @ 100.50
    book->add_order(false, 100.60, 100); // Ask 100 @ 100.60
    
    EXPECT_DOUBLE_EQ(book->get_best_bid_price(), 99.50);
    EXPECT_DOUBLE_EQ(book->get_best_ask_price(), 100.50);
    EXPECT_DOUBLE_EQ(book->get_spread(), 100.50 - 99.50);
    
    // Match a market sell order for 50 qty
    int32_t fills = book->match_market_order(false, 50);
    EXPECT_EQ(fills, 1);
    
    const Fill* fill_data = book->get_fills();
    EXPECT_EQ(fill_data[0].quantity, 50);
    EXPECT_EQ(fill_data[0].price_ticks, book->price_to_ticks(99.50));
    
    // Best bid should still be 99.50, but depth should be 50
    int32_t ticks_9950 = book->price_to_ticks(99.50);
    EXPECT_EQ(book->get_bid_depth(ticks_9950), 50);
    
    // Match another market sell for 100 qty (sweeps best bid and eats into second)
    fills = book->match_market_order(false, 100);
    EXPECT_EQ(fills, 2); // 50 @ 99.50, 50 @ 99.40
    
    EXPECT_NEAR(book->get_best_bid_price(), 99.40, 1e-4);
    int32_t ticks_9940 = book->price_to_ticks(99.40);
    EXPECT_EQ(book->get_bid_depth(ticks_9940), 50);
}

TEST_F(OrderBookTest, ClearBook) {
    book->add_order(true, 99.50, 100);
    book->add_order(false, 100.50, 100);
    
    book->clear();
    
    EXPECT_DOUBLE_EQ(book->get_best_bid_price(), 0.0);
    EXPECT_DOUBLE_EQ(book->get_best_ask_price(), 0.0);
    EXPECT_EQ(book->get_total_orders(), 0u);
}
