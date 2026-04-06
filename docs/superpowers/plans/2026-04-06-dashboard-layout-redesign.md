# Dashboard Layout Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reorganize Dashboard to match BaluApp's layout: Server Status Strip → System Metrics → Power+Network → QuickShare+Services → Sync Summary → Recent Activity → RAID.

**Architecture:** Three new components (ServerStatusStrip, QuickShareCard, SyncSummaryCard) replace four existing ones (SyncOverviewCard, SyncFoldersCard, ActiveTransfersCard, NasDetailsPanel). Dashboard.tsx gets a full layout rewrite.

**Tech Stack:** React 18, TypeScript, Tailwind CSS, lucide-react icons

---

### Task 1: Create ServerStatusStrip component

**Files:**
- Create: `frontend/src/renderer/components/dashboard/ServerStatusStrip.tsx`

- [ ] **Step 1: Create the ServerStatusStrip component**

```tsx
import { useState } from 'react';
import { Server, Power, Moon, Sun, Wifi } from 'lucide-react';
import { formatUptime } from '../../../lib/formatters';

interface ServerStatusStripProps {
  systemInfo: {
    uptime: number;
    serverUptime?: number;
  } | null;
  loading: boolean;
}

type NasStatus = 'online' | 'sleeping' | 'offline' | 'unknown';

function getStatusConfig(status: NasStatus) {
  switch (status) {
    case 'online':
      return { dot: 'bg-emerald-400', text: 'Server Online', color: 'text-emerald-400' };
    case 'sleeping':
      return { dot: 'bg-amber-400', text: 'NAS schläft', color: 'text-amber-400' };
    case 'offline':
      return { dot: 'bg-red-400', text: 'Server Offline', color: 'text-red-400' };
    default:
      return { dot: 'bg-slate-400', text: 'Unknown', color: 'text-slate-400' };
  }
}

export function ServerStatusStrip({ systemInfo, loading }: ServerStatusStripProps) {
  const [actionInProgress, setActionInProgress] = useState<string | null>(null);

  // Derive status from systemInfo availability
  const nasStatus: NasStatus = loading ? 'unknown' : systemInfo ? 'online' : 'offline';
  const config = getStatusConfig(nasStatus);

  const uptimeSeconds = systemInfo
    ? (typeof systemInfo.serverUptime === 'number' && systemInfo.serverUptime > 0
        ? systemInfo.serverUptime
        : systemInfo.uptime)
    : 0;

  const handlePowerAction = async (action: string) => {
    setActionInProgress(action);
    try {
      await window.electronAPI.sendBackendCommand({
        type: 'power_action',
        data: { action },
      });
    } catch (err) {
      console.error(`Power action '${action}' failed:`, err);
    } finally {
      setActionInProgress(null);
    }
  };

  return (
    <div className="rounded-xl border border-white/10 bg-gradient-to-br from-slate-500/10 to-slate-600/10 px-4 py-3 backdrop-blur-sm transition-all hover:border-slate-500/30">
      <div className="flex items-center justify-between">
        {/* Left: Status info */}
        <div className="flex items-center space-x-3">
          <div className="rounded-lg bg-slate-700/50 p-1.5">
            <Server className="h-4 w-4 text-slate-400" />
          </div>
          <div className="flex items-center space-x-2">
            <span className={`h-2.5 w-2.5 rounded-full ${config.dot} ${nasStatus === 'online' ? '' : nasStatus === 'sleeping' ? 'animate-pulse' : ''}`} />
            <span className={`text-sm font-medium ${config.color}`}>{config.text}</span>
          </div>
          {nasStatus === 'online' && uptimeSeconds > 0 && (
            <span className="text-xs text-slate-400">
              Uptime: {formatUptime(uptimeSeconds)}
            </span>
          )}
        </div>

        {/* Right: Power actions */}
        <div className="flex items-center space-x-2">
          {nasStatus === 'online' && (
            <>
              <button
                onClick={() => handlePowerAction('soft_sleep')}
                disabled={!!actionInProgress}
                className="flex items-center space-x-1 px-2 py-1 rounded-lg bg-white/5 hover:bg-amber-500/20 text-xs text-slate-400 hover:text-amber-300 transition-all disabled:opacity-50"
                title="Soft Sleep"
              >
                <Moon className="h-3 w-3" />
                <span className="hidden sm:inline">Sleep</span>
              </button>
              <button
                onClick={() => handlePowerAction('suspend')}
                disabled={!!actionInProgress}
                className="flex items-center space-x-1 px-2 py-1 rounded-lg bg-white/5 hover:bg-red-500/20 text-xs text-slate-400 hover:text-red-300 transition-all disabled:opacity-50"
                title="Suspend"
              >
                <Power className="h-3 w-3" />
                <span className="hidden sm:inline">Suspend</span>
              </button>
            </>
          )}
          {nasStatus === 'sleeping' && (
            <>
              <button
                onClick={() => handlePowerAction('wol')}
                disabled={!!actionInProgress}
                className="flex items-center space-x-1 px-2 py-1 rounded-lg bg-white/5 hover:bg-emerald-500/20 text-xs text-slate-400 hover:text-emerald-300 transition-all disabled:opacity-50"
                title="Wake-on-LAN"
              >
                <Wifi className="h-3 w-3" />
                <span className="hidden sm:inline">WOL</span>
              </button>
              <button
                onClick={() => handlePowerAction('wake')}
                disabled={!!actionInProgress}
                className="flex items-center space-x-1 px-2 py-1 rounded-lg bg-white/5 hover:bg-emerald-500/20 text-xs text-slate-400 hover:text-emerald-300 transition-all disabled:opacity-50"
                title="Server Wake"
              >
                <Sun className="h-3 w-3" />
                <span className="hidden sm:inline">Wake</span>
              </button>
            </>
          )}
          {nasStatus === 'offline' && (
            <button
              onClick={() => handlePowerAction('wol')}
              disabled={!!actionInProgress}
              className="flex items-center space-x-1 px-2 py-1 rounded-lg bg-white/5 hover:bg-emerald-500/20 text-xs text-slate-400 hover:text-emerald-300 transition-all disabled:opacity-50"
              title="Wake-on-LAN"
            >
              <Wifi className="h-3 w-3" />
              <span className="hidden sm:inline">WOL</span>
            </button>
          )}
        </div>
      </div>
    </div>
  );
}
```

- [ ] **Step 2: Verify no TypeScript errors**

Run: `cd frontend && npx tsc --noEmit --pretty 2>&1 | head -20`
Expected: No errors related to ServerStatusStrip

- [ ] **Step 3: Commit**

```bash
git add frontend/src/renderer/components/dashboard/ServerStatusStrip.tsx
git commit -m "feat(dashboard): add ServerStatusStrip component with power actions"
```

---

### Task 2: Create QuickShareCard placeholder component

**Files:**
- Create: `frontend/src/renderer/components/dashboard/QuickShareCard.tsx`

- [ ] **Step 1: Create the QuickShareCard placeholder**

```tsx
import { Share2, FolderUp, FolderDown } from 'lucide-react';

export function QuickShareCard() {
  return (
    <div className="rounded-xl border border-white/10 bg-gradient-to-br from-blue-500/10 to-indigo-600/10 p-4 backdrop-blur-sm transition-all hover:border-blue-500/30 hover:shadow-lg hover:shadow-blue-500/20">
      <div className="flex items-center space-x-2 mb-3">
        <div className="rounded-lg bg-gradient-to-br from-blue-500/20 to-indigo-500/20 p-1.5">
          <Share2 className="h-4 w-4 text-blue-400" />
        </div>
        <h3 className="text-sm font-medium text-slate-300">Quick Share</h3>
      </div>

      <div className="space-y-2">
        <div className="flex items-center justify-between">
          <div className="flex items-center space-x-1.5">
            <FolderUp className="h-3.5 w-3.5 text-blue-400" />
            <span className="text-xs text-slate-400">Shared by me</span>
          </div>
          <span className="text-sm font-semibold text-white">0</span>
        </div>
        <div className="flex items-center justify-between">
          <div className="flex items-center space-x-1.5">
            <FolderDown className="h-3.5 w-3.5 text-indigo-400" />
            <span className="text-xs text-slate-400">Shared with me</span>
          </div>
          <span className="text-sm font-semibold text-white">0</span>
        </div>
      </div>

      <div className="mt-3 pt-2 border-t border-white/10">
        <p className="text-xs text-slate-500 text-center">Coming soon</p>
      </div>
    </div>
  );
}
```

- [ ] **Step 2: Commit**

```bash
git add frontend/src/renderer/components/dashboard/QuickShareCard.tsx
git commit -m "feat(dashboard): add QuickShareCard placeholder component"
```

---

### Task 3: Create SyncSummaryCard (merged from SyncOverview + ActiveTransfers + SyncFolders)

**Files:**
- Create: `frontend/src/renderer/components/dashboard/SyncSummaryCard.tsx`

This component merges content from `SyncOverviewCard`, `ActiveTransfersCard`, and `SyncFoldersCard`. It reuses the same logic and styling patterns.

- [ ] **Step 1: Create the SyncSummaryCard component**

```tsx
import { useEffect, useState } from 'react';
import {
  CheckCircle2, RefreshCw, Pause, AlertTriangle,
  ArrowUp, ArrowDown, ArrowUpDown, FolderSync,
} from 'lucide-react';
import { formatSpeed, formatBytes, formatRelativeTime, getFileName } from '../../../lib/formatters';
import { SyncStats } from '../../hooks/useSyncStatus';

interface SyncFolder {
  id: string;
  local_path: string;
  remote_path: string;
  status: string;
  enabled: boolean;
  size: number;
  last_sync: string;
  sync_direction: string;
}

interface SyncSummaryCardProps {
  syncStats: SyncStats | null;
  syncError: string | null;
  loading: boolean;
}

function getStatusConfig(status: string) {
  switch (status) {
    case 'syncing':
      return {
        icon: RefreshCw,
        label: 'Syncing...',
        color: 'text-emerald-400',
        bgColor: 'bg-emerald-500/20',
        dotColor: 'bg-emerald-400',
        animate: true,
      };
    case 'paused':
      return {
        icon: Pause,
        label: 'Paused',
        color: 'text-amber-400',
        bgColor: 'bg-amber-500/20',
        dotColor: 'bg-amber-400',
        animate: false,
      };
    case 'error':
      return {
        icon: AlertTriangle,
        label: 'Error',
        color: 'text-red-400',
        bgColor: 'bg-red-500/20',
        dotColor: 'bg-red-400',
        animate: false,
      };
    default:
      return {
        icon: CheckCircle2,
        label: 'All synced',
        color: 'text-emerald-400',
        bgColor: 'bg-emerald-500/20',
        dotColor: 'bg-emerald-400',
        animate: false,
      };
  }
}

function getStatusBadge(status: string) {
  switch (status) {
    case 'syncing':
      return { label: 'Syncing', className: 'bg-emerald-500/20 text-emerald-300' };
    case 'paused':
      return { label: 'Paused', className: 'bg-amber-500/20 text-amber-300' };
    case 'error':
      return { label: 'Error', className: 'bg-red-500/20 text-red-300' };
    default:
      return { label: 'Synced', className: 'bg-blue-500/20 text-blue-300' };
  }
}

function getDirectionInfo(direction: string) {
  switch (direction) {
    case 'upload':
      return { icon: ArrowUp, label: 'Upload', className: 'text-emerald-400' };
    case 'download':
      return { icon: ArrowDown, label: 'Download', className: 'text-purple-400' };
    default:
      return { icon: ArrowUpDown, label: 'Bidirectional', className: 'text-blue-400' };
  }
}

export function SyncSummaryCard({ syncStats, syncError, loading }: SyncSummaryCardProps) {
  const [folders, setFolders] = useState<SyncFolder[]>([]);
  const [foldersLoading, setFoldersLoading] = useState(true);

  const fetchFolders = async () => {
    try {
      const response = await window.electronAPI.sendBackendCommand({ type: 'get_folders' });
      if (response?.folders) {
        setFolders(response.folders);
      }
    } catch (err) {
      console.error('Failed to fetch folders:', err);
    } finally {
      setFoldersLoading(false);
    }
  };

  useEffect(() => {
    fetchFolders();
    const interval = setInterval(fetchFolders, 15000);
    return () => clearInterval(interval);
  }, []);

  const handleSyncAll = async () => {
    try {
      await window.electronAPI.sendBackendCommand({ type: 'trigger_sync', data: {} });
    } catch (err) {
      console.error('Failed to trigger sync:', err);
    }
  };

  if (loading) {
    return (
      <div className="rounded-xl border border-white/10 bg-gradient-to-br from-emerald-500/10 to-cyan-600/10 p-5 backdrop-blur-sm">
        <div className="space-y-3">
          <div className="h-6 w-48 bg-slate-700/30 rounded animate-pulse" />
          <div className="h-4 w-full bg-slate-700/30 rounded animate-pulse" />
          <div className="h-2 w-full bg-slate-700/30 rounded animate-pulse" />
        </div>
      </div>
    );
  }

  const status = syncStats?.status || 'idle';
  const config = getStatusConfig(status);
  const StatusIcon = config.icon;
  const isSyncing = status === 'syncing';
  const totalFiles = syncStats?.totalFiles || 0;
  const processedFiles = syncStats?.processedFiles || 0;
  const progressPercent = totalFiles > 0
    ? Math.min((processedFiles / totalFiles) * 100, 100)
    : (isSyncing ? 0 : 100);
  const totalPending = (syncStats?.pendingUploads || 0) + (syncStats?.pendingDownloads || 0);

  const hasFileProgress = isSyncing && syncStats?.currentFile && (syncStats?.currentFileSize ?? 0) > 0;
  const hasOverallProgress = isSyncing && totalFiles > 0;

  return (
    <div className="rounded-xl border border-white/10 bg-gradient-to-br from-emerald-500/10 to-cyan-600/10 p-5 backdrop-blur-sm transition-all hover:border-emerald-500/30">
      {/* Header: Status + Counters + Sync All */}
      <div className="flex items-center justify-between mb-4">
        <div className="flex items-center space-x-3">
          <div className={`rounded-lg ${config.bgColor} p-2`}>
            <StatusIcon className={`h-5 w-5 ${config.color} ${config.animate ? 'animate-spin' : ''}`} />
          </div>
          <div>
            <div className="flex items-center space-x-2">
              <span className={`h-2 w-2 rounded-full ${config.dotColor} ${config.animate ? 'animate-pulse' : ''}`} />
              <span className={`text-lg font-semibold ${config.color}`}>{config.label}</span>
            </div>
            {isSyncing && totalFiles > 0 && (
              <p className="text-xs text-slate-400 mt-0.5">
                {processedFiles} / {totalFiles} files
              </p>
            )}
          </div>
        </div>

        <div className="flex items-center space-x-4">
          {/* Upload / Download counters */}
          <div className="flex items-center space-x-1.5">
            <ArrowUp className="h-4 w-4 text-emerald-400" />
            <span className="text-sm font-medium text-white">{syncStats?.pendingUploads || 0}</span>
            {syncStats?.uploadSpeed && syncStats.uploadSpeed > 0 && (
              <span className="text-xs text-emerald-400">{formatSpeed(syncStats.uploadSpeed)}</span>
            )}
          </div>
          <div className="flex items-center space-x-1.5">
            <ArrowDown className="h-4 w-4 text-purple-400" />
            <span className="text-sm font-medium text-white">{syncStats?.pendingDownloads || 0}</span>
            {syncStats?.downloadSpeed && syncStats.downloadSpeed > 0 && (
              <span className="text-xs text-purple-400">{formatSpeed(syncStats.downloadSpeed)}</span>
            )}
          </div>
          <button
            onClick={handleSyncAll}
            className="flex items-center space-x-1 px-2 py-1 rounded-lg bg-white/5 hover:bg-white/10 text-xs text-slate-400 hover:text-white transition-all"
            title="Sync all folders"
          >
            <RefreshCw className="h-3 w-3" />
            <span>Sync All</span>
          </button>
        </div>
      </div>

      {/* Active Transfers (only when syncing) */}
      {(hasFileProgress || hasOverallProgress) && (
        <div className="mb-4 space-y-2 rounded-lg bg-white/5 p-3">
          {hasFileProgress && syncStats?.currentFile && (
            <div className="space-y-1.5">
              <div className="flex items-center justify-between">
                <div className="flex items-center space-x-1.5 min-w-0 flex-1">
                  {(syncStats.uploadSpeed ?? 0) > 0 ? (
                    <ArrowUp className="h-3.5 w-3.5 text-emerald-400 flex-shrink-0" />
                  ) : (
                    <ArrowDown className="h-3.5 w-3.5 text-purple-400 flex-shrink-0" />
                  )}
                  <span className="text-xs text-white truncate" title={syncStats.currentFile}>
                    {syncStats.currentFile}
                  </span>
                </div>
                <div className="flex items-center space-x-2 flex-shrink-0 ml-2">
                  <span className="text-[10px] text-slate-400">
                    {formatBytes(syncStats.currentFileTransferred || 0)} / {formatBytes(syncStats.currentFileSize || 0)}
                  </span>
                  {(syncStats.uploadSpeed ?? 0) > 0 && (
                    <span className="text-[10px] text-emerald-400">{formatSpeed(syncStats.uploadSpeed)}</span>
                  )}
                  {(syncStats.downloadSpeed ?? 0) > 0 && (syncStats.uploadSpeed ?? 0) === 0 && (
                    <span className="text-[10px] text-purple-400">{formatSpeed(syncStats.downloadSpeed)}</span>
                  )}
                </div>
              </div>
              <div className="h-1.5 bg-slate-700 rounded-full overflow-hidden">
                <div
                  className="h-full bg-gradient-to-r from-blue-500 to-blue-400 transition-all duration-150"
                  style={{ width: `${Math.min(syncStats.currentFilePercent || 0, 100)}%` }}
                />
              </div>
            </div>
          )}

          {hasOverallProgress && syncStats?.totalFiles && (
            <div className="space-y-1.5">
              <div className="flex items-center justify-between">
                <span className="text-xs text-slate-400">Overall progress</span>
                <span className="text-xs text-slate-300">
                  {syncStats.processedFiles || 0} / {syncStats.totalFiles} files
                </span>
              </div>
              <div className="h-1.5 bg-slate-700 rounded-full overflow-hidden">
                <div
                  className="h-full bg-gradient-to-r from-emerald-500 to-emerald-400 transition-all duration-300"
                  style={{
                    width: `${Math.min(
                      ((syncStats.processedFiles || 0) / (syncStats.totalFiles || 1)) * 100,
                      100
                    )}%`,
                  }}
                />
              </div>
            </div>
          )}
        </div>
      )}

      {/* Summary Progress Bar (when not showing detailed transfers) */}
      {!hasFileProgress && !hasOverallProgress && (
        <div className="h-2 bg-slate-700/50 rounded-full overflow-hidden mb-3">
          <div
            className={`h-full rounded-full transition-all duration-500 ${
              status === 'error'
                ? 'bg-gradient-to-r from-red-500 to-red-400'
                : status === 'paused'
                  ? 'bg-gradient-to-r from-amber-500 to-amber-400'
                  : 'bg-gradient-to-r from-emerald-500 to-emerald-400'
            }`}
            style={{ width: `${progressPercent}%` }}
          />
        </div>
      )}

      {/* Footer info */}
      <div className="flex items-center justify-between text-xs text-slate-400 mb-3">
        <span>
          Last sync: {syncStats?.lastSync ? formatRelativeTime(syncStats.lastSync) : 'Never'}
        </span>
        <span>
          {syncStats?.syncFolderCount ?? 0} folders {totalPending > 0 ? `• ${totalPending} pending` : ''}
        </span>
      </div>

      {/* Sync Folders List */}
      {foldersLoading ? (
        <div className="space-y-2">
          {[...Array(3)].map((_, i) => (
            <div key={i} className="h-8 bg-slate-700/30 rounded animate-pulse" />
          ))}
        </div>
      ) : folders.length === 0 ? (
        <p className="text-sm text-slate-500 py-2 text-center">No sync folders configured</p>
      ) : (
        <div className="space-y-1.5 border-t border-white/10 pt-3">
          {folders.map((folder) => {
            const badge = getStatusBadge(folder.status);
            const direction = getDirectionInfo(folder.sync_direction);
            const DirIcon = direction.icon;
            return (
              <div
                key={folder.id}
                className="flex items-center justify-between py-1.5 px-2 rounded-lg bg-white/5"
              >
                <div className="flex items-center space-x-2 min-w-0 flex-1">
                  <FolderSync className="h-3.5 w-3.5 text-orange-400 flex-shrink-0" />
                  <span className="text-xs text-white truncate" title={folder.local_path}>
                    {getFileName(folder.local_path)}
                  </span>
                </div>
                <div className="flex items-center space-x-2 flex-shrink-0 ml-2">
                  <span className={`px-2 py-0.5 rounded-full text-[10px] font-medium ${badge.className}`}>
                    {badge.label}
                  </span>
                  <span title={direction.label}>
                    <DirIcon className={`h-3 w-3 ${direction.className}`} />
                  </span>
                  <span className="text-[10px] text-slate-500 w-14 text-right">
                    {formatRelativeTime(folder.last_sync)}
                  </span>
                </div>
              </div>
            );
          })}
        </div>
      )}

      {/* Manage link */}
      {folders.length > 0 && (
        <div className="mt-3 pt-2 border-t border-white/10">
          <a href="#/sync" className="text-xs text-slate-400 hover:text-white transition-colors">
            Manage folders →
          </a>
        </div>
      )}

      {/* Error Banner */}
      {syncError && (
        <div className="mt-3 rounded-lg bg-red-500/10 border border-red-500/20 p-2.5">
          <div className="flex items-center space-x-2">
            <AlertTriangle className="h-4 w-4 text-red-400 flex-shrink-0" />
            <p className="text-xs text-red-300">{syncError}</p>
          </div>
        </div>
      )}
    </div>
  );
}
```

- [ ] **Step 2: Verify no TypeScript errors**

Run: `cd frontend && npx tsc --noEmit --pretty 2>&1 | head -20`
Expected: No errors related to SyncSummaryCard

- [ ] **Step 3: Commit**

```bash
git add frontend/src/renderer/components/dashboard/SyncSummaryCard.tsx
git commit -m "feat(dashboard): add SyncSummaryCard merging overview, transfers, and folders"
```

---

### Task 4: Rewrite Dashboard.tsx layout

**Files:**
- Modify: `frontend/src/renderer/pages/Dashboard.tsx` (full rewrite)

- [ ] **Step 1: Rewrite Dashboard.tsx with new layout**

Replace the entire content of `frontend/src/renderer/pages/Dashboard.tsx` with:

```tsx
import { Activity } from 'lucide-react';
import { useSyncStatus } from '../hooks/useSyncStatus';
import { ServerStatusStrip } from '../components/dashboard/ServerStatusStrip';
import { QuickStatsGrid } from '../components/dashboard/QuickStatsGrid';
import { PowerCard } from '../components/PowerCard';
import { NetworkWidget } from '../components/dashboard/NetworkWidget';
import { QuickShareCard } from '../components/dashboard/QuickShareCard';
import { ServicesPanel } from '../components/dashboard/ServicesPanel';
import { SyncSummaryCard } from '../components/dashboard/SyncSummaryCard';
import { RecentActivityCard } from '../components/dashboard/RecentActivityCard';
import { RaidStatusCard } from '../components/dashboard/RaidStatusCard';

interface DashboardProps {
  user: { username: string; serverUrl?: string };
  onLogout: () => void;
}

export default function Dashboard({ user: _user, onLogout: _onLogout }: DashboardProps) {
  const { syncStats, systemInfo, raidStatus, loading, syncError } = useSyncStatus();

  return (
    <div className="space-y-6">
      {/* Page Header */}
      <div>
        <h1 className="text-3xl font-bold text-white flex items-center space-x-3">
          <div className="rounded-lg bg-gradient-to-br from-blue-500 to-blue-600 p-3">
            <Activity className="h-6 w-6 text-white" />
          </div>
          <span>Dashboard</span>
        </h1>
      </div>

      {/* Row 1: Server Status Strip */}
      <ServerStatusStrip systemInfo={systemInfo} loading={loading} />

      {/* Row 2: System Metrics Grid (2x2 / 4-col) */}
      <QuickStatsGrid systemInfo={systemInfo} loading={loading} />

      {/* Row 3: Power + Network */}
      <div className="grid gap-6 md:grid-cols-2">
        <PowerCard />
        <NetworkWidget />
      </div>

      {/* Row 4: Quick Share + Services */}
      <div className="grid gap-6 md:grid-cols-2">
        <QuickShareCard />
        <ServicesPanel />
      </div>

      {/* Row 5: Sync Summary */}
      <SyncSummaryCard syncStats={syncStats} syncError={syncError} loading={loading} />

      {/* Row 6: Recent Activity */}
      <RecentActivityCard />

      {/* Row 7: RAID Status (conditional) */}
      <RaidStatusCard raidStatus={raidStatus} />
    </div>
  );
}
```

- [ ] **Step 2: Verify no TypeScript errors**

Run: `cd frontend && npx tsc --noEmit --pretty 2>&1 | head -20`
Expected: No errors

- [ ] **Step 3: Commit**

```bash
git add frontend/src/renderer/pages/Dashboard.tsx
git commit -m "feat(dashboard): rewrite layout to match BaluApp hierarchy"
```

---

### Task 5: Delete obsolete components

**Files:**
- Delete: `frontend/src/renderer/components/dashboard/SyncOverviewCard.tsx`
- Delete: `frontend/src/renderer/components/dashboard/SyncFoldersCard.tsx`
- Delete: `frontend/src/renderer/components/dashboard/ActiveTransfersCard.tsx`
- Delete: `frontend/src/renderer/components/dashboard/NasDetailsPanel.tsx`

- [ ] **Step 1: Check for other imports of these components**

Run: `cd frontend && grep -rn "SyncOverviewCard\|SyncFoldersCard\|ActiveTransfersCard\|NasDetailsPanel" src/ --include="*.tsx" --include="*.ts" | grep -v "^src/renderer/components/dashboard/"`
Expected: Only `Dashboard.tsx` should reference these — and we already removed those imports in Task 4.

- [ ] **Step 2: Delete the files**

```bash
cd frontend
rm src/renderer/components/dashboard/SyncOverviewCard.tsx
rm src/renderer/components/dashboard/SyncFoldersCard.tsx
rm src/renderer/components/dashboard/ActiveTransfersCard.tsx
rm src/renderer/components/dashboard/NasDetailsPanel.tsx
```

- [ ] **Step 3: Verify build still works**

Run: `cd frontend && npx tsc --noEmit --pretty 2>&1 | head -20`
Expected: No errors

- [ ] **Step 4: Commit**

```bash
git add -u frontend/src/renderer/components/dashboard/
git commit -m "refactor(dashboard): remove obsolete components replaced by new layout"
```

---

### Task 6: Verify and test

- [ ] **Step 1: Run TypeScript check on full frontend**

Run: `cd frontend && npx tsc --noEmit --pretty`
Expected: Clean (no errors)

- [ ] **Step 2: Run Vite dev build to verify bundling**

Run: `cd frontend && npx vite build 2>&1 | tail -10`
Expected: Build succeeds

- [ ] **Step 3: Final commit (if any fixes needed)**

Only if previous steps required fixes.
