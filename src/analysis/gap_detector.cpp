#include "analysis/gap_detector.hpp"
#include "util/timestamp.hpp"
#include <sstream>
#include <iomanip>

namespace mda {

std::optional<GapDetector::GapEvent>
GapDetector::on_packet(uint32_t seq_num, uint64_t timestamp_ns) {
    ++total_packets_;

    if (!initialized_) {
        next_expected_seq_ = seq_num + 1;
        initialized_ = true;
        return std::nullopt;
    }

    // Duplicate / reorder — silently skip, do not update expectation
    if (seq_num < next_expected_seq_) return std::nullopt;

    // In-sequence
    if (seq_num == next_expected_seq_) {
        next_expected_seq_ = seq_num + 1;
        return std::nullopt;
    }

    // Gap detected
    GapEvent ev{};
    ev.timestamp_ns = timestamp_ns;
    ev.expected_seq = next_expected_seq_;
    ev.received_seq = seq_num;
    ev.gap_size     = seq_num - next_expected_seq_;
    total_missing_ += ev.gap_size;

    if (ev.gap_size > largest_gap_) {
        largest_gap_       = ev.gap_size;
        largest_gap_event_ = ev;
    }
    gap_log_.push_back(ev);
    next_expected_seq_ = seq_num + 1;
    return ev;
}

double GapDetector::gap_rate() const {
    if (total_packets_ == 0) return 0.0;
    return static_cast<double>(gap_log_.size()) / static_cast<double>(total_packets_);
}

std::string GapDetector::report() const {
    std::ostringstream oss;
    oss << "\nGap Analysis Summary\n"
        << "====================\n"
        << "Timestamp source: Kernel Software (SO_TIMESTAMP)\n"
        << "Total packets received:  " << total_packets_ << "\n"
        << "Total gaps detected:     " << gap_log_.size() << "\n"
        << "Total missing messages:  " << total_missing_ << "\n"
        << std::fixed << std::setprecision(5)
        << "Gap rate:                " << gap_rate() * 100.0 << "%\n";

    if (!gap_log_.empty()) {
        oss << "Largest gap:             " << largest_gap_event_.gap_size
            << " messages (seq " << largest_gap_event_.expected_seq
            << " \xe2\x86\x92 " << largest_gap_event_.received_seq << ") at "
            << format_ns_hms(largest_gap_event_.timestamp_ns) << "\n";

        if (gap_log_.size() > 1) {
            uint64_t dt_sum = 0;
            for (size_t i = 1; i < gap_log_.size(); ++i)
                dt_sum += gap_log_[i].timestamp_ns - gap_log_[i-1].timestamp_ns;
            double mean_s = static_cast<double>(dt_sum)
                          / static_cast<double>(gap_log_.size() - 1) / 1e9;
            oss << std::setprecision(1)
                << "Mean time between gaps:  " << mean_s / 60.0 << " minutes\n";
        }

        oss << "\nGap Timeline:\n";
        for (const auto& ev : gap_log_) {
            oss << format_ns_hms(ev.timestamp_ns)
                << "  seq " << ev.expected_seq << " \xe2\x86\x92 " << ev.received_seq
                << "  gap=" << ev.gap_size << "\n";
        }
    }
    return oss.str();
}

} // namespace mda
