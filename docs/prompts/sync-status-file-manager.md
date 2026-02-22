# Claude-Prompt: Sync-Status im BaluHost File Manager

> **Kontext**: BaluDesk (Desktop-Sync-Client) synced Ordner bidirektional mit BaluHost NAS. Im BaluHost File Manager soll sichtbar sein, welche Verzeichnisse aktiv von Desktop-Clients gesynced werden. Normale User sehen nur ihre eigenen Sync-Ordner, Admins sehen alles.

---

## Aufgabe

Implementiere ein Feature, das im BaluHost File Manager anzeigt, welche Ordner von Desktop-Clients gesynced werden. BaluDesk wird periodisch seine aktiven Sync-Ordner an BaluHost melden (dieser Report-Endpoint muss hier gebaut werden). Der File Manager zeigt dann einen Sync-Badge neben Ordnern, die aktiv gesynced werden.

**Scope**: Nur BaluHost-seitige Änderungen (Backend + Frontend). Die BaluDesk-seitige Report-Funktion wird separat implementiert.

---

## 1. Neues Model: `DesktopSyncFolder`

Erstelle `backend/app/models/desktop_sync_folder.py`:

```python
"""Track which remote folders are actively synced by desktop clients."""

from datetime import datetime
from sqlalchemy import String, DateTime, Integer, Boolean, ForeignKey, UniqueConstraint
from sqlalchemy.orm import Mapped, mapped_column
from sqlalchemy.sql import func

from app.models.base import Base


class DesktopSyncFolder(Base):
    """Tracks remote folders that are actively synced by BaluDesk clients."""
    __tablename__ = "desktop_sync_folders"
    __table_args__ = (
        UniqueConstraint("device_id", "remote_path", name="uq_device_remote_path"),
    )

    id: Mapped[int] = mapped_column(Integer, primary_key=True, index=True)
    user_id: Mapped[int] = mapped_column(Integer, ForeignKey("users.id"), nullable=False, index=True)
    device_id: Mapped[str] = mapped_column(String(255), nullable=False, index=True)
    device_name: Mapped[str] = mapped_column(String(255), nullable=False)
    platform: Mapped[str] = mapped_column(String(50), nullable=False)  # "windows" | "mac" | "linux"
    remote_path: Mapped[str] = mapped_column(String(1000), nullable=False, index=True)
    sync_direction: Mapped[str] = mapped_column(String(50), nullable=False, default="bidirectional")  # "bidirectional" | "push" | "pull"
    is_active: Mapped[bool] = mapped_column(Boolean, default=True, nullable=False)
    last_reported_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())
```

**Registrierung in `backend/app/models/__init__.py`**:

```python
# Zum Import-Block hinzufuegen:
from app.models.desktop_sync_folder import DesktopSyncFolder

# Zu __all__ hinzufuegen:
"DesktopSyncFolder",
```

### Hinweis: Bestehende sync_state.py Models

Die Models `SyncState`, `SyncMetadata`, `SyncFileVersion` aus `backend/app/models/sync_state.py` sind **nicht** in `__init__.py` registriert und haben **keine funktionierende Migration** (die Migration `27cc09d8d50c_add_sync_state_metadata_and_versioning_.py` hat leere `upgrade()`/`downgrade()`-Funktionen). Das neue `DesktopSyncFolder`-Model ist bewusst **unabhaengig** von diesen, um keine kaputte Dependency-Chain zu erzeugen.

---

## 2. Alembic Migration

Erstelle eine neue Migration. **Wichtig**: Die `sync_states`-Tabelle existiert moeglicherweise noch nicht in der DB, weil die Migration `27cc09d8d50c` leer ist. Die neue Migration fuer `desktop_sync_folders` sollte eigenstaendig sein und NICHT von `sync_states` abhaengen.

```bash
cd backend
alembic revision --autogenerate -m "add_desktop_sync_folders_table"
```

Die generierte Migration sollte ungefaehr so aussehen:

```python
"""add_desktop_sync_folders_table"""

from alembic import op
import sqlalchemy as sa

# revision identifiers
revision = '...'  # wird automatisch generiert
down_revision = '...'  # letzte existierende Migration


def upgrade() -> None:
    op.create_table(
        'desktop_sync_folders',
        sa.Column('id', sa.Integer(), nullable=False),
        sa.Column('user_id', sa.Integer(), nullable=False),
        sa.Column('device_id', sa.String(length=255), nullable=False),
        sa.Column('device_name', sa.String(length=255), nullable=False),
        sa.Column('platform', sa.String(length=50), nullable=False),
        sa.Column('remote_path', sa.String(length=1000), nullable=False),
        sa.Column('sync_direction', sa.String(length=50), nullable=False),
        sa.Column('is_active', sa.Boolean(), nullable=False),
        sa.Column('last_reported_at', sa.DateTime(timezone=True), server_default=sa.text('now()')),
        sa.Column('created_at', sa.DateTime(timezone=True), server_default=sa.text('now()')),
        sa.ForeignKeyConstraint(['user_id'], ['users.id']),
        sa.PrimaryKeyConstraint('id'),
        sa.UniqueConstraint('device_id', 'remote_path', name='uq_device_remote_path'),
    )
    op.create_index(op.f('ix_desktop_sync_folders_id'), 'desktop_sync_folders', ['id'])
    op.create_index(op.f('ix_desktop_sync_folders_user_id'), 'desktop_sync_folders', ['user_id'])
    op.create_index(op.f('ix_desktop_sync_folders_device_id'), 'desktop_sync_folders', ['device_id'])
    op.create_index(op.f('ix_desktop_sync_folders_remote_path'), 'desktop_sync_folders', ['remote_path'])


def downgrade() -> None:
    op.drop_index(op.f('ix_desktop_sync_folders_remote_path'), table_name='desktop_sync_folders')
    op.drop_index(op.f('ix_desktop_sync_folders_device_id'), table_name='desktop_sync_folders')
    op.drop_index(op.f('ix_desktop_sync_folders_user_id'), table_name='desktop_sync_folders')
    op.drop_index(op.f('ix_desktop_sync_folders_id'), table_name='desktop_sync_folders')
    op.drop_table('desktop_sync_folders')
```

### Optional: Leere sync_states-Migration befuellen

Falls gewuenscht, kann die leere Migration `27cc09d8d50c` nachtraeglich befuellt werden, damit `sync_states`, `sync_metadata` und `sync_file_versions` tatsaechlich erstellt werden. Das ist fuer dieses Feature **nicht zwingend noetig**, aber fuer zukuenftige Sync-Features relevant. Die Models dafuer stehen in `backend/app/models/sync_state.py`.

---

## 3. Schemas

### Neue Schemas in `backend/app/schemas/sync.py`

Am Ende der Datei anfuegen (nach den bestehenden Progressive-Sync-Schemas):

```python
# ============================================================================
# DESKTOP SYNC FOLDER TRACKING
# ============================================================================

class SyncFolderReport(BaseModel):
    """Single folder report from BaluDesk client."""
    remote_path: str = Field(..., description="Remote path on BaluHost being synced")
    sync_direction: str = Field("bidirectional", description="bidirectional, push, or pull")


class ReportSyncFoldersRequest(BaseModel):
    """BaluDesk reports its active sync folders."""
    device_id: str = Field(..., description="Unique device identifier")
    device_name: str = Field(..., description="Human-readable device name")
    platform: str = Field(..., description="windows, mac, or linux")
    folders: list[SyncFolderReport] = Field(..., description="Currently active sync folders")


class ReportSyncFoldersResponse(BaseModel):
    """Response after processing sync folder report."""
    accepted: int = Field(..., description="Number of folders accepted")
    deactivated: int = Field(..., description="Number of previously active folders now deactivated")


class SyncDeviceInfo(BaseModel):
    """Info about a device syncing a specific folder. Used in file listing enrichment."""
    device_name: str
    platform: str
    sync_direction: str
    last_reported_at: str


class SyncedFolderInfo(BaseModel):
    """Full info about a synced folder (for admin/user overview)."""
    remote_path: str
    device_id: str
    device_name: str
    platform: str
    sync_direction: str
    is_active: bool
    last_reported_at: str
    username: str | None = None  # nur fuer Admins sichtbar


class SyncedFoldersResponse(BaseModel):
    """List of synced folders."""
    folders: list[SyncedFolderInfo]
```

### FileItem erweitern in `backend/app/schemas/files.py`

Das bestehende `FileItem`-Schema um ein optionales Feld erweitern:

```python
class FileItem(BaseModel):
    name: str
    path: str
    size: int
    type: Literal["file", "directory"]
    modified_at: datetime
    owner_id: str | None = None
    mime_type: str | None = None
    file_id: int | None = None
    sync_info: list["SyncDeviceInfo"] | None = None  # NEU
```

Dafuer muss `SyncDeviceInfo` importiert werden:

```python
from app.schemas.sync import SyncDeviceInfo
```

**Achtung**: Der Import muss NACH der Definition von `FileItem` stehen oder als Forward Reference geloest werden, um zirkulaere Imports zu vermeiden. Alternativ den Import ans Ende der Datei setzen und `model_rebuild()` aufrufen, oder `SyncDeviceInfo` direkt in `files.py` definieren. Bevorzugte Loesung: Import am Anfang der Datei, da `schemas/sync.py` nicht von `schemas/files.py` abhaengt.

---

## 4. API Endpoints

### `POST /api/sync/report-folders` — BaluDesk meldet aktive Ordner

In `backend/app/api/routes/sync.py` hinzufuegen:

```python
from app.models.desktop_sync_folder import DesktopSyncFolder
from app.schemas.sync import ReportSyncFoldersRequest, ReportSyncFoldersResponse, SyncedFoldersResponse, SyncedFolderInfo

@router.post("/report-folders", response_model=ReportSyncFoldersResponse)
async def report_sync_folders(
    request: ReportSyncFoldersRequest,
    db: AsyncSession = Depends(get_db),
    current_user: User = Depends(get_current_user),
):
    """
    BaluDesk client reports its currently active sync folders.

    Upsert-Logik:
    - Fuer jede gemeldete Folder: INSERT oder UPDATE (device_id + remote_path als Key)
    - Alle NICHT gemeldeten Folders dieses Geraets werden als is_active=False markiert
    """
    now = func.now()
    reported_paths = set()
    accepted = 0

    for folder in request.folders:
        reported_paths.add(folder.remote_path)

        # Upsert: Suche bestehenden Eintrag
        stmt = select(DesktopSyncFolder).where(
            DesktopSyncFolder.device_id == request.device_id,
            DesktopSyncFolder.remote_path == folder.remote_path,
        )
        result = await db.execute(stmt)
        existing = result.scalar_one_or_none()

        if existing:
            existing.device_name = request.device_name
            existing.platform = request.platform
            existing.sync_direction = folder.sync_direction
            existing.is_active = True
            existing.last_reported_at = now
        else:
            db.add(DesktopSyncFolder(
                user_id=current_user.id,
                device_id=request.device_id,
                device_name=request.device_name,
                platform=request.platform,
                remote_path=folder.remote_path,
                sync_direction=folder.sync_direction,
                is_active=True,
                last_reported_at=now,
            ))
        accepted += 1

    # Nicht mehr gemeldete Folders dieses Geraets deaktivieren
    deactivate_stmt = (
        update(DesktopSyncFolder)
        .where(
            DesktopSyncFolder.device_id == request.device_id,
            DesktopSyncFolder.user_id == current_user.id,
            DesktopSyncFolder.is_active == True,
            DesktopSyncFolder.remote_path.notin_(reported_paths) if reported_paths else True,
        )
        .values(is_active=False)
    )
    result = await db.execute(deactivate_stmt)
    deactivated = result.rowcount

    await db.commit()

    return ReportSyncFoldersResponse(accepted=accepted, deactivated=deactivated)
```

### `GET /api/sync/synced-folders` — Uebersicht gesyncter Ordner

```python
@router.get("/synced-folders", response_model=SyncedFoldersResponse)
async def get_synced_folders(
    active_only: bool = True,
    db: AsyncSession = Depends(get_db),
    current_user: User = Depends(get_current_user),
):
    """
    Liste aller gesyncten Ordner.
    - Normale User: nur eigene Ordner
    - Admins: alle Ordner (mit Username)
    """
    stmt = select(DesktopSyncFolder)

    if active_only:
        stmt = stmt.where(DesktopSyncFolder.is_active == True)

    if not current_user.is_admin:
        stmt = stmt.where(DesktopSyncFolder.user_id == current_user.id)

    result = await db.execute(stmt)
    folders = result.scalars().all()

    # Fuer Admins: Usernamen aufloesen
    user_names = {}
    if current_user.is_admin:
        user_ids = {f.user_id for f in folders}
        if user_ids:
            users_result = await db.execute(
                select(User).where(User.id.in_(user_ids))
            )
            user_names = {u.id: u.username for u in users_result.scalars().all()}

    return SyncedFoldersResponse(
        folders=[
            SyncedFolderInfo(
                remote_path=f.remote_path,
                device_id=f.device_id,
                device_name=f.device_name,
                platform=f.platform,
                sync_direction=f.sync_direction,
                is_active=f.is_active,
                last_reported_at=f.last_reported_at.isoformat(),
                username=user_names.get(f.user_id) if current_user.is_admin else None,
            )
            for f in folders
        ]
    )
```

### Noetige Imports fuer sync.py

Stelle sicher, dass diese Imports oben in der Datei stehen (manche sind vermutlich schon vorhanden):

```python
from sqlalchemy import select, update
from sqlalchemy.sql import func
from app.models.desktop_sync_folder import DesktopSyncFolder
from app.models.user import User
from app.schemas.sync import (
    ReportSyncFoldersRequest, ReportSyncFoldersResponse,
    SyncedFoldersResponse, SyncedFolderInfo,
)
```

---

## 5. File-List-Enrichment

### Helper-Funktion in `backend/app/api/routes/files.py`

Fuege eine Helper-Funktion hinzu, die `FileItem`-Listen mit Sync-Infos anreichert:

```python
from app.models.desktop_sync_folder import DesktopSyncFolder
from app.schemas.sync import SyncDeviceInfo

async def _enrich_with_sync_info(
    files: list[dict],
    current_path: str,
    user_id: int,
    is_admin: bool,
    db: AsyncSession,
) -> list[dict]:
    """
    Reichert die File-Liste mit Sync-Info an.
    Fuer jeden Ordner wird geprueft, ob er von einem Desktop-Client gesynced wird.

    Args:
        files: Liste von FileItem-Dicts (vor Pydantic-Serialisierung)
        current_path: Aktueller Verzeichnispfad
        user_id: ID des anfragenden Users
        is_admin: Ob der User Admin ist
        db: Datenbank-Session
    """
    # Nur Directories koennen gesynced werden
    directories = [f for f in files if f.get("type") == "directory"]
    if not directories:
        return files

    # Alle aktiven Sync-Folders laden, die fuer diesen User relevant sind
    stmt = select(DesktopSyncFolder).where(
        DesktopSyncFolder.is_active == True,
    )
    if not is_admin:
        stmt = stmt.where(DesktopSyncFolder.user_id == user_id)

    result = await db.execute(stmt)
    sync_folders = result.scalars().all()

    if not sync_folders:
        return files

    # Index aufbauen: remote_path -> list[SyncDeviceInfo]
    sync_map: dict[str, list[SyncDeviceInfo]] = {}
    for sf in sync_folders:
        # Normalisiere Pfade fuer Vergleich (ohne trailing slash)
        normalized = sf.remote_path.rstrip("/")
        if normalized not in sync_map:
            sync_map[normalized] = []
        sync_map[normalized].append(SyncDeviceInfo(
            device_name=sf.device_name,
            platform=sf.platform,
            sync_direction=sf.sync_direction,
            last_reported_at=sf.last_reported_at.isoformat(),
        ))

    # Files anreichern
    for f in files:
        if f.get("type") != "directory":
            continue
        # Vollstaendigen Pfad des Ordners ermitteln
        folder_path = f.get("path", "").rstrip("/")
        if folder_path in sync_map:
            f["sync_info"] = [info.model_dump() for info in sync_map[folder_path]]

    return files
```

### Integration in die File-Listing-Logik

In `backend/app/api/routes/files.py` gibt es mehrere Stellen, an denen `FileListResponse` zurueckgegeben wird. **Vor jedem `return FileListResponse(...)`** muss der Enrichment-Aufruf eingefuegt werden.

Suche nach allen Stellen mit `return FileListResponse` und fuege jeweils vorher ein:

```python
# Vor dem return: Sync-Info anreichern
files_dicts = [f.dict() if hasattr(f, 'dict') else f for f in files_list]
files_dicts = await _enrich_with_sync_info(
    files=files_dicts,
    current_path=current_path_variable,  # anpassen je nach Kontext
    user_id=current_user.id,
    is_admin=current_user.is_admin,
    db=db,
)
return FileListResponse(files=[FileItem(**f) for f in files_dicts])
```

**Typische Stellen** (genaue Zeilennummern koennen variieren):

1. **Normales Directory-Listing** — Der Hauptpfad fuer regulaere Verzeichnisse
2. **Root-Listing fuer Non-Admins** — Der spezielle Pfad, der virtuelle Eintraege wie `Shared/` und User-Home zeigt
3. **Shared-with-me-Listing** — Falls vorhanden, der Pfad fuer geteilte Dateien

**Wichtig**: Die genaue Integration haengt von der aktuellen Struktur der `list_files`-Funktion ab. Lies den Code sorgfaeltig und identifiziere alle `return FileListResponse`-Stellen. Im Zweifelsfall den Enrichment-Call als Wrapper um die gesamte Response setzen.

---

## 6. Frontend-Aenderungen

### Types erweitern: `client/src/components/file-manager/types.ts`

```typescript
// Neu hinzufuegen:
export interface SyncDeviceInfo {
  deviceName: string;
  platform: 'windows' | 'mac' | 'linux';
  syncDirection: 'bidirectional' | 'push' | 'pull';
  lastReportedAt: string;
}

// FileItem erweitern:
export interface FileItem {
  name: string;
  path: string;
  size: number;
  type: 'file' | 'directory';
  modifiedAt: string;
  ownerId?: number;
  ownerName?: string;
  file_id?: number;
  syncInfo?: SyncDeviceInfo[];  // NEU
}

// ApiFileItem erweitern:
export interface ApiFileItem {
  name: string;
  path: string;
  size: number;
  type: 'file' | 'directory';
  modified_at?: string;
  mtime?: string;
  ownerId?: number;
  owner_id?: number;
  ownerName?: string;
  owner_name?: string;
  file_id?: number;
  sync_info?: Array<{           // NEU
    device_name: string;
    platform: string;
    sync_direction: string;
    last_reported_at: string;
  }>;
}
```

### API-Mapping: `client/src/pages/FileManager.tsx`

In der Stelle, wo `ApiFileItem[]` zu `FileItem[]` gemappt wird (snake_case -> camelCase), das neue Feld hinzufuegen:

```typescript
// Im bestehenden Mapping (suche nach modified_at -> modifiedAt):
syncInfo: apiFile.sync_info?.map(si => ({
  deviceName: si.device_name,
  platform: si.platform as 'windows' | 'mac' | 'linux',
  syncDirection: si.sync_direction as 'bidirectional' | 'push' | 'pull',
  lastReportedAt: si.last_reported_at,
})),
```

### Sync-Badge: `client/src/components/file-manager/FileListView.tsx`

Fuege einen teal-farbigen Sync-Badge neben dem Ordnernamen hinzu, wenn `syncInfo` vorhanden ist.

**Icon**: Verwende `Monitor` aus `lucide-react` (ist bereits als Dependency im Projekt).

```tsx
import { Monitor } from 'lucide-react';

// Im JSX, neben dem Dateinamen (innerhalb der Zeile fuer Directories):
{file.syncInfo && file.syncInfo.length > 0 && (
  <span
    className="inline-flex items-center gap-1 ml-2 px-1.5 py-0.5 rounded text-xs font-medium bg-teal-100 text-teal-700 dark:bg-teal-900/30 dark:text-teal-400"
    title={file.syncInfo.map(s =>
      `${s.deviceName} (${s.platform}, ${s.syncDirection})`
    ).join('\n')}
  >
    <Monitor className="w-3 h-3" />
    {file.syncInfo.length === 1
      ? file.syncInfo[0].deviceName
      : `${file.syncInfo.length} Geraete`}
  </span>
)}
```

**Platzierung**: Der Badge kommt direkt nach dem Ordnernamen, in derselben Zeile. Sowohl in der Desktop-Tabellenansicht als auch in der Mobil-Kartenansicht.

---

## 7. Zusammenfassung der zu aendernden Dateien

| Datei | Aktion | Beschreibung |
|---|---|---|
| `backend/app/models/desktop_sync_folder.py` | **NEU** | DesktopSyncFolder Model |
| `backend/app/models/__init__.py` | EDIT | Import + `__all__` erweitern |
| `backend/alembic/versions/xxx_add_desktop_sync_folders.py` | **NEU** | Alembic Migration |
| `backend/app/schemas/sync.py` | EDIT | 5 neue Schemas anfuegen |
| `backend/app/schemas/files.py` | EDIT | `sync_info` Feld auf FileItem + Import |
| `backend/app/api/routes/sync.py` | EDIT | 2 neue Endpoints + Imports |
| `backend/app/api/routes/files.py` | EDIT | `_enrich_with_sync_info()` Helper + Aufrufe vor jedem `return FileListResponse` |
| `client/src/components/file-manager/types.ts` | EDIT | `SyncDeviceInfo` Interface + FileItem/ApiFileItem erweitern |
| `client/src/pages/FileManager.tsx` | EDIT | `sync_info` -> `syncInfo` Mapping |
| `client/src/components/file-manager/FileListView.tsx` | EDIT | Teal Sync-Badge mit Monitor-Icon |

---

## 8. Verifikations-Checkliste

Nach der Implementierung pruefen:

- [ ] `DesktopSyncFolder` Model nutzt `Mapped[]`-Pattern wie andere Models (z.B. `sync_state.py`)
- [ ] Migration ist eigenstaendig und haengt NICHT von der leeren `sync_states`-Migration ab
- [ ] `POST /api/sync/report-folders` erfordert JWT-Auth (`get_current_user`)
- [ ] Upsert-Logik: Unique Constraint auf `(device_id, remote_path)`, UPDATE bei bestehendem Eintrag
- [ ] Deaktivierung: Nicht gemeldete Folders desselben Geraets werden `is_active=False`
- [ ] `GET /api/sync/synced-folders`: Normale User sehen nur eigene, Admins sehen alle (mit Username)
- [ ] File-Enrichment: Nur aktive Sync-Folders (`is_active=True`) werden angezeigt
- [ ] File-Enrichment: Admin sieht Sync-Info aller User, normaler User nur eigene
- [ ] Frontend: `sync_info` korrekt von snake_case zu camelCase gemappt
- [ ] Frontend: Badge nur bei `type === "directory"` und `syncInfo.length > 0`
- [ ] Frontend: Tooltip zeigt Device-Name, Platform und Sync-Direction
- [ ] Keine zirkulaeren Imports zwischen `schemas/files.py` und `schemas/sync.py`

---

## 9. Testbarkeit

Um das Feature manuell zu testen (bevor BaluDesk die Report-Funktion hat):

```bash
# Device-Code-Flow durchfuehren und Token erhalten, dann:
curl -X POST http://192.168.178.53/api/sync/report-folders \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "test-device-001",
    "device_name": "Sven PC",
    "platform": "windows",
    "folders": [
      {"remote_path": "/Sven/Documents", "sync_direction": "bidirectional"},
      {"remote_path": "/Sven/Photos", "sync_direction": "push"}
    ]
  }'

# Dann im File Manager pruefen: /Sven/Documents und /Sven/Photos sollten Sync-Badge zeigen
```

---

## 10. Spaetere Erweiterungen (nicht in diesem Scope)

- **BaluDesk-seitig**: Periodischer Report der aktiven Sync-Ordner (neuer IPC-Handler `report_sync_folders`)
- **Stale-Detection**: Cronjob der Ordner als inaktiv markiert, wenn `last_reported_at` > 24h
- **Sync-Status im Badge**: Nicht nur "wird gesynced", sondern auch "syncing...", "error", "paused"
- **Klick auf Badge**: Detail-Dialog mit allen syncenden Geraeten und deren Status
