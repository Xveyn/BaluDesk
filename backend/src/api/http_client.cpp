#include "http_client.h"
#include "../utils/logger.h"
#include "../utils/raid_info.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Null-safe replacement for json::value() — handles JSON null values gracefully.
// nlohmann's .value("key", default) throws when the key exists but is null.
template<typename T>
T safe_value(const nlohmann::json& j, const std::string& key, const T& default_val) {
    if (j.contains(key) && !j[key].is_null()) {
        return j[key].get<T>();
    }
    return default_val;
}

namespace baludesk {

// ============================================================================
// Helper Structures
// ============================================================================

struct WriteCallbackData {
    std::string* buffer;
};

struct ReadCallbackData {
    std::ifstream* file;
};

// ============================================================================
// HttpClient Implementation
// ============================================================================

HttpClient::HttpClient(const std::string& baseUrl)
    : baseUrl_(baseUrl), curl_(nullptr), connectTimeout_(30),
      lowSpeedLimit_(1024), lowSpeedTime_(60), verbose_(false) {
    // B2: curl_global_init moved to main.cpp (must be called once per process)
    curl_ = curl_easy_init();
    
    if (!curl_) {
        Logger::critical("Failed to initialize libcurl");
        throw std::runtime_error("Failed to initialize libcurl");
    }
    // Ensure local requests bypass any system proxy settings (important on Windows)
    // so that calls to 127.0.0.1 or localhost do not try to use an external proxy.
    try {
        const char* noProxy = "127.0.0.1;localhost";
        curl_easy_setopt(curl_, CURLOPT_NOPROXY, noProxy);
    } catch (...) {
        // Non-fatal: continue without nopxy if setting fails
    }
    // Disable any proxy explicitly for this handle. Verbose logging
    // is controlled by `verbose_` and set per-request when enabled.
    try {
        curl_easy_setopt(curl_, CURLOPT_PROXY, "");
    } catch (...) {
    }
}

HttpClient::~HttpClient() {
    if (curl_) {
        curl_easy_cleanup(curl_);
    }
    // B2: curl_global_cleanup moved to main.cpp
}

// ============================================================================
// Authentication
// ============================================================================

bool HttpClient::login(const std::string& username, const std::string& password) {
    Logger::info("Attempting login for user: {}", username);
    
    try {
        json requestBody = {
            {"username", username},
            {"password", password}
        };
        
        std::string response = performRequest(
            baseUrl_ + "/api/auth/login",
            "POST",
            requestBody.dump()
        );
        
        auto responseJson = json::parse(response);
        
        if (responseJson.contains("access_token")) {
            authToken_ = responseJson["access_token"].get<std::string>();
            Logger::info("Login successful, token acquired");
            return true;
        }
        
        Logger::error("Login failed: No access token in response");
        return false;
        
    } catch (const std::exception& e) {
        Logger::error("Login failed: {}", e.what());
        return false;
    }
}

void HttpClient::setAuthToken(const std::string& token) {
    authToken_ = token;
    Logger::debug("Auth token updated");
}

void HttpClient::clearAuthToken() {
    authToken_.clear();
    Logger::debug("Auth token cleared");
}

bool HttpClient::isAuthenticated() const {
    return !authToken_.empty();
}

bool HttpClient::registerDevice(const std::string& deviceId, const std::string& deviceName) {
    Logger::info("Registering desktop device: {} ({})", deviceName, deviceId);

    if (!isAuthenticated()) {
        Logger::error("Cannot register device: Not authenticated");
        return false;
    }

    try {
        json requestBody = {
            {"device_id", deviceId},
            {"device_name", deviceName}
        };

        std::string response = performRequest(
            baseUrl_ + "/api/sync/register-desktop",
            "POST",
            requestBody.dump()
        );

        auto responseJson = json::parse(response);

        if (responseJson.contains("device_id")) {
            std::string status = responseJson.value("status", "");
            if (status == "registered") {
                Logger::info("Device registered successfully");
            } else if (status == "already_registered") {
                Logger::info("Device already registered (re-registration successful)");
            } else {
                Logger::info("Device registration response: {}", status);
            }
            return true;
        }

        Logger::error("Device registration failed: No device_id in response");
        return false;

    } catch (const std::exception& e) {
        Logger::error("Device registration failed: {}", e.what());
        return false;
    }
}

std::string HttpClient::refreshAccessToken(const std::string& refreshToken) {
    Logger::info("Attempting to refresh access token");

    try {
        json requestBody = {
            {"refresh_token", refreshToken}
        };

        std::string response = performRequest(
            baseUrl_ + "/api/auth/refresh",
            "POST",
            requestBody.dump()
        );

        auto responseJson = json::parse(response);

        if (responseJson.contains("access_token")) {
            std::string newToken = responseJson["access_token"].get<std::string>();
            authToken_ = newToken;
            Logger::info("Access token refreshed successfully");
            return newToken;
        }

        Logger::error("Token refresh failed: No access_token in response");
        throw std::runtime_error("Token refresh failed: No access_token in response");

    } catch (const TokenExpiredException&) {
        // Refresh token itself is expired
        Logger::error("Refresh token is also expired");
        throw;
    } catch (const std::exception& e) {
        Logger::error("Token refresh failed: {}", e.what());
        throw;
    }
}

// ============================================================================
// File Operations
// ============================================================================

std::vector<RemoteFile> HttpClient::listFiles(const std::string& remotePath) {
    Logger::debug("Listing files: {}", remotePath);
    
    std::vector<RemoteFile> files;
    
    try {
        std::string url = baseUrl_ + "/api/files/list?path=" +
                         curl_easy_escape(curl_, remotePath.c_str(), static_cast<int>(remotePath.length()));

        std::string response = performRequest(url, "GET");
        auto responseJson = json::parse(response);

        if (responseJson.contains("files") && responseJson["files"].is_array()) {
            for (const auto& fileJson : responseJson["files"]) {
                RemoteFile file;
                file.name = fileJson.value("name", "");
                file.path = fileJson.value("path", "");
                file.size = fileJson.value("size", 0);
                file.isDirectory = (fileJson.value("type", "file") == "directory");
                file.modifiedAt = fileJson.value("modified_at", "");
                files.push_back(file);
            }
        }
        
        Logger::debug("Listed {} files/directories", files.size());
        
    } catch (const std::exception& e) {
        Logger::error("Failed to list files: {}", e.what());
    }
    
    return files;
}

bool HttpClient::uploadFile(const std::string& localPath, const std::string& remotePath) {
    Logger::info("Uploading: {} -> {}", localPath, remotePath);

    if (!isAuthenticated()) {
        Logger::error("Cannot upload: Not authenticated");
        return false;
    }

    // K3: Verify file exists — no longer reading entire file into RAM
    if (!std::filesystem::exists(localPath)) {
        Logger::error("Cannot open file: {}", localPath);
        return false;
    }

    try {
        // Rate limit: wait for a slot before uploading
        uploadRateLimiter_.acquire();

        auto scopedHandle = handlePool_.acquire();
        CURL* uploadCurl = scopedHandle.get();
        if (verbose_) {
            curl_easy_setopt(uploadCurl, CURLOPT_VERBOSE, 1L);
        }

        struct curl_slist* headers = nullptr;
        std::string authHeader = "Authorization: Bearer " + authToken_;
        headers = curl_slist_append(headers, authHeader.c_str());

        std::string url = baseUrl_ + "/api/files/upload";

        // Extract directory part from remotePath
        std::string directoryPart = remotePath;
        auto lastDirSlash = directoryPart.find_last_of('/');
        if (lastDirSlash != std::string::npos) {
            directoryPart = directoryPart.substr(0, lastDirSlash);
        } else {
            directoryPart = "";
        }

        // Extract filename from local path
        std::string filename = localPath;
        auto lastSlash = filename.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            filename = filename.substr(lastSlash + 1);
        }

        // Build multipart form data using curl_mime
        curl_mime* mime = curl_mime_init(uploadCurl);

        // "path" form field
        curl_mimepart* pathPart = curl_mime_addpart(mime);
        curl_mime_name(pathPart, "path");
        curl_mime_data(pathPart, directoryPart.c_str(), CURL_ZERO_TERMINATED);

        // K3: "file" form field — stream from disk (not buffered in RAM)
        curl_mimepart* part = curl_mime_addpart(mime);
        curl_mime_name(part, "file");
        curl_mime_filedata(part, localPath.c_str());
        curl_mime_filename(part, filename.c_str());

        std::string responseBuffer;
        WriteCallbackData callbackData{&responseBuffer};

        curl_easy_setopt(uploadCurl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(uploadCurl, CURLOPT_MIMEPOST, mime);
        curl_easy_setopt(uploadCurl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(uploadCurl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(uploadCurl, CURLOPT_WRITEDATA, &callbackData);
        applyTimeoutSettings(uploadCurl);

        CURLcode res = curl_easy_perform(uploadCurl);

        long httpCode = 0;
        curl_easy_getinfo(uploadCurl, CURLINFO_RESPONSE_CODE, &httpCode);

        curl_mime_free(mime);
        curl_slist_free_all(headers);
        // Handle is returned to pool automatically by ScopedHandle destructor

        if (res != CURLE_OK) {
            Logger::error("Upload failed: {}", curl_easy_strerror(res));
            return false;
        }

        if (httpCode >= 200 && httpCode < 300) {
            Logger::info("Upload successful (HTTP {})", httpCode);
            return true;
        } else if (httpCode == 429) {
            int retryAfter = 60;
            try {
                auto errJson = json::parse(responseBuffer);
                if (errJson.contains("retry_after")) {
                    retryAfter = errJson["retry_after"].get<int>();
                }
            } catch (...) {}
            Logger::warn("Upload rate limited (429), retry after {}s: {}", retryAfter, remotePath);
            throw RateLimitException(retryAfter);
        } else if (httpCode == 401) {
            Logger::warn("Upload unauthorized (401), token may be expired");
            throw TokenExpiredException();
        } else if (httpCode == 507) {
            Logger::error("Storage quota exceeded (507): {}", remotePath);
            throw QuotaExceededException();
        } else {
            Logger::error("Upload failed with HTTP {}: {}", httpCode, responseBuffer);
            return false;
        }

    } catch (const TokenExpiredException&) {
        throw;
    } catch (const RateLimitException&) {
        throw;
    } catch (const QuotaExceededException&) {
        throw;
    } catch (const std::exception& e) {
        Logger::error("Upload exception: {}", e.what());
        return false;
    }
}

bool HttpClient::uploadFile(const std::string& localPath, const std::string& remotePath,
                            TransferProgressCallback progressCb) {
    Logger::info("Uploading (with progress): {} -> {}", localPath, remotePath);

    if (!isAuthenticated()) {
        Logger::error("Cannot upload: Not authenticated");
        return false;
    }

    // Verify file exists and is readable
    if (!std::filesystem::exists(localPath)) {
        Logger::error("Cannot open file: {}", localPath);
        return false;
    }

    try {
        // Rate limit: wait for a slot before uploading
        uploadRateLimiter_.acquire();

        auto scopedHandle = handlePool_.acquire();
        CURL* uploadCurl = scopedHandle.get();
        if (verbose_) {
            curl_easy_setopt(uploadCurl, CURLOPT_VERBOSE, 1L);
        }

        struct curl_slist* headers = nullptr;
        std::string authHeader = "Authorization: Bearer " + authToken_;
        headers = curl_slist_append(headers, authHeader.c_str());

        std::string url = baseUrl_ + "/api/files/upload";

        // Extract directory part from remotePath
        std::string directoryPart = remotePath;
        auto lastDirSlash = directoryPart.find_last_of('/');
        if (lastDirSlash != std::string::npos) {
            directoryPart = directoryPart.substr(0, lastDirSlash);
        } else {
            directoryPart = "";
        }

        // Extract filename from local path
        std::string filename = localPath;
        auto lastSlash = filename.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            filename = filename.substr(lastSlash + 1);
        }

        // Build multipart form data using curl_mime
        curl_mime* mime = curl_mime_init(uploadCurl);

        // "path" form field
        curl_mimepart* pathPart = curl_mime_addpart(mime);
        curl_mime_name(pathPart, "path");
        curl_mime_data(pathPart, directoryPart.c_str(), CURL_ZERO_TERMINATED);

        // "file" form field — stream from disk for progress tracking
        curl_mimepart* part = curl_mime_addpart(mime);
        curl_mime_name(part, "file");
        curl_mime_filedata(part, localPath.c_str());
        curl_mime_filename(part, filename.c_str());

        std::string responseBuffer;
        WriteCallbackData callbackData{&responseBuffer};

        curl_easy_setopt(uploadCurl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(uploadCurl, CURLOPT_MIMEPOST, mime);
        curl_easy_setopt(uploadCurl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(uploadCurl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(uploadCurl, CURLOPT_WRITEDATA, &callbackData);
        applyTimeoutSettings(uploadCurl);

        // Progress callback
        if (progressCb) {
            curl_easy_setopt(uploadCurl, CURLOPT_XFERINFOFUNCTION, transferProgressCallback);
            curl_easy_setopt(uploadCurl, CURLOPT_XFERINFODATA, &progressCb);
            curl_easy_setopt(uploadCurl, CURLOPT_NOPROGRESS, 0L);
        }

        CURLcode res = curl_easy_perform(uploadCurl);

        long httpCode = 0;
        curl_easy_getinfo(uploadCurl, CURLINFO_RESPONSE_CODE, &httpCode);

        curl_mime_free(mime);
        curl_slist_free_all(headers);
        // Handle is returned to pool automatically by ScopedHandle destructor

        if (res != CURLE_OK) {
            Logger::error("Upload failed: {}", curl_easy_strerror(res));
            return false;
        }

        if (httpCode >= 200 && httpCode < 300) {
            Logger::info("Upload successful (HTTP {})", httpCode);
            return true;
        } else if (httpCode == 429) {
            int retryAfter = 60;
            try {
                auto errJson = json::parse(responseBuffer);
                if (errJson.contains("retry_after")) {
                    retryAfter = errJson["retry_after"].get<int>();
                }
            } catch (...) {}
            Logger::warn("Upload rate limited (429), retry after {}s: {}", retryAfter, remotePath);
            throw RateLimitException(retryAfter);
        } else if (httpCode == 401) {
            Logger::warn("Upload unauthorized (401), token may be expired");
            throw TokenExpiredException();
        } else if (httpCode == 507) {
            Logger::error("Storage quota exceeded (507): {}", remotePath);
            throw QuotaExceededException();
        } else {
            Logger::error("Upload failed with HTTP {}: {}", httpCode, responseBuffer);
            return false;
        }

    } catch (const TokenExpiredException&) {
        throw;
    } catch (const RateLimitException&) {
        throw;
    } catch (const QuotaExceededException&) {
        throw;
    } catch (const std::exception& e) {
        Logger::error("Upload exception: {}", e.what());
        return false;
    }
}

bool HttpClient::uploadFileBatch(const std::vector<std::string>& localPaths,
                                  const std::string& remoteDir) {
    Logger::info("Uploading batch: {} files to {}", localPaths.size(), remoteDir);

    if (!isAuthenticated()) {
        Logger::error("Cannot upload: Not authenticated");
        return false;
    }

    if (localPaths.empty()) {
        Logger::debug("Empty batch, skipping");
        return true;
    }

    // Verify all files exist
    for (const auto& localPath : localPaths) {
        if (!std::filesystem::exists(localPath)) {
            Logger::error("Batch upload: file not found: {}", localPath);
            return false;
        }
    }

    try {
        // Rate limit: wait for a slot before uploading
        uploadRateLimiter_.acquire();

        auto scopedHandle = handlePool_.acquire();
        CURL* uploadCurl = scopedHandle.get();
        if (verbose_) {
            curl_easy_setopt(uploadCurl, CURLOPT_VERBOSE, 1L);
        }

        struct curl_slist* headers = nullptr;
        std::string authHeader = "Authorization: Bearer " + authToken_;
        headers = curl_slist_append(headers, authHeader.c_str());

        std::string url = baseUrl_ + "/api/files/upload";

        // Build multipart form data using curl_mime
        curl_mime* mime = curl_mime_init(uploadCurl);

        // "path" form field (target directory)
        curl_mimepart* pathPart = curl_mime_addpart(mime);
        curl_mime_name(pathPart, "path");
        curl_mime_data(pathPart, remoteDir.c_str(), CURL_ZERO_TERMINATED);

        // Multiple "files" form fields (same name -> FastAPI list[UploadFile])
        for (const auto& localPath : localPaths) {
            std::string filename = localPath;
            auto lastSlash = filename.find_last_of("/\\");
            if (lastSlash != std::string::npos) {
                filename = filename.substr(lastSlash + 1);
            }

            curl_mimepart* part = curl_mime_addpart(mime);
            curl_mime_name(part, "files");  // "files" (plural) not "file"
            curl_mime_filedata(part, localPath.c_str());
            curl_mime_filename(part, filename.c_str());
        }

        std::string responseBuffer;
        WriteCallbackData callbackData{&responseBuffer};

        curl_easy_setopt(uploadCurl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(uploadCurl, CURLOPT_MIMEPOST, mime);
        curl_easy_setopt(uploadCurl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(uploadCurl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(uploadCurl, CURLOPT_WRITEDATA, &callbackData);
        applyTimeoutSettings(uploadCurl);

        CURLcode res = curl_easy_perform(uploadCurl);

        long httpCode = 0;
        curl_easy_getinfo(uploadCurl, CURLINFO_RESPONSE_CODE, &httpCode);

        curl_mime_free(mime);
        curl_slist_free_all(headers);
        // Handle is returned to pool automatically by ScopedHandle destructor

        if (res != CURLE_OK) {
            Logger::error("Batch upload failed: {}", curl_easy_strerror(res));
            return false;
        }

        if (httpCode >= 200 && httpCode < 300) {
            try {
                auto respJson = json::parse(responseBuffer);
                int uploaded = respJson.value("uploaded", 0);
                Logger::info("Batch upload successful: {}/{} files (HTTP {})",
                            uploaded, localPaths.size(), httpCode);
            } catch (...) {
                Logger::info("Batch upload successful (HTTP {})", httpCode);
            }
            return true;
        } else if (httpCode == 429) {
            int retryAfter = 60;
            try {
                auto errJson = json::parse(responseBuffer);
                if (errJson.contains("retry_after")) {
                    retryAfter = errJson["retry_after"].get<int>();
                }
            } catch (...) {}
            Logger::warn("Batch upload rate limited (429), retry after {}s", retryAfter);
            throw RateLimitException(retryAfter);
        } else if (httpCode == 401) {
            Logger::warn("Batch upload unauthorized (401), token may be expired");
            throw TokenExpiredException();
        } else if (httpCode == 507) {
            Logger::error("Storage quota exceeded (507) during batch upload");
            throw QuotaExceededException();
        } else {
            Logger::error("Batch upload failed with HTTP {}: {}", httpCode, responseBuffer);
            return false;
        }

    } catch (const TokenExpiredException&) {
        throw;
    } catch (const RateLimitException&) {
        throw;
    } catch (const QuotaExceededException&) {
        throw;
    } catch (const std::exception& e) {
        Logger::error("Batch upload exception: {}", e.what());
        return false;
    }
}

bool HttpClient::downloadFile(const std::string& remotePath, const std::string& localPath) {
    Logger::info("Downloading: {} -> {}", remotePath, localPath);

    if (!isAuthenticated()) {
        Logger::error("Cannot download: Not authenticated");
        return false;
    }

    // H2: Atomic download — write to .tmp file, rename on success
    std::string tmpPath = localPath + ".baludesk.tmp";

    std::ofstream outFile(tmpPath, std::ios::binary);
    if (!outFile) {
        Logger::error("Cannot create temp file: {}", tmpPath);
        return false;
    }

    try {
        auto scopedHandle = handlePool_.acquire();
        CURL* downloadCurl = scopedHandle.get();
        if (verbose_) {
            curl_easy_setopt(downloadCurl, CURLOPT_VERBOSE, 1L);
        }

        struct curl_slist* headers = nullptr;
        std::string authHeader = "Authorization: Bearer " + authToken_;
        headers = curl_slist_append(headers, authHeader.c_str());

        // A1: Use path parameter format matching BaluHost: GET /api/files/download/{path}
        std::string url = baseUrl_ + "/api/files/download/" + remotePath;

        curl_easy_setopt(downloadCurl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(downloadCurl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(downloadCurl, CURLOPT_HTTPGET, 1L);
        // K2: Stream directly to file instead of buffering in RAM
        curl_easy_setopt(downloadCurl, CURLOPT_WRITEFUNCTION, writeFileCallback);
        curl_easy_setopt(downloadCurl, CURLOPT_WRITEDATA, &outFile);
        applyTimeoutSettings(downloadCurl);

        CURLcode res = curl_easy_perform(downloadCurl);

        long httpCode = 0;
        curl_easy_getinfo(downloadCurl, CURLINFO_RESPONSE_CODE, &httpCode);

        curl_slist_free_all(headers);
        // Handle returned to pool by ScopedHandle destructor
        outFile.close();

        if (res != CURLE_OK) {
            Logger::error("Download failed: {}", curl_easy_strerror(res));
            std::filesystem::remove(tmpPath);
            return false;
        }

        if (httpCode >= 200 && httpCode < 300) {
            // Atomic rename: tmp -> final path
            std::error_code ec;
            std::filesystem::rename(tmpPath, localPath, ec);
            if (ec) {
                Logger::error("Failed to rename tmp file: {}", ec.message());
                std::filesystem::remove(tmpPath);
                return false;
            }
            auto fileSize = std::filesystem::file_size(localPath);
            Logger::info("Download successful ({} bytes)", fileSize);
            return true;
        } else if (httpCode == 401) {
            std::filesystem::remove(tmpPath);
            Logger::warn("Download unauthorized (401): {}", remotePath);
            throw TokenExpiredException();
        } else if (httpCode == 429) {
            std::filesystem::remove(tmpPath);
            Logger::warn("Download rate limited (429): {}", remotePath);
            throw RateLimitException(60);
        } else {
            std::filesystem::remove(tmpPath);
            Logger::error("Download failed with HTTP {}", httpCode);
            return false;
        }

    } catch (const TokenExpiredException&) {
        std::filesystem::remove(tmpPath);
        throw;
    } catch (const RateLimitException&) {
        std::filesystem::remove(tmpPath);
        throw;
    } catch (const std::exception& e) {
        Logger::error("Download exception: {}", e.what());
        std::filesystem::remove(tmpPath);
        return false;
    }
}

bool HttpClient::deleteFile(const std::string& remotePath) {
    Logger::info("Deleting remote file: {}", remotePath);
    
    try {
        std::string url = baseUrl_ + "/api/files?path=" + 
                         curl_easy_escape(curl_, remotePath.c_str(), static_cast<int>(remotePath.length()));
        
        std::string response = performRequest(url, "DELETE");
        
        Logger::info("Delete successful");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Delete failed: {}", e.what());
        return false;
    }
}

// ============================================================================
// Sync Operations
// ============================================================================

std::vector<RemoteChange> HttpClient::getChangesSince(const std::string& timestamp) {
    Logger::debug("Getting changes since: {}", timestamp);
    
    std::vector<RemoteChange> changes;
    
    try {
        std::string url = baseUrl_ + "/api/sync/changes?since=" + 
                         curl_easy_escape(curl_, timestamp.c_str(), static_cast<int>(timestamp.length()));
        
        std::string response = performRequest(url, "GET");
        auto responseJson = json::parse(response);
        
        if (responseJson.contains("changes") && responseJson["changes"].is_array()) {
            for (const auto& changeJson : responseJson["changes"]) {
                RemoteChange change;
                change.path = changeJson.value("path", "");
                change.action = changeJson.value("action", "");
                change.timestamp = changeJson.value("timestamp", "");
                changes.push_back(change);
            }
        }
        
        Logger::debug("Retrieved {} changes", changes.size());
        
    } catch (const std::exception& e) {
        Logger::error("Failed to get changes: {}", e.what());
    }
    
    return changes;
}

// ============================================================================
// Delta-Sync
// ============================================================================

DeltaSyncResponse HttpClient::performDeltaSync(const std::string& deviceId,
                                                const std::vector<FileMetadataEntry>& fileList,
                                                const std::string& changeToken) {
    Logger::info("Performing delta sync for device: {}", deviceId);

    DeltaSyncResponse result;

    try {
        // Build file_list JSON array
        json fileListJson = json::array();
        for (const auto& entry : fileList) {
            fileListJson.push_back({
                {"path", entry.path},
                {"hash", entry.hash},
                {"size", entry.size},
                {"modified_at", entry.modified_at}
            });
        }

        json requestBody = {
            {"device_id", deviceId},
            {"file_list", fileListJson}
        };

        if (!changeToken.empty()) {
            requestBody["change_token"] = changeToken;
        }

        std::string response = performRequest(
            baseUrl_ + "/api/sync/changes",
            "POST",
            requestBody.dump()
        );

        auto responseJson = json::parse(response);

        // Parse to_download
        if (responseJson.contains("to_download") && responseJson["to_download"].is_array()) {
            for (const auto& item : responseJson["to_download"]) {
                RemoteFile file;
                file.name = safe_value<std::string>(item, "name", "");
                file.path = safe_value<std::string>(item, "path", "");
                file.size = safe_value<uint64_t>(item, "size", 0);
                file.isDirectory = safe_value<bool>(item, "is_directory", false);
                file.modifiedAt = safe_value<std::string>(item, "modified_at", "");
                file.hash = safe_value<std::string>(item, "hash", "");
                result.toDownload.push_back(file);
            }
        }

        // A3: to_upload removed — BaluHost does not return this field.
        // Local uploads are determined client-side via change_detector.

        // Parse to_delete
        if (responseJson.contains("to_delete") && responseJson["to_delete"].is_array()) {
            for (const auto& item : responseJson["to_delete"]) {
                if (item.is_string()) {
                    result.toDelete.push_back(item.get<std::string>());
                } else if (item.is_object()) {
                    result.toDelete.push_back(safe_value<std::string>(item, "path", ""));
                }
            }
        }

        // A2: Parse conflicts with BaluHost field names
        if (responseJson.contains("conflicts") && responseJson["conflicts"].is_array()) {
            for (const auto& item : responseJson["conflicts"]) {
                DeltaConflict conflict;
                conflict.path = safe_value<std::string>(item, "path", "");
                conflict.clientHash = safe_value<std::string>(item, "client_hash", "");
                conflict.serverHash = safe_value<std::string>(item, "server_hash", "");
                conflict.serverModifiedAt = safe_value<std::string>(item, "server_modified_at", "");
                result.conflicts.push_back(conflict);
            }
        }

        // Parse change token
        result.changeToken = safe_value<std::string>(responseJson, "change_token", "");

        Logger::info("Delta sync result: {} downloads, {} deletes, {} conflicts",
                     result.toDownload.size(), result.toDelete.size(), result.conflicts.size());

    } catch (const std::exception& e) {
        Logger::error("Delta sync failed: {}", e.what());
        throw;
    }

    return result;
}

// ============================================================================
// Configuration
// ============================================================================

void HttpClient::setTimeout(long timeout) {
    connectTimeout_ = timeout;
    Logger::debug("Connect timeout set to {} seconds", timeout);
}

void HttpClient::applyTimeoutSettings(CURL* handle) {
    // Connection timeout: how long to wait for TCP+TLS handshake
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, connectTimeout_);
    // No hard transfer timeout (CURLOPT_TIMEOUT=0) — large files need unlimited time
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, 0L);
    // Stall detection: abort if transfer speed drops below lowSpeedLimit_ bytes/sec
    // for lowSpeedTime_ seconds (default: <1KB/s for 60s = abort)
    curl_easy_setopt(handle, CURLOPT_LOW_SPEED_LIMIT, lowSpeedLimit_);
    curl_easy_setopt(handle, CURLOPT_LOW_SPEED_TIME, lowSpeedTime_);
}

void HttpClient::setVerbose(bool verbose) {
    verbose_ = verbose;
    Logger::debug("Verbose mode: {}", verbose ? "enabled" : "disabled");
}

// ============================================================================
// Private Methods
// ============================================================================

std::string HttpClient::performRequest(const std::string& url, const std::string& method, const std::string& body) {
    // K4: Create a fresh CURL handle per request to avoid race conditions
    // when sync runs in a detached thread while other API calls happen concurrently.
    // This also fixes M8: CURLOPT_CUSTOMREQUEST persisting across calls.
    CURL* handle = curl_easy_init();
    if (!handle) {
        throw std::runtime_error("Failed to create CURL handle");
    }

    std::string responseBuffer;
    WriteCallbackData callbackData{&responseBuffer};

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    if (isAuthenticated()) {
        std::string authHeader = "Authorization: Bearer " + authToken_;
        headers = curl_slist_append(headers, authHeader.c_str());
    }

    curl_easy_setopt(handle, CURLOPT_NOPROXY, "127.0.0.1;localhost");
    curl_easy_setopt(handle, CURLOPT_PROXY, "");
    curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
    Logger::debug("HTTP request: {} {}", method, url);
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &callbackData);
    applyTimeoutSettings(handle);

    if (verbose_) {
        curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L);
    }

    if (method == "POST") {
        curl_easy_setopt(handle, CURLOPT_POST, 1L);
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, body.c_str());
    } else if (method == "DELETE") {
        curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "DELETE");
    } else if (method == "PUT") {
        curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, body.c_str());
    } else {
        curl_easy_setopt(handle, CURLOPT_HTTPGET, 1L);
    }

    CURLcode res = curl_easy_perform(handle);

    long httpCode = 0;
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_slist_free_all(headers);
    curl_easy_cleanup(handle);

    if (res != CURLE_OK) {
        std::string error = "CURL error: " + std::string(curl_easy_strerror(res));
        Logger::error(error);
        throw std::runtime_error(error);
    }

    if (httpCode == 401) {
        Logger::warn("HTTP 401 Unauthorized: token may be expired");
        throw TokenExpiredException();
    }

    if (httpCode == 429) {
        int retryAfter = 60;
        try {
            auto errJson = json::parse(responseBuffer);
            if (errJson.contains("retry_after")) {
                retryAfter = errJson["retry_after"].get<int>();
            }
        } catch (...) {}
        Logger::warn("HTTP 429 Rate Limited, retry after {}s", retryAfter);
        throw RateLimitException(retryAfter);
    }

    if (httpCode >= 400) {
        std::string error = "HTTP error " + std::to_string(httpCode) + ": " + responseBuffer;
        Logger::error(error);
        throw std::runtime_error(error);
    }

    return responseBuffer;
}

size_t HttpClient::writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    auto* data = static_cast<WriteCallbackData*>(userp);
    
    if (data && data->buffer) {
        data->buffer->append(static_cast<char*>(contents), totalSize);
    }
    
    return totalSize;
}

size_t HttpClient::readCallback(void* ptr, size_t size, size_t nmemb, void* userp) {
    size_t maxSize = size * nmemb;
    auto* data = static_cast<ReadCallbackData*>(userp);
    
    if (data && data->file && data->file->good()) {
        data->file->read(static_cast<char*>(ptr), maxSize);
        return data->file->gcount();
    }
    
    return 0;
}

// Write callback for file downloads
size_t HttpClient::writeFileCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    auto* file = static_cast<std::ofstream*>(userp);
    
    if (file && file->is_open()) {
        file->write(static_cast<char*>(contents), totalSize);
        return totalSize;
    }
    
    return 0;
}

// Progress callback (download-only, used by downloadFileWithProgress)
int HttpClient::progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                                curl_off_t ultotal, curl_off_t ulnow) {
    (void)ultotal;  // Intentionally unused
    (void)ulnow;    // Intentionally unused
    auto* callback = static_cast<ProgressCallback*>(clientp);

    if (callback && dltotal > 0) {
        DownloadProgress progress;
        progress.bytesDownloaded = static_cast<size_t>(dlnow);
        progress.totalBytes = static_cast<size_t>(dltotal);
        progress.percentage = (static_cast<double>(dlnow) / static_cast<double>(dltotal)) * 100.0;

        (*callback)(progress);
    }

    return 0;  // Return 0 to continue, non-zero to abort
}

// Transfer progress callback (upload + download, used by uploadFile with progress)
// NOTE: This is a C-compatible callback invoked by libcurl. Exceptions must NOT
// propagate across the C frame boundary (undefined behavior), so we wrap in try-catch.
int HttpClient::transferProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                                          curl_off_t ultotal, curl_off_t ulnow) {
    try {
        auto* callback = static_cast<TransferProgressCallback*>(clientp);
        if (callback && *callback) {
            TransferProgress progress{};
            if (ultotal > 0) {
                progress.bytesTransferred = static_cast<size_t>(ulnow);
                progress.totalBytes = static_cast<size_t>(ultotal);
                progress.percentage = (static_cast<double>(ulnow) / static_cast<double>(ultotal)) * 100.0;
                progress.isUpload = true;
            } else if (dltotal > 0) {
                progress.bytesTransferred = static_cast<size_t>(dlnow);
                progress.totalBytes = static_cast<size_t>(dltotal);
                progress.percentage = (static_cast<double>(dlnow) / static_cast<double>(dltotal)) * 100.0;
                progress.isUpload = false;
            } else {
                return 0;
            }
            (*callback)(progress);
        }
    } catch (...) {
        // Exceptions must not propagate across C boundary — abort transfer
        return 1;
    }
    return 0;
}

// Download file with Range support (resume capability)
bool HttpClient::downloadFileRange(
    const std::string& remotePath,
    const std::string& localPath,
    size_t startByte,
    size_t endByte
) {
    Logger::info("Downloading file range: {} (bytes {}-{})", remotePath, startByte, 
                endByte > 0 ? std::to_string(endByte) : "end");
    
    try {
        std::ofstream file(localPath, std::ios::binary | std::ios::app);
        if (!file.is_open()) {
            Logger::error("Cannot open file for writing: {}", localPath);
            return false;
        }
        
        CURL* downloadCurl = curl_easy_init();
        if (!downloadCurl) {
            Logger::error("Failed to create download handle");
            return false;
        }
        // Bypass proxy for local addresses to avoid proxy interference
        curl_easy_setopt(downloadCurl, CURLOPT_NOPROXY, "127.0.0.1;localhost");
        
        struct curl_slist* headers = nullptr;
        std::string authHeader = "Authorization: Bearer " + authToken_;
        headers = curl_slist_append(headers, authHeader.c_str());
        
        // Set Range header for resume
        std::string rangeHeader = "Range: bytes=" + std::to_string(startByte) + "-";
        if (endByte > 0) {
            rangeHeader += std::to_string(endByte);
        }
        headers = curl_slist_append(headers, rangeHeader.c_str());
        
        // A1: Use path parameter format matching BaluHost: GET /api/files/download/{path}
        std::string url = baseUrl_ + "/api/files/download/" + remotePath;

        curl_easy_setopt(downloadCurl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(downloadCurl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(downloadCurl, CURLOPT_WRITEFUNCTION, writeFileCallback);
        curl_easy_setopt(downloadCurl, CURLOPT_WRITEDATA, &file);
        applyTimeoutSettings(downloadCurl);
        curl_easy_setopt(downloadCurl, CURLOPT_FOLLOWLOCATION, 1L);

        if (verbose_) {
            curl_easy_setopt(downloadCurl, CURLOPT_VERBOSE, 1L);
        }

        CURLcode res = curl_easy_perform(downloadCurl);

        long httpCode = 0;
        curl_easy_getinfo(downloadCurl, CURLINFO_RESPONSE_CODE, &httpCode);

        curl_slist_free_all(headers);
        curl_easy_cleanup(downloadCurl);
        file.close();

        if (res != CURLE_OK) {
            Logger::error("Download range failed: {}", curl_easy_strerror(res));
            return false;
        }
        
        // Accept both 200 (full content) and 206 (partial content)
        if (httpCode == 200 || httpCode == 206) {
            Logger::info("Download range successful (HTTP {})", httpCode);
            return true;
        } else {
            Logger::error("Download range failed with HTTP {}", httpCode);
            return false;
        }
        
    } catch (const std::exception& e) {
        Logger::error("Exception during download range: {}", e.what());
        return false;
    }
}

// Simple GET helper
std::string HttpClient::get(const std::string& path) {
    // path may be absolute (full URL) or relative path starting with '/'
    std::string url;
    if (path.rfind("http://", 0) == 0 || path.rfind("https://", 0) == 0) {
        url = path;
    } else {
        url = baseUrl_ + path;
    }
    return performRequest(url, "GET");
}

// Download with progress callback
bool HttpClient::downloadFileWithProgress(
    const std::string& remotePath,
    const std::string& localPath,
    ProgressCallback callback
) {
    Logger::info("Downloading file with progress: {}", remotePath);
    
    try {
        std::ofstream file(localPath, std::ios::binary);
        if (!file.is_open()) {
            Logger::error("Cannot open file for writing: {}", localPath);
            return false;
        }
        
        CURL* downloadCurl = curl_easy_init();
        if (!downloadCurl) {
            Logger::error("Failed to create download handle");
            return false;
        }
        
        struct curl_slist* headers = nullptr;
        std::string authHeader = "Authorization: Bearer " + authToken_;
        headers = curl_slist_append(headers, authHeader.c_str());
        
        // A1: Use path parameter format matching BaluHost: GET /api/files/download/{path}
        std::string url = baseUrl_ + "/api/files/download/" + remotePath;

        curl_easy_setopt(downloadCurl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(downloadCurl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(downloadCurl, CURLOPT_WRITEFUNCTION, writeFileCallback);
        curl_easy_setopt(downloadCurl, CURLOPT_WRITEDATA, &file);
        applyTimeoutSettings(downloadCurl);
        curl_easy_setopt(downloadCurl, CURLOPT_FOLLOWLOCATION, 1L);

        // Enable progress callback
        curl_easy_setopt(downloadCurl, CURLOPT_XFERINFOFUNCTION, progressCallback);
        curl_easy_setopt(downloadCurl, CURLOPT_XFERINFODATA, &callback);
        curl_easy_setopt(downloadCurl, CURLOPT_NOPROGRESS, 0L);
        
        if (verbose_) {
            curl_easy_setopt(downloadCurl, CURLOPT_VERBOSE, 1L);
        }
        
        CURLcode res = curl_easy_perform(downloadCurl);
        
        long httpCode = 0;
        curl_easy_getinfo(downloadCurl, CURLINFO_RESPONSE_CODE, &httpCode);
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(downloadCurl);
        file.close();
        
        if (res != CURLE_OK) {
            Logger::error("Download with progress failed: {}", curl_easy_strerror(res));
            return false;
        }
        
        if (httpCode >= 200 && httpCode < 300) {
            Logger::info("Download with progress successful (HTTP {})", httpCode);
            return true;
        } else {
            Logger::error("Download with progress failed with HTTP {}", httpCode);
            return false;
        }
        
    } catch (const std::exception& e) {
        Logger::error("Exception during download with progress: {}", e.what());
        return false;
    }
}

// ============================================================================
// System Info from BaluHost Server
// ============================================================================

SystemInfoFromServer HttpClient::getSystemInfoFromServer() {
    Logger::debug("Fetching system info from BaluHost server");

    try {
        std::string response = get("/api/system/info");
        auto json = nlohmann::json::parse(response);

        SystemInfoFromServer info;
        info.cpuUsage = json["cpu"]["usage"].get<double>();
        info.cpuCores = json["cpu"]["cores"].get<uint32_t>();
        info.cpuFrequency = json["cpu"]["frequency_mhz"].get<uint32_t>();
        info.memoryTotal = json["memory"]["total"].get<uint64_t>();
        info.memoryUsed = json["memory"]["used"].get<uint64_t>();
        info.memoryAvailable = json["memory"]["available"].get<uint64_t>();
        info.diskTotal = json["disk"]["total"].get<uint64_t>();
        info.diskUsed = json["disk"]["used"].get<uint64_t>();
        info.diskAvailable = json["disk"]["available"].get<uint64_t>();
        info.uptime = json["uptime"].get<uint64_t>();

        Logger::debug("System info fetched successfully from server");
        return info;
    } catch (const std::exception& e) {
        Logger::error("Failed to fetch system info from server: {}", e.what());
        throw;
    }
}

RaidStatusFromServer HttpClient::getRaidStatusFromServer() {
    Logger::debug("Fetching RAID status from BaluHost server");

    try {
        std::string response = get("/api/system/raid/status");
        auto json = nlohmann::json::parse(response);

        RaidStatusFromServer status;
        status.devMode = safe_value<bool>(json, "dev_mode", false);

        if (json.contains("arrays") && json["arrays"].is_array()) {
            for (const auto& arrayJson : json["arrays"]) {
                RaidArray array;
                array.name = safe_value<std::string>(arrayJson, "name", "");
                array.level = safe_value<std::string>(arrayJson, "level", "");
                array.status = safe_value<std::string>(arrayJson, "status", "");
                array.size_bytes = safe_value<long long>(arrayJson, "size_bytes", 0LL);
                array.resync_progress = safe_value<double>(arrayJson, "resync_progress", 0.0);

                if (arrayJson.contains("devices") && arrayJson["devices"].is_array()) {
                    for (const auto& devJson : arrayJson["devices"]) {
                        RaidDevice device;
                        device.name = safe_value<std::string>(devJson, "name", "");
                        device.state = safe_value<std::string>(devJson, "state", "");
                        array.devices.push_back(device);
                    }
                }

                status.arrays.push_back(array);
            }
        }

        Logger::debug("RAID status fetched successfully from server ({} arrays)",
                     status.arrays.size());
        return status;
    } catch (const std::exception& e) {
        Logger::error("Failed to fetch RAID status from server: {}", e.what());
        throw;
    }
}

} // namespace baludesk

