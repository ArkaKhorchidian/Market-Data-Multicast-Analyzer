#pragma once
#include "packet.hpp"
#include <string>
#include <functional>
#include <optional>
#include <cstdint>

struct pcap;
typedef struct pcap pcap_t;

namespace mda {

class LiveCapture {
public:
    struct Config {
        std::string interface;
        std::string multicast_group;  // e.g., "233.54.12.111"
        uint16_t    port       = 0;   // 0 = all ports
        int         snaplen    = 9000;
        int         timeout_ms = 100;
        bool        promisc    = false;
    };

    explicit LiveCapture(const Config& config);
    ~LiveCapture();

    LiveCapture(const LiveCapture&)            = delete;
    LiveCapture& operator=(const LiveCapture&) = delete;

    bool start();
    void stop();

    // Returns next captured packet, or nullopt on timeout / error
    std::optional<Packet> next_packet();

    bool               is_active() const { return active_; }
    const std::string& error()     const { return error_; }

private:
    Config      config_;
    pcap_t*     pcap_   = nullptr;
    bool        active_ = false;
    std::string error_;

    bool join_multicast_group();
    bool apply_bpf_filter();
};

} // namespace mda
