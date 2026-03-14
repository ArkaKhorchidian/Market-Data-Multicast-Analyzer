# Market Data Multicast Analyzer

A C++20 tool for capturing, parsing, and analyzing UDP multicast market data feeds. Measures packet timing, detects sequence gaps, reconstructs order book state from feed messages, and produces latency distribution reports. Includes a pcap replay mode for offline analysis of captured sessions.

---

## Motivation

Market data at exchanges and HFT firms travels over **UDP multicast** — not TCP. Unlike TCP, UDP provides no retransmission. The application layer must detect dropped packets, handle out-of-order delivery, and recover from bursts. These failure modes — sequence gaps, latency spikes, burst storms — are the daily operational concerns of market data engineers at Nasdaq, Virtu, Citadel, and every firm running co-located infrastructure.

This tool measures exactly those failure modes and makes them legible.

---

## What It Measures

### 1. Sequence Gap Detection

Gaps are the primary failure mode of UDP multicast feeds. When a packet is dropped — by a congested NIC, a switch buffer overflow, or a software bug — the receiver must detect the gap from sequence numbers and request a retransmit (or mark its book state as stale).

```
Gap Analysis Summary
====================
Timestamp source: Kernel Software (SO_TIMESTAMP)
Total packets received:  1,247,832
Total gaps detected:     23
Total missing messages:  47
Gap rate:                0.00184%
Largest gap:             8 messages (seq 847203 → 847211) at 09:31:42.847123
Mean time between gaps:  18.3 minutes

Gap Timeline:
09:31:42.847  seq 847203 → 847211  gap=8
09:47:11.293  seq 1024891 → 1024893  gap=2
10:12:38.001  seq 2847122 → 2847123  gap=1
```

### 2. Inter-Arrival Time (IAT) / Latency Distribution

```
Latency / Inter-Arrival Time Analysis
======================================
Timestamp source: Kernel Software (SO_TIMESTAMP)
Total packets:    1,247,832
Mean IAT:         8.01 us
Stddev IAT:       24.33 us
p50  IAT:         7.85 us
p99  IAT:         42.10 us
p99.9 IAT:        318.50 us
Mean pkt/s:       124783
Peak pkt/s:       287441
```

### 3. Burst Detection

A burst is N or more packets arriving within a sliding time window. Bursts stress the downstream processing pipeline and correlate with market events (news prints, auction opens, flash crashes).

```
Burst Detection Report
======================
Total bursts detected: 3
09:31:00.100  count=1243  duration=982.14 us  rate=1.27 Mpps
09:47:11.290  count=847   duration=711.22 us  rate=1.19 Mpps
10:12:38.000  count=3021  duration=1888.01 us  rate=1.60 Mpps
```

### 4. Order Book Reconstruction

Reconstruct L2 book state from a sequence of Add / Delete / Trade messages:

```
09:30:00.000123
  BID: 149.95 x 200 | 149.90 x 450 | 149.85 x 1200
  ASK: 150.00 x 300 | 150.05 x 800 | 150.10 x 2100
  Spread: 5.00¢  Mid: 149.975

09:30:00.012441  [ADD BID 149.97 x 100]
  BID: 149.97 x 100 | 149.95 x 200 | 149.90 x 450
  ASK: 150.00 x 300 | 150.05 x 800 | 150.10 x 2100
  Spread: 3.00¢  Mid: 149.985
```

---

## Architecture

```
Packet source                Decode layer             Analysis layer
─────────────                ────────────             ──────────────
pcap file  ──┐               Ethernet header          GapDetector
             ├─► PcapReader ─► IPv4 header      ──►  LatencyTracker
live NIC  ───┘               UDP header               BurstDetector
             LiveCapture      Synthetic feed           BookReconstructor
                              ITCH 5.0 / MOLDUDP64
                                                       ▼
                                              Reporter + CsvWriter
                                              Python visualisation
```

Key design constraints (enforced throughout):
- **No heap allocation on the hot path** — `RingBuffer<Packet, N>` is pre-allocated
- **All network header fields byte-swapped** via `ntohl`/`ntohs` — documented at the top of `ethernet.hpp`
- **All parse functions return result types**, never throw — malformed packets silently fail
- **Timestamp source documented in every report** — a latency number without a stated source is meaningless

---

## Building

```bash
# Prerequisites: CMake 3.20+, clang++ or g++ with C++20 support, libpcap-dev
# macOS:  brew install cmake libpcap googletest
# Linux:  apt install cmake libpcap-dev libgtest-dev

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

This produces:
- `build/analyzer`        — main CLI tool
- `build/feed_generator`  — synthetic feed sender
- `build/book_viewer`     — book state replay viewer
- `build/run_tests`       — unit test suite (if GTest found)

Running tests with sanitizers:
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
make run_tests && ./run_tests
```

---

## Usage

### Offline pcap analysis

```bash
# Gap + latency report from a pcap file
./analyzer --pcap session.pcap --gap-report --latency-report

# ITCH 5.0 feed with book reconstruction and CSV export
./analyzer --pcap nasdaq.pcap --feed itch --book --csv output.csv

# Then visualise with Python
python3 scripts/plot_latency.py output.csv
python3 scripts/plot_gaps.py    output.csv
```

### Live capture

```bash
# Live multicast join on eth0 (requires root / CAP_NET_RAW)
sudo ./analyzer --live eth0 \
    --group 233.54.12.111 --port 26477 \
    --feed itch --gap-report --latency-report --burst-report
```

### Synthetic feed testing

```bash
# Terminal 1 — receive
sudo ./analyzer --live lo --group 239.1.1.1 --port 9000 \
    --gap-report --latency-report --burst-report

# Terminal 2 — send 100k msgs/sec, inject 2 gaps and 1 burst
./feed_generator --group 239.1.1.1 --port 9000 \
    --rate 100000 --duration 60 \
    --gap-at 50000 --gap-size 3 \
    --burst-at 30 --burst-count 5000
```

### Full CLI reference

```
Usage: analyzer [OPTIONS]

Modes:
  --pcap FILE           Analyse offline pcap file
  --live INTERFACE      Live capture on network interface
  --group IP            Multicast group to join (live mode only)
  --port PORT           UDP destination port filter

Feed format:
  --feed synthetic      Parse synthetic feed format (default)
  --feed itch           Parse ITCH 5.0 / MOLDUDP64 format

Analysis options:
  --gap-report          Print sequence gap analysis
  --latency-report      Print IAT latency distribution
  --burst-report        Print burst event log
  --book                Reconstruct and display order book

Output options:
  --csv FILE            Write per-packet CSV for Python visualisation
  --verbose             Print each decoded message
```

---

## Feed Formats

### Synthetic Feed (default)

A simple 34-byte binary format for testing:

```
struct SyntheticFeedMsg {
    uint32_t seq_num;          // monotonically increasing per-channel
    uint64_t exchange_ts_ns;   // exchange-side send timestamp (nanoseconds)
    uint8_t  msg_type;         // 'A'=add, 'D'=delete, 'T'=trade, 'H'=heartbeat
    uint64_t order_id;
    uint8_t  side;             // 'B'=bid, 'S'=ask
    int64_t  price;            // fixed-point, 8 decimal places
    uint32_t quantity;
} __attribute__((packed));
```

All multi-byte fields are in **network byte order** (big-endian).

### ITCH 5.0 / MOLDUDP64

Partial implementation of [NASDAQ TotalView-ITCH 5.0](https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHspecification.pdf) framed in [MOLDUDP64](https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/moldudp64.pdf):

| Type | Name | Size |
|------|------|------|
| `A`  | Add Order (No MPID) | 36 bytes |
| `D`  | Order Delete        | 19 bytes |
| `E`  | Order Executed      | 31 bytes |
| `U`  | Order Replace       | 35 bytes |
| `P`  | Trade (Non-Cross)   | 44 bytes |

Use `--feed itch` to activate. Real ITCH data is available free from NASDAQ's historical data portal.

---

## Network Primer

**Why UDP multicast?** Exchanges must deliver the same price update to hundreds of subscribers simultaneously. TCP requires one connection per subscriber — expensive and unscalable. UDP multicast delivers one packet to a multicast group and the network replicates it to all subscribers. The NIC declares interest in a multicast group via IGMP (`IP_ADD_MEMBERSHIP`), exactly analogous to subscribing to an RF channel.

**Why does this cause gaps?** UDP provides no acknowledgment or retransmission. A dropped packet — from a congested switch buffer, a slow NIC, or a software stall — is gone. The receiver detects the gap via sequence numbers and must either request a retransmit via a separate gap-fill channel (e.g., TCP request/response) or mark downstream state (order book) as potentially stale.

**Timestamp sources (in order of decreasing accuracy):**

| Source | Accuracy | How to get it |
|--------|----------|---------------|
| Hardware (NIC) | < 100 ns | `SO_TIMESTAMPING`, requires kernel + driver support |
| Kernel software | ~1 µs | `SO_TIMESTAMP`, applied at receive; what libpcap provides |
| Application | ~10 µs | `clock_gettime` at `pcap_next_ex()` call |

This tool uses **kernel software timestamps** (via libpcap). All reports state the timestamp source explicitly.

---

## RF / Networking Background Connection

This repo sits at the intersection of RF/wireless and HFT infrastructure:

| RF / Wireless | Market data |
|--------------|-------------|
| Subscribe to a frequency / channel | Join a multicast group (IGMP) |
| Packet loss under fading / interference | Sequence gaps from UDP drops |
| SNR → packet error rate | Network congestion → UDP drop rate |
| IAT jitter analysis | Inter-arrival time distribution |
| Link quality measurement | Latency percentile reporting |

The measurement methodology — timing histograms, percentile distributions, burst detection — is identical in both domains.

---

## References

1. [NASDAQ TotalView-ITCH 5.0 Specification](https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHspecification.pdf)
2. [MOLDUDP64 Specification](https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/moldudp64.pdf)
3. Stevens, W.R. (2003). *Unix Network Programming, Vol. 1* — multicast sockets, `IP_ADD_MEMBERSHIP`, `SO_TIMESTAMP`
4. [libpcap documentation](https://www.tcpdump.org/manpages/pcap.3pcap.html)

---

## Portfolio Context

This is the eighth repo in a quant GitHub portfolio:

```
Exchange → [multicast-analyzer  captures UDP feed          ]
         → [fix-parser          handles order acknowledgements]
         → [lockfree-spsc       queues messages between threads]
         → [lob-simulator       maintains book state           ]
         → [latency-harness     benchmarks every hop           ]

Research layer:
  [market-impact-model]      informs order sizing
  [factor-model-alpha]       generates alpha signals
  [regime-vol-forecasting]   sizes risk limits
```
