#pragma once
// Fixed-size lock-free SPSC ring buffer for packet storage.
//
// Design goals:
//   - Zero heap allocation on the packet processing path (hot path).
//   - Single-producer / single-consumer (SPSC): one capture thread pushes,
//     one analysis thread pops. No mutex required.
//   - Capacity must be a power of two (enforced by static_assert) so that
//     index wrap-around is a bitmask instead of a modulo.

#include <atomic>
#include <array>
#include <optional>
#include <cstddef>

namespace mda {

template<typename T, size_t N>
class RingBuffer {
    static_assert(N > 0 && (N & (N - 1)) == 0, "N must be a power of two");

public:
    // Push item from the producer thread.
    // Returns false (packet dropped) if the buffer is full.
    bool push(const T& item) noexcept {
        size_t head = head_.load(std::memory_order_relaxed);
        size_t next = (head + 1) & mask_;
        if (next == tail_.load(std::memory_order_acquire))
            return false; // full
        buf_[head] = item;
        head_.store(next, std::memory_order_release);
        return true;
    }

    // Pop item from the consumer thread.
    std::optional<T> pop() noexcept {
        size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire))
            return std::nullopt; // empty
        T item = buf_[tail];
        tail_.store((tail + 1) & mask_, std::memory_order_release);
        return item;
    }

    bool   empty() const noexcept {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

    size_t size() const noexcept {
        size_t h = head_.load(std::memory_order_acquire);
        size_t t = tail_.load(std::memory_order_acquire);
        return (h - t) & mask_;
    }

    static constexpr size_t capacity() { return N - 1; }

private:
    static constexpr size_t mask_ = N - 1;
    alignas(64) std::atomic<size_t> head_{0}; // producer writes
    alignas(64) std::atomic<size_t> tail_{0}; // consumer writes
    std::array<T, N> buf_{};
};

} // namespace mda
