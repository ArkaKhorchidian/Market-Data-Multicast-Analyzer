#pragma once
#include "../analysis/gap_detector.hpp"
#include "../analysis/latency_tracker.hpp"
#include "../analysis/burst_detector.hpp"
#include "../util/timestamp.hpp"
#include <string>
#include <ostream>

namespace mda {

class Reporter {
public:
    struct Config {
        TimestampSource ts_source      = TimestampSource::KernelSoftware;
        bool            show_gap       = true;
        bool            show_latency   = true;
        bool            show_burst     = true;
    };

    explicit Reporter(const Config& cfg) : cfg_(cfg) {}

    void write_gap_report    (const GapDetector&    gd, std::ostream& out) const;
    void write_latency_report(const LatencyTracker& lt, std::ostream& out) const;
    void write_burst_report  (const BurstDetector&  bd, std::ostream& out) const;

    void write_all(const GapDetector& gd, const LatencyTracker& lt,
                   const BurstDetector& bd, std::ostream& out) const;

private:
    Config cfg_;
};

} // namespace mda
