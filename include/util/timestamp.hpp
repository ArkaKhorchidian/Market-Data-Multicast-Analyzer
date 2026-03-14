#pragma once
#include <cstdint>
#include <string>

namespace mda {

// Which timestamping mechanism provided the packet timestamp.
// This MUST be documented in every output report — a latency number without
// a stated timestamp source is meaningless.
enum class TimestampSource {
    Hardware,        // SO_TIMESTAMPING — NIC level, sub-microsecond accuracy
    KernelSoftware,  // SO_TIMESTAMP    — kernel receive time, ~microsecond accuracy
    Application,     // clock_gettime at pcap_next_ex() call — includes scheduler jitter
};

struct PacketTimestamp {
    uint64_t        ns;     // nanoseconds since epoch
    TimestampSource source;
};

inline const char* ts_source_name(TimestampSource src) {
    switch (src) {
        case TimestampSource::Hardware:       return "Hardware (SO_TIMESTAMPING)";
        case TimestampSource::KernelSoftware: return "Kernel Software (SO_TIMESTAMP)";
        case TimestampSource::Application:    return "Application (clock_gettime)";
    }
    return "Unknown";
}

// Current time as nanoseconds since epoch (CLOCK_REALTIME)
uint64_t now_ns();

// Convert pcap timeval fields to nanoseconds since epoch
inline uint64_t timeval_to_ns(long tv_sec, long tv_usec) {
    return static_cast<uint64_t>(tv_sec)  * 1'000'000'000ULL
         + static_cast<uint64_t>(tv_usec) * 1'000ULL;
}

// Format a nanosecond timestamp as HH:MM:SS.microseconds
std::string format_ns_hms(uint64_t ns);

// Format a nanosecond duration as a human-readable string (e.g., "1.23 us")
std::string format_ns_human(uint64_t ns);

} // namespace mda
