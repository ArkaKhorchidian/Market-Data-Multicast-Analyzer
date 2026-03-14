#pragma once
#include <cstdint>
#include <cstddef>

namespace mda {

constexpr size_t MAX_PACKET_SIZE = 9000; // jumbo frame support

struct Packet {
    uint64_t timestamp_ns;    // capture timestamp, nanoseconds since epoch
    uint32_t len;             // wire length
    uint32_t caplen;          // captured length (may be < len if snaplen truncated)
    uint8_t  data[MAX_PACKET_SIZE];
};

} // namespace mda
