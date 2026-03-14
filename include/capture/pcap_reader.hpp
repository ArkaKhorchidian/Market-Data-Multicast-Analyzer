#pragma once
#include "packet.hpp"
#include <string>
#include <functional>
#include <optional>
#include <cstdint>

// Forward-declare pcap_t to avoid pulling in the full pcap header here
struct pcap;
typedef struct pcap pcap_t;

namespace mda {

class PcapReader {
public:
    explicit PcapReader(const std::string& filepath);
    ~PcapReader();

    PcapReader(const PcapReader&)            = delete;
    PcapReader& operator=(const PcapReader&) = delete;

    // Returns next packet or nullopt at EOF / error
    std::optional<Packet> next_packet();

    // Process all packets with a callback; returns total count
    uint64_t process_all(std::function<void(const Packet&)> callback);

    bool               is_open() const { return pcap_ != nullptr; }
    const std::string& error()   const { return error_; }

private:
    pcap_t*     pcap_  = nullptr;
    std::string error_;
};

} // namespace mda
