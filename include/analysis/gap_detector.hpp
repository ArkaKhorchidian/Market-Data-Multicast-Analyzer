#pragma once
#include <cstdint>
#include <optional>
#include <vector>
#include <string>

namespace mda {

// Detects sequence number gaps in a UDP multicast feed.
//
// Gaps are the primary failure mode of UDP multicast: unlike TCP, UDP provides
// no retransmission. The application layer must detect gaps and either request
// a retransmission via a gap-fill channel or mark downstream state as stale.
class GapDetector {
public:
    struct GapEvent {
        uint64_t timestamp_ns;  // when the gap was detected
        uint32_t expected_seq;  // what we expected
        uint32_t received_seq;  // what we got
        uint32_t gap_size;      // received - expected (number of missing messages)
    };

    // Process one packet.
    // Returns GapEvent if a gap is detected, nullopt if sequence is contiguous.
    // Silently ignores duplicate / out-of-order (too-low) sequence numbers.
    std::optional<GapEvent> on_packet(uint32_t seq_num, uint64_t timestamp_ns);

    uint64_t total_packets()      const { return total_packets_; }
    uint64_t total_gaps()         const { return gap_log_.size(); }
    uint64_t total_missing_msgs() const { return total_missing_; }
    double   gap_rate()           const;  // gaps / total_packets

    const std::vector<GapEvent>& gap_log() const { return gap_log_; }

    std::string report() const;

private:
    uint32_t next_expected_seq_{0};
    bool     initialized_{false};
    uint64_t total_packets_{0};
    uint64_t total_missing_{0};
    uint32_t largest_gap_{0};
    GapEvent largest_gap_event_{};
    std::vector<GapEvent> gap_log_;
};

} // namespace mda
