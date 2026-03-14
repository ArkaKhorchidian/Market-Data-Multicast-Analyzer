#include "analysis/book_reconstructor.hpp"
#include "util/timestamp.hpp"
#include <sstream>
#include <iomanip>

namespace mda {

void BookReconstructor::add_to_level(char side, int64_t price, uint32_t qty) {
    auto& lvl_map = (side == 'B' || side == 'b') ? bid_levels_ :
                    reinterpret_cast<decltype(bid_levels_)&>(ask_levels_);
    // Can't use the same reference trick for differently-typed maps; use if/else:
    if (side == 'B' || side == 'b') {
        auto& [q, cnt] = bid_levels_[price];
        q += qty; ++cnt;
    } else {
        auto& [q, cnt] = ask_levels_[price];
        q += qty; ++cnt;
    }
    (void)lvl_map; // suppress warning
}

void BookReconstructor::remove_from_level(char side, int64_t price, uint32_t qty) {
    if (side == 'B' || side == 'b') {
        auto it = bid_levels_.find(price);
        if (it == bid_levels_.end()) return;
        it->second.first  = (it->second.first  > qty) ? it->second.first  - qty : 0;
        it->second.second = (it->second.second  > 0)  ? it->second.second - 1   : 0;
        if (it->second.first == 0) bid_levels_.erase(it);
    } else {
        auto it = ask_levels_.find(price);
        if (it == ask_levels_.end()) return;
        it->second.first  = (it->second.first  > qty) ? it->second.first  - qty : 0;
        it->second.second = (it->second.second  > 0)  ? it->second.second - 1   : 0;
        if (it->second.first == 0) ask_levels_.erase(it);
    }
}

void BookReconstructor::on_add(uint64_t order_id, char side, int64_t price,
                                uint32_t qty, uint64_t timestamp_ns) {
    orders_[order_id] = Order{price, qty, side};
    add_to_level(side, price, qty);
    last_update_ns_ = timestamp_ns;
}

void BookReconstructor::on_delete(uint64_t order_id, uint64_t timestamp_ns) {
    auto it = orders_.find(order_id);
    if (it == orders_.end()) { stale_ = true; return; }
    remove_from_level(it->second.side, it->second.price, it->second.qty);
    orders_.erase(it);
    last_update_ns_ = timestamp_ns;
}

void BookReconstructor::on_execute(uint64_t order_id, uint32_t executed_qty,
                                    uint64_t timestamp_ns) {
    auto it = orders_.find(order_id);
    if (it == orders_.end()) { stale_ = true; return; }
    remove_from_level(it->second.side, it->second.price, executed_qty);
    if (it->second.qty <= executed_qty)
        orders_.erase(it);
    else
        it->second.qty -= executed_qty;
    last_update_ns_ = timestamp_ns;
}

void BookReconstructor::on_replace(uint64_t old_id, uint64_t new_id,
                                    int64_t new_price, uint32_t new_qty,
                                    uint64_t timestamp_ns) {
    char side = 'B';
    auto it = orders_.find(old_id);
    if (it != orders_.end()) side = it->second.side;
    on_delete(old_id, timestamp_ns);
    on_add(new_id, side, new_price, new_qty, timestamp_ns);
}

void BookReconstructor::on_trade(int64_t /*price*/, uint32_t /*qty*/,
                                  uint64_t timestamp_ns) {
    last_update_ns_ = timestamp_ns;
}

std::vector<BookReconstructor::Level> BookReconstructor::bids(int depth) const {
    std::vector<Level> result;
    result.reserve(static_cast<size_t>(depth));
    for (const auto& [price, qc] : bid_levels_) {
        if (static_cast<int>(result.size()) >= depth) break;
        result.push_back(Level{price, qc.first, qc.second});
    }
    return result;
}

std::vector<BookReconstructor::Level> BookReconstructor::asks(int depth) const {
    std::vector<Level> result;
    result.reserve(static_cast<size_t>(depth));
    for (const auto& [price, qc] : ask_levels_) {
        if (static_cast<int>(result.size()) >= depth) break;
        result.push_back(Level{price, qc.first, qc.second});
    }
    return result;
}

int64_t BookReconstructor::best_bid()  const {
    return bid_levels_.empty() ? 0 : bid_levels_.begin()->first;
}
int64_t BookReconstructor::best_ask()  const {
    return ask_levels_.empty() ? 0 : ask_levels_.begin()->first;
}
int64_t BookReconstructor::mid_price() const { return (best_bid() + best_ask()) / 2; }
int64_t BookReconstructor::spread()    const { return best_ask() - best_bid(); }

std::string BookReconstructor::snapshot_str(int depth, double price_scale) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);

    auto bid_lvls = bids(depth);
    auto ask_lvls = asks(depth);

    oss << "  BID: ";
    for (size_t i = 0; i < bid_lvls.size(); ++i) {
        if (i > 0) oss << " | ";
        oss << bid_lvls[i].price / price_scale << " x " << bid_lvls[i].qty;
    }
    if (bid_lvls.empty()) oss << "(empty)";
    oss << "\n";

    oss << "  ASK: ";
    for (size_t i = 0; i < ask_lvls.size(); ++i) {
        if (i > 0) oss << " | ";
        oss << ask_lvls[i].price / price_scale << " x " << ask_lvls[i].qty;
    }
    if (ask_lvls.empty()) oss << "(empty)";
    oss << "\n";

    if (best_bid() > 0 && best_ask() > 0) {
        oss << std::setprecision(4)
            << "  Spread: " << std::setprecision(2) << spread() / price_scale * 100.0
            << "\xc2\xa2"   // UTF-8 cent sign
            << "  Mid: " << mid_price() / price_scale << "\n";
    }
    if (stale_) oss << "  [STALE -- sequence gap detected]\n";
    return oss.str();
}

} // namespace mda
