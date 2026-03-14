#include "output/reporter.hpp"
#include <ostream>

namespace mda {

void Reporter::write_gap_report(const GapDetector& gd, std::ostream& out) const {
    out << gd.report();
}

void Reporter::write_latency_report(const LatencyTracker& lt, std::ostream& out) const {
    out << lt.report();
}

void Reporter::write_burst_report(const BurstDetector& bd, std::ostream& out) const {
    out << bd.report();
}

void Reporter::write_all(const GapDetector& gd, const LatencyTracker& lt,
                          const BurstDetector& bd, std::ostream& out) const {
    out << "=== Market Data Multicast Analyzer Report ===\n"
        << "Timestamp source: " << ts_source_name(cfg_.ts_source) << "\n";
    if (cfg_.show_gap)     write_gap_report(gd, out);
    if (cfg_.show_latency) write_latency_report(lt, out);
    if (cfg_.show_burst)   write_burst_report(bd, out);
}

} // namespace mda
