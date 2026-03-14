#pragma once
// Simplified NASDAQ ITCH 5.0 parser
//
// Reference: NASDAQ TotalView-ITCH 5.0 Specification
// https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHspecification.pdf
//
// ITCH messages are framed with a 2-byte big-endian length prefix inside a
// MOLDUDP64 datagram (the NASDAQ multicast transport).
//
// Supported message types:
//   'A'  Add Order (No MPID)   — 36 bytes
//   'D'  Order Delete          — 19 bytes
//   'E'  Order Executed        — 31 bytes
//   'U'  Order Replace         — 35 bytes
//   'P'  Trade (Non-Cross)     — 44 bytes

#include "feed_message.hpp"
#include <cstdint>
#include <span>
#include <optional>
#include <vector>
#include <arpa/inet.h>

namespace mda {

// ---------------------------------------------------------------------------
// MOLDUDP64 transport header (20 bytes)
// ---------------------------------------------------------------------------
struct MoldUDP64Header {
    char     session[10];   // ASCII session identifier
    uint64_t seq_num;       // sequence number of first message — network byte order
    uint16_t msg_count;     // number of messages in this datagram — network byte order
} __attribute__((packed));

static_assert(sizeof(MoldUDP64Header) == 20);

// ---------------------------------------------------------------------------
// ITCH wire structs (network byte order throughout)
// ---------------------------------------------------------------------------

struct ItchAddOrder {        // type 'A', 36 bytes
    uint8_t  msg_type;
    uint16_t stock_locate;
    uint16_t tracking_num;
    uint8_t  timestamp[6];  // nanoseconds since midnight, 6-byte big-endian
    uint64_t order_ref;
    uint8_t  side;          // 'B' or 'S'
    uint32_t shares;
    char     stock[8];      // left-justified, space-padded
    uint32_t price;         // fixed-point, 4 decimal places (multiply by 10000 for 8dp)
} __attribute__((packed));

static_assert(sizeof(ItchAddOrder) == 36);

struct ItchOrderDelete {     // type 'D', 19 bytes
    uint8_t  msg_type;
    uint16_t stock_locate;
    uint16_t tracking_num;
    uint8_t  timestamp[6];
    uint64_t order_ref;
} __attribute__((packed));

static_assert(sizeof(ItchOrderDelete) == 19);

struct ItchOrderExecuted {   // type 'E', 31 bytes
    uint8_t  msg_type;
    uint16_t stock_locate;
    uint16_t tracking_num;
    uint8_t  timestamp[6];
    uint64_t order_ref;
    uint32_t executed_shares;
    uint64_t match_num;
} __attribute__((packed));

static_assert(sizeof(ItchOrderExecuted) == 31);

struct ItchOrderReplace {    // type 'U', 35 bytes
    uint8_t  msg_type;
    uint16_t stock_locate;
    uint16_t tracking_num;
    uint8_t  timestamp[6];
    uint64_t orig_order_ref;
    uint64_t new_order_ref;
    uint32_t shares;
    uint32_t price;
} __attribute__((packed));

static_assert(sizeof(ItchOrderReplace) == 35);

struct ItchTrade {           // type 'P', 44 bytes
    uint8_t  msg_type;
    uint16_t stock_locate;
    uint16_t tracking_num;
    uint8_t  timestamp[6];
    uint64_t order_ref;
    uint8_t  side;
    uint32_t shares;
    char     stock[8];
    uint32_t price;
    uint64_t match_num;
} __attribute__((packed));

static_assert(sizeof(ItchTrade) == 44);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Decode 6-byte big-endian timestamp to nanoseconds
inline uint64_t itch_timestamp_ns(const uint8_t ts[6]) {
    uint64_t ns = 0;
    for (int i = 0; i < 6; ++i) ns = (ns << 8) | ts[i];
    return ns;
}

// ---------------------------------------------------------------------------
// Parsing API
// ---------------------------------------------------------------------------

// Parse one ITCH message from a raw buffer (without the 2-byte MOLDUDP64 length prefix).
// Returns nullopt for unrecognised or truncated messages.
std::optional<FeedMessage> parse_itch_message(std::span<const uint8_t> msg);

// Parse all ITCH messages from a complete MOLDUDP64-framed UDP payload.
std::vector<FeedMessage> parse_mold_udp64(std::span<const uint8_t> payload);

} // namespace mda
