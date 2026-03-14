#pragma once
// Multicast group management utilities

#include <string>
#include <arpa/inet.h>

namespace mda {

// Validate that an IP string falls in the multicast range 224.0.0.0/4
inline bool is_valid_multicast_group(const std::string& ip) {
    struct in_addr addr{};
    if (inet_aton(ip.c_str(), &addr) == 0) return false;
    return (ntohl(addr.s_addr) >> 28) == 0xEU;
}

// Convert dotted-decimal string to host-byte-order uint32_t
inline uint32_t ip_to_host(const std::string& ip) {
    struct in_addr addr{};
    inet_aton(ip.c_str(), &addr);
    return ntohl(addr.s_addr);
}

} // namespace mda
