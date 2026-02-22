#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <queue>
#include <mutex>
#include <atomic>
#include <thread>
#include <future>
#include <chrono>
#include "change_detector.h"  // For DetectedChange, ConflictInfo
#include "../api/http_client.h"  // For TokenExpiredException
#include "../utils/credential_store.h"
#include "../utils/logger.h"  // For withTokenRefresh template

namespace baludesk {

// Forward declarations
class FileWatcher;
class Database;
class ConflictResolver;
class ChangeDetector;

// Sync status enumeration
enum class SyncStatus {
    IDLE,
    SYNCING,
    PAUSED,
    SYNC_ERROR
};

// Sync direction per folder
enum class SyncDirection {
    BIDIRECTIONAL,  // Full two-way sync (default)
    PUSH,           // Local → NAS only (Send Only)
    PULL            // NAS → Local only (Receive Only)
};

// File change action
enum class FileAction {
    CREATED,
    MODIFIED,
    DELETED
};

// File event structure
struct FileEvent {
    std::string path;
    FileAction action;
    uint64_t size;
    std::string timestamp;
};

// Sync folder configuration
struct SyncFolder {
    std::string id;
    std::string localPath;
    std::string remotePath;
    SyncStatus status;
    bool enabled;
    std::string createdAt;
    std::string lastSync;
    uint64_t size;  // Folder size in bytes
    SyncDirection direction = SyncDirection::BIDIRECTIONAL;
    std::string conflictResolution = "ask";
};

// Sync statistics
struct SyncStats {
    SyncStatus status;
    uint64_t uploadSpeed;      // bytes/sec
    uint64_t downloadSpeed;
    uint32_t pendingUploads;
    uint32_t pendingDownloads;
    std::string lastSync;
    std::string currentFile;   // Currently syncing file
    uint32_t totalFiles;       // Total files to process
    uint32_t processedFiles;   // Files processed so far
    uint64_t currentFileSize;        // Size of current file in bytes
    uint64_t currentFileTransferred; // Bytes transferred so far
    double   currentFilePercent;     // 0.0 - 100.0
};

/**
 * SyncEngine - Core synchronization engine
 * 
 * Responsibilities:
 * - Manage sync folders
 * - Coordinate file watching, change detection, and sync operations
 * - Handle conflicts
 * - Provide sync status updates
 */
class SyncEngine {
public:
    SyncEngine();
    ~SyncEngine();

    // Lifecycle
    bool initialize(const std::string& dbPath, const std::string& serverUrl);
    void start();
    void stop();
    bool isRunning() const;

    // Authentication
    bool login(const std::string& username, const std::string& password, const std::string& serverUrl = "");
    void logout();
    bool isAuthenticated() const;

    // Device Code Flow authentication
    bool setTokens(const std::string& serverUrl, const std::string& accessToken,
                   const std::string& refreshToken, const std::string& username);
    std::string checkStoredTokens();  // returns "authenticated" | "needs_pairing"

    // Auth-required callback (called when token refresh fails)
    using AuthRequiredCallback = std::function<void()>;
    void setAuthRequiredCallback(AuthRequiredCallback cb);

    // Get stored username/serverUrl for frontend
    std::string getStoredUsername() const;
    std::string getStoredServerUrl() const;

    // Sync folder management
    bool addSyncFolder(SyncFolder& folder);  // Modified to set folder.id
    bool removeSyncFolder(const std::string& folderId);
    bool pauseSync(const std::string& folderId);
    bool resumeSync(const std::string& folderId);
    bool updateSyncFolderSettings(const std::string& folderId, SyncDirection direction, const std::string& conflictResolution);
    std::vector<SyncFolder> getSyncFolders() const;

    // Sync operations & state
    void triggerSync(const std::string& folderId = "");
    void triggerBidirectionalSync(const std::string& folderId = "");  // Sprint 3 - Active
    SyncStats getSyncState() const;

    // Callbacks for status updates
    using StatusCallback = std::function<void(const SyncStats&)>;
    using FileChangeCallback = std::function<void(const FileEvent&)>;
    using ErrorCallback = std::function<void(const std::string&)>;

    void setStatusCallback(StatusCallback callback);
    void setFileChangeCallback(FileChangeCallback callback);
    void setErrorCallback(ErrorCallback callback);

    // Database access for IPC
    Database* getDatabase() const { return database_.get(); }

    // HttpClient access for IPC (to fetch server system info)
    HttpClient* getHttpClient() const { return httpClient_.get(); }

private:
    // Internal sync loop
    void syncLoop();
    void processFileEvent(const FileEvent& event);
    void scanLocalChanges(const SyncFolder& folder);
    void fetchRemoteChanges(const SyncFolder& folder);
    void uploadFile(const std::string& localPath, const std::string& remotePath);
    void downloadFile(const std::string& remotePath, const std::string& localPath);
    void handleConflict(const std::string& path);
    
    // Retry logic with exponential backoff
    template<typename Func>
    bool retryWithBackoff(const Func& operation, int maxRetries = 3, int initialDelayMs = 1000) {
        for (int attempt = 0; attempt < maxRetries; ++attempt) {
            try {
                if (operation()) {
                    if (attempt > 0) {
                        Logger::info("Retry successful on attempt {}/{}", attempt + 1, maxRetries);
                    }
                    return true;
                }
            } catch (const std::exception& e) {
                Logger::warn("Attempt {}/{} failed: {}", attempt + 1, maxRetries, e.what());
            }
            
            if (attempt < maxRetries - 1) {
                int delayMs = static_cast<int>(initialDelayMs * std::pow(2.0, static_cast<double>(attempt)));
                Logger::debug("Retrying in {}ms...", delayMs);
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            }
        }
        
        Logger::error("Operation failed after {} retries", maxRetries);
        return false;
    }
    
    // Delta-Sync implementation
    void performDeltaSync(const SyncFolder& folder);

    // Sprint 3 methods - Active
    void syncBidirectional(const SyncFolder& folder);
    void handleRemoteChange(const DetectedChange& change, const SyncFolder& folder);
    void handleLocalChange(const DetectedChange& change, const SyncFolder& folder);
    void resolveConflict(const ConflictInfo& conflict, const SyncFolder& folder);

    // Device registration helper (extracted from login)
    void registerDeviceIfNeeded();

    // Token refresh wrapper: executes operation, retries once on TokenExpiredException
    template<typename Func>
    auto withTokenRefresh(const Func& operation) -> decltype(operation()) {
        try {
            return operation();
        } catch (const TokenExpiredException&) {
            Logger::info("Token expired, attempting refresh...");
            std::string refreshToken = CredentialStore::loadToken("refresh_token");
            if (refreshToken.empty() || !httpClient_) {
                Logger::error("No refresh token available");
                authenticated_ = false;
                if (authRequiredCallback_) authRequiredCallback_();
                throw;
            }
            try {
                std::string newToken = httpClient_->refreshAccessToken(refreshToken);
                CredentialStore::saveToken("access_token", newToken);
                Logger::info("Token refreshed, retrying operation...");
                return operation();
            } catch (const std::exception& e) {
                Logger::error("Token refresh failed: {}", e.what());
                authenticated_ = false;
                if (authRequiredCallback_) authRequiredCallback_();
                throw;
            }
        }
    }

    // Internal sync folders without size calculation (hot path)
    std::vector<SyncFolder> getSyncFoldersForSync() const;

    // Update stats
    void updateStats();
    void notifyStatusChange();
    void notifyStatusChangeThrottled();

    // Components
    std::unique_ptr<FileWatcher> fileWatcher_;
    std::unique_ptr<HttpClient> httpClient_;
    std::unique_ptr<Database> database_;
    std::unique_ptr<ConflictResolver> conflictResolver_;
    std::unique_ptr<ChangeDetector> changeDetector_;

    // State
    std::atomic<bool> running_{false};
    std::atomic<bool> authenticated_{false};
    std::thread syncThread_;
    std::mutex mutex_;

    // Event queue
    std::queue<FileEvent> eventQueue_;
    std::mutex queueMutex_;

    // Callbacks
    StatusCallback statusCallback_;
    FileChangeCallback fileChangeCallback_;
    ErrorCallback errorCallback_;
    AuthRequiredCallback authRequiredCallback_;

    // Stored credentials (loaded from CredentialStore)
    std::string storedUsername_;
    std::string storedServerUrl_;

    // Stats
    SyncStats stats_;
    std::mutex statsMutex_;

    // Quota backoff: pause uploads for 10 minutes after a 507 error
    std::atomic<bool> quotaExceeded_{false};
    std::chrono::steady_clock::time_point quotaExceededAt_;

    // Transfer timing for speed calculation
    std::chrono::steady_clock::time_point transferStart_;

    // Rate-limited activity log cleanup (max once per hour)
    std::chrono::steady_clock::time_point lastLogCleanup_{};

    // Status change throttling (max every 200ms)
    std::chrono::steady_clock::time_point lastStatusNotify_;

    // Prevent concurrent sync runs (auto-loop vs manual trigger)
    std::atomic<bool> syncing_{false};

    // RAII guard that resets syncing_ flag on scope exit (even on exceptions)
    class SyncGuard {
    public:
        explicit SyncGuard(std::atomic<bool>& flag) : flag_(flag) {}
        ~SyncGuard() { flag_ = false; }
        SyncGuard(const SyncGuard&) = delete;
        SyncGuard& operator=(const SyncGuard&) = delete;
    private:
        std::atomic<bool>& flag_;
    };
};

} // namespace baludesk
