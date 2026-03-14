#pragma once
#include <cstdint>

namespace mda {

// Generic decoded message type tag
enum class MsgType : uint8_t {
    Add       = 'A',
    Delete    = 'D',
    Trade     = 'T',
    Heartbeat = 'H',
    Unknown   = 0,
};

// Common decoded message representation — used by all feed parsers so that
// the analysis layer is feed-format agnostic.
struct FeedMessage {
    uint32_t seq_num;
    uint64_t exchange_ts_ns;  // exchange-side send timestamp (ns since epoch/midnight)
    MsgType  msg_type;
    uint64_t order_id;
    char     side;            // 'B' = bid, 'S' = ask
    int64_t  price;           // fixed-point, 8 decimal places (e.g., 10000000000 = $100.00)
    uint32_t quantity;
};

} // namespace mda
