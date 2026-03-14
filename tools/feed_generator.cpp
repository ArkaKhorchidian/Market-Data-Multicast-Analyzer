// feed_generator — synthetic multicast feed generator for testing
//
// Sends SyntheticFeedMsg datagrams to a multicast group at a configurable
// rate, with optional controlled gap and burst injection.
//
// Usage:
//   feed_generator --group 239.1.1.1 --port 9000 --rate 100000 --duration 60
//   feed_generator --group 239.1.1.1 --port 9000 --rate 10000  --gap-at 5000 --gap-size 3
//   feed_generator --group 239.1.1.1 --port 9000 --rate 50000  --burst-at 30 --burst-count 5000

#include "decode/feed/synthetic_feed.hpp"
#include "util/timestamp.hpp"

#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

using namespace mda;

static void print_usage(const char* prog) {
    std::cout <<
        "Usage: " << prog << " [OPTIONS]\n\n"
        "  --group IP        Multicast group (default: 239.1.1.1)\n"
        "  --port PORT       UDP port        (default: 9000)\n"
        "  --rate N          Messages/second (default: 10000)\n"
        "  --duration S      Duration in seconds (default: 10)\n"
        "  --gap-at SEQ      Inject a gap before this sequence number\n"
        "  --gap-size N      Size of injected gap (default: 3)\n"
        "  --burst-at S      Inject a burst at this elapsed second\n"
        "  --burst-count N   Number of extra packets in burst (default: 1000)\n"
        "  --ttl N           IP multicast TTL (default: 1)\n";
}

struct Args {
    std::string group       = "239.1.1.1";
    uint16_t    port        = 9000;
    uint32_t    rate        = 10000;
    uint32_t    duration    = 10;
    uint32_t    gap_at      = 0;       // 0 = disabled
    uint32_t    gap_size    = 3;
    uint32_t    burst_at    = 0;       // elapsed seconds, 0 = disabled
    uint32_t    burst_count = 1000;
    int         ttl         = 1;
};

static Args parse_args(int argc, char* argv[]) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string s = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) { std::cerr << "Missing arg for " << s << "\n"; exit(1); }
            return argv[++i];
        };
        if      (s == "--group")       a.group       = next();
        else if (s == "--port")        a.port        = static_cast<uint16_t>(std::stoi(next()));
        else if (s == "--rate")        a.rate        = static_cast<uint32_t>(std::stoul(next()));
        else if (s == "--duration")    a.duration    = static_cast<uint32_t>(std::stoul(next()));
        else if (s == "--gap-at")      a.gap_at      = static_cast<uint32_t>(std::stoul(next()));
        else if (s == "--gap-size")    a.gap_size    = static_cast<uint32_t>(std::stoul(next()));
        else if (s == "--burst-at")    a.burst_at    = static_cast<uint32_t>(std::stoul(next()));
        else if (s == "--burst-count") a.burst_count = static_cast<uint32_t>(std::stoul(next()));
        else if (s == "--ttl")         a.ttl         = std::stoi(next());
        else if (s == "--help" || s == "-h") { print_usage(argv[0]); exit(0); }
        else { std::cerr << "Unknown option: " << s << "\n"; exit(1); }
    }
    return a;
}

// Busy-wait until target_ns (CLOCK_MONOTONIC)
static void wait_until_ns(uint64_t target_ns) {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now = static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL
                 + static_cast<uint64_t>(ts.tv_nsec);
    if (now >= target_ns) return;
    // Short sleep for the bulk, then spin for the last microsecond
    uint64_t sleep_ns = (target_ns - now > 50'000) ? (target_ns - now - 50'000) : 0;
    if (sleep_ns > 0) {
        struct timespec req{ static_cast<time_t>(sleep_ns / 1'000'000'000ULL),
                             static_cast<long>(sleep_ns % 1'000'000'000ULL) };
        nanosleep(&req, nullptr);
    }
    do {
        clock_gettime(CLOCK_MONOTONIC, &ts);
        now = static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL
            + static_cast<uint64_t>(ts.tv_nsec);
    } while (now < target_ns);
}

static void send_msg(int sock, const struct sockaddr_in& dest,
                     uint32_t seq, char msg_type, uint64_t ts_ns) {
    SyntheticFeedMsg wire{};
    // Write all multi-byte fields in network byte order
    uint32_t seq_n = htonl(seq);
    std::memcpy(&wire.seq_num, &seq_n, 4);

    uint64_t ts_n = __builtin_bswap64(ts_ns);
    std::memcpy(&wire.exchange_ts_ns, &ts_n, 8);

    wire.msg_type = static_cast<uint8_t>(msg_type);

    uint64_t oid_n = __builtin_bswap64(static_cast<uint64_t>(seq));
    std::memcpy(&wire.order_id, &oid_n, 8);

    wire.side = static_cast<uint8_t>('B');

    int64_t  price_host = 15000'00000000LL; // $150.00 fixed-point 8dp
    uint64_t price_u    = __builtin_bswap64(static_cast<uint64_t>(price_host));
    std::memcpy(&wire.price, &price_u, 8);

    uint32_t qty_n = htonl(100);
    std::memcpy(&wire.quantity, &qty_n, 4);

    sendto(sock, &wire, sizeof(wire), 0,
           reinterpret_cast<const struct sockaddr*>(&dest), sizeof(dest));
}

int main(int argc, char* argv[]) {
    Args args = parse_args(argc, argv);

    // Create UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    // Set multicast TTL
    unsigned char ttl = static_cast<unsigned char>(args.ttl);
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

    struct sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port   = htons(args.port);
    inet_aton(args.group.c_str(), &dest.sin_addr);

    uint64_t interval_ns = 1'000'000'000ULL / args.rate;
    uint64_t total_msgs  = static_cast<uint64_t>(args.rate) * args.duration;

    std::cout << "Sending " << args.rate << " msgs/sec to "
              << args.group << ":" << args.port
              << " for " << args.duration << " seconds\n";
    if (args.gap_at > 0)
        std::cout << "Will inject gap of " << args.gap_size
                  << " at seq " << args.gap_at << "\n";
    if (args.burst_at > 0)
        std::cout << "Will inject burst of " << args.burst_count
                  << " extra packets at t=" << args.burst_at << "s\n";

    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t start_mono = static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL
                        + static_cast<uint64_t>(ts.tv_nsec);

    uint32_t seq = 1;
    uint32_t gaps_injected = 0, bursts_injected = 0;
    bool burst_done = false;

    for (uint64_t i = 0; i < total_msgs; ++i) {
        uint64_t target = start_mono + i * interval_ns;
        wait_until_ns(target);

        uint64_t now = now_ns();

        // Gap injection
        if (args.gap_at > 0 && seq == args.gap_at) {
            std::cout << "Injecting gap at seq " << seq
                      << " (size=" << args.gap_size << ") ...\n";
            seq += args.gap_size; // skip sequence numbers
            ++gaps_injected;
        }

        // Burst injection
        uint64_t elapsed_s = (target - start_mono) / 1'000'000'000ULL;
        if (!burst_done && args.burst_at > 0 && elapsed_s >= args.burst_at) {
            std::cout << "Injecting burst of " << args.burst_count
                      << " packets at t=" << elapsed_s << "s ...\n";
            for (uint32_t b = 0; b < args.burst_count; ++b)
                send_msg(sock, dest, seq++, 'A', now);
            ++bursts_injected;
            burst_done = true;
        }

        char msg_type = (i % 10 == 9) ? 'T' : (i % 5 == 4) ? 'D' : 'A';
        send_msg(sock, dest, seq++, msg_type, now);
    }

    std::cout << "Done. Sent " << seq - 1 << " messages"
              << ", " << gaps_injected   << " gap(s) injected"
              << ", " << bursts_injected << " burst(s) injected.\n";

    close(sock);
    return 0;
}
