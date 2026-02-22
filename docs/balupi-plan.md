# BaluPi — Technischer Implementierungsplan

> **Raspberry Pi 3B+ als Smart-Cache-Tier für das BaluHost-Ökosystem**
> Stand: Februar 2026

---

## 1. Überblick

BaluPi ist ein Always-On Raspberry Pi 3B+, der zwei Hauptaufgaben erfüllt:

1. **Smart Cache**: Mobile-First-Sync-Endpunkt mit 80/20-Zugriffsmuster-Optimierung
2. **Energy Monitoring**: 24/7 Strommessung des NAS (und weiterer Geräte) über Tapo Smart Plugs

Der Pi läuft dauerhaft (~ 4W), während der NAS (~ 50–120W) nur bei Bedarf aufgewacht wird.

### Ökosystem-Einordnung

```
BaluApp (Android) ──────┐
                        ▼
                   ┌─────────┐    WOL / REST     ┌──────────┐
                   │ BaluPi  │ ◄───────────────► │ BaluHost │
                   │ (Cache) │    Sync & Energy   │  (NAS)   │
                   └─────────┘                    └──────────┘
                        ▲
BaluDesk (Desktop) ─────┘
```

---

## 2. Tech Stack

| Komponente | Technologie | Begründung |
|---|---|---|
| Runtime | Python 3.11+ | Gleiche Sprache wie BaluHost, VS Code Remote SSH |
| Framework | FastAPI 0.110+ | API-kompatibel mit BaluHost, async-native |
| ORM | SQLAlchemy 2.0 + aiosqlite | Gleiche Modelle wie BaluHost, async SQLite |
| Database | SQLite 3.40+ (WAL-Modus) | Leichtgewichtig, kein DB-Server nötig |
| Validation | Pydantic v2 | Schema-Sharing mit BaluHost |
| HTTP Client | httpx | Async-fähig, für NAS-Kommunikation |
| Energy | python-kasa 0.7+ | Tapo P110/P115 Energiemessung |
| Discovery | zeroconf | mDNS-basierte NAS-Erkennung |
| Monitoring | psutil | Pi-Systemressourcen |
| Server | Uvicorn (1–2 Worker) | Ressourcenschonend für Pi |
| Frontend | Shared React App (BaluHost) | Build auf Dev-Maschine, Static Deploy |
| Task Queue | APScheduler | Lightweight Scheduler für Sync/Energy Jobs |

### pyproject.toml (Dependencies)

```toml
[project]
name = "balupi"
version = "0.1.0"
requires-python = ">=3.11"

dependencies = [
    "fastapi>=0.110.0",
    "uvicorn[standard]>=0.27.0",
    "sqlalchemy[asyncio]>=2.0.25",
    "aiosqlite>=0.19.0",
    "pydantic>=2.5.0",
    "pydantic-settings>=2.1.0",
    "httpx>=0.26.0",
    "python-kasa>=0.7.0",
    "zeroconf>=0.131.0",
    "psutil>=5.9.0",
    "apscheduler>=3.10.0",
    "python-multipart>=0.0.6",
    "aiofiles>=23.2.0",
    "python-jose[cryptography]>=3.3.0",
    "passlib[bcrypt]>=1.7.4",
]

[project.optional-dependencies]
dev = [
    "pytest>=7.4.0",
    "pytest-asyncio>=0.23.0",
    "httpx", # TestClient
    "ruff>=0.2.0",
    "mypy>=1.8.0",
]
```

---

## 3. Funktionsumfang (Scope)

### P0 — Core Foundation (Woche 1–2)
- FastAPI-Grundgerüst mit Health-Check
- SQLite-Datenbank mit WAL-Modus
- Konfigurationssystem (YAML/ENV)
- Logging (structlog)
- Systemd-Service-Setup
- Pi-Systemstatus-API (`/api/system/status`)

### P1 — Energy Monitoring (Woche 3–5) ⚡ PRIMARY
- Tapo-Geräte-Discovery und -Verwaltung
- Echtzeit-Energiemessung (30s Intervall)
- Historische Energiedaten (SQLite, komprimiert)
- Kosten-Berechnung (konfigurierbare Tarife)
- NAS-Erkennung via Tapo (Leistung > 30W → NAS läuft)
- Energy-Dashboard-API
- Alerts bei Anomalien (Überspannung, Geräteausfall)

### P2 — NAS Discovery & Handshake (Woche 5–7)
- mDNS-basierte NAS-Suche
- Dual-Detection: mDNS + Tapo-Leistungsschwelle
- Wake-on-LAN (WOL)
- Handshake-Protokoll (Version, Capabilities, Manifest)
- Connection-Health-Monitoring
- Auto-Reconnect mit Exponential Backoff

### P3 — Smart Cache & File Sync (Woche 7–11)
- 80/20 Hot-Cache-Strategie (häufigstes 20% lokal)
- Mobile-First Upload-Proxy (App → Pi → NAS)
- Bidirektionaler Delta-Sync mit NAS
- LRU-Eviction bei Speicherknappheit
- Thumbnail-Generierung für Fotos
- Chunk-basierter Upload/Download (für große Dateien)
- Offline-Queue bei NAS-Nichterreichbarkeit

### P4 — Client-Integration (Woche 11–13)
- BaluApp: Pi als bevorzugter Sync-Endpunkt
- BaluDesk: Pi-Profil in Server-Liste
- Auth-Forwarding (Pi validiert Tokens gegen NAS)
- Conflict Resolution (Server-Wins default)

### P5 — Polish & Hardening (Woche 13–14)
- Auto-Update-Mechanismus (git pull + systemd restart)
- Watchdog (systemd, Auto-Restart)
- Log-Rotation
- Backup der SQLite-DB
- Dokumentation

---

## 4. API-Kompatibilität

BaluPi implementiert eine **Teilmenge** der BaluHost-API, sodass Clients transparent wechseln können.

### Implementierte Endpunkte

| Endpunkt | BaluHost | BaluPi | Anmerkung |
|---|---|---|---|
| `GET /api/health` | ✅ | ✅ | Identisch |
| `POST /api/auth/login` | ✅ | ✅ | Forwarded an NAS oder lokaler Cache |
| `GET /api/files/list` | ✅ | ✅ | Nur gecachte Dateien + Metadaten |
| `GET /api/files/download/{id}` | ✅ | ✅ | Cache-Hit → lokal, Miss → Proxy zu NAS |
| `POST /api/files/upload` | ✅ | ✅ | Speichert lokal, queued Sync zu NAS |
| `DELETE /api/files/{id}` | ✅ | ✅ | Markiert gelöscht, propagiert zu NAS |
| `GET /api/files/sync/status` | ✅ | ✅ | Pi-spezifischer Sync-Status |
| `GET /api/files/thumbnail/{id}` | ✅ | ✅ | Lokal generiert |
| `GET /api/shares/*` | ✅ | ❌ | Zu komplex für Pi |
| `GET /api/admin/*` | ✅ | ❌ | Nur auf NAS |
| `GET /api/users/*` | ✅ | 🔶 | Nur Auth-relevante Subset |

### Pi-exklusive Endpunkte

| Endpunkt | Beschreibung |
|---|---|
| `GET /api/energy/current` | Aktuelle Leistung aller Tapo-Geräte |
| `GET /api/energy/history` | Historische Energiedaten (Tag/Woche/Monat) |
| `GET /api/energy/costs` | Kostenberechnung nach Tarif |
| `GET /api/energy/summary` | Zusammenfassung (Durchschnitt, Peak, Kosten) |
| `GET /api/tapo/devices` | Liste aller Tapo-Geräte |
| `POST /api/tapo/devices/discover` | Netzwerk-Scan nach Tapo-Geräten |
| `PUT /api/tapo/devices/{id}` | Gerät konfigurieren (Name, Rolle) |
| `POST /api/tapo/devices/{id}/toggle` | Gerät ein/ausschalten |
| `GET /api/system/status` | Pi CPU, RAM, Temp, Disk, Uptime |
| `GET /api/cache/stats` | Cache-Statistiken (Hit-Rate, Größe) |
| `POST /api/nas/wol` | Wake-on-LAN senden |
| `GET /api/nas/status` | NAS-Erreichbarkeit und Status |

---

## 5. Datenbank-Schema

```sql
-- ============================================
-- Core Tables
-- ============================================

CREATE TABLE users (
    id          TEXT PRIMARY KEY,
    username    TEXT NOT NULL UNIQUE,
    token_hash  TEXT,               -- Gecachter Auth-Token-Hash
    nas_user_id TEXT,               -- Referenz auf BaluHost User-ID
    last_auth   TEXT,               -- Letzter erfolgreicher Auth-Zeitpunkt
    created_at  TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE servers (
    id          TEXT PRIMARY KEY,
    name        TEXT NOT NULL,
    url         TEXT NOT NULL,       -- https://nas.local:8000
    mac_address TEXT,                -- Für WOL
    tapo_device_id TEXT,             -- Zugehöriger Tapo-Plug (NAS-Erkennung)
    power_threshold REAL DEFAULT 30.0, -- Watt-Schwelle für "NAS läuft"
    is_online   INTEGER DEFAULT 0,
    last_seen   TEXT,
    created_at  TEXT NOT NULL DEFAULT (datetime('now'))
);

-- ============================================
-- File & Cache Tables
-- ============================================

CREATE TABLE cached_files (
    id           TEXT PRIMARY KEY,
    nas_file_id  TEXT NOT NULL,      -- BaluHost File-ID
    server_id    TEXT NOT NULL REFERENCES servers(id),
    relative_path TEXT NOT NULL,
    filename     TEXT NOT NULL,
    mime_type    TEXT,
    size_bytes   INTEGER NOT NULL,
    hash_sha256  TEXT NOT NULL,
    is_dirty     INTEGER DEFAULT 0,  -- Lokale Änderung, noch nicht synced
    access_count INTEGER DEFAULT 0,  -- Für LRU/Hotness-Score
    last_accessed TEXT,
    cached_at    TEXT NOT NULL DEFAULT (datetime('now')),
    modified_at  TEXT NOT NULL,
    UNIQUE(server_id, relative_path)
);

CREATE TABLE upload_queue (
    id          TEXT PRIMARY KEY,
    user_id     TEXT NOT NULL REFERENCES users(id),
    server_id   TEXT NOT NULL REFERENCES servers(id),
    local_path  TEXT NOT NULL,
    remote_path TEXT,
    file_size   INTEGER NOT NULL,
    status      TEXT NOT NULL DEFAULT 'pending', -- pending, uploading, synced, failed
    retry_count INTEGER DEFAULT 0,
    error_msg   TEXT,
    created_at  TEXT NOT NULL DEFAULT (datetime('now')),
    synced_at   TEXT
);

CREATE TABLE sync_log (
    id          TEXT PRIMARY KEY,
    server_id   TEXT NOT NULL REFERENCES servers(id),
    direction   TEXT NOT NULL,       -- 'push' | 'pull'
    file_path   TEXT NOT NULL,
    action      TEXT NOT NULL,       -- 'create' | 'update' | 'delete'
    status      TEXT NOT NULL,       -- 'success' | 'failed' | 'conflict'
    bytes_transferred INTEGER DEFAULT 0,
    duration_ms INTEGER,
    error_msg   TEXT,
    created_at  TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE conflicts (
    id          TEXT PRIMARY KEY,
    server_id   TEXT NOT NULL REFERENCES servers(id),
    file_path   TEXT NOT NULL,
    local_hash  TEXT NOT NULL,
    remote_hash TEXT NOT NULL,
    resolution  TEXT,                -- 'local' | 'remote' | 'renamed' | NULL
    resolved_at TEXT,
    detected_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE file_metadata_cache (
    id           TEXT PRIMARY KEY,
    server_id    TEXT NOT NULL REFERENCES servers(id),
    nas_file_id  TEXT NOT NULL,
    relative_path TEXT NOT NULL,
    filename     TEXT NOT NULL,
    mime_type    TEXT,
    size_bytes   INTEGER,
    hash_sha256  TEXT,
    is_directory INTEGER DEFAULT 0,
    parent_path  TEXT,
    modified_at  TEXT,
    cached_at    TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE(server_id, nas_file_id)
);

-- ============================================
-- Energy Monitoring Tables
-- ============================================

CREATE TABLE tapo_devices (
    id          TEXT PRIMARY KEY,
    name        TEXT NOT NULL,        -- z.B. "NAS Plug", "Monitor Plug"
    ip_address  TEXT NOT NULL,
    mac_address TEXT,
    model       TEXT,                 -- P110, P115, etc.
    role        TEXT DEFAULT 'generic', -- 'nas', 'monitor', 'generic'
    is_online   INTEGER DEFAULT 1,
    firmware    TEXT,
    last_seen   TEXT,
    created_at  TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE energy_samples (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id   TEXT NOT NULL REFERENCES tapo_devices(id),
    power_mw    INTEGER NOT NULL,     -- Milliwatt (Genauigkeit)
    voltage_mv  INTEGER,              -- Millivolt
    current_ma  INTEGER,              -- Milliampere
    energy_wh   INTEGER,              -- Wattstunden seit letztem Reset
    timestamp   TEXT NOT NULL DEFAULT (datetime('now'))
);

-- Komprimierte historische Daten (Stunden-Aggregat)
CREATE TABLE energy_hourly (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id   TEXT NOT NULL REFERENCES tapo_devices(id),
    hour        TEXT NOT NULL,         -- '2026-02-22T14:00:00'
    avg_power_w REAL NOT NULL,
    max_power_w REAL NOT NULL,
    min_power_w REAL NOT NULL,
    energy_wh   REAL NOT NULL,
    sample_count INTEGER NOT NULL,
    UNIQUE(device_id, hour)
);

-- Tages-Aggregat
CREATE TABLE energy_daily (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id   TEXT NOT NULL REFERENCES tapo_devices(id),
    date        TEXT NOT NULL,         -- '2026-02-22'
    avg_power_w REAL NOT NULL,
    max_power_w REAL NOT NULL,
    min_power_w REAL NOT NULL,
    energy_wh   REAL NOT NULL,
    cost_cents  REAL,                  -- Berechnete Kosten
    UNIQUE(device_id, date)
);

CREATE TABLE energy_price_config (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT NOT NULL,         -- z.B. "Normaltarif", "Nachttarif"
    price_per_kwh_cents REAL NOT NULL, -- ct/kWh
    valid_from  TEXT,                  -- Uhrzeit oder Datum
    valid_to    TEXT,
    is_active   INTEGER DEFAULT 1,
    created_at  TEXT NOT NULL DEFAULT (datetime('now'))
);

-- ============================================
-- Indexes
-- ============================================

CREATE INDEX idx_cached_files_access ON cached_files(access_count DESC, last_accessed DESC);
CREATE INDEX idx_cached_files_server ON cached_files(server_id);
CREATE INDEX idx_cached_files_path ON cached_files(relative_path);
CREATE INDEX idx_upload_queue_status ON upload_queue(status);
CREATE INDEX idx_sync_log_server ON sync_log(server_id, created_at DESC);
CREATE INDEX idx_energy_samples_device ON energy_samples(device_id, timestamp DESC);
CREATE INDEX idx_energy_hourly_device ON energy_hourly(device_id, hour DESC);
CREATE INDEX idx_energy_daily_device ON energy_daily(device_id, date DESC);
CREATE INDEX idx_file_metadata_path ON file_metadata_cache(server_id, relative_path);
```

---

## 6. Verzeichnisstruktur

```
balupi/
├── backend/
│   ├── app/
│   │   ├── __init__.py
│   │   ├── main.py                  # FastAPI App-Factory
│   │   ├── config.py                # Settings (Pydantic BaseSettings)
│   │   ├── database.py              # SQLAlchemy Engine & Session
│   │   │
│   │   ├── models/                  # SQLAlchemy ORM Models
│   │   │   ├── __init__.py
│   │   │   ├── user.py
│   │   │   ├── server.py
│   │   │   ├── cached_file.py
│   │   │   ├── upload_queue.py
│   │   │   ├── sync_log.py
│   │   │   ├── conflict.py
│   │   │   ├── tapo_device.py
│   │   │   ├── energy.py
│   │   │   └── file_metadata.py
│   │   │
│   │   ├── schemas/                 # Pydantic Schemas (Request/Response)
│   │   │   ├── __init__.py
│   │   │   ├── auth.py
│   │   │   ├── files.py
│   │   │   ├── energy.py
│   │   │   ├── tapo.py
│   │   │   ├── system.py
│   │   │   ├── cache.py
│   │   │   └── sync.py
│   │   │
│   │   ├── api/                     # API Router
│   │   │   ├── __init__.py
│   │   │   ├── deps.py              # Dependency Injection
│   │   │   └── routes/
│   │   │       ├── __init__.py
│   │   │       ├── health.py
│   │   │       ├── auth.py
│   │   │       ├── files.py
│   │   │       ├── energy.py
│   │   │       ├── tapo.py
│   │   │       ├── system.py
│   │   │       ├── cache.py
│   │   │       ├── nas.py
│   │   │       └── sync.py
│   │   │
│   │   ├── services/                # Business Logic
│   │   │   ├── __init__.py
│   │   │   ├── auth_service.py      # Token-Validierung (lokal + NAS)
│   │   │   ├── cache_service.py     # LRU-Cache, Eviction, Hotness
│   │   │   ├── sync_service.py      # Bidirektionaler Sync mit NAS
│   │   │   ├── energy_service.py    # Messung, Aggregation, Kosten
│   │   │   ├── tapo_service.py      # Tapo-Geräte-Verwaltung
│   │   │   ├── nas_service.py       # NAS-Discovery, WOL, Handshake
│   │   │   ├── upload_service.py    # Upload-Queue-Processing
│   │   │   ├── thumbnail_service.py # Pillow-basierte Thumbnails
│   │   │   └── scheduler.py         # APScheduler Jobs (Energy, Sync)
│   │   │
│   │   └── utils/
│   │       ├── __init__.py
│   │       ├── hashing.py           # SHA-256 File Hashing
│   │       ├── wol.py               # Wake-on-LAN Implementierung
│   │       └── storage.py           # Disk-Space-Management
│   │
│   ├── tests/
│   │   ├── __init__.py
│   │   ├── conftest.py              # Fixtures (Test-DB, Client)
│   │   ├── test_energy.py
│   │   ├── test_cache.py
│   │   ├── test_sync.py
│   │   ├── test_tapo.py
│   │   ├── test_nas.py
│   │   └── test_auth.py
│   │
│   ├── alembic/                     # DB-Migrationen (optional)
│   │   ├── env.py
│   │   └── versions/
│   │
│   ├── pyproject.toml
│   └── alembic.ini
│
├── frontend/                        # Shared React App (Build-Artifact)
│   ├── dist/                        # Vite Build Output (static files)
│   └── README.md
│
├── deploy/
│   ├── balupi.service               # systemd Service
│   ├── balupi-energy.timer          # systemd Timer für Energy-Komprimierung
│   ├── install.sh                   # One-Click-Setup auf Pi
│   ├── update.sh                    # Auto-Update-Script
│   └── nginx.conf                   # Optional: Reverse Proxy
│
├── data/                            # Runtime-Daten (nicht in Git)
│   ├── balupi.db                    # SQLite Database
│   ├── cache/                       # Gecachte Dateien
│   │   ├── files/                   # Vollständige Dateien
│   │   └── thumbnails/              # Generierte Thumbnails
│   └── logs/
│       └── balupi.log
│
├── docs/
│   ├── api.md                       # API-Dokumentation
│   ├── energy-monitoring.md         # Energy-Feature-Docs
│   ├── deployment.md                # Pi-Setup-Anleitung
│   └── architecture.md              # Architektur-Übersicht
│
├── .env.example
├── .gitignore
├── CLAUDE.md                        # Claude-Kontextdatei
├── LICENSE                          # MIT
└── README.md
```

---

## 7. Storage-Layout (USB-SSD)

```
/mnt/ssd/balupi/
├── data/
│   ├── balupi.db              # ~50 MB (SQLite + Energy-Daten)
│   ├── cache/
│   │   ├── files/             # Hot-Cache: ~80% der SSD
│   │   └── thumbnails/        # ~5% der SSD
│   └── logs/                  # ~100 MB (rotiert)
│
└── config/
    └── config.yaml            # Laufzeit-Konfiguration
```

### Speicher-Budget (256 GB SSD)

| Bereich | Größe | Beschreibung |
|---|---|---|
| System (OS) | ~8 GB | Raspberry Pi OS Lite |
| BaluPi App | ~200 MB | Python + Dependencies |
| SQLite DB | ~50 MB | Metadaten + Energy-Daten (1 Jahr) |
| File Cache | ~200 GB | Hot-Cache (80/20 Dateien) |
| Thumbnails | ~10 GB | Generierte Vorschaubilder |
| Upload Queue | ~20 GB | Temporär: Mobile Uploads vor NAS-Sync |
| Logs | ~100 MB | Rotiert (7 Tage) |
| Reserve | ~17 GB | Headroom für SSD-Wear + Temp |

### Eviction-Strategie

```python
# Hotness Score = access_count * recency_weight
# recency_weight = 1.0 / (1 + days_since_last_access)
#
# Eviction bei: disk_usage > 85%
# Lösche Dateien mit niedrigstem Hotness Score
# Nie löschen: Dateien in Upload-Queue (is_dirty=1)
```

---

## 8. Handshake-Protokoll

### Dual-Detection: NAS finden

```
1. mDNS Query: _baluhost._tcp.local
    → Direkte Antwort wenn NAS online

2. Tapo Power Check (parallel):
    → NAS-Plug Leistung > 30W → NAS läuft (wahrscheinlich)
    → NAS-Plug Leistung < 5W  → NAS schläft
    → Kombination: mDNS + Power = höchste Konfidenz
```

### Verbindungsaufbau

```
BaluPi                              BaluHost NAS
  │                                      │
  │──── GET /api/health ────────────────►│
  │◄─── 200 { version, capabilities } ──│
  │                                      │
  │──── POST /api/auth/login ───────────►│
  │◄─── 200 { token, user } ────────────│
  │                                      │
  │──── GET /api/sync/manifest ─────────►│
  │     ?since=<last_sync_timestamp>     │
  │◄─── 200 { changes: [...] } ─────────│
  │                                      │
  │──── POST /api/pi/register ──────────►│  (Neuer Endpunkt auf NAS)
  │     { pi_id, capabilities,           │
  │       cache_size, energy_data }      │
  │◄─── 200 { sync_config, priority } ──│
  │                                      │
  │──── Bidirektionaler Sync ───────────►│
  │◄────────────────────────────────────│
```

### NAS-Wake-Sequenz

```python
async def ensure_nas_available(self) -> bool:
    # 1. Check: Läuft NAS schon?
    if await self.check_nas_online():
        return True

    # 2. Check: Tapo sagt NAS hat Strom?
    tapo_power = await self.get_nas_power()
    if tapo_power > self.power_threshold:
        # NAS hat Strom aber antwortet nicht → warte
        await asyncio.sleep(30)
        return await self.check_nas_online()

    # 3. WOL senden
    send_wol(self.nas_mac_address)

    # 4. Warten mit Timeout
    for attempt in range(12):  # Max 2 Minuten
        await asyncio.sleep(10)
        if await self.check_nas_online():
            return True

    return False  # NAS nicht erreichbar
```

---

## 9. Mobile-First Sync Flow

```
┌──────────┐         ┌──────────┐         ┌──────────┐
│ BaluApp  │         │ BaluPi   │         │ BaluHost │
│ (Handy)  │         │ (Cache)  │         │  (NAS)   │
└────┬─────┘         └────┬─────┘         └────┬─────┘
     │                    │                     │
     │ ── Upload Photo ──►│                     │
     │    (sofort, LAN)   │                     │
     │◄── 200 OK ─────── │                     │
     │                    │                     │
     │                    │ [NAS online?]        │
     │                    │── Ja ──► Sync ──────►│
     │                    │         sofort       │
     │                    │                      │
     │                    │── Nein ► Queue       │
     │                    │         (Upload-Q)   │
     │                    │                      │
     │ ── Request File ──►│                      │
     │                    │ [Cache Hit?]         │
     │◄── 200 (cached) ──│── Ja                 │
     │                    │                      │
     │                    │── Nein               │
     │                    │   [NAS online?]      │
     │                    │── Ja ► Fetch ───────►│
     │                    │◄────── File ─────────│
     │◄── 200 (fetched) ─│ (+ cache lokal)      │
     │                    │                      │
     │                    │── Nein               │
     │◄── 404/503 ───────│ (nicht verfügbar)    │
```

### Upload-Priorisierung

1. **Sofort**: Kleine Dateien (< 10 MB) → direkt Cache + Queue
2. **Batch**: Große Dateien → Chunk-Upload, Background-Sync
3. **Low Priority**: Thumbnails → generiert auf Pi, Sync bei Gelegenheit

---

## 10. Energy Monitoring Architektur

### Datenfluss

```
Tapo P110 ──── python-kasa ──── BaluPi ──── Dashboard
  (30s Poll)                    (SQLite)    (React)
                                   │
                                   ▼
                              Aggregation Jobs:
                              • Stündlich → energy_hourly
                              • Täglich  → energy_daily
                              • Monatlich → Cleanup (raw > 7d)
```

### Mess-Intervalle

| Daten | Intervall | Retention |
|---|---|---|
| Raw Samples | 30 Sekunden | 7 Tage |
| Stunden-Aggregat | 1 Stunde | 1 Jahr |
| Tages-Aggregat | 1 Tag | Unbegrenzt |
| Kosten-Berechnung | Täglich | Unbegrenzt |

### Speicherverbrauch (Energy-Daten)

```
Raw Samples:    ~40 Bytes/Sample × 2/min × 60 min × 24h × 7d × N Geräte
                = ~40 × 2880 × 7 × N = ~806 KB/Gerät/Woche
                Bei 3 Geräten: ~2.4 MB/Woche → ~17 MB max (7d Retention)

Hourly:         ~60 Bytes × 24 × 365 × N = ~526 KB/Gerät/Jahr
Daily:          ~50 Bytes × 365 × N = ~18 KB/Gerät/Jahr

Gesamt (3 Geräte, 1 Jahr): ~20 MB
→ Vernachlässigbar für SQLite
```

### NAS-Erkennung via Energy

```python
class NasDetectionService:
    """Erkennt NAS-Status über Tapo Smart Plug Leistungsmessung."""

    THRESHOLDS = {
        "off":     (0, 2),        # 0–2W: Komplett aus
        "standby": (2, 15),       # 2–15W: Standby/S3 Sleep
        "idle":    (30, 60),      # 30–60W: Idle, bereit
        "active":  (60, 200),     # 60–200W: Unter Last
    }

    async def get_nas_state(self) -> str:
        power_w = await self.tapo.get_current_power()
        for state, (low, high) in self.THRESHOLDS.items():
            if low <= power_w < high:
                return state
        return "unknown"

    async def should_wake_nas(self, reason: str) -> bool:
        state = await self.get_nas_state()
        if state in ("idle", "active"):
            return False  # Schon wach
        if state == "off":
            return False  # Komplett aus, kein WOL möglich
        # standby → WOL sinnvoll
        return True
```

---

## 11. Hardware-Limits & Optimierungen

### Raspberry Pi 3B+ Constraints

| Ressource | Limit | Strategie |
|---|---|---|
| RAM | 1 GB | Max 1–2 Uvicorn Worker, SQLite in WAL-Modus, Streaming für große Dateien |
| CPU | 4× A53 @ 1.4 GHz | Thumbnail-Gen limitiert, kein Heavy Processing |
| USB 2.0 | ~35 MB/s (SSD) | Nicht der Bottleneck bei Netzwerk-Sync |
| Netzwerk | ~300 Mbit/s (effektiv) | Ausreichend für LAN-Sync |
| Power | ~4W idle | 24/7 Betrieb: ~1 kWh/Monat ≈ 0,30€/Monat |
| Temp | < 80°C | Passivkühlung + Monitoring |

### Optimierungs-Strategien

```python
# 1. Streaming statt In-Memory für große Dateien
async def stream_file(path: Path) -> AsyncGenerator[bytes, None]:
    async with aiofiles.open(path, "rb") as f:
        while chunk := await f.read(64 * 1024):  # 64 KB Chunks
            yield chunk

# 2. Connection Pooling begrenzen
engine = create_async_engine(
    "sqlite+aiosqlite:///data/balupi.db",
    pool_size=5,        # Max 5 gleichzeitige Verbindungen
    max_overflow=0,
)

# 3. Memory-basiertes Thumbnail-Caching (LRU, max 50 MB)
from functools import lru_cache

# 4. Lazy Loading für Energy-Daten
# Nur aggregierte Daten laden, Raw on demand
```

---

## 12. Phasen-Plan (Timeline)

```
Woche  1  2  3  4  5  6  7  8  9  10  11  12  13  14
       ├──┤                                            P0: Foundation
          ├────────┤                                   P1: Energy ⚡
                   ├─────┤                             P2: NAS Discovery
                         ├───────────┤                 P3: Smart Cache
                                     ├─────┤           P4: Client-Integration
                                           ├─────┤     P5: Polish
```

### Meilensteine

| Woche | Meilenstein | Prüfkriterium |
|---|---|---|
| 2 | Pi läuft mit FastAPI | `/api/health` erreichbar, systemd stabil |
| 5 | Energy Monitoring live | Dashboard zeigt Echtzeit-Leistung aller Tapo-Geräte |
| 7 | NAS-Handshake funktioniert | Pi erkennt NAS (mDNS + Tapo), WOL funktioniert |
| 11 | Erster File-Sync | BaluApp → Pi → NAS Upload-Kette funktioniert |
| 13 | Client-Integration | BaluApp nutzt Pi als Primary, Fallback zu NAS |
| 14 | Production-Ready | Watchdog, Logging, Auto-Update, Dokumentation |

---

## 13. Entscheidungs-Log

| Entscheidung | Gewählt | Alternativen | Begründung |
|---|---|---|---|
| Sprache | Python 3.11+ | Go, Rust, Node.js | Gleiche Sprache wie BaluHost, VS Code Remote SSH, schnellste Entwicklung |
| Framework | FastAPI | Flask, Django | Async-native, API-kompatibel mit BaluHost, automatische OpenAPI-Docs |
| Datenbank | SQLite (WAL) | PostgreSQL, Redis | Kein DB-Server nötig, WAL-Modus für Concurrency, ressourcenschonend |
| ORM | SQLAlchemy 2.0 | Tortoise, Raw SQL | Identisch zu BaluHost, Model-Sharing möglich |
| Energy-Lib | python-kasa | Custom HTTP | Offiziell unterstützt, aktiv maintained, Tapo P110/P115 Support |
| Frontend | Shared React (BaluHost) | Separate App, No Frontend | Wiederverwendung, konsistentes UX, Build auf Dev-Maschine |
| Repo-Struktur | Eigenständig | Monorepo, Subtree | Unabhängige Releases, eigenes CI/CD, klare Verantwortung |
| Scheduler | APScheduler | Celery, Cron | In-Process, kein Redis/RabbitMQ nötig, leichtgewichtig |
| Cache-Strategie | 80/20 Hot-Cache | Full Mirror, Simple LRU | Optimiert für reale Zugriffsmuster, spart Speicher |
| NAS-Detection | mDNS + Tapo Dual | Nur mDNS, Nur Ping | Höchste Zuverlässigkeit: mDNS für IP, Tapo für Power-State |

---

## 14. Offene Fragen

- [ ] BaluHost: Neuer Endpunkt `POST /api/pi/register` nötig?
- [ ] BaluHost: Sync-Manifest-Endpoint (`GET /api/sync/manifest?since=...`) vorhanden?
- [ ] Frontend: Shared Build-Pipeline oder separater Build?
- [ ] Energy: Tapo-Account-Credentials sicher speichern (Keyring auf Pi?)
- [ ] Netzwerk: VPN/WireGuard auch für Pi oder nur LAN?
- [ ] Upgrade-Pfad: Pi 4 oder Pi 5 in Zukunft? (Mehr RAM, USB 3.0)
