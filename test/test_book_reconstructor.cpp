#include <gtest/gtest.h>
#include "analysis/book_reconstructor.hpp"

using namespace mda;

TEST(BookReconstructor, EmptyBook) {
    BookReconstructor book;
    EXPECT_EQ(book.best_bid(), 0);
    EXPECT_EQ(book.best_ask(), 0);
    EXPECT_TRUE(book.bids(5).empty());
    EXPECT_TRUE(book.asks(5).empty());
}

TEST(BookReconstructor, AddBidSortedDesc) {
    BookReconstructor book;
    book.on_add(1, 'B', 100, 10, 1000);
    book.on_add(2, 'B', 200, 20, 2000);
    book.on_add(3, 'B', 300, 30, 3000);
    EXPECT_EQ(book.best_bid(), 300);
    auto levels = book.bids(5);
    ASSERT_EQ(levels.size(), 3u);
    EXPECT_EQ(levels[0].price, 300); // best first
    EXPECT_EQ(levels[1].price, 200);
    EXPECT_EQ(levels[2].price, 100);
}

TEST(BookReconstructor, AddAskSortedAsc) {
    BookReconstructor book;
    book.on_add(1, 'S', 300, 10, 1000);
    book.on_add(2, 'S', 100, 20, 2000);
    book.on_add(3, 'S', 200, 30, 3000);
    EXPECT_EQ(book.best_ask(), 100);
    auto levels = book.asks(5);
    ASSERT_EQ(levels.size(), 3u);
    EXPECT_EQ(levels[0].price, 100); // best first
    EXPECT_EQ(levels[1].price, 200);
    EXPECT_EQ(levels[2].price, 300);
}

TEST(BookReconstructor, DeleteBestBid) {
    BookReconstructor book;
    book.on_add(1, 'B', 10000, 100, 1000);
    book.on_add(2, 'B', 10100, 200, 2000);
    book.on_delete(2, 3000); // remove best bid
    EXPECT_EQ(book.best_bid(), 10000);
}

TEST(BookReconstructor, SpreadAndMid) {
    BookReconstructor book;
    book.on_add(1, 'B', 14995, 200, 1000);
    book.on_add(2, 'S', 15000, 300, 2000);
    EXPECT_EQ(book.spread(),    5);
    EXPECT_EQ(book.mid_price(), 14997); // integer division
}

TEST(BookReconstructor, DepthLimit) {
    BookReconstructor book;
    for (int i = 1; i <= 10; ++i)
        book.on_add(static_cast<uint64_t>(i), 'B', i * 100, 50, 1000);
    auto bids = book.bids(3);
    EXPECT_EQ(bids.size(), 3u);
}

TEST(BookReconstructor, Execute) {
    BookReconstructor book;
    book.on_add(1, 'S', 10000, 200, 1000);
    book.on_execute(1, 100, 2000); // partial fill
    auto asks = book.asks(1);
    ASSERT_EQ(asks.size(), 1u);
    EXPECT_EQ(asks[0].qty, 100u);
}

TEST(BookReconstructor, ExecuteFullRemoves) {
    BookReconstructor book;
    book.on_add(1, 'S', 10000, 100, 1000);
    book.on_execute(1, 100, 2000); // full fill
    EXPECT_TRUE(book.asks(5).empty());
}

TEST(BookReconstructor, StaleOnMissingDelete) {
    BookReconstructor book;
    EXPECT_FALSE(book.is_stale());
    book.on_delete(9999, 1000); // order not found
    EXPECT_TRUE(book.is_stale());
}

TEST(BookReconstructor, Replace) {
    BookReconstructor book;
    book.on_add(1, 'B', 10000, 100, 1000);
    book.on_replace(1, 2, 10050, 150, 2000); // improve price and qty
    EXPECT_EQ(book.best_bid(), 10050);
    auto bids = book.bids(1);
    ASSERT_EQ(bids.size(), 1u);
    EXPECT_EQ(bids[0].qty, 150u);
}

TEST(BookReconstructor, MarkStaleManual) {
    BookReconstructor book;
    EXPECT_FALSE(book.is_stale());
    book.mark_stale();
    EXPECT_TRUE(book.is_stale());
}
