#include <atomic>
#include <array>
#include <emmintrin.h>
#include <type_traits>
#include <utility>

template<typename T, size_t Capacity>
class SpmcRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    static_assert(Capacity >= 16 && Capacity <= (1ULL << 30));

    static constexpr size_t MASK          = Capacity - 1;
    static constexpr size_t SEQ_MASK      = MASK;                   // sequence 低位用于槽索引
    static constexpr size_t SEQ_INCR      = Capacity;               // 每次完整循环 +Capacity

public:
    SpmcRingBuffer() {
        for (size_t i = 0; i < Capacity; ++i) {
            seq_[i].store(i, std::memory_order_relaxed);
        }
    }

    ~SpmcRingBuffer() = default;

    SpmcRingBuffer(const SpmcRingBuffer&) = delete;
    SpmcRingBuffer& operator=(const SpmcRingBuffer&) = delete;

    // ────────────────────────────────────────────────
    //  单生产者 push
    // ────────────────────────────────────────────────

    template<typename... Args>
    bool try_emplace(Args&&... args) {
        size_t tail = tail_.load(std::memory_order_relaxed);
        size_t pos  = tail & MASK;

        // 读取当前槽位的预期 sequence（tail 对应的序列号）
        size_t expected_seq = tail;

        // 如果槽位的 sequence 不是我们期望的值，说明还没被消费完（队列满）
        if (seq_[pos].load(std::memory_order_acquire) != expected_seq) {
            return false;
        }

        // 原地构造
        new (&slots_[pos]) T(std::forward<Args>(args)...);

        // 更新 sequence 为下一个值（tail + 1）
        seq_[pos].store(tail + 1, std::memory_order_release);

        // 推进 tail
        tail_.store(tail + 1, std::memory_order_relaxed);

        return true;
    }

    // ────────────────────────────────────────────────
    //  多消费者 pop
    // ────────────────────────────────────────────────

    bool try_pop(T& out) {
        
        size_t head = head_.load(std::memory_order_relaxed);
        while(1) {
            size_t pos  = head & MASK;

            // 读取当前槽位的 sequence
            size_t seq = seq_[pos].load(std::memory_order_acquire);
            // 没数据
            if (seq < head + 1) {
                return false;
            }

            if (seq > head + 1) {
                head = head_.load(std::memory_order_relaxed);
                continue;
            }

            if (head_.compare_exchange_strong(head, head + 1,
                                                std::memory_order_acquire,
                                                std::memory_order_relaxed)) {
                if constexpr (!std::is_trivially_destructible_v<T> ||
                          !std::is_trivially_copyable_v<T>) {
                    out = std::move(slots_[pos]);
                    slots_[pos].~T();
                } else {
                    // trivial 类型直接拷贝（int/float 等最快）
                    out = slots_[pos];
                    // 无需析构
                }
                // 成功，标记槽位可重用（写成 head + Capacity）
                seq_[pos].store(head + SEQ_INCR, std::memory_order_release);
                return true;
            }
            _mm_pause();
        }
    }

    // ────────────────────────────────────────────────
    //  查询（近似值）
    // ────────────────────────────────────────────────

    bool empty() const noexcept {
        return size() == 0;
    }

    size_t size() const noexcept {
        size_t t = tail_.load(std::memory_order_acquire);
        size_t h = head_.load(std::memory_order_acquire);
        return (t > h) ? (t - h) : 0;
    }

    constexpr size_t capacity() const noexcept { return Capacity; }

private:
    alignas(64) std::atomic<size_t> tail_{0};
    alignas(64) std::atomic<size_t> head_{0};

    alignas(64) std::array<std::atomic<size_t>, Capacity> seq_;
    alignas(64) std::array<T, Capacity> slots_{};
};