#include "util/timestamp.hpp"
#include <time.h>
#include <sstream>
#include <iomanip>

namespace mda {

uint64_t now_ns() {
    struct timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    return static_cast<uint64_t>(ts.tv_sec)  * 1'000'000'000ULL
         + static_cast<uint64_t>(ts.tv_nsec);
}

std::string format_ns_hms(uint64_t ns) {
    uint64_t sec  = ns / 1'000'000'000ULL;
    uint64_t usec = (ns % 1'000'000'000ULL) / 1000ULL;
    uint64_t hour = (sec / 3600) % 24;
    uint64_t min  = (sec /   60) % 60;
    uint64_t s    =  sec         % 60;

    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(2) << hour << ':'
        << std::setw(2) << min  << ':'
        << std::setw(2) << s    << '.'
        << std::setw(6) << usec;
    return oss.str();
}

std::string format_ns_human(uint64_t ns) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    if      (ns < 1'000ULL)         oss << ns              << " ns";
    else if (ns < 1'000'000ULL)     oss << ns / 1e3        << " us";
    else if (ns < 1'000'000'000ULL) oss << ns / 1e6        << " ms";
    else                            oss << ns / 1e9        << " s";
    return oss.str();
}

} // namespace mda
