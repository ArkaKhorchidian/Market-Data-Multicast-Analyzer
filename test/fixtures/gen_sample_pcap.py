#!/usr/bin/env python3
"""Generate a deterministic sample.pcap for unit tests.

Contains 20 synthetic feed messages (Ethernet/IPv4/UDP):
  - Sequences 1-18 contiguous (no gap)
  - seq 19 skipped → gap of 1 at seq 20
  - seq 20 present
  - Burst: seqs 21-25 sent within 500 us

Requires: pip install scapy
"""

import struct
import time

try:
    from scapy.all import (
        Ether, IP, UDP, Raw, wrpcap, rdpcap
    )
    HAS_SCAPY = True
except ImportError:
    HAS_SCAPY = False


SRC_MAC  = "00:11:22:33:44:55"
DST_MAC  = "01:00:5e:01:01:01"  # multicast MAC for 239.1.1.1
SRC_IP   = "10.0.0.1"
DST_IP   = "239.1.1.1"
DST_PORT = 9000
SRC_PORT = 12345

BASE_TS  = 1_700_000_000.0   # arbitrary epoch seconds


def make_synthetic_payload(seq: int, ts_ns: int, msg_type: str = 'A',
                            order_id: int = 1, side: str = 'B',
                            price: int = 15000_00000000,
                            qty: int = 100) -> bytes:
    """Pack a SyntheticFeedMsg in network byte order (34 bytes)."""
    return struct.pack(
        ">IQBQbIx",   # 4+8+1+8+1+4 = 26 bytes ... use explicit layout below
    )


def make_payload(seq: int, ts_ns: int, msg_type: int = ord('A'),
                 order_id: int = 1, side: int = ord('B'),
                 price: int = 15000_00000000, qty: int = 100) -> bytes:
    # struct SyntheticFeedMsg (packed, network byte order):
    #   uint32  seq_num
    #   uint64  exchange_ts_ns
    #   uint8   msg_type
    #   uint64  order_id
    #   uint8   side
    #   int64   price
    #   uint32  quantity
    # Total = 4+8+1+8+1+8+4 = 34 bytes
    return struct.pack(">IQBQBqI",
                       seq, ts_ns, msg_type, order_id, side, price, qty)


def build_packets():
    packets = []
    base_ns = int(BASE_TS * 1e9)

    def add(seq, ts_ns, msg_type=ord('A')):
        payload = make_payload(seq=seq, ts_ns=ts_ns, msg_type=msg_type)
        pkt = (Ether(src=SRC_MAC, dst=DST_MAC) /
               IP(src=SRC_IP, dst=DST_IP) /
               UDP(sport=SRC_PORT, dport=DST_PORT) /
               Raw(load=payload))
        # Scapy uses float seconds for timestamps
        pkt.time = ts_ns / 1e9
        packets.append(pkt)

    # Sequences 1-18 at 10 us intervals
    for seq in range(1, 19):
        add(seq, base_ns + (seq - 1) * 10_000)

    # seq 19 skipped (gap)
    # seq 20 at expected time slot of seq 19
    add(20, base_ns + 19 * 10_000)

    # Burst: seqs 21-25 within 500 us
    burst_base = base_ns + 20 * 10_000
    for i, seq in enumerate(range(21, 26)):
        add(seq, burst_base + i * 100)   # 100 ns apart

    return packets


def main():
    if not HAS_SCAPY:
        print("scapy not installed — writing raw pcap manually")
        write_raw_pcap()
        return

    pkts = build_packets()
    out_path = "sample.pcap"
    wrpcap(out_path, pkts)
    print(f"Written {len(pkts)} packets to {out_path}")


def write_raw_pcap():
    """Write a minimal pcap without scapy (global header + packet records)."""
    import os

    PCAP_MAGIC   = 0xa1b2c3d4
    PCAP_VERSION = (2, 4)
    LINKTYPE_EN10MB = 1

    def pcap_global_header():
        return struct.pack("<IHHiIII",
                           PCAP_MAGIC,
                           PCAP_VERSION[0], PCAP_VERSION[1],
                           0,       # thiszone
                           0,       # sigfigs
                           65535,   # snaplen
                           LINKTYPE_EN10MB)

    def pcap_record(ts_ns: int, data: bytes):
        ts_sec  = ts_ns // 1_000_000_000
        ts_usec = (ts_ns % 1_000_000_000) // 1000
        return struct.pack("<IIII", ts_sec, ts_usec,
                           len(data), len(data)) + data

    def eth_ip_udp_payload(payload: bytes) -> bytes:
        # Ethernet (14) + IP (20) + UDP (8) + payload
        dst_mac = bytes([0x01, 0x00, 0x5e, 0x01, 0x01, 0x01])
        src_mac = bytes([0x00, 0x11, 0x22, 0x33, 0x44, 0x55])
        ethertype = struct.pack(">H", 0x0800)
        eth = dst_mac + src_mac + ethertype

        udp_payload = payload
        udp_len = 8 + len(udp_payload)
        udp = struct.pack(">HHHH", 12345, 9000, udp_len, 0)

        ip_proto = 0x11
        ip_tot   = 20 + udp_len
        # Simplified IP header (no options, checksum=0)
        src_ip = struct.pack(">BBBB", 10, 0, 0, 1)
        dst_ip = struct.pack(">BBBB", 239, 1, 1, 1)
        ip = (struct.pack(">BBHHHBBH", 0x45, 0, ip_tot, 0, 0, 64, ip_proto, 0)
              + src_ip + dst_ip)

        return eth + ip + udp + udp_payload

    base_ns = int(BASE_TS * 1e9)
    out_path = os.path.join(os.path.dirname(__file__), "sample.pcap")

    with open(out_path, "wb") as f:
        f.write(pcap_global_header())

        seqs = list(range(1, 19)) + [20] + list(range(21, 26))
        for i, seq in enumerate(seqs):
            if seq <= 18:
                ts_ns = base_ns + (seq - 1) * 10_000
            elif seq == 20:
                ts_ns = base_ns + 19 * 10_000
            else:  # burst
                burst_base = base_ns + 20 * 10_000
                ts_ns = burst_base + (seq - 21) * 100

            payload = make_payload(seq=seq, ts_ns=ts_ns)
            frame   = eth_ip_udp_payload(payload)
            f.write(pcap_record(ts_ns, frame))

    print(f"Written {len(seqs)} packets to {out_path}")


if __name__ == "__main__":
    main()
