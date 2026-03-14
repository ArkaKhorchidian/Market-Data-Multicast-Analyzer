#pragma once
// Synthetic test feed format
//
// 34 bytes per message; fits in a single UDP datagram.
// All multi-byte fields are in network byte order (big-endian).
// See ethernet.hpp for the byte-order rule.

#include "feed_message.hpp"
#include <cstdint>
#include <span>
#include <optional>

namespace mda {

struct SyntheticFeedMsg {
    uint32_t seq_num;          // monotonically increasing per-channel
    uint64_t exchange_ts_ns;   // exchange-side send timestamp (nanoseconds)
    uint8_t  msg_type;         // 'A'=add, 'D'=delete, 'T'=trade, 'H'=heartbeat
    uint64_t order_id;
    uint8_t  side;             // 'B'=bid, 'S'=ask
    int64_t  price;            // fixed-point, 8 decimal places
    uint32_t quantity;
} __attribute__((packed));

static_assert(sizeof(SyntheticFeedMsg) == 34, "SyntheticFeedMsg must be 34 bytes");

// Parse a synthetic feed message from a UDP payload.
// All multi-byte fields are converted from network to host byte order.
// Returns nullopt if the payload is too small or msg_type is unrecognised.
std::optional<FeedMessage> parse_synthetic(std::span<const uint8_t> payload);

} // namespace mda
