#pragma once
// IPv4 header parsing — see ethernet.hpp for byte-order and packed-struct notes.

#include <cstdint>
#include <span>
#include <arpa/inet.h>

namespace mda {

struct IPv4Header {
    uint8_t  version_ihl;    // version (4 bits, must be 4) | IHL (4 bits, *4 = header bytes)
    uint8_t  dscp_ecn;
    uint16_t total_length;   // network byte order; header + payload
    uint16_t identification;
    uint16_t flags_fragment; // flags (3 bits) | fragment offset (13 bits)
    uint8_t  ttl;
    uint8_t  protocol;       // 0x11 = UDP
    uint16_t checksum;       // network byte order
    uint32_t src_ip;         // network byte order
    uint32_t dst_ip;         // network byte order
    // Options: variable length = (IHL * 4) - 20 bytes, not parsed
} __attribute__((packed));

static_assert(sizeof(IPv4Header) == 20, "IPv4Header fixed portion must be 20 bytes");

constexpr uint8_t IPPROTO_UDP_VAL    = 0x11;
constexpr size_t  IPV4_MIN_HEADER_LEN = 20;

inline uint8_t ip_ihl_bytes(const IPv4Header& h) {
    return static_cast<uint8_t>((h.version_ihl & 0x0F) * 4);
}

inline uint8_t ip_version(const IPv4Header& h) {
    return (h.version_ihl >> 4) & 0x0F;
}

inline bool is_udp(const IPv4Header& h) {
    return h.protocol == IPPROTO_UDP_VAL;
}

// Multicast: 224.0.0.0/4 — first octet high nibble == 0xE
inline bool is_multicast_dst(const IPv4Header& h) {
    return (ntohl(h.dst_ip) >> 28) == 0xEU;
}

// Parse IPv4 header from raw bytes.
// Validates: minimum size, version == 4, protocol == UDP.
// Returns pointer into overlay (zero-copy), or nullptr on failure.
inline const IPv4Header* parse_ipv4(std::span<const uint8_t> data) {
    if (data.size() < IPV4_MIN_HEADER_LEN) return nullptr;
    const auto* hdr = reinterpret_cast<const IPv4Header*>(data.data());
    if (ip_version(*hdr) != 4) return nullptr;
    if (!is_udp(*hdr)) return nullptr;
    uint8_t ihl = ip_ihl_bytes(*hdr);
    if (ihl < IPV4_MIN_HEADER_LEN) return nullptr;
    if (data.size() < ihl) return nullptr;
    return hdr;
}

} // namespace mda
