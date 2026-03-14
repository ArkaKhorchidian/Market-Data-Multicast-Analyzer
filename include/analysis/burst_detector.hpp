#pragma once
#include <cstdint>
#include <optional>
#include <vector>
#include <deque>
#include <string>

namespace mda {

// Detects packet bursts: N or more packets arriving within a sliding time window.
// Bursts stress the downstream processing pipeline and are a key failure mode
// in market data systems during high-volatility market events.
class BurstDetector {
public:
    struct BurstEvent {
        uint64_t start_ns;
        uint64_t end_ns;
        uint32_t packet_count;
        double   duration_us;
        double   implied_rate_mpps; // millions of packets per second
    };

    // min_packets: minimum packets in window to qualify as a burst
    // window_ns:   sliding window width in nanoseconds
    BurstDetector(uint32_t min_packets, uint64_t window_ns);

    // Returns BurstEvent on the first tick of each new burst, nullopt otherwise.
    std::optional<BurstEvent> on_packet(uint64_t timestamp_ns);

    const std::vector<BurstEvent>& bursts() const { return bursts_; }
    std::string report() const;

private:
    uint32_t min_packets_;
    uint64_t window_ns_;
    std::deque<uint64_t>    window_;
    std::vector<BurstEvent> bursts_;
    uint64_t last_burst_start_ns_{0};
};

} // namespace mda
