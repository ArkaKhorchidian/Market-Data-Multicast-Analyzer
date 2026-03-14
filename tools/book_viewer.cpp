// book_viewer — replay a pcap / synthetic feed and print order book state at each update
//
// Usage:
//   book_viewer --pcap session.pcap [--feed itch] [--depth 5]
//
// Output example:
//   09:30:00.000123
//     BID: 149.95 x 200 | 149.90 x 450 | 149.85 x 1200
//     ASK: 150.00 x 300 | 150.05 x 800 | 150.10 x 2100
//     Spread: 5.00c  Mid: 149.975

#include "capture/pcap_reader.hpp"
#include "decode/ethernet.hpp"
#include "decode/ip.hpp"
#include "decode/udp.hpp"
#include "decode/feed/synthetic_feed.hpp"
#include "decode/feed/itch_lite.hpp"
#include "analysis/book_reconstructor.hpp"
#include "analysis/gap_detector.hpp"
#include "util/timestamp.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <span>
#include <cstdlib>

using namespace mda;

static void print_usage(const char* prog) {
    std::cout << "Usage: " << prog
              << " --pcap FILE [--feed synthetic|itch] [--depth N] [--port PORT]\n";
}

struct Args {
    std::string pcap_file;
    bool        feed_itch = false;
    int         depth     = 5;
    uint16_t    port      = 0;
};

static Args parse_args(int argc, char* argv[]) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string s = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) { std::cerr << "Missing arg for " << s << "\n"; exit(1); }
            return argv[++i];
        };
        if      (s == "--pcap")  a.pcap_file = next();
        else if (s == "--feed") { if (next() == "itch") a.feed_itch = true; }
        else if (s == "--depth") a.depth = std::stoi(next());
        else if (s == "--port")  a.port  = static_cast<uint16_t>(std::stoi(next()));
        else if (s == "--help" || s == "-h") { print_usage(argv[0]); exit(0); }
        else { std::cerr << "Unknown option: " << s << "\n"; exit(1); }
    }
    if (a.pcap_file.empty()) { print_usage(argv[0]); exit(1); }
    return a;
}

int main(int argc, char* argv[]) {
    Args args = parse_args(argc, argv);

    PcapReader reader(args.pcap_file);
    if (!reader.is_open()) {
        std::cerr << "Cannot open pcap: " << reader.error() << "\n";
        return 1;
    }

    BookReconstructor book;
    GapDetector       gap;
    uint64_t          pkt_count = 0;

    reader.process_all([&](const Packet& pkt) {
        ++pkt_count;
        auto raw = std::span<const uint8_t>(pkt.data, pkt.caplen);

        const EthernetHeader* eth = parse_ethernet(raw);
        if (!eth) return;

        const IPv4Header* ip = parse_ipv4(raw.subspan(ETHERNET_HEADER_LEN));
        if (!ip) return;

        auto udp_span = raw.subspan(ETHERNET_HEADER_LEN + ip_ihl_bytes(*ip));
        std::span<const uint8_t> payload;
        const UDPHeader* udp = parse_udp(udp_span, payload, args.port);
        if (!udp) return;

        std::vector<FeedMessage> msgs;
        if (args.feed_itch) {
            msgs = parse_mold_udp64(payload);
        } else {
            auto m = parse_synthetic(payload);
            if (m) msgs.push_back(*m);
        }

        bool book_changed = false;
        for (const auto& msg : msgs) {
            auto gap_ev = gap.on_packet(msg.seq_num, pkt.timestamp_ns);
            if (gap_ev) {
                book.mark_stale();
                std::cout << format_ns_hms(pkt.timestamp_ns)
                          << "  *** GAP detected: expected seq "
                          << gap_ev->expected_seq << ", got "
                          << gap_ev->received_seq
                          << " (missing " << gap_ev->gap_size << " messages) ***\n";
            }

            switch (msg.msg_type) {
                case MsgType::Add:
                    book.on_add(msg.order_id, msg.side, msg.price,
                                msg.quantity, pkt.timestamp_ns);
                    std::cout << format_ns_hms(pkt.timestamp_ns)
                              << "  [ADD "
                              << (msg.side == 'B' ? "BID" : "ASK")
                              << " " << msg.price / 1e8
                              << " x " << msg.quantity << "]\n";
                    book_changed = true;
                    break;
                case MsgType::Delete:
                    book.on_delete(msg.order_id, pkt.timestamp_ns);
                    std::cout << format_ns_hms(pkt.timestamp_ns)
                              << "  [DEL oid=" << msg.order_id << "]\n";
                    book_changed = true;
                    break;
                case MsgType::Trade:
                    book.on_trade(msg.price, msg.quantity, pkt.timestamp_ns);
                    std::cout << format_ns_hms(pkt.timestamp_ns)
                              << "  [TRADE " << msg.price / 1e8
                              << " x " << msg.quantity << "]\n";
                    book_changed = true;
                    break;
                default:
                    break;
            }

            if (book_changed) {
                std::cout << book.snapshot_str(args.depth) << "\n";
                book_changed = false;
            }
        }
    });

    std::cerr << "\nProcessed " << pkt_count << " packets.\n"
              << "Final book state:\n"
              << book.snapshot_str(args.depth) << "\n"
              << "Total gaps: " << gap.total_gaps()
              << "  Missing msgs: " << gap.total_missing_msgs() << "\n";
    return 0;
}
