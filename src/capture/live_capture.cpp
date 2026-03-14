#include "capture/live_capture.hpp"
#include <pcap/pcap.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

namespace mda {

LiveCapture::LiveCapture(const Config& config) : config_(config) {}

LiveCapture::~LiveCapture() { stop(); }

bool LiveCapture::start() {
    char errbuf[PCAP_ERRBUF_SIZE] = {};
    pcap_ = pcap_open_live(config_.interface.c_str(),
                            config_.snaplen,
                            config_.promisc ? 1 : 0,
                            config_.timeout_ms,
                            errbuf);
    if (!pcap_) { error_ = errbuf; return false; }
    if (!apply_bpf_filter()) return false;
    if (!config_.multicast_group.empty() && !join_multicast_group()) return false;
    active_ = true;
    return true;
}

void LiveCapture::stop() {
    active_ = false;
    if (pcap_) { pcap_close(pcap_); pcap_ = nullptr; }
}

std::optional<Packet> LiveCapture::next_packet() {
    if (!pcap_ || !active_) return std::nullopt;

    struct pcap_pkthdr* hdr  = nullptr;
    const uint8_t*      data = nullptr;
    if (pcap_next_ex(pcap_, &hdr, &data) != 1) return std::nullopt;

    Packet pkt{};
    pkt.timestamp_ns = static_cast<uint64_t>(hdr->ts.tv_sec)  * 1'000'000'000ULL
                     + static_cast<uint64_t>(hdr->ts.tv_usec) * 1'000ULL;
    pkt.len    = hdr->len;
    pkt.caplen = hdr->caplen;
    size_t copy_len = std::min(static_cast<size_t>(hdr->caplen), sizeof(pkt.data));
    std::memcpy(pkt.data, data, copy_len);
    return pkt;
}

bool LiveCapture::apply_bpf_filter() {
    std::string filter = "udp";
    if (config_.port != 0)
        filter += " and dst port " + std::to_string(config_.port);
    if (!config_.multicast_group.empty())
        filter += " and dst host " + config_.multicast_group;

    struct bpf_program fp{};
    if (pcap_compile(pcap_, &fp, filter.c_str(), 1, PCAP_NETMASK_UNKNOWN) < 0) {
        error_ = pcap_geterr(pcap_);
        return false;
    }
    if (pcap_setfilter(pcap_, &fp) < 0) {
        error_ = pcap_geterr(pcap_);
        pcap_freecode(&fp);
        return false;
    }
    pcap_freecode(&fp);
    return true;
}

bool LiveCapture::join_multicast_group() {
    int sock = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { error_ = "socket() failed"; return false; }

    struct ip_mreq mreq{};
    if (inet_aton(config_.multicast_group.c_str(), &mreq.imr_multiaddr) == 0) {
        error_ = "Invalid multicast group: " + config_.multicast_group;
        ::close(sock);
        return false;
    }
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        error_ = "IP_ADD_MEMBERSHIP failed for: " + config_.multicast_group;
        ::close(sock);
        return false;
    }
    ::close(sock);
    return true;
}

} // namespace mda
