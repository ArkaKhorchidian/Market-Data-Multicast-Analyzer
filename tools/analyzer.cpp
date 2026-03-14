// analyzer — main CLI tool for Market Data Multicast Analyzer
//
// Usage:
//   analyzer --pcap FILE  [--feed synthetic|itch] [--gap-report] [--latency-report]
//            [--burst-report] [--book] [--csv FILE] [--verbose]
//   analyzer --live IFACE --group IP --port PORT [same analysis flags]

#include "capture/pcap_reader.hpp"
#include "capture/live_capture.hpp"
#include "decode/ethernet.hpp"
#include "decode/ip.hpp"
#include "decode/udp.hpp"
#include "decode/feed/synthetic_feed.hpp"
#include "decode/feed/itch_lite.hpp"
#include "analysis/gap_detector.hpp"
#include "analysis/latency_tracker.hpp"
#include "analysis/burst_detector.hpp"
#include "analysis/book_reconstructor.hpp"
#include "output/reporter.hpp"
#include "output/csv_writer.hpp"
#include "util/timestamp.hpp"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <span>
#include <cstring>
#include <cstdlib>
#include <optional>

// ---------------------------------------------------------------------------
// Argument parsing helpers
// ---------------------------------------------------------------------------
static void print_usage(const char* prog) {
    std::cout <<
        "Usage: " << prog << " [OPTIONS]\n\n"
        "Modes:\n"
        "  --pcap FILE           Analyse offline pcap file\n"
        "  --live INTERFACE      Live capture on network interface\n"
        "  --group IP            Multicast group to join (live mode only)\n"
        "  --port PORT           UDP destination port filter\n\n"
        "Feed format:\n"
        "  --feed synthetic      Parse synthetic feed format (default)\n"
        "  --feed itch           Parse ITCH 5.0 / MOLDUDP64 format\n\n"
        "Analysis options:\n"
        "  --gap-report          Print sequence gap analysis\n"
        "  --latency-report      Print IAT latency distribution\n"
        "  --burst-report        Print burst event log\n"
        "  --book                Reconstruct and display order book\n\n"
        "Output options:\n"
        "  --csv FILE            Write per-packet CSV for Python visualisation\n"
        "  --verbose             Print each decoded message\n\n"
        "Examples:\n"
        "  analyzer --pcap session.pcap --gap-report --latency-report\n"
        "  analyzer --live eth0 --group 233.54.12.111 --port 26477 --feed itch\n"
        "  analyzer --pcap nasdaq.pcap --feed itch --book --csv output.csv\n";
}

struct Args {
    std::string pcap_file;
    std::string live_iface;
    std::string group;
    uint16_t    port          = 0;
    bool        feed_itch     = false;
    bool        gap_report    = false;
    bool        latency_report = false;
    bool        burst_report  = false;
    bool        book          = false;
    std::string csv_file;
    bool        verbose       = false;
};

static Args parse_args(int argc, char* argv[]) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string s = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) { std::cerr << "Missing argument for " << s << "\n"; exit(1); }
            return argv[++i];
        };
        if      (s == "--pcap")            a.pcap_file      = next();
        else if (s == "--live")            a.live_iface     = next();
        else if (s == "--group")           a.group          = next();
        else if (s == "--port")            a.port           = static_cast<uint16_t>(std::stoi(next()));
        else if (s == "--feed") {
            std::string f = next();
            if (f == "itch") a.feed_itch = true;
        }
        else if (s == "--gap-report")      a.gap_report     = true;
        else if (s == "--latency-report")  a.latency_report = true;
        else if (s == "--burst-report")    a.burst_report   = true;
        else if (s == "--book")            a.book           = true;
        else if (s == "--csv")             a.csv_file       = next();
        else if (s == "--verbose")         a.verbose        = true;
        else if (s == "--help" || s == "-h") { print_usage(argv[0]); exit(0); }
        else { std::cerr << "Unknown option: " << s << "\n"; exit(1); }
    }
    if (a.pcap_file.empty() && a.live_iface.empty()) {
        print_usage(argv[0]);
        exit(1);
    }
    return a;
}

// ---------------------------------------------------------------------------
// Packet processing pipeline
// ---------------------------------------------------------------------------
struct Pipeline {
    mda::GapDetector      gap;
    mda::LatencyTracker   lat;
    mda::BurstDetector    burst{10, 1'000'000}; // 10 pkts in 1 ms
    mda::BookReconstructor book;
    std::optional<mda::CsvWriter> csv;
    uint64_t last_ts_ns = 0;
    bool     verbose    = false;
    bool     feed_itch  = false;
    bool     show_book  = false;

    void process_packet(const mda::Packet& pkt) {
        using namespace mda;

        auto raw = std::span<const uint8_t>(pkt.data, pkt.caplen);

        // --- Ethernet ---
        const EthernetHeader* eth = parse_ethernet(raw);
        if (!eth) return;
        auto ip_span = raw.subspan(ETHERNET_HEADER_LEN);

        // --- IPv4 ---
        const IPv4Header* ip = parse_ipv4(ip_span);
        if (!ip) return;
        uint8_t ihl = ip_ihl_bytes(*ip);
        auto udp_span = ip_span.subspan(ihl);

        // --- UDP ---
        std::span<const uint8_t> payload;
        const UDPHeader* udp = parse_udp(udp_span, payload);
        if (!udp) return;

        // --- Latency / burst tracking (done at UDP level regardless of feed) ---
        lat.on_packet(pkt.timestamp_ns);
        burst.on_packet(pkt.timestamp_ns);

        uint64_t iat_ns = (last_ts_ns > 0) ? pkt.timestamp_ns - last_ts_ns : 0;
        last_ts_ns = pkt.timestamp_ns;

        // --- Feed parsing ---
        std::vector<FeedMessage> msgs;
        if (feed_itch) {
            msgs = parse_mold_udp64(payload);
        } else {
            auto m = parse_synthetic(payload);
            if (m) msgs.push_back(*m);
        }

        for (const auto& msg : msgs) {
            auto gap_ev = gap.on_packet(msg.seq_num, pkt.timestamp_ns);

            if (gap_ev && show_book) book.mark_stale();

            if (show_book) {
                switch (msg.msg_type) {
                    case MsgType::Add:
                        book.on_add(msg.order_id, msg.side, msg.price,
                                    msg.quantity, pkt.timestamp_ns);
                        break;
                    case MsgType::Delete:
                        book.on_delete(msg.order_id, pkt.timestamp_ns);
                        break;
                    case MsgType::Trade:
                        book.on_trade(msg.price, msg.quantity, pkt.timestamp_ns);
                        break;
                    default: break;
                }
                std::cout << format_ns_hms(pkt.timestamp_ns) << "\n"
                          << book.snapshot_str(5) << "\n";
            }

            if (csv) {
                csv->write_packet(pkt.timestamp_ns, iat_ns, msg.seq_num,
                                  static_cast<char>(msg.msg_type),
                                  gap_ev.has_value(),
                                  gap_ev ? gap_ev->gap_size : 0);
            }

            if (verbose) {
                std::cout << format_ns_hms(pkt.timestamp_ns)
                          << "  seq=" << msg.seq_num
                          << "  type=" << static_cast<char>(msg.msg_type)
                          << "  oid=" << msg.order_id
                          << "  side=" << msg.side
                          << "  qty=" << msg.quantity;
                if (gap_ev)
                    std::cout << "  GAP(" << gap_ev->gap_size << ")";
                std::cout << "\n";
            }
        }
    }
};

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    Args args = parse_args(argc, argv);

    // Default to all reports if none specified
    bool any_report = args.gap_report || args.latency_report || args.burst_report;
    if (!any_report && !args.book && !args.verbose) {
        args.gap_report = args.latency_report = args.burst_report = true;
    }

    Pipeline pipeline;
    pipeline.verbose   = args.verbose;
    pipeline.feed_itch = args.feed_itch;
    pipeline.show_book = args.book;

    if (!args.csv_file.empty()) {
        pipeline.csv.emplace(args.csv_file);
        if (!pipeline.csv->is_open()) {
            std::cerr << "Cannot open CSV file: " << args.csv_file << "\n";
            return 1;
        }
        pipeline.csv->write_header();
    }

    uint64_t pkt_count = 0;

    if (!args.pcap_file.empty()) {
        // --- pcap mode ---
        mda::PcapReader reader(args.pcap_file);
        if (!reader.is_open()) {
            std::cerr << "Cannot open pcap: " << reader.error() << "\n";
            return 1;
        }
        std::cerr << "Reading " << args.pcap_file << " ...\n";
        pkt_count = reader.process_all([&](const mda::Packet& pkt) {
            pipeline.process_packet(pkt);
        });
    } else {
        // --- live mode ---
        mda::LiveCapture::Config cfg;
        cfg.interface       = args.live_iface;
        cfg.multicast_group = args.group;
        cfg.port            = args.port;

        mda::LiveCapture cap(cfg);
        if (!cap.start()) {
            std::cerr << "Capture failed: " << cap.error() << "\n";
            return 1;
        }
        std::cerr << "Capturing on " << args.live_iface;
        if (!args.group.empty()) std::cerr << " group=" << args.group;
        if (args.port)           std::cerr << " port=" << args.port;
        std::cerr << " (Ctrl-C to stop)\n";

        while (cap.is_active()) {
            auto pkt = cap.next_packet();
            if (!pkt) continue;
            pipeline.process_packet(*pkt);
            ++pkt_count;
        }
    }

    std::cerr << "Processed " << pkt_count << " packets.\n\n";

    // --- Reports ---
    mda::Reporter::Config rcfg;
    rcfg.show_gap     = args.gap_report;
    rcfg.show_latency = args.latency_report;
    rcfg.show_burst   = args.burst_report;
    mda::Reporter reporter(rcfg);
    reporter.write_all(pipeline.gap, pipeline.lat, pipeline.burst, std::cout);

    return 0;
}
