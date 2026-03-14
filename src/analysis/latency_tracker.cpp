#include "analysis/latency_tracker.hpp"
#include "analysis/stats.hpp"
#include "util/timestamp.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace mda {

void LatencyTracker::on_packet(uint64_t timestamp_ns) {
    if (first_) {
        first_        = false;
        first_ts_ns_  = timestamp_ns;
        last_ts_ns_   = timestamp_ns;
        packet_count_ = 1;
        timestamps_ns_.push_back(timestamp_ns);
        return;
    }
    uint64_t iat = timestamp_ns - last_ts_ns_;
    iats_ns_.push_back(iat);
    last_ts_ns_ = timestamp_ns;
    timestamps_ns_.push_back(timestamp_ns);
    ++packet_count_;
}

double LatencyTracker::iat_percentile(double p) const {
    std::vector<uint64_t> sorted = iats_ns_;
    std::sort(sorted.begin(), sorted.end());
    return percentile_sorted(sorted, p);
}

double LatencyTracker::iat_mean_ns()   const { return mean(iats_ns_); }
double LatencyTracker::iat_stddev_ns() const { return stddev(iats_ns_); }

double LatencyTracker::iat_min_ns() const {
    if (iats_ns_.empty()) return 0.0;
    return static_cast<double>(*std::min_element(iats_ns_.begin(), iats_ns_.end()));
}

double LatencyTracker::iat_max_ns() const {
    if (iats_ns_.empty()) return 0.0;
    return static_cast<double>(*std::max_element(iats_ns_.begin(), iats_ns_.end()));
}

double LatencyTracker::mean_packets_per_second() const {
    if (packet_count_ < 2) return 0.0;
    double dur = static_cast<double>(last_ts_ns_ - first_ts_ns_) / 1e9;
    if (dur <= 0) return 0.0;
    return static_cast<double>(packet_count_) / dur;
}

double LatencyTracker::peak_packets_per_second() const {
    if (timestamps_ns_.size() < 2) return 0.0;
    const uint64_t window = 1'000'000'000ULL;
    double peak = 0.0;
    size_t lo = 0;
    for (size_t hi = 0; hi < timestamps_ns_.size(); ++hi) {
        while (timestamps_ns_[hi] - timestamps_ns_[lo] > window) ++lo;
        double rate = static_cast<double>(hi - lo + 1);
        if (rate > peak) peak = rate;
    }
    return peak;
}

std::string LatencyTracker::report() const {
    std::ostringstream oss;
    oss << "\nLatency / Inter-Arrival Time Analysis\n"
        << "======================================\n"
        << "Timestamp source: Kernel Software (SO_TIMESTAMP)\n"
        << "Total packets:    " << packet_count_ << "\n"
        << std::fixed << std::setprecision(2);

    auto fmt = [](double ns) { return format_ns_human(static_cast<uint64_t>(ns)); };
    oss << "Mean IAT:         " << fmt(iat_mean_ns())              << "\n"
        << "Stddev IAT:       " << fmt(iat_stddev_ns())            << "\n"
        << "Min IAT:          " << fmt(iat_min_ns())               << "\n"
        << "Max IAT:          " << fmt(iat_max_ns())               << "\n"
        << "p50  IAT:         " << fmt(iat_percentile(0.50))       << "\n"
        << "p99  IAT:         " << fmt(iat_percentile(0.99))       << "\n"
        << "p99.9 IAT:        " << fmt(iat_percentile(0.999))      << "\n"
        << std::setprecision(0)
        << "Mean pkt/s:       " << mean_packets_per_second()       << "\n"
        << "Peak pkt/s:       " << peak_packets_per_second()       << "\n";
    return oss.str();
}

} // namespace mda
