# Capturing Real Multicast Data

## Live capture with tcpdump / analyzer

```bash
# Capture 60 seconds of multicast on a specific group and port
sudo tcpdump -i eth0 -w session.pcap \
    udp and dst host 233.54.12.111 and dst port 26477 \
    -G 60 -W 1

# Or use the analyzer directly in live mode (requires root / CAP_NET_RAW)
sudo ./analyzer --live eth0 --group 233.54.12.111 --port 26477 \
    --feed itch --gap-report --latency-report --csv session.csv
```

## NASDAQ TotalView-ITCH historical data

NASDAQ provides free historical ITCH data at:
https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/

Files are gzip-compressed binary. The ITCH 5.0 parser in this repo handles
these files when replayed through the MOLDUDP64 framing layer.

### Re-encapsulating ITCH data into pcap for replay

The raw ITCH files are not pcap — they contain framed MOLDUDP64 datagrams.
Use `itch2pcap` or a custom script to wrap them in Ethernet/IP/UDP headers
before replaying with the analyzer.

## Synthetic feed for testing

The `feed_generator` tool sends synthetic packets to a multicast group at a
configurable rate with controllable gap and burst injection:

```bash
# Terminal 1: start receiving
sudo ./analyzer --live lo --group 239.1.1.1 --port 9000 \
    --gap-report --latency-report --burst-report

# Terminal 2: generate feed
./feed_generator --group 239.1.1.1 --port 9000 \
    --rate 100000 --duration 60 \
    --gap-at 50000 --gap-size 3 \
    --burst-at 30 --burst-count 5000
```

## Timestamp accuracy note

pcap (libpcap) provides **kernel software timestamps** applied at packet
receipt — approximately microsecond accuracy. This is the timestamp source
used by all reports from this tool. Hardware NIC timestamps (SO_TIMESTAMPING,
sub-microsecond) require kernel and driver support and are not used in the
default configuration.

All latency reports state the timestamp source explicitly.
