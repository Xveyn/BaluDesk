# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**BaluDesk** is a cross-platform desktop synchronization client for BaluHost NAS, providing seamless background file synchronization with a modern, intuitive GUI.

### Architecture

```
┌──────────────────────────────────────────┐
│       Electron Frontend (React)          │
│  • User Interface                        │
│  • System Tray Integration               │
│  • Settings Management                   │
└──────────────┬───────────────────────────┘
               │ IPC (JSON Messages)
┌──────────────┴───────────────────────────┐
│      C++ Backend (Sync Engine)           │
│  • Filesystem Watcher                    │
│  • HTTP Client (libcurl)                 │
│  • SQLite Database                       │
│  • Conflict Resolution                   │
└──────────────┬───────────────────────────┘
               │ REST API
┌──────────────┴───────────────────────────┐
│      BaluHost NAS (FastAPI)              │
│  • File Storage                          │
│  • User Management                       │
│  • Sync Endpoints                        │
└──────────────────────────────────────────┘
```

### Components

- **Backend**: C++ sync engine located in `backend/`
- **Frontend**: Electron + React + TypeScript UI located in `frontend/`
- **vcpkg**: C++ package manager (submodule) for dependency management

**Current Status**: Active development, production release planned

---

## Directory Structure

```
.
├── backend/               # C++ Sync Engine
│   ├── src/
│   │   ├── api/          # HTTP client (libcurl)
│   │   ├── db/           # SQLite database layer
│   │   ├── ipc/          # IPC server (JSON over stdin/stdout)
│   │   ├── services/     # SSH/VPN services
│   │   ├── sync/         # Sync engine & file watcher
│   │   └── utils/        # Config, logging, credentials
│   ├── tests/            # Google Test unit tests
│   └── CMakeLists.txt    # CMake build configuration
│
├── frontend/             # Electron Frontend
│   ├── src/
│   │   ├── main/        # Electron main process
│   │   ├── renderer/    # React UI
│   │   │   ├── components/
│   │   │   ├── pages/
│   │   │   └── hooks/
│   │   └── lib/         # Shared utilities
│   └── package.json
│
├── docs/                 # Technical documentation
│   ├── build-and-export.md
│   ├── conflict-resolution.md
│   ├── electron-integration.md
│   ├── frontend-backend-integration.md
│   ├── local-direct-access.md
│   ├── network-resilience.md
│   ├── remote-server-implementation.md
│   ├── remote-server-start.md
│   ├── settings-panel.md
│   └── ssh-vpn-services.md
│
├── vcpkg/               # C++ package manager (submodule)
├── LICENSE              # MIT License
└── README.md            # Project documentation
```

---

## Common Development Commands

### Quick Start

```bash
# Start both backend and frontend
python start.py

# Or start components separately:
python start.py --backend   # Only C++ Backend
python start.py --frontend  # Only Electron Frontend
```

### C++ Backend

```bash
cd backend

# Configure with vcpkg (first time only)
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[path-to-vcpkg]/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build . --config Release

# Run tests
ctest --output-on-failure

# Run backend directly
./baludesk_backend
```

**Dependencies** (managed via vcpkg):
- libcurl 8.5+ (HTTP client)
- SQLite 3.40+ (local database)
- nlohmann/json 3.11+ (JSON parsing)
- spdlog 1.12+ (logging)
- Google Test 1.14+ (testing)

### Electron Frontend

```bash
cd frontend

# Install dependencies
npm install

# Development mode (with hot reload)
npm run dev

# Build for production
npm run build

# Create installers
npm run package  # Creates installers in dist-electron/
```

**Tech Stack**:
- Electron 28
- React 18 + TypeScript 5
- Vite 5 (build tool)
- Tailwind CSS 3 (styling)
- Zustand 4 (state management)
- Electron Forge 7 (packaging)

---

## Code Standards

### C++ Backend

- **Modern C++17** required
- **CMake 3.20+** for build system
- **Google Test** for unit testing
- **spdlog** for structured logging
- **Platform-agnostic** code with platform-specific implementations in separate files

**Naming conventions**:
- Classes: `PascalCase` (e.g., `SyncEngine`, `FileWatcher`)
- Functions: `snake_case` (e.g., `start_sync()`, `handle_file_change()`)
- Constants: `UPPER_SNAKE_CASE` (e.g., `MAX_RETRIES`)

**Example**:
```cpp
#include "sync_engine.h"
#include <spdlog/spdlog.h>

class SyncEngine {
public:
    SyncEngine(const std::string& config_path);

    bool start_sync();
    void stop_sync();

private:
    std::unique_ptr<FileWatcher> watcher_;
    std::shared_ptr<spdlog::logger> logger_;
};
```

### TypeScript Frontend

- **Functional components** with hooks (no class components)
- **TypeScript strict mode** enabled
- **Tailwind CSS** for all styling
- **IPC communication** via `ipcRenderer` with type-safe messages
- **Error handling** with user-friendly toast notifications

**Example component**:
```typescript
interface SyncFolderProps {
  folder: SyncFolder;
  onRemove: (id: string) => Promise<void>;
}

export const SyncFolderCard: React.FC<SyncFolderProps> = ({ folder, onRemove }) => {
  const [isRemoving, setIsRemoving] = useState(false);

  const handleRemove = async () => {
    setIsRemoving(true);
    try {
      await onRemove(folder.id);
      toast.success('Folder removed');
    } catch (error) {
      toast.error('Failed to remove folder');
    } finally {
      setIsRemoving(false);
    }
  };

  return (
    <div className="p-4 border rounded-lg">
      {/* Component UI */}
    </div>
  );
};
```

---

## Key Features

### 1. File Synchronization
- **Bidirectional sync** between local folders and BaluHost NAS
- **Real-time file watching** (inotify on Linux, FSEvents on macOS, ReadDirectoryChangesW on Windows)
- **Change detection** using file hashes (SHA-256)
- **Conflict resolution** strategies (server-wins, client-wins, ask-user)

### 2. Authentication & Security
- **Credential storage** using OS keychain:
  - Windows: Credential Manager
  - macOS: Keychain
  - Linux: libsecret
- **HTTPS only** with TLS 1.2+ certificate validation
- **JWT token** authentication with BaluHost backend

### 3. Remote Server Profiles
- **Multi-server support** with saved profiles
- **Auto-discovery** via mDNS/Bonjour
- **VPN integration** for remote access
- **SSH tunneling** support

### 4. UI Features
- **System tray integration** with status indicators
- **Desktop notifications** for sync events
- **Activity log** with real-time updates
- **Settings panel** with bandwidth limits, auto-start, etc.

---

## IPC Communication

The frontend (Electron) communicates with the C++ backend via **JSON messages over stdin/stdout**.

### Message Format

```typescript
// Request from Frontend
{
  "id": "unique-request-id",
  "method": "start_sync",
  "params": {
    "folder_id": "abc123"
  }
}

// Response from Backend
{
  "id": "unique-request-id",
  "result": {
    "success": true,
    "message": "Sync started"
  }
}

// Event from Backend (no id)
{
  "event": "file_changed",
  "data": {
    "path": "/path/to/file.txt",
    "action": "modified"
  }
}
```

### Available Methods

**Sync Operations**:
- `start_sync` - Start synchronization for a folder
- `stop_sync` - Stop synchronization
- `pause_sync` - Pause synchronization temporarily
- `get_sync_status` - Get current sync status

**Folder Management**:
- `add_folder` - Add a new sync folder
- `remove_folder` - Remove a sync folder
- `list_folders` - List all configured folders

**Server Management**:
- `add_server` - Add a BaluHost server profile
- `remove_server` - Remove a server profile
- `list_servers` - List all server profiles
- `test_connection` - Test connection to a server

---

## Database Schema

**SQLite database**: `baludesk.db`

### Tables

**sync_folders**:
- `id` (TEXT, PRIMARY KEY)
- `name` (TEXT)
- `local_path` (TEXT)
- `remote_path` (TEXT)
- `server_id` (TEXT, FOREIGN KEY)
- `enabled` (INTEGER)
- `last_sync` (TEXT)

**servers**:
- `id` (TEXT, PRIMARY KEY)
- `name` (TEXT)
- `url` (TEXT)
- `username` (TEXT)
- `created_at` (TEXT)

**file_metadata**:
- `id` (TEXT, PRIMARY KEY)
- `folder_id` (TEXT, FOREIGN KEY)
- `path` (TEXT)
- `hash` (TEXT)
- `size` (INTEGER)
- `modified_time` (TEXT)
- `sync_status` (TEXT)

**conflicts**:
- `id` (TEXT, PRIMARY KEY)
- `folder_id` (TEXT, FOREIGN KEY)
- `path` (TEXT)
- `local_hash` (TEXT)
- `remote_hash` (TEXT)
- `detected_at` (TEXT)
- `resolution` (TEXT)

---

## Testing Strategy

### C++ Backend Tests

Located in `backend/tests/`, using **Google Test**:

- `sync_engine_test.cpp` - Sync engine core logic
- `file_watcher_test.cpp` - File system monitoring
- `database_test.cpp` - SQLite operations
- `conflict_resolver_test.cpp` - Conflict resolution
- `credential_store_test.cpp` - Credential storage
- `http_client_test.cpp` - API communication

**Run tests**:
```bash
cd backend/build
ctest --output-on-failure -V
```

### Frontend Tests

- **Unit tests**: Vitest (configured)
- **E2E tests**: Playwright (planned)
- **Component tests**: React Testing Library (planned)

---

## Platform-Specific Code

### File Watcher Implementations

- **Windows**: `file_watcher_windows.cpp` (ReadDirectoryChangesW API)
- **macOS**: `file_watcher_macos.cpp` (FSEvents API)
- **Linux**: `file_watcher_linux.cpp` (inotify API)

The `FileWatcher` class provides a unified interface:

```cpp
class FileWatcher {
public:
    virtual void start(const std::string& path) = 0;
    virtual void stop() = 0;
    virtual ~FileWatcher() = default;
};

// Platform-specific implementations
#ifdef _WIN32
    using PlatformFileWatcher = FileWatcherWindows;
#elif __APPLE__
    using PlatformFileWatcher = FileWatcherMacOS;
#elif __linux__
    using PlatformFileWatcher = FileWatcherLinux;
#endif
```

---

## Build & Packaging

### Windows

```bash
# Build
cd backend/build
cmake --build . --config Release

# Create installer
cd ../../frontend
npm run package
# Output: dist-electron/BaluDesk-Setup-x.x.x.exe
```

### macOS

```bash
# Build
cd backend/build
cmake --build . --config Release

# Create DMG
cd ../../frontend
npm run package
# Output: dist-electron/BaluDesk-x.x.x.dmg
```

### Linux

```bash
# Build
cd backend/build
cmake --build . --config Release

# Create AppImage/DEB/RPM
cd ../../frontend
npm run package
# Output: dist-electron/baludesk_x.x.x_amd64.deb
```

---

## Development Tips

1. **Use `start.py`** for combined frontend/backend development
2. **Check backend logs** in `backend/baludesk.log`
3. **Check frontend DevTools** (Ctrl+Shift+I in Electron app)
4. **Test IPC messages** using the built-in DevTools console
5. **Mock BaluHost API** using the test server in `backend/tests/mock_server.py` (if available)

---

## Common Issues & Solutions

**Backend won't compile**:
- Ensure vcpkg is initialized: `git submodule update --init --recursive`
- Install dependencies: `vcpkg install curl sqlite3 nlohmann-json spdlog gtest`

**Frontend can't communicate with backend**:
- Check if backend process is running
- Verify IPC messages in logs
- Ensure backend is in PATH when running via Electron

**File watcher not detecting changes**:
- Check file system permissions
- Verify platform-specific implementation is compiled
- Increase inotify watch limit on Linux: `echo 524288 | sudo tee /proc/sys/fs/inotify/max_user_watches`

**Credential storage fails**:
- Windows: Ensure Credential Manager service is running
- macOS: Grant Keychain access in System Preferences
- Linux: Install libsecret: `sudo apt install libsecret-1-dev`

---

## Git Workflow

- **Main branch**: `main` (stable releases)
- **Development branch**: `develop` (active development)
- Features branch off from `develop`, PRs merge to `main`

---

## Documentation

- `README.md` - Project overview, installation, usage
- `ARCHITECTURE.md` - Detailed architecture documentation
- `TODO.md` - Development roadmap and planned features
- `docs/` - Technical implementation docs (network resilience, conflict resolution, etc.)
- `backend/BUILD_GUIDE.md` - Detailed C++ build instructions
- `frontend/README.md` - Frontend-specific documentation

---

## Quick Reference: Finding Things

**Sync engine core**: `backend/src/sync/sync_engine.cpp`
**File watcher**: `backend/src/sync/file_watcher*.cpp`
**HTTP client**: `backend/src/api/http_client.cpp`
**IPC server**: `backend/src/ipc/ipc_server.cpp`
**Database layer**: `backend/src/db/database.cpp`
**Credential storage**: `backend/src/utils/credential_store.cpp`

**Frontend IPC client**: `frontend/src/renderer/lib/ipc-client.ts`
**Dashboard page**: `frontend/src/renderer/pages/Dashboard.tsx`
**Settings panel**: `frontend/src/renderer/components/SettingsPanel.tsx`
**Sync folder management**: `frontend/src/renderer/pages/Sync.tsx`

---

## Contact & Support

- **GitHub**: https://github.com/Xveyn/BaluDesk
- **Parent Project**: https://github.com/Xveyn/BaluHost
- **Maintainer**: Xveyn
- **License**: MIT

---

## Related Projects

- **BaluHost**: Main NAS backend and web UI - https://github.com/Xveyn/BaluHost
- **BaluApp**: Android mobile app - https://github.com/Xveyn/BaluApp
