#pragma once
#include <cstdint>
#include <map>
#include <unordered_map>
#include <vector>
#include <string>
#include <functional>

namespace mda {

// Reconstructs an L2 order book from a sequence of feed messages.
//
// Prices are stored as fixed-point integers (8 decimal places).
// Bid side sorted descending (best bid first); ask side ascending (best ask first).
// If a sequence gap is detected upstream, callers should call mark_stale() so
// that the book is clearly flagged in output.
class BookReconstructor {
public:
    struct Level {
        int64_t  price;
        uint32_t qty;
        uint32_t order_count;
    };

    void on_add(uint64_t order_id, char side, int64_t price,
                uint32_t qty, uint64_t timestamp_ns);
    void on_delete(uint64_t order_id, uint64_t timestamp_ns);
    void on_execute(uint64_t order_id, uint32_t executed_qty, uint64_t timestamp_ns);
    void on_replace(uint64_t old_order_id, uint64_t new_order_id,
                    int64_t new_price, uint32_t new_qty, uint64_t timestamp_ns);
    void on_trade(int64_t price, uint32_t qty, uint64_t timestamp_ns);

    void mark_stale() { stale_ = true; }

    // Snapshot — depth = number of price levels to return
    std::vector<Level> bids(int depth = 5) const;
    std::vector<Level> asks(int depth = 5) const;

    int64_t best_bid()  const;
    int64_t best_ask()  const;
    int64_t mid_price() const;
    int64_t spread()    const;

    bool     is_stale()       const { return stale_; }
    uint64_t last_update_ns() const { return last_update_ns_; }

    // Human-readable snapshot string.
    // price_scale: divisor to convert fixed-point to dollars (default 1e8 for 8dp)
    std::string snapshot_str(int depth = 5, double price_scale = 1e8) const;

private:
    struct Order { int64_t price; uint32_t qty; char side; };

    // Bid: descending price order (best bid first)
    std::map<int64_t, std::pair<uint32_t,uint32_t>, std::greater<int64_t>> bid_levels_;
    // Ask: ascending price order (best ask first)
    std::map<int64_t, std::pair<uint32_t,uint32_t>>                         ask_levels_;
    std::unordered_map<uint64_t, Order> orders_;

    bool     stale_          = false;
    uint64_t last_update_ns_ = 0;

    void add_to_level(char side, int64_t price, uint32_t qty);
    void remove_from_level(char side, int64_t price, uint32_t qty);
};

} // namespace mda
