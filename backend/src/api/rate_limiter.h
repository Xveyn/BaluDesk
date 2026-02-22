#pragma once

#include <chrono>
#include <deque>
#include <mutex>
#include <condition_variable>

namespace baludesk {

/**
 * Token-Bucket Rate Limiter with sliding window.
 * Thread-safe. acquire() blocks until a slot is available.
 */
class RateLimiter {
public:
    /**
     * @param maxRequests  Maximum requests allowed in the time window
     * @param windowSeconds  Time window in seconds
     */
    RateLimiter(int maxRequests, int windowSeconds);

    /**
     * Blocks until a request slot is available.
     * @param timeout  Maximum time to wait (default 120s)
     * @return true if slot acquired, false if timed out
     */
    bool acquire(std::chrono::milliseconds timeout = std::chrono::milliseconds(120000));

    /** Non-blocking check: can we acquire a slot right now? */
    bool canAcquire();

    /** Number of requests remaining in current window */
    int remaining();

private:
    void purgeExpired();

    int maxRequests_;
    std::chrono::seconds window_;
    std::deque<std::chrono::steady_clock::time_point> timestamps_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

} // namespace baludesk
