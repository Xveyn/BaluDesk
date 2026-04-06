# Dashboard Layout Redesign - Design Spec

**Goal:** Reorganize the BaluDesk Dashboard layout to match the BaluApp mobile app's information hierarchy, while keeping the existing visual styling (dark glassmorphism, card gradients, hover effects).

**Motivation:** The mobile app prioritizes server status and system metrics at the top, with sync as a secondary summary. The current desktop Dashboard has sync dominating the top and NAS details hidden in a collapsible panel. Aligning both UIs creates a consistent experience.

---

## Layout (top to bottom)

### 1. Server Status Strip (new, full width)
A compact status bar at the top showing NAS online/sleeping/offline state.

- **Left side:** Color-coded status dot (green=online, orange=sleeping, red=offline) + status text + uptime
- **Right side:** Power action buttons:
  - When **online:** "Soft Sleep", "Suspend"
  - When **sleeping:** "Wake (WOL)", "Wake (Server)"
  - When **offline:** "Wake-on-LAN" (if available)
- Styled consistently with existing card design (rounded-xl, border-white/10, gradient bg)
- Data source: `useSyncStatus()` for systemInfo (contains uptime), plus a new IPC command for NAS status and power actions

**IPC commands needed:**
- `get_nas_status` → `{ status: 'online'|'sleeping'|'offline', uptimeSeconds: number }`
- `power_action` → `{ action: 'wol'|'wake'|'soft_sleep'|'suspend' }`

Since the backend may not yet implement these, the component will gracefully handle missing data (show "Unknown" status, disable buttons).

### 2. System Metrics Grid (existing QuickStatsGrid, full width)
The existing 2x2/4-col grid of CPU, RAM, Disk, Uptime cards. No changes to the component itself, just moved from inside the NAS collapsible to a top-level position.

### 3. Two-column row: Power + Network
- **Left:** Existing `PowerCard` (from `components/PowerCard.tsx`)
- **Right:** Existing `NetworkWidget` (from `components/dashboard/NetworkWidget.tsx`)
- Grid: `md:grid-cols-2`, stacks on mobile

### 4. Two-column row: Quick Share (placeholder) + Services
- **Left:** New `QuickShareCard` placeholder showing "Coming soon" with share icon and zeroed stats
- **Right:** Existing `ServicesPanel`
- Grid: `md:grid-cols-2`, stacks on mobile

### 5. Sync Summary Card (new, full width)
Merges content from three existing cards:
- **SyncOverviewCard:** Status badge, upload/download counters, progress bar, error banner
- **ActiveTransfersCard:** Current file transfer progress, overall file progress
- **SyncFoldersCard:** Folder list with status badges

Layout within the card:
- **Top:** Status badge + transfer counters (from SyncOverviewCard)
- **Middle:** Active transfer progress (from ActiveTransfersCard) — only shown when syncing
- **Below:** Progress bar + folder list (compact, from SyncFoldersCard)
- **Bottom:** Error banner (if applicable), "Manage folders" link

### 6. Recent Activity Card (existing, full width)
No changes to the component, just positioned here.

### 7. RAID Status Card (existing, conditional, full width)
No changes to the component. Only renders if RAID data exists.

---

## Components to create

1. **`ServerStatusStrip.tsx`** — new component in `components/dashboard/`
2. **`QuickShareCard.tsx`** — new placeholder component in `components/dashboard/`
3. **`SyncSummaryCard.tsx`** — new merged component in `components/dashboard/`

## Components to delete

1. **`SyncOverviewCard.tsx`** — absorbed into SyncSummaryCard
2. **`SyncFoldersCard.tsx`** — absorbed into SyncSummaryCard
3. **`ActiveTransfersCard.tsx`** — absorbed into SyncSummaryCard
4. **`NasDetailsPanel.tsx`** — layout dissolved, children promoted to top level

## Components unchanged (only repositioned)

- `QuickStatsGrid.tsx`
- `NetworkWidget.tsx`
- `ServicesPanel.tsx`
- `RaidStatusCard.tsx`
- `PowerCard.tsx` (stays in `components/PowerCard.tsx`)

## File modified

- **`Dashboard.tsx`** — complete rewrite of layout to new ordering and new imports

---

## Out of scope

- No styling changes (colors, gradients, hover effects, fonts stay the same)
- No new hooks or data fetching beyond what's needed for ServerStatusStrip
- No changes to backend IPC handlers (ServerStatusStrip gracefully handles missing commands)
- No Quick Share functionality (placeholder only)
