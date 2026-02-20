# BaluDesk Backend - C++ Sync Engine

<div align="center">

**High-Performance Cross-Platform File Synchronization Engine**

[![C++17](https://img.shields.io/badge/C++-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![CMake](https://img.shields.io/badge/CMake-3.20+-green.svg)](https://cmake.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)]()
[![License](https://img.shields.io/badge/License-MIT-orange.svg)](LICENSE)

</div>

---

## 🚀 Quick Start

### Build & Run (Platform-Specific)

#### Windows
**Important**: On Windows, you MUST use `build.bat`, NOT `build.sh`:
```cmd
# 1. Install dependencies (see BUILD_GUIDE.md for Windows setup)
build.bat release

# 2. Run the backend
.\build\Release\baludesk-backend.exe

# 3. Run tests
.\build\Release\baludesk-tests.exe
```

#### Linux/macOS
```bash
# 1. Install dependencies (see BUILD_GUIDE.md for details)
./build.sh Release

# 2. Run the backend
./build/baludesk-backend

# 3. Run tests
./build/baludesk-tests
```

> 💡 **Tip**: See [BUILD_GUIDE.md](BUILD_GUIDE.md) for detailed platform-specific instructions.

---

## 📋 Features

### ✅ Implemented (Sprint 1 & 2)
- ✅ **HTTP Client** - libcurl wrapper with JWT token management
- ✅ **SQLite Database** - Local metadata storage with migrations
- ✅ **Logger System** - Structured logging with spdlog (rotation, levels)
- ✅ **Sync Engine Core** - Bidirectional sync foundation
- ✅ **IPC Server** - JSON-based communication with Electron frontend
- ✅ **Config Management** - JSON configuration file support
- ✅ **Unit Tests** - GoogleTest framework with fixtures

### 🚧 In Progress (Sprint 3)
- ⚙️ **Filesystem Watcher** - Cross-platform file change detection
- ⚙️ **Change Detector** - Efficient delta detection
- ⚙️ **Conflict Resolver** - Smart conflict resolution strategies

### 📅 Planned (Future Sprints)
- 📋 Resume on failure (chunked uploads with checkpoints)
- 📋 Bandwidth throttling
- 📋 Compression (zlib/gzip)
- 📋 Parallel upload/download thread pool

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    BaluDesk C++ Backend                     │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │ Sync Engine  │  │ File Watcher │  │   Conflict   │     │
│  │              │←→│              │←→│   Resolver   │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
│         ↕                  ↕                  ↕            │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │  HTTP Client │  │   Database   │  │   IPC Server │     │
│  │   (libcurl)  │  │   (SQLite)   │  │    (stdio)   │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
│         ↕                                     ↕            │
│  ┌──────────────────────────────────────────────────┐      │
│  │            Utilities (Logger, Config)            │      │
│  └──────────────────────────────────────────────────┘      │
└─────────────────────────────────────────────────────────────┘
         ↓                                     ↑
   REST API (HTTPS)                     JSON Messages
         ↓                                     ↑
┌─────────────────┐              ┌──────────────────────┐
│  BaluHost NAS   │              │ Electron Frontend    │
│  (FastAPI)      │              │ (React + TypeScript) │
└─────────────────┘              └──────────────────────┘
```

---

## 🛠️ Technology Stack

| Component | Technology | Purpose |
|-----------|-----------|---------|
| **Language** | C++17 | High performance, cross-platform |
| **Build System** | CMake 3.20+ | Cross-platform build configuration |
| **HTTP Client** | libcurl 8.5+ | HTTPS communication with NAS |
| **Database** | SQLite3 3.40+ | Local metadata & sync state |
| **JSON** | nlohmann/json 3.11+ | API communication |
| **Logging** | spdlog 1.12+ | Structured logging with rotation |
| **Testing** | GoogleTest 1.14+ | Unit & integration tests |
| **Threading** | std::thread (C++11) | Async sync operations |

---

## 📁 Project Structure

```
backend/
├── CMakeLists.txt              # Build configuration
├── build.sh / build.bat        # Build scripts
├── README.md                   # This file
├── BUILD_GUIDE.md              # Detailed build instructions
├── BEST_PRACTICES.md           # C++ coding guidelines
│
├── src/                        # Source code
│   ├── main.cpp               # Entry point
│   ├── api/                   # HTTP client & auth
│   │   ├── http_client.h/cpp
│   │   └── ...
│   ├── db/                    # SQLite database layer
│   │   ├── database.h/cpp
│   │   └── ...
│   ├── sync/                  # Sync engine components
│   │   ├── sync_engine.h/cpp
│   │   ├── file_watcher.h/cpp
│   │   ├── conflict_resolver.h/cpp
│   │   └── ...
│   ├── ipc/                   # IPC communication
│   │   ├── ipc_server.h/cpp
│   │   └── ...
│   └── utils/                 # Utilities (logger, config)
│       ├── logger.h/cpp
│       ├── config.h/cpp
│       └── ...
│
├── tests/                      # Unit tests
│   ├── logger_test.cpp
│   ├── database_test.cpp
│   └── ...
│
└── build/                      # Build artifacts (generated)
    ├── baludesk-backend       # Main executable
    └── baludesk-tests         # Test executable
```

---

## 🧪 Testing

### Run All Tests
```bash
./build/baludesk-tests
```

### Run Specific Test Suite
```bash
./build/baludesk-tests --gtest_filter=DatabaseTest.*
./build/baludesk-tests --gtest_filter=LoggerTest.*
```

### Code Coverage (Linux/macOS)
```bash
cmake .. -DCMAKE_CXX_FLAGS="--coverage"
make
./baludesk-tests
gcov -r src/*.cpp
```

---

## 🐛 Debugging

### GDB (Linux/macOS)
```bash
./build.sh Debug
gdb ./build/baludesk-backend
```

### Visual Studio Debugger (Windows)
1. Open `build/BaluDeskBackend.sln`
2. Set `baludesk-backend` as startup project
3. Press F5

### Memory Leak Detection
```bash
valgrind --leak-check=full ./build/baludesk-backend
```

---

## 📊 Performance

### Current Benchmarks (Release Build)
- **File Metadata Lookup**: ~0.5ms (SQLite indexed)
- **Checksum Calculation**: ~150 MB/s (SHA256)
- **HTTP Upload**: ~50 MB/s (local network)
- **Sync Cycle**: ~2s for 1000 files

### Optimization Tips
- Enable Link-Time Optimization: `-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON`
- Profile-Guided Optimization (PGO)
- Compile with `-march=native`

---

## 🔒 Security

### Implemented
✅ JWT token management with automatic refresh  
✅ HTTPS-only communication (TLS 1.2+)  
✅ SQL injection prevention (prepared statements)  
✅ No sensitive data in logs  

### Planned
🔜 Token storage in OS keychain  
🔜 Certificate pinning (optional)  
🔜 Memory protection for credentials  

---

## 📚 Documentation

| Document | Purpose |
|----------|---------|
| [BUILD_GUIDE.md](BUILD_GUIDE.md) | Detailed build instructions for all platforms |
| [BEST_PRACTICES.md](BEST_PRACTICES.md) | C++17 coding standards & patterns |
| [ARCHITECTURE.md](../ARCHITECTURE.md) | High-level system design |
| [TODO.md](../TODO.md) | Roadmap & feature tracking |

---

## 🤝 Contributing

### Code Style
- **Formatting**: ClangFormat (Google Style)
- **Naming**: `camelCase` for variables, `PascalCase` for classes
- **Namespaces**: `baludesk::<module>`

### Pull Request Checklist
- [ ] Code compiles without warnings (`-Wall -Wextra -Werror`)
- [ ] All tests pass (`./build/baludesk-tests`)
- [ ] New features have unit tests
- [ ] Documentation updated
- [ ] No memory leaks (valgrind clean)

---

## 📝 License

MIT License - See [LICENSE](../LICENSE) for details

---

## 🔗 Related Projects

- [BaluHost Backend (FastAPI)](../../backend/) - Python NAS backend
- [BaluHost Frontend (React)](../../client/) - Web UI
- [BaluDesk Frontend (Electron)](../frontend/) - Desktop UI

---

**Built with ❤️ by the BaluHost Team**  
**Last Updated:** January 4, 2026
