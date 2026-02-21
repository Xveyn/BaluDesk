// ChangeDetector implementation with DB comparison for proper change detection
#include "change_detector.h"
#include "../db/database.h"
#include "../utils/sha256.h"
#include "../utils/logger.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <unordered_set>

namespace fs = std::filesystem;

namespace baludesk {

// Directories and file patterns to exclude from sync
static const std::vector<std::string> EXCLUDED_DIRS = {
    ".venv", "venv", ".env", "env",
    "node_modules",
    ".git",
    "__pycache__", ".pytest_cache", ".mypy_cache",
    ".tox",
    ".idea", ".vscode",
    "build", "dist", "out",
    ".next", ".nuxt",
    "target",          // Rust/Java
    "Thumbs.db",
    ".DS_Store",
};

static const std::vector<std::string> EXCLUDED_EXTENSIONS = {
    ".pyc", ".pyo",
    ".o", ".obj", ".a", ".lib", ".dll", ".so", ".dylib",
    ".exe",
    ".tmp", ".temp", ".swp", ".swo",
};

// Check if a path component matches any excluded directory name
static bool shouldExcludePath(const std::string& relativePath) {
    // Check directory components
    std::string path = relativePath;
    std::replace(path.begin(), path.end(), '\\', '/');

    for (const auto& excluded : EXCLUDED_DIRS) {
        // Match as path component: "/excluded/" or starts with "excluded/"
        std::string pattern1 = "/" + excluded + "/";
        std::string pattern2 = excluded + "/";
        if (path.find(pattern1) != std::string::npos ||
            path.substr(0, pattern2.size()) == pattern2 ||
            path == excluded) {
            return true;
        }
    }

    // Check file extensions
    auto dotPos = path.rfind('.');
    if (dotPos != std::string::npos) {
        std::string ext = path.substr(dotPos);
        for (const auto& excluded : EXCLUDED_EXTENSIONS) {
            if (ext == excluded) return true;
        }
    }

    return false;
}

ChangeDetector::ChangeDetector(Database* db, HttpClient* httpClient)
    : db_(db), httpClient_(httpClient) {
    Logger::info("ChangeDetector initialized");
}

ChangeDetector::~ChangeDetector() {
    Logger::info("ChangeDetector destroyed");
}

std::vector<DetectedChange> ChangeDetector::detectRemoteChanges(
    const std::string& /*syncFolderId*/,
    const std::chrono::system_clock::time_point& /*since*/
) {
    // Remote changes are now handled via Delta-Sync protocol (performDeltaSync)
    return {};
}

std::vector<DetectedChange> ChangeDetector::detectLocalChanges(
    const std::string& syncFolderId,
    const std::string& localPath
) {
    std::vector<DetectedChange> changes;
    try {
        if (!fs::exists(localPath)) return changes;

        // Load known files from DB for comparison
        std::unordered_map<std::string, FileMetadata> knownFiles;
        if (db_) {
            auto dbFiles = db_->getFilesInFolder(syncFolderId);
            for (auto& f : dbFiles) {
                knownFiles[f.path] = std::move(f);
            }
        }

        // Track which DB entries we've seen on disk
        std::unordered_set<std::string> seenPaths;

        // Scan local filesystem (skip excluded directories entirely)
        size_t scannedFiles = 0;
        for (auto it = fs::recursive_directory_iterator(localPath); it != fs::recursive_directory_iterator(); ) {
            if (it->is_directory()) {
                std::string dirName = it->path().filename().string();
                bool skip = false;
                for (const auto& excluded : EXCLUDED_DIRS) {
                    if (dirName == excluded) { skip = true; break; }
                }
                if (skip) {
                    it.disable_recursion_pending();
                    ++it;
                    continue;
                }
                ++it;
                continue;
            }
            if (!it->is_regular_file()) { ++it; continue; }
            const auto& entry = *it;

            std::string fullPath = entry.path().string();
            std::string relativePath = fullPath.substr(localPath.length());
            std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
            if (!relativePath.empty() && relativePath[0] == '/') relativePath = relativePath.substr(1);

            // Skip excluded paths (.venv, node_modules, __pycache__, etc.)
            if (shouldExcludePath(relativePath)) { ++it; continue; }

            seenPaths.insert(relativePath);

            size_t fileSize = fs::file_size(entry.path());

            // Compare against DB
            auto knownIt = knownFiles.find(relativePath);
            if (knownIt == knownFiles.end()) {
                // File not in DB -> CREATED (skip hash — not needed for detection)
                DetectedChange change;
                change.path = relativePath;
                change.type = ChangeType::CREATED;
                change.timestamp = std::chrono::system_clock::now();
                change.hash = std::nullopt;
                change.size = fileSize;
                change.isRemote = false;
                changes.push_back(change);
            } else {
                // File in DB -> hash needed to detect MODIFIED
                std::string hash = calculateFileHash(fullPath);
                if (knownIt->second.checksum != hash) {
                    DetectedChange change;
                    change.path = relativePath;
                    change.type = ChangeType::MODIFIED;
                    change.timestamp = std::chrono::system_clock::now();
                    change.hash = hash;
                    change.size = fileSize;
                    change.isRemote = false;
                    changes.push_back(change);
                }
                // else: hash matches -> unchanged, skip
            }
            scannedFiles++;
            if (scannedFiles % 500 == 0) {
                Logger::info("Change detection: {} files scanned...", scannedFiles);
            }
            ++it;
        }

        Logger::info("Change detection complete: {} files scanned, {} changes found",
                     scannedFiles, changes.size());

        // Check for deleted files (in DB but not on disk)
        for (const auto& [path, metadata] : knownFiles) {
            if (seenPaths.find(path) == seenPaths.end()) {
                DetectedChange change;
                change.path = path;
                change.type = ChangeType::DELETED;
                change.timestamp = std::chrono::system_clock::now();
                change.hash = std::nullopt;
                change.size = 0;
                change.isRemote = false;
                changes.push_back(change);
            }
        }

    } catch (const std::exception& e) {
        Logger::error("Failed to detect local changes: " + std::string(e.what()));
    }
    return changes;
}

std::vector<ConflictInfo> ChangeDetector::detectConflicts(
    const std::vector<DetectedChange>& localChanges,
    const std::vector<DetectedChange>& remoteChanges
) {
    std::vector<ConflictInfo> conflicts;

    // Build a map of remote changes by path
    std::unordered_map<std::string, const DetectedChange*> remoteMap;
    for (const auto& rc : remoteChanges) {
        remoteMap[rc.path] = &rc;
    }

    // Find overlapping paths
    for (const auto& lc : localChanges) {
        auto it = remoteMap.find(lc.path);
        if (it != remoteMap.end()) {
            const auto& rc = *it->second;
            // Both sides changed the same file -> conflict
            // (unless both deleted it)
            if (lc.type == ChangeType::DELETED && rc.type == ChangeType::DELETED) {
                continue;
            }
            ConflictInfo conflict;
            conflict.path = lc.path;
            conflict.localTimestamp = lc.timestamp;
            conflict.remoteTimestamp = rc.timestamp;
            conflict.localHash = lc.hash.value_or("");
            conflict.remoteHash = rc.hash.value_or("");
            conflicts.push_back(conflict);
        }
    }

    return conflicts;
}

bool ChangeDetector::hasFileChanged(
    const std::string& path,
    const std::chrono::system_clock::time_point& /*timestamp*/,
    const std::string& hash
) {
    if (!db_) return true;

    auto metadata = db_->getFileMetadata(path);
    if (!metadata) return true;  // Not in DB -> changed

    return metadata->checksum != hash;
}

std::string ChangeDetector::calculateFileHash(const std::string& filePath) {
    try {
        return sha256_file(filePath);
    } catch (const std::exception& e) {
        Logger::error("Failed to calculate hash: " + std::string(e.what()));
        return "";
    }
}

void ChangeDetector::scanDirectory(
    const std::string& dirPath,
    const std::string& basePath,
    std::vector<DetectedChange>& changes
) {
    for (const auto& entry : fs::recursive_directory_iterator(dirPath)) {
        if (!entry.is_regular_file()) continue;

        std::string fullPath = entry.path().string();
        std::string relativePath = fullPath.substr(basePath.length());
        std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
        if (!relativePath.empty() && relativePath[0] == '/') relativePath = relativePath.substr(1);

        // Skip excluded paths
        if (shouldExcludePath(relativePath)) continue;

        size_t size = fs::file_size(entry.path());
        std::string hash = calculateFileHash(fullPath);

        DetectedChange change;
        change.path = relativePath;
        change.type = ChangeType::CREATED;
        change.timestamp = std::chrono::system_clock::now();
        change.hash = hash;
        change.size = size;
        change.isRemote = false;
        changes.push_back(change);
    }
}

} // namespace baludesk
