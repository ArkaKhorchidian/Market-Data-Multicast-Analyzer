#include <gtest/gtest.h>
#include "analysis/gap_detector.hpp"

using namespace mda;

TEST(GapDetector, NoGapSequential) {
    GapDetector gd;
    for (uint32_t i = 1; i <= 100; ++i)
        EXPECT_FALSE(gd.on_packet(i, i * 1000).has_value()) << "seq=" << i;
    EXPECT_EQ(gd.total_gaps(), 0u);
    EXPECT_EQ(gd.total_missing_msgs(), 0u);
}

TEST(GapDetector, SingleGap) {
    GapDetector gd;
    gd.on_packet(1, 1000);
    gd.on_packet(2, 2000);
    auto ev = gd.on_packet(5, 5000); // missed seq 3 and 4
    ASSERT_TRUE(ev.has_value());
    EXPECT_EQ(ev->expected_seq, 3u);
    EXPECT_EQ(ev->received_seq, 5u);
    EXPECT_EQ(ev->gap_size,     2u);
    EXPECT_EQ(gd.total_gaps(),         1u);
    EXPECT_EQ(gd.total_missing_msgs(), 2u);
}

TEST(GapDetector, DuplicateIgnored) {
    GapDetector gd;
    gd.on_packet(1, 1000);
    gd.on_packet(2, 2000);
    auto ev = gd.on_packet(2, 2000); // duplicate — must be silently ignored
    EXPECT_FALSE(ev.has_value());
    EXPECT_EQ(gd.total_gaps(), 0u);
}

TEST(GapDetector, OldSeqIgnored) {
    GapDetector gd;
    gd.on_packet(10, 10000);
    gd.on_packet(11, 11000);
    auto ev = gd.on_packet(5, 5000); // old — must not generate a gap
    EXPECT_FALSE(ev.has_value());
    EXPECT_EQ(gd.total_gaps(), 0u);
}

TEST(GapDetector, MultipleGaps) {
    GapDetector gd;
    gd.on_packet(1, 1000);
    gd.on_packet(3, 3000);   // expected=2, got=3 → gap=1; next_expected=4
    gd.on_packet(10, 10000); // expected=4, got=10 → gap=6; next_expected=11
    EXPECT_EQ(gd.total_gaps(),         2u);
    EXPECT_EQ(gd.total_missing_msgs(), 7u); // 1+6 = 7
}

TEST(GapDetector, GapRate) {
    GapDetector gd;
    gd.on_packet(1, 1000);
    gd.on_packet(2, 2000);
    gd.on_packet(4, 4000); // 1 gap out of 3 packets received
    EXPECT_NEAR(gd.gap_rate(), 1.0 / 3.0, 1e-9);
}

TEST(GapDetector, ZeroGapRateEmpty) {
    GapDetector gd;
    EXPECT_EQ(gd.gap_rate(), 0.0);
}

TEST(GapDetector, FirstPacketSetsExpectation) {
    GapDetector gd;
    // Start at seq 100 — should be fine
    gd.on_packet(100, 1000);
    auto ev = gd.on_packet(101, 2000);
    EXPECT_FALSE(ev.has_value());
    EXPECT_EQ(gd.total_gaps(), 0u);
}
