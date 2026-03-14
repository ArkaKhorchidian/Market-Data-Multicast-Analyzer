#include <gtest/gtest.h>
#include "analysis/latency_tracker.hpp"
#include <cmath>

using namespace mda;

TEST(LatencyTracker, Empty) {
    LatencyTracker lt;
    EXPECT_EQ(lt.packet_count(), 0u);
    EXPECT_EQ(lt.iat_mean_ns(),  0.0);
    EXPECT_TRUE(lt.iats_ns().empty());
}

TEST(LatencyTracker, SinglePacket) {
    LatencyTracker lt;
    lt.on_packet(1'000'000);
    EXPECT_EQ(lt.packet_count(), 1u);
    EXPECT_TRUE(lt.iats_ns().empty()); // no IAT yet — need at least 2 packets
}

TEST(LatencyTracker, UniformIAT) {
    LatencyTracker lt;
    const uint64_t step = 10'000; // 10 us
    for (int i = 0; i < 1000; ++i)
        lt.on_packet(static_cast<uint64_t>(i) * step);
    EXPECT_NEAR(lt.iat_mean_ns(),         static_cast<double>(step), 1.0);
    EXPECT_NEAR(lt.iat_percentile(0.5),   static_cast<double>(step), 1.0);
    EXPECT_NEAR(lt.iat_percentile(1.0),   static_cast<double>(step), 1.0);
    EXPECT_NEAR(lt.iat_stddev_ns(),       0.0, 1.0);
}

TEST(LatencyTracker, MinMax) {
    LatencyTracker lt;
    lt.on_packet(0);
    lt.on_packet(1000);
    lt.on_packet(3000);  // IATs: 1000, 2000
    EXPECT_EQ(lt.iat_min_ns(), 1000.0);
    EXPECT_EQ(lt.iat_max_ns(), 2000.0);
}

TEST(LatencyTracker, MeanPacketRate) {
    LatencyTracker lt;
    // 1000 packets over 1 second (1 ms IAT)
    for (int i = 0; i < 1000; ++i)
        lt.on_packet(static_cast<uint64_t>(i) * 1'000'000);
    EXPECT_NEAR(lt.mean_packets_per_second(), 1000.0, 5.0);
}

TEST(LatencyTracker, Percentile99GTp50) {
    LatencyTracker lt;
    lt.on_packet(0);
    // 99 uniform packets then one large outlier
    for (int i = 1; i <= 99; ++i)
        lt.on_packet(static_cast<uint64_t>(i) * 1000);
    lt.on_packet(99 * 1000ULL + 1'000'000); // last IAT = 1ms vs 1us
    EXPECT_GT(lt.iat_percentile(0.99), lt.iat_percentile(0.50));
}
