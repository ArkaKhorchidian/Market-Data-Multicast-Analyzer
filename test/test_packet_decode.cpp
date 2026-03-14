#include <gtest/gtest.h>
#include "decode/ethernet.hpp"
#include "decode/ip.hpp"
#include "decode/udp.hpp"
#include <array>
#include <arpa/inet.h>

using namespace mda;

// Build a minimal Ethernet II + IPv4 + UDP packet (42 bytes, no payload)
// dst_ip = 224.1.1.1 (multicast), dst_port = 9000, protocol = UDP
static std::array<uint8_t, 42> make_udp_pkt(uint16_t dst_port = 9000,
                                              uint8_t  protocol = 0x11) {
    std::array<uint8_t, 42> p{};

    // Ethernet (14 bytes)
    for (int i = 0; i < 6; ++i) p[i] = 0xff;          // dst mac
    p[6]=0x00; p[7]=0x11; p[8]=0x22; p[9]=0x33; p[10]=0x44; p[11]=0x55; // src
    p[12] = 0x08; p[13] = 0x00;                        // ethertype IPv4

    // IPv4 (20 bytes, offset 14)
    p[14] = 0x45;                                       // version=4, IHL=5
    // total_length = 28 (20 IP + 8 UDP) in big-endian
    p[16] = 0x00; p[17] = 0x1c;                        // 28 in big-endian
    p[22] = 64;                                         // TTL
    p[23] = protocol;                                   // protocol
    // src 10.0.0.1
    p[26]=10; p[27]=0; p[28]=0; p[29]=1;
    // dst 224.1.1.1
    p[30]=224; p[31]=1; p[32]=1; p[33]=1;

    // UDP (8 bytes, offset 34)
    // Store fields in big-endian (network byte order) directly from host values.
    // Do NOT use htons() here — shift the raw host value to produce big-endian bytes.
    uint16_t sp_val = 12345, dp_val = dst_port, ul_val = 8;
    p[34] = (sp_val >> 8) & 0xff; p[35] = sp_val & 0xff;
    p[36] = (dp_val >> 8) & 0xff; p[37] = dp_val & 0xff;
    p[38] = (ul_val >> 8) & 0xff; p[39] = ul_val & 0xff;

    return p;
}

// ----------------------------- Ethernet tests --------------------------------

TEST(Ethernet, ValidIPv4) {
    auto pkt = make_udp_pkt();
    auto sp  = std::span<const uint8_t>(pkt.data(), pkt.size());
    ASSERT_NE(parse_ethernet(sp), nullptr);
    EXPECT_EQ(ntohs(parse_ethernet(sp)->ethertype), 0x0800u);
}

TEST(Ethernet, TooShort) {
    std::array<uint8_t, 10> buf{};
    EXPECT_EQ(parse_ethernet({buf.data(), buf.size()}), nullptr);
}

TEST(Ethernet, WrongEthertype_IPv6) {
    auto pkt = make_udp_pkt();
    pkt[12] = 0x86; pkt[13] = 0xDD; // IPv6
    EXPECT_EQ(parse_ethernet({pkt.data(), pkt.size()}), nullptr);
}

TEST(Ethernet, WrongEthertype_VLAN) {
    auto pkt = make_udp_pkt();
    pkt[12] = 0x81; pkt[13] = 0x00; // 802.1Q
    EXPECT_EQ(parse_ethernet({pkt.data(), pkt.size()}), nullptr);
}

// ------------------------------ IP tests -------------------------------------

TEST(IPv4, ValidUDPMulticast) {
    auto pkt = make_udp_pkt();
    auto sp  = std::span<const uint8_t>(pkt.data() + 14, pkt.size() - 14);
    const IPv4Header* ip = parse_ipv4(sp);
    ASSERT_NE(ip, nullptr);
    EXPECT_EQ(ip_version(*ip),    4u);
    EXPECT_TRUE(is_udp(*ip));
    EXPECT_TRUE(is_multicast_dst(*ip));
    EXPECT_EQ(ip_ihl_bytes(*ip), 20u);
}

TEST(IPv4, ByteOrderDstIP) {
    // dst_ip bytes [30..33] = 224.1.1.1 — first octet in host order should be 224
    auto pkt = make_udp_pkt();
    auto sp  = std::span<const uint8_t>(pkt.data() + 14, pkt.size() - 14);
    const IPv4Header* ip = parse_ipv4(sp);
    ASSERT_NE(ip, nullptr);
    EXPECT_EQ((ntohl(ip->dst_ip) >> 24) & 0xFF, 224u);
}

TEST(IPv4, NonUDPProtocolRejected) {
    auto pkt = make_udp_pkt(9000, 0x06); // TCP
    auto sp  = std::span<const uint8_t>(pkt.data() + 14, pkt.size() - 14);
    EXPECT_EQ(parse_ipv4(sp), nullptr);
}

// ------------------------------ UDP tests ------------------------------------

TEST(UDP, ValidPacket) {
    auto pkt = make_udp_pkt(9000);
    auto sp  = std::span<const uint8_t>(pkt.data() + 34, pkt.size() - 34);
    std::span<const uint8_t> payload;
    const UDPHeader* udp = parse_udp(sp, payload, 9000);
    ASSERT_NE(udp, nullptr);
    EXPECT_EQ(ntohs(udp->dst_port), 9000u);
    EXPECT_EQ(ntohs(udp->src_port), 12345u);
}

TEST(UDP, PortFilterMismatch) {
    auto pkt = make_udp_pkt(9000);
    auto sp  = std::span<const uint8_t>(pkt.data() + 34, pkt.size() - 34);
    std::span<const uint8_t> payload;
    EXPECT_EQ(parse_udp(sp, payload, 8888), nullptr);
}

TEST(UDP, PortFilterZeroAcceptsAny) {
    auto pkt = make_udp_pkt(1234);
    auto sp  = std::span<const uint8_t>(pkt.data() + 34, pkt.size() - 34);
    std::span<const uint8_t> payload;
    EXPECT_NE(parse_udp(sp, payload, 0), nullptr); // port_filter=0 → accept any
}
