#include "capture/pcap_reader.hpp"
#include <pcap/pcap.h>
#include <cstring>

namespace mda {

PcapReader::PcapReader(const std::string& filepath) {
    char errbuf[PCAP_ERRBUF_SIZE] = {};
    pcap_ = pcap_open_offline(filepath.c_str(), errbuf);
    if (!pcap_) error_ = errbuf;
}

PcapReader::~PcapReader() {
    if (pcap_) { pcap_close(pcap_); pcap_ = nullptr; }
}

std::optional<Packet> PcapReader::next_packet() {
    if (!pcap_) return std::nullopt;

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

uint64_t PcapReader::process_all(std::function<void(const Packet&)> callback) {
    uint64_t count = 0;
    while (auto pkt = next_packet()) { callback(*pkt); ++count; }
    return count;
}

} // namespace mda
