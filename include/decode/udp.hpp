#pragma once
// UDP header parsing — see ethernet.hpp for byte-order and packed-struct notes.

#include <cstdint>
#include <span>
#include <arpa/inet.h>

namespace mda {

struct UDPHeader {
    uint16_t src_port;  // network byte order
    uint16_t dst_port;  // network byte order
    uint16_t length;    // network byte order; includes UDP header (8 bytes) + payload
    uint16_t checksum;  // network byte order
} __attribute__((packed));

static_assert(sizeof(UDPHeader) == 8, "UDPHeader must be 8 bytes");

constexpr size_t UDP_HEADER_LEN = 8;

// Parse UDP header from raw bytes.
// Sets payload_out to the UDP payload span.
// port_filter == 0 accepts any destination port.
// Returns pointer into overlay (zero-copy), or nullptr on failure.
inline const UDPHeader* parse_udp(std::span<const uint8_t>  data,
                                   std::span<const uint8_t>& payload_out,
                                   uint16_t                   port_filter = 0) {
    if (data.size() < UDP_HEADER_LEN) return nullptr;
    const auto* hdr = reinterpret_cast<const UDPHeader*>(data.data());
    if (port_filter != 0 && ntohs(hdr->dst_port) != port_filter) return nullptr;
    uint16_t udp_len = ntohs(hdr->length);
    if (udp_len < UDP_HEADER_LEN) return nullptr;
    if (data.size() < udp_len) return nullptr;
    payload_out = data.subspan(UDP_HEADER_LEN, udp_len - UDP_HEADER_LEN);
    return hdr;
}

} // namespace mda
