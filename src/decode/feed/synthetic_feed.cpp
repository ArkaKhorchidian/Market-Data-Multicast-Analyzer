#include "decode/feed/synthetic_feed.hpp"
#include <cstring>
#include <arpa/inet.h>

// be64toh is POSIX; on macOS it's in <machine/endian.h> via <arpa/inet.h>
#if defined(__APPLE__)
#  include <machine/endian.h>
#  define be64toh(x) __builtin_bswap64(x)
#endif

namespace mda {

std::optional<FeedMessage> parse_synthetic(std::span<const uint8_t> payload) {
    if (payload.size() < sizeof(SyntheticFeedMsg)) return std::nullopt;

    SyntheticFeedMsg raw{};
    std::memcpy(&raw, payload.data(), sizeof(raw));

    FeedMessage msg{};
    // All multi-byte fields are big-endian on the wire — byte-swap to host order.
    msg.seq_num        = ntohl(raw.seq_num);
    msg.exchange_ts_ns = be64toh(raw.exchange_ts_ns);
    msg.msg_type       = static_cast<MsgType>(raw.msg_type);
    msg.order_id       = be64toh(raw.order_id);
    msg.side           = static_cast<char>(raw.side);

    // price is a signed 64-bit value — memcpy then byte-swap
    uint64_t price_net_u = 0;
    std::memcpy(&price_net_u, &raw.price, sizeof(price_net_u));
    msg.price    = static_cast<int64_t>(be64toh(price_net_u));
    msg.quantity = ntohl(raw.quantity);

    // Reject unknown message types early so callers can trust the result.
    switch (msg.msg_type) {
        case MsgType::Add:
        case MsgType::Delete:
        case MsgType::Trade:
        case MsgType::Heartbeat:
            break;
        default:
            return std::nullopt;
    }
    return msg;
}

} // namespace mda
