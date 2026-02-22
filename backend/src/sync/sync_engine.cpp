#include "sync/sync_engine.h"
#include "sync/file_watcher_v2.h"
#include "api/http_client.h"
#include "db/database.h"
#include "sync/conflict_resolver.h"
#include "sync/change_detector.h"
#include "utils/logger.h"
#include "utils/settings_manager.h"
#include "utils/credential_store.h"
#include "utils/sha256.h"
#include <chrono>
#include <thread>
#include <filesystem>
#include <ctime>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <unordered_set>
#include <future>
#include <map>

namespace baludesk {

SyncEngine::SyncEngine() {
    stats_.status = SyncStatus::IDLE;
    stats_.uploadSpeed = 0;
    stats_.downloadSpeed = 0;
    stats_.pendingUploads = 0;
    stats_.pendingDownloads = 0;
    stats_.totalFiles = 0;
    stats_.processedFiles = 0;
    stats_.currentFileSize = 0;
    stats_.currentFileTransferred = 0;
    stats_.currentFilePercent = 0.0;
}

SyncEngine::~SyncEngine() {
    stop();
}

bool SyncEngine::initialize(const std::string& dbPath, const std::string& serverUrl) {
    try {
        Logger::info("Initializing SyncEngine...");
        
        // Initialize database
        database_ = std::make_unique<Database>(dbPath);
        if (!database_->initialize()) {
            Logger::error("Failed to initialize database");
            return false;
        }

        // Initialize HTTP client
        httpClient_ = std::make_unique<HttpClient>(serverUrl);

        // Initialize other components
        fileWatcher_ = std::make_unique<FileWatcher>();
        conflictResolver_ = std::make_unique<ConflictResolver>(
            database_.get(), 
            httpClient_.get()
        );
        changeDetector_ = std::make_unique<ChangeDetector>(
            database_.get(),
            httpClient_.get()
        );

        // Set file watcher callback
        fileWatcher_->setCallback([this](const FileEvent& event) {
            std::lock_guard<std::mutex> lock(queueMutex_);
            eventQueue_.push(event);
        });

        Logger::info("SyncEngine initialized successfully");
        return true;
    } catch (const std::exception& e) {
        Logger::error("Failed to initialize SyncEngine: " + std::string(e.what()));
        return false;
    }
}

void SyncEngine::start() {
    if (running_) {
        Logger::warn("SyncEngine already running");
        return;
    }

    Logger::info("Starting SyncEngine...");
    running_ = true;
    stats_.status = SyncStatus::IDLE;

    // Start sync loop in separate thread
    syncThread_ = std::thread([this]() {
        syncLoop();
    });

    // Start file watchers for all enabled folders
    auto folders = getSyncFolders();
    for (const auto& folder : folders) {
        if (folder.enabled && folder.status != SyncStatus::PAUSED) {
            fileWatcher_->watch(folder.localPath);
        }
    }

    notifyStatusChange();
}

void SyncEngine::stop() {
    if (!running_) {
        return;
    }

    Logger::info("Stopping SyncEngine...");
    running_ = false;

    // Stop file watcher
    if (fileWatcher_) {
        fileWatcher_->stop();
    }

    // Wait for sync thread
    if (syncThread_.joinable()) {
        syncThread_.join();
    }

    // Wait for all managed background syncs to finish (prevents use-after-free)
    {
        std::lock_guard<std::mutex> lock(pendingSyncsMutex_);
        for (auto& fut : pendingSyncs_) {
            if (fut.valid()) {
                fut.wait();
            }
        }
        pendingSyncs_.clear();
    }

    stats_.status = SyncStatus::IDLE;
    notifyStatusChange();
}

void SyncEngine::launchManagedSync(const std::string& folderId) {
    std::lock_guard<std::mutex> lock(pendingSyncsMutex_);

    // Clean up completed futures
    pendingSyncs_.erase(
        std::remove_if(pendingSyncs_.begin(), pendingSyncs_.end(),
            [](const std::future<void>& f) {
                return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
            }),
        pendingSyncs_.end()
    );

    // Launch new managed sync
    pendingSyncs_.push_back(std::async(std::launch::async, [this, folderId]() {
        try {
            triggerSync(folderId);
        } catch (const std::exception& e) {
            Logger::error("Background sync failed for folder {}: {}", folderId, e.what());
        }
    }));
}

bool SyncEngine::isRunning() const {
    return running_;
}

bool SyncEngine::login(const std::string& username, const std::string& password, const std::string& serverUrl) {
    Logger::info("Attempting login for user: " + username);

    if (!httpClient_) {
        Logger::error("HTTP client not initialized");
        return false;
    }

    // Check if we need to create a new HttpClient with a different server URL
    std::unique_ptr<HttpClient> tempClient;
    HttpClient* clientToUse = httpClient_.get();

    if (!serverUrl.empty() && serverUrl != httpClient_->getBaseUrl()) {
        Logger::info("Creating new HTTP client with server URL: " + serverUrl);
        tempClient = std::make_unique<HttpClient>(serverUrl);
        clientToUse = tempClient.get();
    } else if (!serverUrl.empty()) {
        Logger::info("Server URL matches current client, reusing existing client");
    } else {
        Logger::info("No server URL provided, using existing HTTP client with URL: " + httpClient_->getBaseUrl());
    }

    if (clientToUse->login(username, password)) {
        authenticated_ = true;
        Logger::info("Login successful");

        // If we used a temporary client, replace the main httpClient_ with it
        if (tempClient) {
            Logger::info("Replacing HTTP client with new server URL: " + serverUrl);
            httpClient_ = std::move(tempClient);

            // Update ConflictResolver and ChangeDetector with new client
            if (conflictResolver_) {
                conflictResolver_ = std::make_unique<ConflictResolver>(
                    database_.get(),
                    httpClient_.get()
                );
            }
            if (changeDetector_) {
                changeDetector_ = std::make_unique<ChangeDetector>(
                    database_.get(),
                    httpClient_.get()
                );
            }
        }

        // Auto-register desktop device after successful login
        registerDeviceIfNeeded();

        return true;
    }

    authenticated_ = false;
    Logger::error("Login failed");
    return false;
}

void SyncEngine::logout() {
    authenticated_ = false;
    if (httpClient_) {
        httpClient_->clearAuthToken();
    }

    // Clear all stored credentials from OS keychain
    CredentialStore::deleteToken("access_token");
    CredentialStore::deleteToken("refresh_token");
    CredentialStore::deleteToken("server_url");
    CredentialStore::deleteToken("paired_username");

    storedUsername_.clear();
    storedServerUrl_.clear();

    Logger::info("Logged out and credentials cleared from keychain");
}

bool SyncEngine::setTokens(const std::string& serverUrl, const std::string& accessToken,
                           const std::string& refreshToken, const std::string& username) {
    Logger::info("Setting tokens for user: {} @ {}", username, serverUrl);

    try {
        // Create/replace HttpClient with new server URL
        httpClient_ = std::make_unique<HttpClient>(serverUrl);
        httpClient_->setAuthToken(accessToken);

        // Update ConflictResolver and ChangeDetector with new client
        if (database_) {
            if (conflictResolver_) {
                conflictResolver_ = std::make_unique<ConflictResolver>(
                    database_.get(), httpClient_.get()
                );
            }
            if (changeDetector_) {
                changeDetector_ = std::make_unique<ChangeDetector>(
                    database_.get(), httpClient_.get()
                );
            }
        }

        // Store all credentials in OS keychain
        CredentialStore::saveToken("access_token", accessToken);
        CredentialStore::saveToken("refresh_token", refreshToken);
        CredentialStore::saveToken("server_url", serverUrl);
        CredentialStore::saveToken("paired_username", username);

        storedUsername_ = username;
        storedServerUrl_ = serverUrl;
        authenticated_ = true;

        // Register device if needed
        registerDeviceIfNeeded();

        Logger::info("Tokens set successfully for user: {}", username);
        return true;

    } catch (const std::exception& e) {
        Logger::error("Failed to set tokens: {}", e.what());
        return false;
    }
}

std::string SyncEngine::checkStoredTokens() {
    Logger::info("Checking stored tokens...");

    try {
        std::string accessToken = CredentialStore::loadToken("access_token");
        std::string serverUrl = CredentialStore::loadToken("server_url");

        if (accessToken.empty() || serverUrl.empty()) {
            Logger::info("No stored tokens found, needs pairing");
            return "needs_pairing";
        }

        std::string refreshToken = CredentialStore::loadToken("refresh_token");
        std::string username = CredentialStore::loadToken("paired_username");

        // Create HttpClient and set token
        httpClient_ = std::make_unique<HttpClient>(serverUrl);
        httpClient_->setAuthToken(accessToken);

        // Update ConflictResolver and ChangeDetector with new client
        if (database_) {
            if (conflictResolver_) {
                conflictResolver_ = std::make_unique<ConflictResolver>(
                    database_.get(), httpClient_.get()
                );
            }
            if (changeDetector_) {
                changeDetector_ = std::make_unique<ChangeDetector>(
                    database_.get(), httpClient_.get()
                );
            }
        }

        storedUsername_ = username;
        storedServerUrl_ = serverUrl;

        // Try a lightweight API call to verify the token
        try {
            httpClient_->get("/api/auth/me");
            authenticated_ = true;
            Logger::info("Stored token is valid, user: {}", username);
            return "authenticated";
        } catch (const TokenExpiredException&) {
            // Token expired, try refresh
            Logger::info("Access token expired, attempting refresh...");

            if (refreshToken.empty()) {
                Logger::warn("No refresh token available, needs pairing");
                // Clear invalid credentials
                CredentialStore::deleteToken("access_token");
                CredentialStore::deleteToken("refresh_token");
                CredentialStore::deleteToken("server_url");
                CredentialStore::deleteToken("paired_username");
                return "needs_pairing";
            }

            try {
                std::string newAccessToken = httpClient_->refreshAccessToken(refreshToken);
                // Save new access token
                CredentialStore::saveToken("access_token", newAccessToken);
                authenticated_ = true;
                Logger::info("Token refreshed successfully, user: {}", username);
                return "authenticated";
            } catch (const std::exception& e) {
                Logger::error("Token refresh failed: {}", e.what());
                // Clear all credentials
                CredentialStore::deleteToken("access_token");
                CredentialStore::deleteToken("refresh_token");
                CredentialStore::deleteToken("server_url");
                CredentialStore::deleteToken("paired_username");
                return "needs_pairing";
            }
        } catch (const std::runtime_error& e) {
            // Network error (server offline) — optimistically return authenticated
            // Tokens are kept, sync will fail later when server is back
            std::string errorMsg(e.what());
            if (errorMsg.find("CURL error") != std::string::npos) {
                Logger::warn("Server unreachable, assuming tokens are still valid");
                authenticated_ = true;
                return "authenticated";
            }
            // Other HTTP error → tokens are likely invalid
            Logger::error("Token validation failed: {}", e.what());
            CredentialStore::deleteToken("access_token");
            CredentialStore::deleteToken("refresh_token");
            CredentialStore::deleteToken("server_url");
            CredentialStore::deleteToken("paired_username");
            return "needs_pairing";
        }

    } catch (const std::exception& e) {
        Logger::error("Error checking stored tokens: {}", e.what());
        return "needs_pairing";
    }
}

void SyncEngine::setAuthRequiredCallback(AuthRequiredCallback cb) {
    authRequiredCallback_ = std::move(cb);
}

std::string SyncEngine::getStoredUsername() const {
    return storedUsername_;
}

std::string SyncEngine::getStoredServerUrl() const {
    return storedServerUrl_;
}

void SyncEngine::registerDeviceIfNeeded() {
    auto& settings = SettingsManager::getInstance();

    if (!settings.isDeviceRegistered()) {
        Logger::info("Registering desktop device with backend...");

        std::string deviceId = settings.getDeviceId();
        std::string deviceName = settings.getDeviceName();

        Logger::debug("Device ID: {}, Device Name: {}", deviceId, deviceName);

        if (httpClient_ && httpClient_->registerDevice(deviceId, deviceName)) {
            settings.setDeviceRegistered(true);
            Logger::info("Desktop device registered successfully");
        } else {
            Logger::warn("Failed to register desktop device (non-fatal)");
        }
    } else {
        Logger::debug("Device already registered, skipping registration");
    }
}

bool SyncEngine::isAuthenticated() const {
    return authenticated_;
}

bool SyncEngine::addSyncFolder(SyncFolder& folder) {
    Logger::info("Adding sync folder: " + folder.localPath + " -> " + folder.remotePath);
    
    // Generate ID if not set
    if (folder.id.empty()) {
        folder.id = database_->generateId();
    }
    
    folder.status = SyncStatus::IDLE;
    folder.enabled = true;
    folder.createdAt = std::to_string(std::time(nullptr));

    if (database_->addSyncFolder(folder)) {
        // Start watching this folder
        if (running_) {
            fileWatcher_->watch(folder.localPath);
        }

        // Trigger initial sync in background (non-blocking)
        if (running_ && authenticated_) {
            launchManagedSync(folder.id);
        }

        return true;
    }

    return false;
}

bool SyncEngine::removeSyncFolder(const std::string& folderId) {
    Logger::info("Removing sync folder: " + folderId);
    
    auto folder = database_->getSyncFolder(folderId);
    if (!folder.id.empty()) {
        fileWatcher_->unwatch(folder.localPath);
        return database_->removeSyncFolder(folderId);
    }
    
    return false;
}

bool SyncEngine::pauseSync(const std::string& folderId) {
    auto folder = database_->getSyncFolder(folderId);
    if (!folder.id.empty()) {
        folder.status = SyncStatus::PAUSED;
        fileWatcher_->unwatch(folder.localPath);
        return database_->updateSyncFolder(folder);
    }
    return false;
}

bool SyncEngine::resumeSync(const std::string& folderId) {
    auto folder = database_->getSyncFolder(folderId);
    if (!folder.id.empty()) {
        folder.status = SyncStatus::IDLE;
        fileWatcher_->watch(folder.localPath);
        bool updated = database_->updateSyncFolder(folder);
        if (running_ && authenticated_) {
            launchManagedSync(folderId);
        }
        return updated;
    }
    return false;
}

bool SyncEngine::updateSyncFolderSettings(const std::string& folderId, SyncDirection direction, const std::string& conflictResolution) {
    auto folder = database_->getSyncFolder(folderId);
    if (!folder.id.empty()) {
        std::string dirStr = "bidirectional";
        if (direction == SyncDirection::PUSH) dirStr = "push";
        else if (direction == SyncDirection::PULL) dirStr = "pull";

        bool success = database_->updateSyncFolderSettings(folderId, dirStr, conflictResolution);
        if (success) {
            Logger::info("Updated settings for folder {}: direction={}, conflict={}", folderId, dirStr, conflictResolution);
        }
        return success;
    }
    return false;
}

// Helper function to calculate folder size recursively
uint64_t calculateFolderSize(const std::string& path) {
    uint64_t totalSize = 0;
    try {
        namespace fs = std::filesystem;
        for (const auto& entry : fs::recursive_directory_iterator(path)) {
            if (fs::is_regular_file(entry)) {
                totalSize += fs::file_size(entry);
            }
        }
    } catch (const std::exception& e) {
        Logger::warn("Error calculating folder size for {}: {}", path, e.what());
    }
    return totalSize;
}

std::vector<SyncFolder> SyncEngine::getSyncFolders() const {
    auto folders = database_->getSyncFolders();
    
    // Calculate size for each folder
    for (auto& folder : folders) {
        folder.size = calculateFolderSize(folder.localPath);
    }
    
    return folders;
}

void SyncEngine::triggerSync(const std::string& folderId) {
    // Prevent concurrent sync runs (auto-loop vs manual trigger)
    if (syncing_.exchange(true)) {
        Logger::info("Sync already in progress, queuing for later");
        syncPending_ = true;
        return;
    }
    // RAII guard: resets syncing_ = false on scope exit (normal or exception)
    SyncGuard guard(syncing_);

    Logger::info("Triggering sync" + (folderId.empty() ? "" : " for folder: " + folderId));

    // Periodic cleanup of old failed activity logs (max once per hour)
    auto now = std::chrono::steady_clock::now();
    if (database_ && now - lastLogCleanup_ > std::chrono::hours(1)) {
        database_->cleanupOldFailedLogs();
        lastLogCleanup_ = now;
    }

    if (!authenticated_) {
        Logger::warn("Cannot sync: not authenticated");
        return;
    }
    if (!httpClient_ || !database_ || !changeDetector_) {
        Logger::error("Cannot sync: components not initialized");
        return;
    }

    // Manual sync (specific folder) resets quota backoff
    if (!folderId.empty()) {
        quotaExceeded_ = false;
    }

    auto folders = getSyncFoldersForSync();
    for (const auto& folder : folders) {
        if (folder.enabled && folder.status != SyncStatus::PAUSED) {
            if (folderId.empty() || folder.id == folderId) {
                // Quota backoff: skip automatic syncs for 10 minutes after 507
                if (quotaExceeded_) {
                    auto elapsed = std::chrono::steady_clock::now() - quotaExceededAt_;
                    if (elapsed < std::chrono::minutes(10)) {
                        auto remaining = std::chrono::duration_cast<std::chrono::minutes>(
                            std::chrono::minutes(10) - elapsed).count();
                        Logger::info("Skipping sync for folder {} — quota exceeded, backoff ~{} min remaining",
                                     folder.id, remaining);
                        continue;
                    }
                    // Backoff period expired
                    Logger::info("Quota backoff expired, resuming sync");
                    quotaExceeded_ = false;
                }

                try {
                    withTokenRefresh([this, &folder]() {
                        performDeltaSync(folder);
                        return true;
                    });
                } catch (const std::exception& e) {
                    Logger::error("Sync failed for folder {}: {}", folder.id, e.what());
                    stats_.status = SyncStatus::SYNC_ERROR;
                    notifyStatusChange();
                }
            }
        }
    }

}

void SyncEngine::performDeltaSync(const SyncFolder& folder) {
    Logger::info("Performing delta sync for folder: {} ({})", folder.localPath, folder.remotePath);

    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.status = SyncStatus::SYNCING;
        stats_.currentFile = "";
        stats_.totalFiles = 0;
        stats_.processedFiles = 0;
        stats_.pendingDownloads = 0;
        stats_.pendingUploads = 0;
        stats_.uploadSpeed = 0;
        stats_.downloadSpeed = 0;
        stats_.currentFileSize = 0;
        stats_.currentFileTransferred = 0;
        stats_.currentFilePercent = 0.0;
    }
    notifyStatusChange();

    try {
        // 1. Detect local changes by scanning filesystem and comparing against DB
        auto localChanges = changeDetector_->detectLocalChanges(folder.id, folder.localPath);
        Logger::info("Detected {} local changes for folder {}", localChanges.size(), folder.id);

        // 2. Build file metadata list for delta-sync request
        std::vector<FileMetadataEntry> fileList;
        for (const auto& change : localChanges) {
            if (change.type != ChangeType::DELETED) {
                FileMetadataEntry entry;
                entry.path = change.path;
                entry.hash = change.hash.value_or("");
                entry.size = change.size;
                auto time = std::chrono::system_clock::to_time_t(change.timestamp);
                std::tm timeInfo;
                gmtime_s(&timeInfo, &time);
                std::stringstream ss;
                ss << std::put_time(&timeInfo, "%Y-%m-%dT%H:%M:%SZ");
                entry.modified_at = ss.str();
                fileList.push_back(entry);
            }
        }

        // 3. Send delta-sync request to server
        std::string deviceId = SettingsManager::getInstance().getDeviceId();
        std::string changeToken = database_->getChangeToken(folder.id);

        // Only include full DB file list on first sync (no changeToken).
        // With changeToken, server already knows the full state — only send changes.
        if (changeToken.empty()) {
            auto dbFiles = database_->getFilesInFolder(folder.id);
            std::unordered_set<std::string> changedPaths;
            for (const auto& change : localChanges) {
                changedPaths.insert(change.path);
            }
            for (const auto& dbFile : dbFiles) {
                if (changedPaths.find(dbFile.path) == changedPaths.end()) {
                    FileMetadataEntry entry;
                    entry.path = dbFile.path;
                    entry.hash = dbFile.checksum;
                    entry.size = dbFile.size;
                    entry.modified_at = dbFile.modifiedAt;
                    fileList.push_back(entry);
                }
            }
        }

        // Update folder status to syncing
        database_->updateSyncFolderStatus(folder.id, "syncing");

        DeltaSyncResponse delta;
        bool deltaSyncSucceeded = false;
        try {
            delta = httpClient_->performDeltaSync(deviceId, fileList, changeToken);
            deltaSyncSucceeded = true;
        } catch (const std::exception& e) {
            Logger::warn("Delta sync request failed ({}), falling back to local uploads only", e.what());
            // Continue with empty delta — uploads will still proceed
        }

        // A3: Build local uploads from change detector only (BaluHost has no toUpload field)
        std::vector<std::string> localUploads;
        for (const auto& change : localChanges) {
            if (change.type == ChangeType::CREATED || change.type == ChangeType::MODIFIED) {
                localUploads.push_back(change.path);
            }
        }

        // Calculate total work
        uint32_t totalWork = static_cast<uint32_t>(
            delta.toDownload.size() + localUploads.size() +
            delta.toDelete.size() + delta.conflicts.size()
        );

        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.totalFiles = totalWork;
            stats_.processedFiles = 0;
            stats_.pendingDownloads = static_cast<uint32_t>(delta.toDownload.size());
            stats_.pendingUploads = static_cast<uint32_t>(localUploads.size());
        }
        notifyStatusChange();

        // 4. Process downloads (server has newer versions) — skip for PUSH mode
        if (folder.direction != SyncDirection::PUSH) {

        // --- Parallel download pipeline (3 workers) ---
        if (!delta.toDownload.empty() && running_) {
            const size_t MAX_CONCURRENT_DOWNLOADS = 3;

            struct DownloadResult {
                size_t index;
                bool downloaded;
                bool tokenExpired;
            };

            std::atomic<size_t> nextDownloadIndex{0};
            std::mutex dlResultsMutex;
            std::vector<DownloadResult> dlResults;
            std::atomic<bool> dlTokenExpired{false};

            auto downloadWorker = [&]() {
                while (running_.load() && !dlTokenExpired.load()) {
                    size_t myIndex = nextDownloadIndex.fetch_add(1);
                    if (myIndex >= delta.toDownload.size()) break;

                    auto& remoteFile = delta.toDownload[myIndex];
                    std::string dlLocalPath = folder.localPath + "/" + remoteFile.path;
                    std::string dlRemotePath = folder.remotePath + "/" + remoteFile.path;

                    {
                        std::lock_guard<std::mutex> lock(statsMutex_);
                        stats_.currentFile = remoteFile.path;
                    }
                    notifyStatusChangeThrottled();

                    // Create parent directories
                    std::filesystem::path dlLocalPathObj(dlLocalPath);
                    std::filesystem::create_directories(dlLocalPathObj.parent_path());

                    DownloadResult result{myIndex, false, false};

                    for (int attempt = 0; attempt < 3; ++attempt) {
                        if (!running_.load() || dlTokenExpired.load()) break;
                        try {
                            if (httpClient_->downloadFile(dlRemotePath, dlLocalPath)) {
                                Logger::info("Downloaded: {}", remoteFile.path);
                                result.downloaded = true;
                                break;
                            }
                        } catch (const RateLimitException& rle) {
                            int waitSec = (std::min)(rle.retryAfterSeconds, 120);
                            Logger::warn("Rate limited on download, waiting {}s ({}/3): {}",
                                         waitSec, attempt + 1, remoteFile.path);
                            for (int i = 0; i < waitSec && running_.load(); ++i) {
                                std::this_thread::sleep_for(std::chrono::seconds(1));
                            }
                        } catch (const TokenExpiredException&) {
                            Logger::warn("Token expired during download");
                            result.tokenExpired = true;
                            dlTokenExpired.store(true);
                            break;
                        } catch (const std::exception& e) {
                            Logger::error("Download error (attempt {}/3): {} — {}",
                                          attempt + 1, remoteFile.path, e.what());
                            if (attempt < 2) {
                                std::this_thread::sleep_for(std::chrono::seconds(2 * (attempt + 1)));
                            }
                        }
                    }

                    {
                        std::lock_guard<std::mutex> rlock(dlResultsMutex);
                        dlResults.push_back(result);
                    }

                    {
                        std::lock_guard<std::mutex> lock(statsMutex_);
                        stats_.processedFiles++;
                        if (stats_.pendingDownloads > 0) stats_.pendingDownloads--;
                    }
                    notifyStatusChangeThrottled();
                }
            };

            // Launch download workers
            size_t dlWorkerCount = (std::min)(MAX_CONCURRENT_DOWNLOADS, delta.toDownload.size());
            std::vector<std::thread> dlWorkers;
            for (size_t w = 0; w < dlWorkerCount; ++w) {
                dlWorkers.emplace_back(downloadWorker);
            }
            for (auto& t : dlWorkers) {
                t.join();
            }

            // DB updates in batch transaction (main thread for thread safety)
            database_->beginTransaction();
            bool anyDlTokenExpired = false;
            for (const auto& result : dlResults) {
                auto& remoteFile = delta.toDownload[result.index];
                if (result.tokenExpired) {
                    anyDlTokenExpired = true;
                }
                if (result.downloaded) {
                    // Get local mtime after download
                    std::string dlLocalPath = folder.localPath + "/" + remoteFile.path;
                    std::string localMtime;
                    try {
                        auto mtime = std::filesystem::last_write_time(dlLocalPath);
                        auto mtimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                            mtime.time_since_epoch()).count();
                        localMtime = std::to_string(mtimeMs);
                    } catch (...) {}

                    database_->upsertFileMetadata(
                        remoteFile.path, folder.id, remoteFile.size,
                        remoteFile.hash, remoteFile.modifiedAt, localMtime
                    );
                    database_->logActivity("download", remoteFile.path, folder.id,
                                           "Downloaded from server", static_cast<int64_t>(remoteFile.size), "success");
                } else if (!result.tokenExpired) {
                    database_->logActivity("download", remoteFile.path, folder.id,
                                           "Download failed after 3 attempts", static_cast<int64_t>(remoteFile.size), "failed");
                }
            }
            database_->commitTransaction();

            if (anyDlTokenExpired) {
                throw TokenExpiredException();
            }
        }

        // 5. Process deletions (server deleted these files) — also skip for PUSH mode
        for (const auto& deletePath : delta.toDelete) {
            if (!running_) break;

            std::string localPath = folder.localPath + "/" + deletePath;

            {
                std::lock_guard<std::mutex> lock(statsMutex_);
                stats_.currentFile = deletePath;
            }
            notifyStatusChangeThrottled();

            try {
                if (std::filesystem::exists(localPath)) {
                    std::filesystem::remove(localPath);
                    Logger::info("Deleted local file (server deleted): {}", deletePath);
                }
                database_->deleteFileMetadata(deletePath, folder.id);
                database_->logActivity("delete", deletePath, folder.id,
                                       "Deleted (server removed)", 0, "success");
            } catch (const std::exception& e) {
                Logger::error("Failed to delete {}: {}", deletePath, e.what());
                database_->logActivity("delete", deletePath, folder.id,
                                       std::string("Delete failed: ") + e.what(), 0, "failed");
            }

            {
                std::lock_guard<std::mutex> lock(statsMutex_);
                stats_.processedFiles++;
            }
            notifyStatusChangeThrottled();
        }
        } // end direction != PUSH

        // 6. Process uploads (local changes that need to go to server) — skip for PULL mode
        if (folder.direction != SyncDirection::PULL) {
        bool quotaExceeded = false;

        // --- Batch Upload: Group small files by directory, large files individually ---
        const uint64_t BATCH_SIZE_THRESHOLD = 1024 * 1024; // 1 MB
        const size_t MAX_BATCH_FILES = 50;
        const uint64_t MAX_BATCH_SIZE_BYTES = 50ULL * 1024 * 1024; // 50 MB
        const size_t MAX_CONCURRENT = 4;
        const size_t MAX_CONCURRENT_LARGE = 2;

        struct BatchJob {
            std::string remoteDir;
            std::vector<std::string> localPaths;
            std::vector<std::string> uploadPaths;
        };

        std::vector<std::string> largeFileUploads;
        std::map<std::string, BatchJob> batchesByDir;

        // Classify uploads: handle deletions, separate small vs large files
        for (const auto& uploadPath : localUploads) {
            std::string localPath = folder.localPath + "/" + uploadPath;

            if (!std::filesystem::exists(localPath)) {
                // File was deleted locally — tell server to delete
                std::string remotePath = folder.remotePath + "/" + uploadPath;
                if (httpClient_->deleteFile(remotePath)) {
                    database_->deleteFileMetadata(uploadPath, folder.id);
                    database_->logActivity("delete", uploadPath, folder.id,
                                           "Deleted on server (local removed)", 0, "success");
                }
                {
                    std::lock_guard<std::mutex> lock(statsMutex_);
                    stats_.processedFiles++;
                    if (stats_.pendingUploads > 0) stats_.pendingUploads--;
                }
                notifyStatusChange();
                continue;
            }

            uint64_t fileSize = std::filesystem::file_size(localPath);
            if (fileSize >= BATCH_SIZE_THRESHOLD) {
                largeFileUploads.push_back(uploadPath);
            } else {
                std::string dir;
                auto lastSlash = uploadPath.find_last_of('/');
                if (lastSlash != std::string::npos) {
                    dir = uploadPath.substr(0, lastSlash);
                }
                std::string remoteDir = folder.remotePath + (dir.empty() ? "" : "/" + dir);
                auto& batch = batchesByDir[remoteDir];
                batch.remoteDir = remoteDir;
                batch.localPaths.push_back(localPath);
                batch.uploadPaths.push_back(uploadPath);
            }
        }

        // Split into chunks of max MAX_BATCH_FILES or MAX_BATCH_SIZE_BYTES
        std::vector<BatchJob> allBatches;
        for (auto& [dir, batch] : batchesByDir) {
            BatchJob currentChunk;
            currentChunk.remoteDir = batch.remoteDir;
            uint64_t currentChunkSize = 0;

            for (size_t i = 0; i < batch.localPaths.size(); ++i) {
                uint64_t fSize = 0;
                try { fSize = std::filesystem::file_size(batch.localPaths[i]); } catch (...) {}

                // Start a new chunk if adding this file would exceed limits
                if (!currentChunk.localPaths.empty() &&
                    (currentChunk.localPaths.size() >= MAX_BATCH_FILES ||
                     currentChunkSize + fSize > MAX_BATCH_SIZE_BYTES)) {
                    allBatches.push_back(std::move(currentChunk));
                    currentChunk = BatchJob{};
                    currentChunk.remoteDir = batch.remoteDir;
                    currentChunkSize = 0;
                }

                currentChunk.localPaths.push_back(batch.localPaths[i]);
                currentChunk.uploadPaths.push_back(batch.uploadPaths[i]);
                currentChunkSize += fSize;
            }

            if (!currentChunk.localPaths.empty()) {
                allBatches.push_back(std::move(currentChunk));
            }
        }

        {
            size_t smallFileCount = 0;
            for (const auto& b : allBatches) smallFileCount += b.localPaths.size();
            Logger::info("Upload plan: {} batches ({} small files), {} large files, {} workers",
                         allBatches.size(), smallFileCount, largeFileUploads.size(), MAX_CONCURRENT);
        }

        // --- Sliding-Window Pipeline: Worker threads pull from shared queue ---
        std::atomic<bool> tokenRefreshed{false};
        std::atomic<bool> pipelineQuotaExceeded{false};
        std::mutex tokenRefreshMutex;  // Only one worker refreshes at a time

        // Batch result info for post-processing
        struct BatchResult {
            size_t batchIndex;
            bool uploaded;
            bool tokenExpired;
            bool quotaHit;
        };

        std::mutex resultsMutex;
        std::vector<BatchResult> batchResults;

        // Shared queue index (atomic counter for lock-free distribution)
        std::atomic<size_t> nextBatchIndex{0};

        auto batchWorker = [&]() {
            while (running_.load() && !pipelineQuotaExceeded.load()) {
                // Grab next batch atomically
                size_t myIndex = nextBatchIndex.fetch_add(1);
                if (myIndex >= allBatches.size()) break;

                auto& batch = allBatches[myIndex];
                BatchResult result{myIndex, false, false, false};

                {
                    std::lock_guard<std::mutex> lock(statsMutex_);
                    stats_.currentFile = batch.uploadPaths[0] +
                        (batch.uploadPaths.size() > 1
                            ? " (+" + std::to_string(batch.uploadPaths.size() - 1) + " more)"
                            : "");
                }
                notifyStatusChange();

                // Try upload with retries
                for (int attempt = 0; attempt < 3; ++attempt) {
                    if (!running_.load() || pipelineQuotaExceeded.load()) break;
                    try {
                        result.uploaded = httpClient_->uploadFileBatch(
                            batch.localPaths, batch.remoteDir);
                        break;
                    } catch (const TokenExpiredException&) {
                        result.tokenExpired = true;
                        // Try to refresh token (only one worker does this)
                        std::lock_guard<std::mutex> tlock(tokenRefreshMutex);
                        if (!tokenRefreshed.load()) {
                            Logger::info("Token expired during batch upload, attempting refresh...");
                            std::string refreshToken = CredentialStore::loadToken("refresh_token");
                            if (!refreshToken.empty()) {
                                try {
                                    std::string newToken = httpClient_->refreshAccessToken(refreshToken);
                                    CredentialStore::saveToken("access_token", newToken);
                                    Logger::info("Token refreshed, continuing pipeline");
                                    tokenRefreshed.store(true);
                                    // Retry this batch with new token
                                    result.tokenExpired = false;
                                    continue;
                                } catch (const std::exception& refreshErr) {
                                    Logger::error("Token refresh failed: {}", refreshErr.what());
                                }
                            }
                        } else {
                            // Another worker already refreshed — retry
                            result.tokenExpired = false;
                            continue;
                        }
                        break;
                    } catch (const RateLimitException& rle) {
                        int waitSec = (std::min)(rle.retryAfterSeconds, 120);
                        Logger::warn("Batch rate limited, waiting {}s ({}/3)",
                                     waitSec, attempt + 1);
                        for (int s = 0; s < waitSec && running_.load(); ++s) {
                            std::this_thread::sleep_for(std::chrono::seconds(1));
                        }
                    } catch (const QuotaExceededException&) {
                        result.quotaHit = true;
                        pipelineQuotaExceeded.store(true);
                        break;
                    }
                }

                // Store result
                {
                    std::lock_guard<std::mutex> rlock(resultsMutex);
                    batchResults.push_back(result);
                }
            }
        };

        // Launch worker threads
        auto pipelineStart = std::chrono::steady_clock::now();
        uint64_t pipelineTotalSize = 0;
        for (const auto& b : allBatches) {
            for (const auto& p : b.localPaths) {
                try { pipelineTotalSize += std::filesystem::file_size(p); } catch (...) {}
            }
        }

        {
            size_t workerCount = (std::min)(MAX_CONCURRENT, allBatches.size());
            std::vector<std::thread> workers;
            for (size_t w = 0; w < workerCount; ++w) {
                workers.emplace_back(batchWorker);
            }
            for (auto& t : workers) {
                t.join();
            }
        }

        // Process batch results (DB updates on main thread for thread safety)
        // Wrap all DB writes in a single transaction for performance
        database_->beginTransaction();
        bool anyTokenExpiredUnrecoverable = false;
        for (const auto& result : batchResults) {
            auto& batch = allBatches[result.batchIndex];

            if (result.quotaHit && !quotaExceeded) {
                Logger::error("Storage quota exceeded during batch upload — aborting remaining uploads");
                database_->logActivity("upload", batch.uploadPaths[0], folder.id,
                                       "Storage quota exceeded", 0, "failed");
                quotaExceeded = true;
                quotaExceeded_ = true;
                quotaExceededAt_ = std::chrono::steady_clock::now();
                if (errorCallback_) {
                    errorCallback_("Storage quota exceeded. Uploads paused for 10 minutes. "
                                   "Please free up space on the server or increase your quota.");
                }
            }

            if (result.tokenExpired && !tokenRefreshed.load()) {
                anyTokenExpiredUnrecoverable = true;
            }

            if (result.uploaded) {
                for (size_t k = 0; k < batch.uploadPaths.size(); ++k) {
                    std::string hash;
                    for (const auto& change : localChanges) {
                        if (change.path == batch.uploadPaths[k]) {
                            hash = change.hash.value_or("");
                            break;
                        }
                    }
                    if (hash.empty()) {
                        hash = sha256_file(batch.localPaths[k]);
                    }
                    uint64_t fSize = std::filesystem::file_size(batch.localPaths[k]);
                    auto now = std::chrono::system_clock::to_time_t(
                        std::chrono::system_clock::now());
                    std::tm timeInfo;
                    gmtime_s(&timeInfo, &now);
                    std::stringstream ss;
                    ss << std::put_time(&timeInfo, "%Y-%m-%dT%H:%M:%SZ");

                    // Get local mtime for mtime-based change detection
                    std::string localMtime;
                    try {
                        auto mtime = std::filesystem::last_write_time(batch.localPaths[k]);
                        auto mtimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                            mtime.time_since_epoch()).count();
                        localMtime = std::to_string(mtimeMs);
                    } catch (...) {}

                    database_->upsertFileMetadata(
                        batch.uploadPaths[k], folder.id, fSize, hash, ss.str(), localMtime);
                    database_->logActivity("upload", batch.uploadPaths[k], folder.id,
                                           "Uploaded to server (batch)",
                                           static_cast<int64_t>(fSize), "success");
                    Logger::info("Uploaded (batch): {}", batch.uploadPaths[k]);
                }
            } else if (!quotaExceeded && !result.tokenExpired) {
                for (size_t k = 0; k < batch.uploadPaths.size(); ++k) {
                    uint64_t fSize = std::filesystem::exists(batch.localPaths[k])
                        ? std::filesystem::file_size(batch.localPaths[k]) : 0;
                    database_->logActivity("upload", batch.uploadPaths[k], folder.id,
                                           "Batch upload failed",
                                           static_cast<int64_t>(fSize), "failed");
                    Logger::error("Batch upload failed: {}", batch.uploadPaths[k]);
                }
            }

            {
                std::lock_guard<std::mutex> lock(statsMutex_);
                uint32_t batchSize = static_cast<uint32_t>(batch.uploadPaths.size());
                stats_.processedFiles += batchSize;
                stats_.pendingUploads = (stats_.pendingUploads >= batchSize)
                    ? stats_.pendingUploads - batchSize : 0;
            }
            notifyStatusChangeThrottled();
        }
        database_->commitTransaction();

        if (anyTokenExpiredUnrecoverable) {
            authenticated_ = false;
            if (authRequiredCallback_) authRequiredCallback_();
            throw TokenExpiredException();
        }

        // Estimate upload speed from pipeline duration
        auto pipelineEnd = std::chrono::steady_clock::now();
        auto pipelineMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            pipelineEnd - pipelineStart).count();
        if (pipelineMs > 0 && pipelineTotalSize > 0) {
            {
                std::lock_guard<std::mutex> lock(statsMutex_);
                stats_.uploadSpeed = (pipelineTotalSize * 1000) / static_cast<uint64_t>(pipelineMs);
            }
            notifyStatusChange();
        }

        // --- Large files: Sliding-window with MAX_CONCURRENT_LARGE workers ---
        std::atomic<size_t> nextLargeIndex{0};

        auto largeFileWorker = [&]() {
            while (running_.load() && !pipelineQuotaExceeded.load()) {
                size_t myIndex = nextLargeIndex.fetch_add(1);
                if (myIndex >= largeFileUploads.size()) break;

                const auto& uploadPath = largeFileUploads[myIndex];
                std::string localPath = folder.localPath + "/" + uploadPath;
                std::string remotePath = folder.remotePath + "/" + uploadPath;

                uint64_t fileSize = std::filesystem::file_size(localPath);

                {
                    std::lock_guard<std::mutex> lock(statsMutex_);
                    stats_.currentFile = uploadPath;
                    stats_.currentFileSize = fileSize;
                    stats_.currentFileTransferred = 0;
                    stats_.currentFilePercent = 0.0;
                }
                auto workerTransferStart = std::chrono::steady_clock::now();
                notifyStatusChange();

                // Progress callback for per-file tracking
                auto progressCb = [this, workerTransferStart, lastNotify = std::chrono::steady_clock::time_point{}]
                                   (const TransferProgress& p) mutable {
                    {
                        std::lock_guard<std::mutex> lock(statsMutex_);
                        stats_.currentFileSize = p.totalBytes;
                        stats_.currentFileTransferred = p.bytesTransferred;
                        stats_.currentFilePercent = p.percentage;
                        auto elapsed = std::chrono::steady_clock::now() - workerTransferStart;
                        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
                        if (ms > 0) {
                            stats_.uploadSpeed = (p.bytesTransferred * 1000) / static_cast<uint64_t>(ms);
                        }
                    }
                    auto now = std::chrono::steady_clock::now();
                    if (now - lastNotify >= std::chrono::milliseconds(250)) {
                        lastNotify = now;
                        notifyStatusChange();
                    }
                };

                bool uploaded = false;
                bool quotaHit = false;
                for (int attempt = 0; attempt < 3; ++attempt) {
                    if (!running_.load() || pipelineQuotaExceeded.load()) break;
                    try {
                        if (httpClient_->uploadFile(localPath, remotePath, progressCb)) {
                            uploaded = true;
                            break;
                        } else {
                            break; // Non-exception failure
                        }
                    } catch (const TokenExpiredException&) {
                        Logger::warn("Token expired during large file upload, propagating for refresh");
                        throw;  // Let withTokenRefresh handle it
                    } catch (const RateLimitException& rle) {
                        int waitSec = (std::min)(rle.retryAfterSeconds, 120);
                        Logger::warn("Rate limited on upload, waiting {}s before retry ({}/3): {}",
                                     waitSec, attempt + 1, uploadPath);
                        for (int s = 0; s < waitSec && running_.load(); ++s) {
                            std::this_thread::sleep_for(std::chrono::seconds(1));
                        }
                    } catch (const QuotaExceededException&) {
                        size_t remaining = largeFileUploads.size() - myIndex - 1;
                        Logger::error("Storage quota exceeded — aborting {} remaining uploads", remaining);
                        quotaHit = true;
                        pipelineQuotaExceeded.store(true);
                        break;
                    }
                }

                // DB updates for large files (safe: each worker handles distinct files)
                if (uploaded) {
                    std::string hash;
                    for (const auto& change : localChanges) {
                        if (change.path == uploadPath) {
                            hash = change.hash.value_or("");
                            break;
                        }
                    }
                    if (hash.empty()) {
                        hash = sha256_file(localPath);
                    }
                    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                    std::tm timeInfo;
                    gmtime_s(&timeInfo, &now);
                    std::stringstream ss;
                    ss << std::put_time(&timeInfo, "%Y-%m-%dT%H:%M:%SZ");

                    // Get local mtime for mtime-based change detection
                    std::string localMtime;
                    try {
                        auto mtime = std::filesystem::last_write_time(localPath);
                        auto mtimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                            mtime.time_since_epoch()).count();
                        localMtime = std::to_string(mtimeMs);
                    } catch (...) {}

                    database_->beginTransaction();
                    bool dbOk = database_->upsertFileMetadata(uploadPath, folder.id, fileSize, hash, ss.str(), localMtime);
                    dbOk = dbOk && database_->logActivity("upload", uploadPath, folder.id,
                                           "Uploaded to server", static_cast<int64_t>(fileSize), "success");
                    if (dbOk) {
                        database_->commitTransaction();
                    } else {
                        database_->rollbackTransaction();
                        Logger::error("DB update failed after upload: {}", uploadPath);
                    }
                    Logger::info("Uploaded: {}", uploadPath);
                } else if (quotaHit) {
                    database_->logActivity("upload", uploadPath, folder.id,
                                           "Storage quota exceeded", static_cast<int64_t>(fileSize), "failed");
                    if (!quotaExceeded) {
                        quotaExceeded = true;
                        quotaExceeded_ = true;
                        quotaExceededAt_ = std::chrono::steady_clock::now();
                        if (errorCallback_) {
                            errorCallback_("Storage quota exceeded. Uploads paused for 10 minutes. "
                                           "Please free up space on the server or increase your quota.");
                        }
                    }
                } else {
                    database_->logActivity("upload", uploadPath, folder.id,
                                           "Upload failed", static_cast<int64_t>(fileSize), "failed");
                    Logger::error("Upload failed: {}", uploadPath);
                }

                {
                    std::lock_guard<std::mutex> lock(statsMutex_);
                    stats_.processedFiles++;
                    if (stats_.pendingUploads > 0) stats_.pendingUploads--;
                    stats_.currentFileSize = 0;
                    stats_.currentFileTransferred = 0;
                    stats_.currentFilePercent = 0.0;
                }
                notifyStatusChange();
            }
        };

        if (!largeFileUploads.empty() && !quotaExceeded && running_) {
            size_t largeWorkerCount = (std::min)(MAX_CONCURRENT_LARGE, largeFileUploads.size());
            std::vector<std::thread> largeWorkers;
            for (size_t w = 0; w < largeWorkerCount; ++w) {
                largeWorkers.emplace_back(largeFileWorker);
            }
            for (auto& t : largeWorkers) {
                t.join();
            }
        }

        if (quotaExceeded) {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.pendingUploads = 0;
        }
        } // end direction != PULL

        // 7. Handle conflicts based on direction (A2: uses DeltaConflict with BaluHost fields)
        for (const auto& conflict : delta.conflicts) {
            if (folder.direction == SyncDirection::PUSH) {
                // Push mode: local wins — upload our version
                std::string localPath = folder.localPath + "/" + conflict.path;
                std::string remotePath = folder.remotePath + "/" + conflict.path;
                if (std::filesystem::exists(localPath)) {
                    httpClient_->uploadFile(localPath, remotePath);
                    Logger::info("Conflict resolved (push, local wins): {}", conflict.path);
                    database_->logActivity("conflict", conflict.path, folder.id,
                                           "Auto-resolved: local wins (push mode)", 0, "success");
                }
            } else if (folder.direction == SyncDirection::PULL) {
                // Pull mode: remote wins — download server version
                std::string localPath = folder.localPath + "/" + conflict.path;
                std::string remotePath = folder.remotePath + "/" + conflict.path;
                std::filesystem::path localPathObj(localPath);
                std::filesystem::create_directories(localPathObj.parent_path());
                if (httpClient_->downloadFile(remotePath, localPath)) {
                    database_->upsertFileMetadata(conflict.path, folder.id, 0,
                                                  conflict.serverHash, conflict.serverModifiedAt);
                    Logger::info("Conflict resolved (pull, remote wins): {}", conflict.path);
                    database_->logActivity("conflict", conflict.path, folder.id,
                                           "Auto-resolved: remote wins (pull mode)", 0, "success");
                }
            } else {
                // Bidirectional: log conflict for resolution per policy
                Conflict dbConflict;
                dbConflict.id = database_->generateId();
                dbConflict.path = conflict.path;
                dbConflict.folderId = folder.id;
                dbConflict.localModified = "";
                dbConflict.remoteModified = conflict.serverModifiedAt;
                dbConflict.resolution = "pending";
                database_->logConflict(dbConflict);

                database_->logActivity("conflict", conflict.path, folder.id,
                                       "Conflict detected (client_hash=" + conflict.clientHash +
                                       ", server_hash=" + conflict.serverHash + ")", 0, "pending");
                Logger::warn("Conflict: {} (client_hash={}, server_hash={})",
                             conflict.path, conflict.clientHash, conflict.serverHash);
            }

            {
                std::lock_guard<std::mutex> lock(statsMutex_);
                stats_.processedFiles++;
            }
            notifyStatusChange();
        }

        // 8. Save change token for next sync
        if (deltaSyncSucceeded && !delta.changeToken.empty()) {
            database_->setChangeToken(folder.id, delta.changeToken);
        }

        // 9. Update last sync timestamp and folder status
        database_->updateSyncFolderTimestamp(folder.id);
        database_->updateSyncFolderStatus(folder.id, "idle");

        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.status = SyncStatus::IDLE;
            stats_.currentFile = "";
            stats_.lastSync = std::to_string(std::time(nullptr));
            stats_.currentFileSize = 0;
            stats_.currentFileTransferred = 0;
            stats_.currentFilePercent = 0.0;
            stats_.uploadSpeed = 0;
        }
        notifyStatusChange();

        Logger::info("Delta sync completed for folder: {}", folder.id);

    } catch (const std::exception& e) {
        Logger::error("Delta sync failed for folder {}: {}", folder.id, e.what());
        database_->updateSyncFolderStatus(folder.id, "error");
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.status = SyncStatus::SYNC_ERROR;
            stats_.currentFile = "";
            stats_.totalFiles = 0;
            stats_.processedFiles = 0;
            stats_.pendingDownloads = 0;
            stats_.pendingUploads = 0;
            stats_.uploadSpeed = 0;
            stats_.downloadSpeed = 0;
            stats_.currentFileSize = 0;
            stats_.currentFileTransferred = 0;
            stats_.currentFilePercent = 0.0;
        }
        notifyStatusChange();
        throw;
    }
}

SyncStats SyncEngine::getSyncState() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

void SyncEngine::setStatusCallback(StatusCallback callback) {
    statusCallback_ = callback;
}

void SyncEngine::setFileChangeCallback(FileChangeCallback callback) {
    fileChangeCallback_ = callback;
}

void SyncEngine::setErrorCallback(ErrorCallback callback) {
    errorCallback_ = callback;
}

void SyncEngine::syncLoop() {
    Logger::info("Sync loop started");

    while (running_) {
        try {
            // Process file events from queue
            {
                std::lock_guard<std::mutex> lock(queueMutex_);
                while (!eventQueue_.empty()) {
                    auto event = eventQueue_.front();
                    eventQueue_.pop();
                    processFileEvent(event);
                }
            }

            // Periodic delta-sync for all active folders
            if (authenticated_) {
                triggerSync();
            }

            updateStats();

        } catch (const std::exception& e) {
            Logger::error("Error in sync loop: " + std::string(e.what()));
            if (errorCallback_) {
                errorCallback_(e.what());
            }
        }

        // Interruptible sleep: 30 x 1s instead of 1 x 30s
        // Break early if a sync was queued while we were running
        for (int i = 0; i < 30 && running_; ++i) {
            if (syncPending_.load(std::memory_order_relaxed)) {
                syncPending_ = false;
                Logger::info("Processing queued sync request (early wakeup)");
                break;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    Logger::info("Sync loop stopped");
}

void SyncEngine::processFileEvent(const FileEvent& event) {
    Logger::debug("Processing file event: " + event.path);

    // Find which folder this belongs to
    auto folders = getSyncFoldersForSync();
    for (const auto& folder : folders) {
        if (event.path.find(folder.localPath) == 0) {
            // File belongs to this sync folder
            // Skip upload for PULL-only folders
            if (folder.direction == SyncDirection::PULL) {
                Logger::debug("Skipping upload for PULL folder: {}", folder.id);
                break;
            }

            std::string relativePath = event.path.substr(folder.localPath.length());
            std::string remotePath = folder.remotePath + relativePath;

            switch (event.action) {
                case FileAction::CREATED:
                case FileAction::MODIFIED:
                    uploadFile(event.path, remotePath);
                    break;
                case FileAction::DELETED:
                    // Handle deletion
                    break;
            }

            if (fileChangeCallback_) {
                fileChangeCallback_(event);
            }
            break;
        }
    }
}

void SyncEngine::scanLocalChanges(const SyncFolder& folder) {
    Logger::info("Scanning local changes for: " + folder.localPath);
    
    if (!changeDetector_) {
        Logger::error("ChangeDetector not initialized");
        return;
    }
    
    try {
        // Get current timestamp
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_s(&tm, &now_time_t);
        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &tm);
        std::string currentTimestamp(buffer);
        
        // Detect local changes
        auto localChanges = changeDetector_->detectLocalChanges(folder.id, folder.localPath);
        Logger::debug("Found {} local changes", localChanges.size());
        
        // Update metadata and queue uploads
        for (const auto& change : localChanges) {
            std::string fullPath = folder.localPath + "/" + change.path;
            FileMetadata metadata;
            metadata.path = fullPath;
            metadata.folderId = folder.id;
            metadata.size = change.size;
            metadata.isDirectory = false;
            metadata.modifiedAt = std::to_string(std::chrono::system_clock::to_time_t(change.timestamp));
            metadata.syncStatus = "pending_upload";
            if (change.hash.has_value()) metadata.checksum = change.hash.value();
            database_->upsertFileMetadata(metadata);
            
            if (change.type != ChangeType::DELETED) {
                std::lock_guard<std::mutex> lock(queueMutex_);
                FileEvent event;
                event.path = fullPath;
                event.action = (change.type == ChangeType::CREATED) ? FileAction::CREATED : FileAction::MODIFIED;
                event.size = change.size;
                event.timestamp = currentTimestamp;
                eventQueue_.push(event);
            }
        }
    } catch (const std::exception& e) {
        Logger::error("Error scanning local changes: {}", e.what());
    }
}

void SyncEngine::fetchRemoteChanges(const SyncFolder& folder) {
    Logger::info("Fetching remote changes for: " + folder.remotePath);
    
    if (!authenticated_) {
        Logger::warn("Cannot fetch remote changes: not authenticated");
        return;
    }
    if (!httpClient_) {
        Logger::error("HTTP client not initialized");
        return;
    }
    
    try {
        auto lastSyncFolder = database_->getSyncFolder(folder.id);
        std::string lastSyncTimestamp = lastSyncFolder.lastSync.empty() ? "1970-01-01T00:00:00" : lastSyncFolder.lastSync;
        auto remoteChanges = httpClient_->getChangesSince(lastSyncTimestamp);
        Logger::debug("Found {} remote changes", remoteChanges.size());
        
        for (const auto& remoteChange : remoteChanges) {
            if (remoteChange.path.find(folder.remotePath) != 0) continue;
            std::string relativePath = remoteChange.path.substr(folder.remotePath.length());
            std::string localPath = folder.localPath + relativePath;
            Logger::debug("Remote change detected: {} ({})", localPath, remoteChange.action);
            
            if (remoteChange.action == "deleted") {
                // B1: Use folder-scoped delete to prevent cross-folder collisions
                database_->deleteFileMetadata(localPath, folder.id);
            } else if (remoteChange.action == "created" || remoteChange.action == "modified") {
                FileMetadata metadata;
                metadata.path = localPath;
                metadata.folderId = folder.id;
                metadata.syncStatus = "pending_download";
                database_->upsertFileMetadata(metadata);
                
                std::lock_guard<std::mutex> lock(queueMutex_);
                FileEvent event;
                event.path = localPath;
                event.action = (remoteChange.action == "created") ? FileAction::CREATED : FileAction::MODIFIED;
                event.timestamp = remoteChange.timestamp;
                eventQueue_.push(event);
            }
        }
        database_->updateSyncFolderTimestamp(folder.id);
    } catch (const std::exception& e) {
        Logger::error("Error fetching remote changes: {}", e.what());
    }
}

void SyncEngine::uploadFile(const std::string& localPath, const std::string& remotePath) {
    Logger::info("Uploading: " + localPath + " -> " + remotePath);
    
    if (!authenticated_) {
        Logger::error("Not authenticated");
        return;
    }

    try {
        stats_.status = SyncStatus::SYNCING;
        notifyStatusChange();
        
        if (httpClient_->uploadFile(localPath, remotePath)) {
            Logger::info("Upload successful: " + localPath);
        } else {
            Logger::error("Upload failed: " + localPath);
        }
        
        stats_.status = SyncStatus::IDLE;
        notifyStatusChange();
    } catch (const std::exception& e) {
        std::string errorMsg = "Upload error: ";
        errorMsg += e.what();
        Logger::error(errorMsg);
        stats_.status = SyncStatus::SYNC_ERROR;
        notifyStatusChange();
    }
}

void SyncEngine::downloadFile(const std::string& remotePath, const std::string& localPath) {
    Logger::info("Downloading: {} -> {}", remotePath, localPath);
    
    if (!authenticated_) {
        Logger::error("Cannot download: not authenticated");
        return;
    }
    if (!httpClient_) {
        Logger::error("HTTP client not initialized");
        return;
    }
    
    try {
        std::filesystem::path localPathObj(localPath);
        std::filesystem::create_directories(localPathObj.parent_path());
        
        stats_.status = SyncStatus::SYNCING;
        notifyStatusChange();
        
        // Use retry logic for download with exponential backoff
        bool success = retryWithBackoff([this, &remotePath, &localPath]() {
            return httpClient_->downloadFileWithProgress(
                remotePath, localPath,
                [this](const DownloadProgress& progress) {
                    std::lock_guard<std::mutex> lock(statsMutex_);
                    stats_.downloadSpeed = static_cast<uint64_t>(progress.bytesDownloaded);
                }
            );
        }, 3, 1000);  // 3 retries, starting at 1000ms
        
        if (success) {
            Logger::info("Download successful: {}", localPath);
            if (auto metadata = database_->getFileMetadata(localPath)) {
                FileMetadata updated = metadata.value();
                updated.syncStatus = "synced";
                database_->upsertFileMetadata(updated);
            }
            if (stats_.pendingDownloads > 0) {
                stats_.pendingDownloads--;
            }
        } else {
            Logger::error("Download failed after retries: {}", remotePath);
            stats_.status = SyncStatus::SYNC_ERROR;
        }
        stats_.status = SyncStatus::IDLE;
        notifyStatusChange();
    } catch (const std::exception& e) {
        Logger::error("Download error: {}", e.what());
        stats_.status = SyncStatus::SYNC_ERROR;
        notifyStatusChange();
    }
}

void SyncEngine::handleConflict(const std::string& path) {
    Logger::warn("Conflict detected: {}", path);
    
    if (!database_) {
        Logger::error("Database not initialized");
        return;
    }
    if (!conflictResolver_) {
        Logger::error("ConflictResolver not initialized");
        return;
    }
    
    try {
        auto localMetadata = database_->getFileMetadata(path);
        if (!localMetadata) {
            Logger::warn("File metadata not found for conflict: {}", path);
            return;
        }
        
        auto localTime = std::chrono::system_clock::from_time_t(std::stoll(localMetadata->modifiedAt));
        Conflict conflict;
        conflict.id = database_->generateId();
        conflict.path = path;
        conflict.folderId = localMetadata->folderId;
        conflict.localModified = localMetadata->modifiedAt;
        conflict.remoteModified = std::to_string(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
        conflict.resolution = "pending";
        database_->logConflict(conflict);
        
        auto resolution = conflictResolver_->resolveAuto(path, path, localTime, std::chrono::system_clock::now());
        if (resolution.success) {
            Logger::info("Conflict resolved automatically: {} ({})", path, resolution.action);
            database_->resolveConflict(conflict.id, resolution.action);
            if (resolution.finalPath != path) {
                if (auto metadata = database_->getFileMetadata(path)) {
                    FileMetadata updated = metadata.value();
                    updated.path = resolution.finalPath;
                    database_->upsertFileMetadata(updated);
                    // B1: Use folder-scoped delete via conflict's folderId
                    database_->deleteFileMetadata(path, conflict.folderId);
                }
            }
        } else {
            Logger::warn("Could not resolve conflict automatically: {}", path);
            if (errorCallback_) errorCallback_("Conflict at: " + path + " - Manual resolution needed");
        }
    } catch (const std::exception& e) {
        Logger::error("Error handling conflict: {}", e.what());
    }
}

// Sprint 3 methods - now active
void SyncEngine::triggerBidirectionalSync(const std::string& folderId) {
    if (syncing_.exchange(true)) {
        Logger::info("Bidirectional sync already in progress, skipping");
        return;
    }
    // RAII guard: resets syncing_ = false on scope exit (normal or exception)
    SyncGuard guard(syncing_);

    Logger::info("Triggering bidirectional sync for folder: " +
                (folderId.empty() ? "all" : folderId));

    if (!authenticated_) {
        Logger::warn("Cannot sync: not authenticated");
        return;
    }

    auto folders = getSyncFoldersForSync();

    for (const auto& folder : folders) {
        if (folder.enabled && folder.status != SyncStatus::PAUSED) {
            if (folderId.empty() || folder.id == folderId) {
                syncBidirectional(folder);
            }
        }
    }
}

void SyncEngine::syncBidirectional(const SyncFolder& folder) {
    Logger::info("Starting bidirectional sync for: " + folder.localPath);
    
    try {
        stats_.status = SyncStatus::SYNCING;
        notifyStatusChange();
        
        // 1. Detect local changes
        auto localChanges = changeDetector_->detectLocalChanges(
            folder.id,
            folder.localPath
        );
        Logger::info("Detected " + std::to_string(localChanges.size()) + " local changes");
        
        // 2. Detect remote changes
        auto lastSync = std::chrono::system_clock::now() - std::chrono::hours(24); // Last 24h
        auto remoteChanges = changeDetector_->detectRemoteChanges(
            folder.id,
            lastSync
        );
        Logger::info("Detected " + std::to_string(remoteChanges.size()) + " remote changes");
        
        // 3. Detect conflicts
        auto conflicts = changeDetector_->detectConflicts(localChanges, remoteChanges);
        Logger::info("Detected " + std::to_string(conflicts.size()) + " conflicts");
        
        // 4. Handle conflicts first
        for (const auto& conflict : conflicts) {
            resolveConflict(conflict, folder);
        }
        
        // 5. Process non-conflicting remote changes (downloads)
        for (const auto& change : remoteChanges) {
            // Skip if in conflict list
            bool isConflict = false;
            for (const auto& conflict : conflicts) {
                if (conflict.path == change.path) {
                    isConflict = true;
                    break;
                }
            }
            
            if (!isConflict) {
                handleRemoteChange(change, folder);
            }
        }
        
        // 6. Process non-conflicting local changes (uploads)
        for (const auto& change : localChanges) {
            // Skip if in conflict list
            bool isConflict = false;
            for (const auto& conflict : conflicts) {
                if (conflict.path == change.path) {
                    isConflict = true;
                    break;
                }
            }
            
            if (!isConflict) {
                handleLocalChange(change, folder);
            }
        }
        
        // 7. Update last sync timestamp
        database_->updateSyncFolderTimestamp(folder.id);
        
        stats_.status = SyncStatus::IDLE;
        notifyStatusChange();
        
        Logger::info("Bidirectional sync completed for: " + folder.localPath);
        
    } catch (const std::exception& e) {
        Logger::error("Bidirectional sync failed: " + std::string(e.what()));
        stats_.status = SyncStatus::SYNC_ERROR;
        notifyStatusChange();
    }
}

void SyncEngine::handleRemoteChange(const DetectedChange& change, const SyncFolder& folder) {
    std::string localPath = folder.localPath + "/" + change.path;
    std::string remotePath = folder.remotePath + "/" + change.path;
    
    switch (change.type) {
        case ChangeType::CREATED:
        case ChangeType::MODIFIED:
            Logger::info("Downloading remote change: " + change.path);
            if (httpClient_->downloadFile(remotePath, localPath)) {
                // Convert timestamp to ISO8601 string
                auto time = std::chrono::system_clock::to_time_t(change.timestamp);
                std::tm timeInfo;
                gmtime_s(&timeInfo, &time);
                std::stringstream ss;
                ss << std::put_time(&timeInfo, "%Y-%m-%dT%H:%M:%SZ");
                
                database_->upsertFileMetadata(
                    change.path,
                    folder.id,
                    change.size,
                    change.hash.value_or(""),
                    ss.str()
                );
                stats_.pendingDownloads++;
                notifyStatusChange();
            }
            break;
            
        case ChangeType::DELETED:
            Logger::info("Deleting local file (remote deleted): " + change.path);
            std::filesystem::remove(localPath);
            database_->deleteFileMetadata(change.path, folder.id);
            break;
    }
}

void SyncEngine::handleLocalChange(const DetectedChange& change, const SyncFolder& folder) {
    std::string localPath = folder.localPath + "/" + change.path;
    std::string remotePath = folder.remotePath + "/" + change.path;
    
    switch (change.type) {
        case ChangeType::CREATED:
        case ChangeType::MODIFIED:
            Logger::info("Uploading local change: " + change.path);
            // Use retry logic for upload with exponential backoff
            if (retryWithBackoff([this, &localPath, &remotePath]() {
                return httpClient_->uploadFile(localPath, remotePath);
            }, 3, 1000)) {  // 3 retries, starting at 1000ms
                // Convert timestamp to ISO8601 string
                auto time = std::chrono::system_clock::to_time_t(change.timestamp);
                std::tm timeInfo;
                gmtime_s(&timeInfo, &time);
                std::stringstream ss;
                ss << std::put_time(&timeInfo, "%Y-%m-%dT%H:%M:%SZ");
                
                database_->upsertFileMetadata(
                    change.path,
                    folder.id,
                    change.size,
                    change.hash.value_or(""),
                    ss.str()
                );
                stats_.pendingUploads++;
                notifyStatusChange();
            } else {
                Logger::error("Upload failed after retries: {}", change.path);
            }
            break;
            
        case ChangeType::DELETED:
            Logger::info("Deleting remote file (local deleted): " + change.path);
            // Use retry logic for delete with exponential backoff
            if (retryWithBackoff([this, &remotePath]() {
                return httpClient_->deleteFile(remotePath);
            }, 3, 1000)) {
                database_->deleteFileMetadata(change.path, folder.id);
            } else {
                Logger::error("Delete failed after retries: {}", change.path);
            }
            break;
    }
}

void SyncEngine::resolveConflict(const ConflictInfo& conflict, const SyncFolder& folder) {
    Logger::warn("Resolving conflict for: " + conflict.path);
    
    std::string localPath = folder.localPath + "/" + conflict.path;
    std::string remotePath = folder.remotePath + "/" + conflict.path;
    
    // Use ConflictResolver with default strategy (Last-Write-Wins)
    auto result = conflictResolver_->resolveAuto(
        localPath,
        remotePath,
        conflict.localTimestamp,
        conflict.remoteTimestamp
    );
    
    if (result.success) {
        Logger::info("Conflict resolved: " + conflict.path + " -> " + result.action);
    } else {
        Logger::error("Conflict resolution failed: " + result.errorMessage);
    }
}

void SyncEngine::updateStats() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    
    // Update statistics
    // TODO: Calculate real values
    stats_.lastSync = std::to_string(std::time(nullptr));
}

void SyncEngine::notifyStatusChange() {
    if (statusCallback_) {
        statusCallback_(getSyncState());
    }
}

void SyncEngine::notifyStatusChangeThrottled() {
    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    int64_t lastMs = lastStatusNotifyMs_.load(std::memory_order_relaxed);
    if (nowMs - lastMs >= 200) {
        if (lastStatusNotifyMs_.compare_exchange_weak(lastMs, nowMs,
                std::memory_order_relaxed, std::memory_order_relaxed)) {
            notifyStatusChange();
        }
    }
}

std::vector<SyncFolder> SyncEngine::getSyncFoldersForSync() const {
    return database_->getSyncFolders();
}

} // namespace baludesk
