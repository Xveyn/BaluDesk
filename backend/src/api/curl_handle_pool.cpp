#include "curl_handle_pool.h"
#include "../utils/logger.h"
#include <stdexcept>

namespace baludesk {

// ============================================================================
// ScopedHandle
// ============================================================================

ScopedHandle::ScopedHandle(CURL* handle, CurlHandlePool* pool)
    : handle_(handle), pool_(pool) {}

ScopedHandle::~ScopedHandle() {
    if (handle_ && pool_) {
        pool_->release(handle_);
    }
}

ScopedHandle::ScopedHandle(ScopedHandle&& other) noexcept
    : handle_(other.handle_), pool_(other.pool_) {
    other.handle_ = nullptr;
    other.pool_ = nullptr;
}

ScopedHandle& ScopedHandle::operator=(ScopedHandle&& other) noexcept {
    if (this != &other) {
        if (handle_ && pool_) {
            pool_->release(handle_);
        }
        handle_ = other.handle_;
        pool_ = other.pool_;
        other.handle_ = nullptr;
        other.pool_ = nullptr;
    }
    return *this;
}

// ============================================================================
// CurlHandlePool
// ============================================================================

CurlHandlePool::CurlHandlePool(size_t maxHandles)
    : maxHandles_(maxHandles), totalCreated_(0) {
    Logger::debug("CurlHandlePool created with max {} handles", maxHandles);
}

CurlHandlePool::~CurlHandlePool() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto* handle : available_) {
        curl_easy_cleanup(handle);
    }
    available_.clear();
    Logger::debug("CurlHandlePool destroyed ({} handles cleaned up)", totalCreated_);
}

ScopedHandle CurlHandlePool::acquire() {
    std::unique_lock<std::mutex> lock(mutex_);

    // If a handle is available in the pool, reuse it
    if (!available_.empty()) {
        CURL* handle = available_.back();
        available_.pop_back();
        resetHandle(handle);
        return ScopedHandle(handle, this);
    }

    // If we haven't hit the limit, create a new handle
    if (totalCreated_ < maxHandles_) {
        CURL* handle = curl_easy_init();
        if (!handle) {
            throw std::runtime_error("Failed to create CURL handle");
        }
        totalCreated_++;
        resetHandle(handle);
        Logger::debug("CurlHandlePool: created new handle ({}/{})", totalCreated_, maxHandles_);
        return ScopedHandle(handle, this);
    }

    // All handles in use — wait for one to be returned
    cv_.wait(lock, [this]() { return !available_.empty(); });

    CURL* handle = available_.back();
    available_.pop_back();
    resetHandle(handle);
    return ScopedHandle(handle, this);
}

void CurlHandlePool::release(CURL* handle) {
    if (!handle) return;

    std::lock_guard<std::mutex> lock(mutex_);
    available_.push_back(handle);
    cv_.notify_one();
}

void CurlHandlePool::resetHandle(CURL* handle) {
    // Reset all options to defaults
    curl_easy_reset(handle);

    // Apply persistent connection-reuse and keep-alive options
    curl_easy_setopt(handle, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(handle, CURLOPT_TCP_KEEPIDLE, 60L);
    curl_easy_setopt(handle, CURLOPT_TCP_KEEPINTVL, 30L);
    curl_easy_setopt(handle, CURLOPT_FORBID_REUSE, 0L);

    // Connection cache: keep connections alive for up to 5 minutes
    curl_easy_setopt(handle, CURLOPT_MAXAGE_CONN, 300L);

    // TCP_NODELAY: disable Nagle algorithm for lower latency
    curl_easy_setopt(handle, CURLOPT_TCP_NODELAY, 1L);

    // Larger upload buffer (2 MB for LAN throughput)
    curl_easy_setopt(handle, CURLOPT_UPLOAD_BUFFERSIZE, 2L * 1024L * 1024L);

    // Larger receive buffer (512 KB)
    curl_easy_setopt(handle, CURLOPT_BUFFERSIZE, 512L * 1024L);

    // Bypass proxy for local addresses
    curl_easy_setopt(handle, CURLOPT_NOPROXY, "127.0.0.1;localhost");
    curl_easy_setopt(handle, CURLOPT_PROXY, "");
}

} // namespace baludesk
