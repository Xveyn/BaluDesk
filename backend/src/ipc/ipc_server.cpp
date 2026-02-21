#include "ipc_server.h"
#include "../sync/sync_engine.h"
#include "../utils/logger.h"
#include "../utils/system_info.h"
#include "../utils/settings_manager.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <string>

using json = nlohmann::json;

namespace baludesk {

IpcServer::IpcServer(SyncEngine* engine) : engine_(engine) {}

bool IpcServer::start() {
    Logger::info("IPC Server started, listening on stdin");

    // Register for sync status updates to broadcast to frontend
    if (engine_) {
        engine_->setStatusCallback([this](const SyncStats& state) {
            try {
                std::string status_str = "idle";
                if (state.status == SyncStatus::SYNCING) status_str = "syncing";
                else if (state.status == SyncStatus::PAUSED) status_str = "paused";
                else if (state.status == SyncStatus::SYNC_ERROR) status_str = "error";

                json data = {
                    {"status", status_str},
                    {"uploadSpeed", state.uploadSpeed},
                    {"downloadSpeed", state.downloadSpeed},
                    {"pendingUploads", state.pendingUploads},
                    {"pendingDownloads", state.pendingDownloads},
                    {"lastSync", state.lastSync}
                };

                broadcastEvent("sync_state_update", data);
            } catch (const std::exception& e) {
                Logger::error("Failed to broadcast sync state: {}", e.what());
            }
        });
    }

    // Register auth-required callback to broadcast event when token refresh fails
    engine_->setAuthRequiredCallback([this]() {
        broadcastEvent("auth_required", {{"reason", "token_expired"}});
    });

    return true;
}

void IpcServer::stop() {
    Logger::info("IPC Server stopped");
}

void IpcServer::processMessages() {
    // Non-blocking read from stdin
    std::string line;
    if (std::getline(std::cin, line)) {
        if (line.empty()) return;
        
        try {
            // Parse JSON message
            auto message = json::parse(line);
            
            if (!message.contains("type")) {
                Logger::error("IPC message missing 'type' field");
                return;
            }
            
            std::string type = message["type"];
            Logger::debug("Received IPC message: {}", type);
            
            // Extract request ID for responses
            int requestId = message.value("id", -1);
            
            // Handle different message types
            if (type == "ping") {
                handlePing(requestId);
            }
            else if (type == "set_tokens") {
                handleSetTokens(message, requestId);
            }
            else if (type == "check_stored_tokens") {
                handleCheckStoredTokens(requestId);
            }
            else if (type == "logout") {
                handleLogout(requestId);
            }
            else if (type == "get_device_info") {
                handleGetDeviceInfo(requestId);
            }
            else if (type == "add_sync_folder") {
                handleAddSyncFolder(message, requestId);
            }
            else if (type == "remove_sync_folder") {
                handleRemoveSyncFolder(message, requestId);
            }
            else if (type == "pause_sync") {
                handlePauseSync(message, requestId);
            }
            else if (type == "resume_sync") {
                handleResumeSync(message, requestId);
            }
            else if (type == "update_sync_folder") {
                handleUpdateSyncFolder(message, requestId);
            }
            else if (type == "get_sync_state") {
                handleGetSyncState(requestId);
            }
            else if (type == "get_folders") {
                handleGetFolders(requestId);
            }
            else if (type == "get_system_info") {
                handleGetSystemInfo(requestId);
            }
            else {
                Logger::warn("Unknown IPC message type: {}", type);
                sendError("Unknown command type", requestId);
            }
            
        } catch (const json::exception& e) {
            Logger::error("Failed to parse IPC message: {}", e.what());
        }
    }
}

void IpcServer::handlePing(int requestId) {
    json response = {
        {"type", "pong"},
        {"timestamp", std::time(nullptr)}
    };
    sendResponse(response, requestId);
}

void IpcServer::handleSetTokens(const json& message, int requestId) {
    try {
        if (!message.contains("data")) {
            sendError("Missing data", requestId);
            return;
        }

        auto data = message["data"];
        std::string serverUrl = data.value("serverUrl", "");
        std::string accessToken = data.value("accessToken", "");
        std::string refreshToken = data.value("refreshToken", "");
        std::string username = data.value("username", "");

        if (serverUrl.empty() || accessToken.empty() || refreshToken.empty() || username.empty()) {
            sendError("serverUrl, accessToken, refreshToken and username required", requestId);
            return;
        }

        Logger::info("Setting tokens for user: {} @ {}", username, serverUrl);

        bool success = engine_->setTokens(serverUrl, accessToken, refreshToken, username);

        if (success) {
            currentUsername_ = username;
            json response = {
                {"success", true},
                {"type", "tokens_set"}
            };
            sendResponse(response, requestId);
            Logger::info("Tokens set successfully for user: {}", username);
        } else {
            sendError("Failed to set tokens", requestId);
        }

    } catch (const std::exception& e) {
        sendError(std::string("Set tokens error: ") + e.what(), requestId);
        Logger::error("Set tokens exception: {}", e.what());
    }
}

void IpcServer::handleCheckStoredTokens(int requestId) {
    try {
        std::string status = engine_->checkStoredTokens();

        json response = {
            {"type", "stored_tokens_status"},
            {"success", true},
            {"status", status}
        };

        if (status == "authenticated") {
            response["username"] = engine_->getStoredUsername();
            response["serverUrl"] = engine_->getStoredServerUrl();
            currentUsername_ = engine_->getStoredUsername();
        }

        sendResponse(response, requestId);
        Logger::info("Stored tokens check result: {}", status);

    } catch (const std::exception& e) {
        sendError(std::string("Check stored tokens error: ") + e.what(), requestId);
        Logger::error("Check stored tokens exception: {}", e.what());
    }
}

void IpcServer::handleLogout(int requestId) {
    try {
        engine_->logout();
        currentUsername_.clear();

        json response = {
            {"type", "logged_out"},
            {"success", true}
        };
        sendResponse(response, requestId);
        Logger::info("Logout successful");

    } catch (const std::exception& e) {
        sendError(std::string("Logout error: ") + e.what(), requestId);
        Logger::error("Logout exception: {}", e.what());
    }
}

void IpcServer::handleGetDeviceInfo(int requestId) {
    try {
        auto& settings = SettingsManager::getInstance();

        json response = {
            {"type", "device_info"},
            {"success", true},
            {"data", {
                {"deviceId", settings.getDeviceId()},
                {"deviceName", settings.getDeviceName()},
#ifdef _WIN32
                {"platform", "windows"},
#elif __APPLE__
                {"platform", "macos"},
#elif __linux__
                {"platform", "linux"},
#else
                {"platform", "unknown"},
#endif
            }}
        };
        sendResponse(response, requestId);

    } catch (const std::exception& e) {
        sendError(std::string("Get device info error: ") + e.what(), requestId);
        Logger::error("Get device info exception: {}", e.what());
    }
}

void IpcServer::handleAddSyncFolder(const json& message, int requestId) {
    try {
        if (!message.contains("payload")) {
            sendError("Missing payload");
            return;
        }
        
        auto payload = message["payload"];
        std::string localPath = payload.value("local_path", "");
        std::string remotePath = payload.value("remote_path", "");
        
        if (localPath.empty() || remotePath.empty()) {
            sendError("local_path and remote_path required");
            return;
        }
        
        // Add folder via sync engine
        SyncFolder folder;
        folder.id = ""; // Will be generated by engine
        folder.localPath = localPath;
        folder.remotePath = remotePath;
        folder.enabled = true;
        folder.status = SyncStatus::IDLE;
        
        bool success = engine_->addSyncFolder(folder);
        
        if (success) {
            json response = {
                {"type", "sync_folder_added"},
                {"success", true},
                {"folder_id", folder.id}
            };
            sendResponse(response);
        } else {
            sendError("Failed to add sync folder");
        }
        
    } catch (const std::exception& e) {
        Logger::error("handleAddSyncFolder error: {}", e.what());
        sendError(e.what());
    }
}

void IpcServer::handleRemoveSyncFolder(const json& message, int requestId) {
    try {
        if (!message.contains("payload") || !message["payload"].contains("folder_id")) {
            sendError("Missing folder_id");
            return;
        }
        
        std::string folderId = message["payload"]["folder_id"];
        bool success = engine_->removeSyncFolder(folderId);
        
        json response = {
            {"type", "sync_folder_removed"},
            {"success", success},
            {"folder_id", folderId}
        };
        sendResponse(response, requestId);
        
    } catch (const std::exception& e) {
        Logger::error("handleRemoveSyncFolder error: {}", e.what());
        sendError(e.what(), requestId);
    }
}

void IpcServer::handlePauseSync(const json& message, int requestId) {
    try {
        if (!message.contains("payload") || !message["payload"].contains("folder_id")) {
            sendError("Missing folder_id");
            return;
        }
        
        std::string folderId = message["payload"]["folder_id"];
        engine_->pauseSync(folderId);
        
        json response = {
            {"type", "sync_paused"},
            {"folder_id", folderId}
        };
        sendResponse(response, requestId);
        
    } catch (const std::exception& e) {
        Logger::error("handlePauseSync error: {}", e.what());
        sendError(e.what(), requestId);
    }
}

void IpcServer::handleResumeSync(const json& message, int requestId) {
    try {
        if (!message.contains("payload") || !message["payload"].contains("folder_id")) {
            sendError("Missing folder_id");
            return;
        }
        
        std::string folderId = message["payload"]["folder_id"];
        engine_->resumeSync(folderId);
        
        json response = {
            {"type", "sync_resumed"},
            {"folder_id", folderId}
        };
        sendResponse(response, requestId);
        
    } catch (const std::exception& e) {
        Logger::error("handleResumeSync error: {}", e.what());
        sendError(e.what(), requestId);
    }
}

void IpcServer::handleGetSyncState(int requestId) {
    try {
        auto state = engine_->getSyncState();
        
        std::string status_str = "idle";
        if (state.status == SyncStatus::SYNCING) status_str = "syncing";
        else if (state.status == SyncStatus::PAUSED) status_str = "paused";
        else if (state.status == SyncStatus::SYNC_ERROR) status_str = "error";
        
        // Build a consistent response structure matching other endpoints
        auto folders = engine_->getSyncFolders();
        json data = {
            {"status", status_str},
            {"uploadSpeed", state.uploadSpeed},
            {"downloadSpeed", state.downloadSpeed},
            {"pendingUploads", state.pendingUploads},
            {"pendingDownloads", state.pendingDownloads},
            {"lastSync", state.lastSync},
            {"syncFolderCount", static_cast<int>(folders.size())}
        };

        json response = {
            {"type", "sync_state"},
            {"success", true},
            {"data", data}
        };

        sendResponse(response, requestId);
        
    } catch (const std::exception& e) {
        Logger::error("handleGetSyncState error: {}", e.what());
        sendError(e.what(), requestId);
    }
}

void IpcServer::handleGetFolders(int requestId) {
    try {
        auto folders = engine_->getSyncFolders();
        
        json folderArray = json::array();
        for (const auto& folder : folders) {
            std::string status_str = "idle";
            if (folder.status == SyncStatus::SYNCING) status_str = "syncing";
            else if (folder.status == SyncStatus::PAUSED) status_str = "paused";
            else if (folder.status == SyncStatus::SYNC_ERROR) status_str = "error";
            
            json folderJson = {
                {"id", folder.id},
                {"local_path", folder.localPath},
                {"remote_path", folder.remotePath},
                {"status", status_str},
                {"enabled", folder.enabled},
                {"size", folder.size}
            };
            folderArray.push_back(folderJson);
        }
        
        json response = {
            {"type", "folders_list"},
            {"folders", folderArray}
        };
        sendResponse(response, requestId);
        
    } catch (const std::exception& e) {
        Logger::error("handleGetFolders error: {}", e.what());
        sendError(e.what(), requestId);
    }
}

void IpcServer::sendResponse(const json& response, int requestId) {
    json output = response;
    if (requestId >= 0) {
        output["id"] = requestId;
    }
    std::cout << output.dump() << std::endl;
    std::cout.flush();
}

void IpcServer::sendError(const std::string& error, int requestId) {
    json response = {
        {"type", "error"},
        {"message", error},
        {"error", error},
        {"success", false}
    };
    if (requestId >= 0) {
        response["id"] = requestId;
    }
    std::cout << response.dump() << std::endl;
    std::cout.flush();
}

void IpcServer::broadcastEvent(const std::string& eventType, const json& data) {
    json event = {
        {"type", eventType},
        {"data", data}
    };
    sendResponse(event);
}

void IpcServer::handleGetSystemInfo(int requestId) {
    try {
        // Collect system information
        SystemInfo sysInfo = SystemInfoCollector::getSystemInfo();
        
        // Convert to JSON response
        json response = {
            {"type", "system_info"},
            {"success", true},
            {"data", SystemInfoCollector::toJson(sysInfo)}
        };
        
        sendResponse(response, requestId);
        Logger::debug("System info sent to frontend");
        
    } catch (const std::exception& e) {
        sendError(std::string("Failed to get system info: ") + e.what(), requestId);
        Logger::error("Error in handleGetSystemInfo: {}", e.what());
    }
}

} // namespace baludesk
