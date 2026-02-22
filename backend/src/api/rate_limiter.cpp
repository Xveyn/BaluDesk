#include "rate_limiter.h"

namespace baludesk {

RateLimiter::RateLimiter(int maxRequests, int windowSeconds)
    : maxRequests_(maxRequests), window_(windowSeconds) {}

bool RateLimiter::acquire(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);

    auto deadline = std::chrono::steady_clock::now() + timeout;

    while (true) {
        purgeExpired();

        if (static_cast<int>(timestamps_.size()) < maxRequests_) {
            timestamps_.push_back(std::chrono::steady_clock::now());
            return true;
        }

        // Calculate when the oldest entry expires
        auto oldest = timestamps_.front();
        auto expiry = oldest + window_;
        auto waitUntil = (std::min)(expiry, deadline);

        if (waitUntil <= std::chrono::steady_clock::now()) {
            // Already past deadline
            return false;
        }

        cv_.wait_until(lock, waitUntil);

        if (std::chrono::steady_clock::now() >= deadline) {
            // Check once more after waking
            purgeExpired();
            if (static_cast<int>(timestamps_.size()) < maxRequests_) {
                timestamps_.push_back(std::chrono::steady_clock::now());
                return true;
            }
            return false;
        }
    }
}

bool RateLimiter::canAcquire() {
    std::lock_guard<std::mutex> lock(mutex_);
    purgeExpired();
    return static_cast<int>(timestamps_.size()) < maxRequests_;
}

int RateLimiter::remaining() {
    std::lock_guard<std::mutex> lock(mutex_);
    purgeExpired();
    int used = static_cast<int>(timestamps_.size());
    return (std::max)(0, maxRequests_ - used);
}

void RateLimiter::purgeExpired() {
    auto cutoff = std::chrono::steady_clock::now() - window_;
    while (!timestamps_.empty() && timestamps_.front() < cutoff) {
        timestamps_.pop_front();
    }
}

} // namespace baludesk
