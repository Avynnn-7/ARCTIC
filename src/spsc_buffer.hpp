#pragma once

#include <atomic>
#include <vector>
#include <cstddef>
#include <stdexcept>

namespace arctic {

// spsc ring buffer. fast as hell, lock free.
// capacity MUST be power of two or bitwise math explodes.
template <typename T>
class SPSCBuffer {
public:
    explicit SPSCBuffer(size_t capacity) {
        if (!capacity || (capacity & (capacity - 1)) != 0) {
            throw std::invalid_argument("Capacity must be a power of two");
        }
        buffer_.resize(capacity);
        mask_ = capacity - 1;
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }

    // Producer only
    bool push(const T& item) {
        size_t current_tail = tail_.load(std::memory_order_relaxed);
        size_t next_tail = (current_tail + 1) & mask_;
        
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; // Buffer is full
        }
        
        buffer_[current_tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    // Consumer only
    bool pop(T& item) {
        size_t current_head = head_.load(std::memory_order_relaxed);
        
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false; // Buffer is empty
        }
        
        item = buffer_[current_head];
        head_.store((current_head + 1) & mask_, std::memory_order_release);
        return true;
    }

private:
    std::vector<T> buffer_;
    size_t mask_;
    
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324)
#endif

    // alignas 64 so core 0 and core 1 don't fight over the same cache line
    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;

#ifdef _MSC_VER
#pragma warning(pop)
#endif
};

} // namespace arctic
