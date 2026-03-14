#include "analysis/burst_detector.hpp"
#include "util/timestamp.hpp"
#include <sstream>
#include <iomanip>

namespace mda {

BurstDetector::BurstDetector(uint32_t min_packets, uint64_t window_ns)
    : min_packets_(min_packets), window_ns_(window_ns) {}

std::optional<BurstDetector::BurstEvent>
BurstDetector::on_packet(uint64_t timestamp_ns) {
    window_.push_back(timestamp_ns);

    // Evict timestamps that have fallen outside the sliding window
    while (!window_.empty() && timestamp_ns - window_.front() > window_ns_)
        window_.pop_front();

    if (window_.size() >= min_packets_) {
        uint64_t burst_start = window_.front();
        // Fire only once per burst (don't re-fire while we stay inside the same burst)
        if (burst_start != last_burst_start_ns_) {
            last_burst_start_ns_ = burst_start;
            BurstEvent ev{};
            ev.start_ns     = burst_start;
            ev.end_ns       = timestamp_ns;
            ev.packet_count = static_cast<uint32_t>(window_.size());
            double dur_ns   = static_cast<double>(timestamp_ns - burst_start);
            ev.duration_us  = dur_ns / 1000.0;
            ev.implied_rate_mpps = (dur_ns > 0)
                ? static_cast<double>(ev.packet_count) / dur_ns * 1000.0
                : 0.0;
            bursts_.push_back(ev);
            return ev;
        }
    }
    return std::nullopt;
}

std::string BurstDetector::report() const {
    std::ostringstream oss;
    oss << "\nBurst Detection Report\n"
        << "======================\n"
        << "Total bursts detected: " << bursts_.size() << "\n";
    if (!bursts_.empty()) {
        oss << "\nBurst Events:\n";
        for (const auto& ev : bursts_) {
            oss << format_ns_hms(ev.start_ns)
                << "  count=" << ev.packet_count
                << std::fixed << std::setprecision(2)
                << "  duration=" << ev.duration_us << " us"
                << "  rate=" << ev.implied_rate_mpps << " Mpps\n";
        }
    }
    return oss.str();
}

} // namespace mda
