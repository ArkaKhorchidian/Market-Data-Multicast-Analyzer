#pragma once
// Ethernet II frame parsing
//
// BYTE ORDER: All multi-byte fields in network headers are big-endian (network
// byte order). x86-64 is little-endian. Every multi-byte field MUST be
// converted with ntohl / ntohs before arithmetic. This is enforced throughout
// all header parsers in this codebase. Failure to swap is a common bug source
// and a standard interview question.
//
// PACKED STRUCTS: __attribute__((packed)) is required on all header structs to
// prevent the compiler from inserting alignment padding, which would misalign
// the overlay on raw packet bytes.

#include <cstdint>
#include <span>
#include <arpa/inet.h>

namespace mda {

struct EthernetHeader {
    uint8_t  dst_mac[6];
    uint8_t  src_mac[6];
    uint16_t ethertype;  // network byte order; 0x0800 = IPv4
} __attribute__((packed));

static_assert(sizeof(EthernetHeader) == 14, "EthernetHeader must be 14 bytes");

constexpr uint16_t ETHERTYPE_IPV4 = 0x0800;
constexpr uint16_t ETHERTYPE_VLAN = 0x8100; // 802.1Q — detected but not handled
constexpr size_t   ETHERNET_HEADER_LEN = 14;

// Parse Ethernet header from raw bytes.
// Returns a pointer into the overlay (zero-copy), or nullptr if:
//   - span is too small
//   - ethertype is not IPv4 (0x0800); 802.1Q VLAN-tagged frames (0x8100)
//     are rejected — extend this function if VLAN support is needed.
inline const EthernetHeader* parse_ethernet(std::span<const uint8_t> data) {
    if (data.size() < ETHERNET_HEADER_LEN) return nullptr;
    const auto* hdr = reinterpret_cast<const EthernetHeader*>(data.data());
    if (ntohs(hdr->ethertype) != ETHERTYPE_IPV4) return nullptr;
    return hdr;
}

} // namespace mda
