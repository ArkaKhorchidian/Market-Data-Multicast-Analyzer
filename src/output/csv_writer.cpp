#include "output/csv_writer.hpp"

namespace mda {

CsvWriter::CsvWriter(const std::string& filepath) : out_(filepath) {}

CsvWriter::~CsvWriter() { if (out_.is_open()) out_.close(); }

void CsvWriter::write_header() {
    out_ << "timestamp_ns,iat_ns,seq_num,msg_type,gap_detected,gap_size\n";
}

void CsvWriter::write_packet(uint64_t timestamp_ns, uint64_t iat_ns,
                              uint32_t seq_num,      char     msg_type,
                              bool     gap_detected, uint32_t gap_size) {
    out_ << timestamp_ns << ','
         << iat_ns       << ','
         << seq_num      << ','
         << msg_type     << ','
         << (gap_detected ? 1 : 0) << ','
         << gap_size     << '\n';
}

} // namespace mda
