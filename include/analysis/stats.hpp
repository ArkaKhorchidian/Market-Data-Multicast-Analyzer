#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace mda {

// Compute p-th percentile (p in [0,1]) from a *sorted* vector.
inline double percentile_sorted(const std::vector<uint64_t>& sorted, double p) {
    if (sorted.empty()) return 0.0;
    if (p <= 0.0) return static_cast<double>(sorted.front());
    if (p >= 1.0) return static_cast<double>(sorted.back());
    double idx = p * static_cast<double>(sorted.size() - 1);
    size_t lo  = static_cast<size_t>(idx);
    size_t hi  = lo + 1;
    if (hi >= sorted.size()) return static_cast<double>(sorted.back());
    double frac = idx - static_cast<double>(lo);
    return sorted[lo] * (1.0 - frac) + sorted[hi] * frac;
}

// Compute percentile from unsorted data (copies and sorts internally).
inline double percentile(std::vector<uint64_t> data, double p) {
    std::sort(data.begin(), data.end());
    return percentile_sorted(data, p);
}

inline double mean(const std::vector<uint64_t>& v) {
    if (v.empty()) return 0.0;
    double sum = 0.0;
    for (auto x : v) sum += static_cast<double>(x);
    return sum / static_cast<double>(v.size());
}

inline double stddev(const std::vector<uint64_t>& v) {
    if (v.size() < 2) return 0.0;
    double m  = mean(v);
    double sq = 0.0;
    for (auto x : v) { double d = static_cast<double>(x) - m; sq += d * d; }
    return std::sqrt(sq / static_cast<double>(v.size() - 1));
}

} // namespace mda
