#ifndef __SPSC_SPSCRINGBUFFER__
#define __SPSC_SPSCRINGBUFFER__
#include <atomic>
#include <cstddef>
#include <unistd.h>
#include <utility>
#include <array>

template<typename T, size_t Capacity>
class SpscRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    static_assert(Capacity >= 4, "Capacity too small");

    static constexpr size_t MASK = Capacity - 1;

    // 生产者主要写，消费者主要读
    alignas(64) std::atomic<size_t> write_idx{0};

    // 消费者主要写，生产者主要读
    alignas(64) std::atomic<size_t> read_idx{0};

    // 数据缓冲区也独立 cache line
    alignas(64) std::array<T, Capacity> slots{};

public:
    SpscRingBuffer() = default;
    ~SpscRingBuffer() = default;

    SpscRingBuffer(const SpscRingBuffer&) = delete;
    SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;
    SpscRingBuffer(SpscRingBuffer&&) = delete;
    SpscRingBuffer& operator=(SpscRingBuffer&&) = delete;

    // ────────────────────────────────────────────────
    //  生产者接口（只能单线程调用）
    // ────────────────────────────────────────────────

    // 非阻塞 push
    bool try_emplace(auto&&... args) {
        size_t w = write_idx.load(std::memory_order_relaxed);
        size_t next_w = (w + 1) & MASK;

        // 消费者看到的 read_idx
        if (next_w == read_idx.load(std::memory_order_acquire)) {
            return false;  // full
        }

        // 构造（支持 emplacement）
        new (&slots[w]) T(std::forward<decltype(args)>(args)...);

        // 发布写操作
        write_idx.store(next_w, std::memory_order_release);
        return true;
    }

    // 非阻塞 push（拷贝版）
    bool try_push(const T& value) {
        return try_emplace(value);
    }

    // 非阻塞 push（移动版）
    bool try_push(T&& value) {
        return try_emplace(std::move(value));
    }

    // 阻塞 push（慎用！通常在实时系统中不推荐阻塞）
    void push(const T& value) {
        while (!try_push(value)) {
            // 可选：_mm_pause() / asm volatile("pause" ::: "memory");
        }
    }

    void push(T&& value) {
        while (!try_push(std::move(value))) {}
    }

    // ────────────────────────────────────────────────
    //  消费者接口（只能单线程调用）
    // ────────────────────────────────────────────────

    // 非阻塞 pop
    bool try_pop(T& out) {
        size_t r = read_idx.load(std::memory_order_relaxed);
        size_t w = write_idx.load(std::memory_order_acquire);

        if (r == w) {
            return false;  // empty
        }

        // 移动元素
        out = std::move(slots[r]);

        // 析构原位置（重要！尤其是非 trivial 类型）
        slots[r].~T();

        // 更新 read_idx，发布给生产者
        read_idx.store((r + 1) & MASK, std::memory_order_release);
        return true;
    }

    // 只看不取（peek），常用于检查类型或部分字段
    bool peek(T*& ptr) noexcept {
        size_t r = read_idx.load(std::memory_order_relaxed);
        size_t w = write_idx.load(std::memory_order_acquire);

        if (r == w) {
            return false;
        }

        ptr = std::addressof(slots[r]);
        return true;
    }

    // ────────────────────────────────────────────────
    //  查询接口（const 安全，内存序较弱）
    // ────────────────────────────────────────────────

    bool empty() const noexcept {
        size_t r = read_idx.load(std::memory_order_relaxed);
        size_t w = write_idx.load(std::memory_order_acquire);
        return r == w;
    }

    bool full() const noexcept {
        size_t w = write_idx.load(std::memory_order_relaxed);
        size_t next = (w + 1) & MASK;
        return next == read_idx.load(std::memory_order_acquire);
    }

    size_t size() const noexcept {
        size_t w = write_idx.load(std::memory_order_relaxed);
        size_t r = read_idx.load(std::memory_order_relaxed);
        return (w - r) & MASK;
    }

    constexpr size_t capacity() const noexcept {
        return Capacity;
    }
};


#endif /* __SPSC_SPSCRINGBUFFER__ */
