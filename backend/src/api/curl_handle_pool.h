#pragma once

#include <vector>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <curl/curl.h>

namespace baludesk {

class CurlHandlePool;

/**
 * RAII wrapper that returns a CURL handle to the pool on destruction.
 */
class ScopedHandle {
public:
    ScopedHandle(CURL* handle, CurlHandlePool* pool);
    ~ScopedHandle();

    // Non-copyable, movable
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;
    ScopedHandle(ScopedHandle&& other) noexcept;
    ScopedHandle& operator=(ScopedHandle&& other) noexcept;

    CURL* get() const { return handle_; }
    operator CURL*() const { return handle_; }

private:
    CURL* handle_;
    CurlHandlePool* pool_;
};

/**
 * Pool of reusable CURL handles with connection keep-alive.
 * Thread-safe. acquire() blocks until a handle is available.
 */
class CurlHandlePool {
public:
    /**
     * @param maxHandles  Maximum number of pooled handles (default 6)
     */
    explicit CurlHandlePool(size_t maxHandles = 6);
    ~CurlHandlePool();

    // Non-copyable
    CurlHandlePool(const CurlHandlePool&) = delete;
    CurlHandlePool& operator=(const CurlHandlePool&) = delete;

    /**
     * Acquire a handle from the pool (blocks if all in use).
     * The returned ScopedHandle automatically returns to pool on destruction.
     */
    ScopedHandle acquire();

    /** Return a handle to the pool (called by ScopedHandle destructor) */
    void release(CURL* handle);

private:
    /** Reset handle and apply persistent keep-alive/reuse options */
    void resetHandle(CURL* handle);

    size_t maxHandles_;
    std::vector<CURL*> available_;
    size_t totalCreated_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

} // namespace baludesk
