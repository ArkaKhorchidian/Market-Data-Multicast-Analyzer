#include "decode/feed/itch_lite.hpp"
#include <cstring>
#include <arpa/inet.h>

#if defined(__APPLE__)
#  define be64toh(x) __builtin_bswap64(x)
#endif

namespace mda {

std::optional<FeedMessage> parse_itch_message(std::span<const uint8_t> msg) {
    if (msg.empty()) return std::nullopt;

    uint8_t     msg_type = msg[0];
    FeedMessage out{};

    switch (msg_type) {
        case 'A': {
            if (msg.size() < sizeof(ItchAddOrder)) return std::nullopt;
            ItchAddOrder raw{};
            std::memcpy(&raw, msg.data(), sizeof(raw));
            out.msg_type       = MsgType::Add;
            out.exchange_ts_ns = itch_timestamp_ns(raw.timestamp);
            out.order_id       = be64toh(raw.order_ref);
            out.side           = static_cast<char>(raw.side);
            // ITCH price: 4 decimal places; convert to 8dp (multiply by 10000)
            out.price    = static_cast<int64_t>(ntohl(raw.price)) * 10000LL;
            out.quantity = ntohl(raw.shares);
            break;
        }
        case 'D': {
            if (msg.size() < sizeof(ItchOrderDelete)) return std::nullopt;
            ItchOrderDelete raw{};
            std::memcpy(&raw, msg.data(), sizeof(raw));
            out.msg_type       = MsgType::Delete;
            out.exchange_ts_ns = itch_timestamp_ns(raw.timestamp);
            out.order_id       = be64toh(raw.order_ref);
            break;
        }
        case 'E': {
            if (msg.size() < sizeof(ItchOrderExecuted)) return std::nullopt;
            ItchOrderExecuted raw{};
            std::memcpy(&raw, msg.data(), sizeof(raw));
            out.msg_type       = MsgType::Trade;
            out.exchange_ts_ns = itch_timestamp_ns(raw.timestamp);
            out.order_id       = be64toh(raw.order_ref);
            out.quantity       = ntohl(raw.executed_shares);
            break;
        }
        case 'U': {
            if (msg.size() < sizeof(ItchOrderReplace)) return std::nullopt;
            ItchOrderReplace raw{};
            std::memcpy(&raw, msg.data(), sizeof(raw));
            out.msg_type       = MsgType::Add; // replace = delete old + add new
            out.exchange_ts_ns = itch_timestamp_ns(raw.timestamp);
            out.order_id       = be64toh(raw.new_order_ref);
            out.price    = static_cast<int64_t>(ntohl(raw.price)) * 10000LL;
            out.quantity = ntohl(raw.shares);
            break;
        }
        case 'P': {
            if (msg.size() < sizeof(ItchTrade)) return std::nullopt;
            ItchTrade raw{};
            std::memcpy(&raw, msg.data(), sizeof(raw));
            out.msg_type       = MsgType::Trade;
            out.exchange_ts_ns = itch_timestamp_ns(raw.timestamp);
            out.order_id       = be64toh(raw.order_ref);
            out.side           = static_cast<char>(raw.side);
            out.price    = static_cast<int64_t>(ntohl(raw.price)) * 10000LL;
            out.quantity = ntohl(raw.shares);
            break;
        }
        default:
            return std::nullopt;
    }
    return out;
}

std::vector<FeedMessage> parse_mold_udp64(std::span<const uint8_t> payload) {
    std::vector<FeedMessage> result;
    if (payload.size() < sizeof(MoldUDP64Header)) return result;

    MoldUDP64Header hdr{};
    std::memcpy(&hdr, payload.data(), sizeof(hdr));
    uint16_t msg_count = ntohs(hdr.msg_count);
    uint64_t seq       = be64toh(hdr.seq_num);

    size_t offset = sizeof(MoldUDP64Header);
    for (uint16_t i = 0; i < msg_count && offset + 2 <= payload.size(); ++i) {
        uint16_t msg_len = static_cast<uint16_t>(
            (static_cast<uint16_t>(payload[offset]) << 8) | payload[offset + 1]);
        offset += 2;
        if (offset + msg_len > payload.size()) break;
        auto parsed = parse_itch_message(payload.subspan(offset, msg_len));
        if (parsed) {
            parsed->seq_num = static_cast<uint32_t>(seq + i);
            result.push_back(*parsed);
        }
        offset += msg_len;
    }
    return result;
}

} // namespace mda
