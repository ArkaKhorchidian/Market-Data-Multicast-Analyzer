#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace mda {

// Tracks inter-arrival time (IAT) statistics for a stream of packets.
//
// The timestamp source matters: libpcap provides kernel software timestamps
// (~microsecond accuracy), not hardware NIC timestamps. All numbers produced
// here reflect the libpcap timestamp, documented in every report.
class LatencyTracker {
public:
    void on_packet(uint64_t timestamp_ns);

    // Percentile of IAT in nanoseconds; p in [0.0, 1.0]
    double iat_percentile(double p) const;
    double iat_mean_ns()   const;
    double iat_stddev_ns() const;
    double iat_min_ns()    const;
    double iat_max_ns()    const;

    double   mean_packets_per_second() const;
    double   peak_packets_per_second() const; // peak over any 1-second window

    uint64_t packet_count()  const { return packet_count_; }
    const std::vector<uint64_t>& iats_ns() const { return iats_ns_; }

    std::string report() const;

private:
    bool     first_       = true;
    uint64_t packet_count_ = 0;
    uint64_t first_ts_ns_  = 0;
    uint64_t last_ts_ns_   = 0;
    std::vector<uint64_t> iats_ns_;
    std::vector<uint64_t> timestamps_ns_;
};

} // namespace mda
