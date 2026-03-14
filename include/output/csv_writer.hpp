#pragma once
// Per-packet CSV output for Python visualization scripts.
// Columns: timestamp_ns, iat_ns, seq_num, msg_type, gap_detected, gap_size

#include <string>
#include <fstream>

namespace mda {

class CsvWriter {
public:
    explicit CsvWriter(const std::string& filepath);
    ~CsvWriter();

    CsvWriter(const CsvWriter&)            = delete;
    CsvWriter& operator=(const CsvWriter&) = delete;

    void write_header();
    void write_packet(uint64_t timestamp_ns, uint64_t iat_ns,
                      uint32_t seq_num,      char     msg_type,
                      bool     gap_detected, uint32_t gap_size);

    bool is_open() const { return out_.is_open(); }

private:
    std::ofstream out_;
};

} // namespace mda
