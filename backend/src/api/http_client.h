#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <stdexcept>
#include <curl/curl.h>
#include "rate_limiter.h"
#include "curl_handle_pool.h"

namespace baludesk {

/**
 * Exception thrown when an HTTP 401 response is received,
 * indicating that the auth token has expired.
 */
struct TokenExpiredException : public std::runtime_error {
    TokenExpiredException() : std::runtime_error("Token expired (401)") {}
};

/**
 * Exception thrown when an HTTP 429 response is received,
 * indicating rate limiting. Carries the retry_after delay in seconds.
 */
struct RateLimitException : public std::runtime_error {
    int retryAfterSeconds;
    RateLimitException(int retryAfter)
        : std::runtime_error("Rate limited (429)"), retryAfterSeconds(retryAfter) {}
};

/**
 * Exception thrown when an HTTP 507 response is received,
 * indicating that the server storage quota has been exceeded.
 */
struct QuotaExceededException : public std::runtime_error {
    QuotaExceededException() : std::runtime_error("Storage quota exceeded (507)") {}
};

struct RemoteFile {
    std::string name;
    std::string path;
    uint64_t size;
    bool isDirectory;
    std::string modifiedAt;
    std::string hash;  // SHA256 hash for file integrity
};

struct RemoteChange {
    std::string path;
    std::string action; // created, modified, deleted
    std::string timestamp;
};

struct DownloadProgress {
    size_t bytesDownloaded;
    size_t totalBytes;
    double percentage;
};

struct TransferProgress {
    size_t bytesTransferred;
    size_t totalBytes;
    double percentage;  // 0.0 - 100.0
    bool isUpload;      // true = upload, false = download
};

using TransferProgressCallback = std::function<void(const TransferProgress&)>;

// Delta-Sync types for POST /api/sync/changes
struct FileMetadataEntry {
    std::string path;
    std::string hash;
    uint64_t size;
    std::string modified_at;
};

// A2: Conflict info matching BaluHost response fields
struct DeltaConflict {
    std::string path;
    std::string clientHash;
    std::string serverHash;
    std::string serverModifiedAt;
};

struct DeltaSyncResponse {
    std::vector<RemoteFile> toDownload;    // Server has newer version
    std::vector<std::string> toDelete;     // Server deleted these
    std::vector<DeltaConflict> conflicts;  // Both sides changed (A2: proper BaluHost fields)
    std::string changeToken;               // For next sync
};

/**
 * SystemInfoFromServer - System metrics from BaluHost server
 */
struct SystemInfoFromServer {
    double cpuUsage;
    uint32_t cpuCores;
    uint32_t cpuFrequency;
    uint64_t memoryTotal;
    uint64_t memoryUsed;
    uint64_t memoryAvailable;
    uint64_t diskTotal;
    uint64_t diskUsed;
    uint64_t diskAvailable;
    uint64_t uptime;
};

/**
 * Forward declare RaidArray from raid_info.h
 * (included in .cpp to avoid circular dependencies)
 */
struct RaidArray;

/**
 * RaidStatusFromServer - RAID status from BaluHost server
 */
struct RaidStatusFromServer {
    std::vector<RaidArray> arrays;
    bool devMode;
};

/**
 * HttpClient - REST API client for BaluHost NAS
 * 
 * Handles all HTTP communication using libcurl
 */
class HttpClient {
public:
    explicit HttpClient(const std::string& baseUrl);
    ~HttpClient();

    // Authentication
    bool login(const std::string& username, const std::string& password);
    void setAuthToken(const std::string& token);
    void clearAuthToken();
    bool isAuthenticated() const;
    std::string getAuthToken() const { return authToken_; }

    // Token refresh: POST /api/auth/refresh with refresh token
    // Returns new access token on success, throws on failure
    std::string refreshAccessToken(const std::string& refreshToken);

    // Device Registration
    bool registerDevice(const std::string& deviceId, const std::string& deviceName);

    // File operations
    virtual std::vector<RemoteFile> listFiles(const std::string& path);
    virtual bool uploadFile(const std::string& localPath, const std::string& remotePath);
    virtual bool uploadFile(const std::string& localPath, const std::string& remotePath,
                            TransferProgressCallback progressCb);
    virtual bool uploadFileBatch(const std::vector<std::string>& localPaths,
                                  const std::string& remoteDir);
    virtual bool downloadFile(const std::string& remotePath, const std::string& localPath);
    virtual bool deleteFile(const std::string& remotePath);
    
    // Advanced download with resume support
    bool downloadFileRange(
        const std::string& remotePath, 
        const std::string& localPath,
        size_t startByte,
        size_t endByte = 0  // 0 = until end
    );
    
    // Download with progress callback
    using ProgressCallback = std::function<void(const DownloadProgress&)>;
    bool downloadFileWithProgress(
        const std::string& remotePath,
        const std::string& localPath,
        ProgressCallback callback
    );

    // Sync operations
    std::vector<RemoteChange> getChangesSince(const std::string& timestamp);

    // Delta-Sync: POST /api/sync/changes
    DeltaSyncResponse performDeltaSync(const std::string& deviceId,
                                        const std::vector<FileMetadataEntry>& fileList,
                                        const std::string& changeToken = "");

    // System information from BaluHost server
    SystemInfoFromServer getSystemInfoFromServer();
    RaidStatusFromServer getRaidStatusFromServer();

    // Configuration
    void setTimeout(long timeoutSeconds);  // Sets connect timeout
    void setVerbose(bool verbose);

    // Get current base URL
    std::string getBaseUrl() const { return baseUrl_; }

    // Convenience GET helper returning raw response body
    std::string get(const std::string& path);

private:
    std::string performRequest(const std::string& url, const std::string& method,
                               const std::string& body = "");
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static size_t readCallback(void* ptr, size_t size, size_t nmemb, void* userp);
    static size_t writeFileCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static int progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                               curl_off_t ultotal, curl_off_t ulnow);
    static int transferProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                                        curl_off_t ultotal, curl_off_t ulnow);

    // Apply common timeout settings to a CURL handle: connect timeout + stall detection
    void applyTimeoutSettings(CURL* handle);

    std::string baseUrl_;
    std::string authToken_;
    CURL* curl_;
    long connectTimeout_;   // Connection timeout in seconds (default 30)
    long lowSpeedLimit_;    // Minimum bytes/sec before considering stalled (default 1024)
    long lowSpeedTime_;     // Seconds below lowSpeedLimit before aborting (default 60)
    bool verbose_;

    // Upload optimization: rate limiter (55 req/60s) and connection pool (6 handles)
    RateLimiter uploadRateLimiter_{55, 60};
    CurlHandlePool handlePool_{6};
};

} // namespace baludesk
