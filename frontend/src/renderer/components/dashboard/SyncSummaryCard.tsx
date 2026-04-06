import { useEffect, useState } from 'react';
import {
  CheckCircle2, RefreshCw, Pause, AlertTriangle,
  ArrowUp, ArrowDown, ArrowUpDown, FolderSync,
} from 'lucide-react';
import { formatSpeed, formatBytes, formatRelativeTime, getFileName } from '../../../lib/formatters';
import { SyncStats } from '../../hooks/useSyncStatus';

interface SyncSummaryCardProps {
  syncStats: SyncStats | null;
  syncError: string | null;
  loading: boolean;
}

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
  const totalPending = (syncStats?.pendingUploads || 0) + (syncStats?.pendingDownloads || 0);
  const isSyncing = status === 'syncing';
  const totalFiles = syncStats?.totalFiles || 0;
  const processedFiles = syncStats?.processedFiles || 0;
  const progressPercent = totalFiles > 0
    ? Math.min((processedFiles / totalFiles) * 100, 100)
    : (isSyncing ? 0 : 100);

  const hasFileProgress = isSyncing && syncStats?.currentFile && (syncStats?.currentFileSize ?? 0) > 0;
  const hasOverallProgress = isSyncing && totalFiles > 0;
  const hasActiveTransfers = hasFileProgress || hasOverallProgress;

  return (
    <div className="rounded-xl border border-white/10 bg-gradient-to-br from-emerald-500/10 to-cyan-600/10 p-5 backdrop-blur-sm transition-all hover:border-emerald-500/30">

      {/* 1. Header row */}
      <div className="flex items-center justify-between mb-4">
        {/* Status icon + badge */}
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

        {/* Upload/download counters + Sync All button */}
        <div className="flex items-center space-x-4">
          <div className="flex items-center space-x-1.5">
            <ArrowUp className="h-4 w-4 text-emerald-400" />
            <span className="text-sm font-medium text-white">{syncStats?.pendingUploads || 0}</span>
            {syncStats?.uploadSpeed != null && syncStats.uploadSpeed > 0 && (
              <span className="text-xs text-emerald-400">{formatSpeed(syncStats.uploadSpeed)}</span>
            )}
          </div>
          <div className="flex items-center space-x-1.5">
            <ArrowDown className="h-4 w-4 text-purple-400" />
            <span className="text-sm font-medium text-white">{syncStats?.pendingDownloads || 0}</span>
            {syncStats?.downloadSpeed != null && syncStats.downloadSpeed > 0 && (
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

      {/* 2. Active Transfers section (only when syncing) */}
      {hasActiveTransfers && (
        <div className="rounded-lg bg-white/5 p-3 mb-3 space-y-3">
          {/* Current file transfer */}
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
                    <span className="text-[10px] text-emerald-400">
                      {formatSpeed(syncStats.uploadSpeed)}
                    </span>
                  )}
                  {(syncStats.downloadSpeed ?? 0) > 0 && (syncStats.uploadSpeed ?? 0) === 0 && (
                    <span className="text-[10px] text-purple-400">
                      {formatSpeed(syncStats.downloadSpeed)}
                    </span>
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

          {/* Overall file progress */}
          {hasOverallProgress && totalFiles > 0 && (
            <div className="space-y-1.5">
              <div className="flex items-center justify-between">
                <span className="text-xs text-slate-400">Overall progress</span>
                <span className="text-xs text-slate-300">
                  {processedFiles} / {totalFiles} files
                </span>
              </div>
              <div className="h-1.5 bg-slate-700 rounded-full overflow-hidden">
                <div
                  className="h-full bg-gradient-to-r from-emerald-500 to-emerald-400 transition-all duration-300"
                  style={{
                    width: `${Math.min(
                      ((processedFiles) / (totalFiles || 1)) * 100,
                      100
                    )}%`,
                  }}
                />
              </div>
            </div>
          )}
        </div>
      )}

      {/* 3. Summary progress bar (only when NOT showing active transfers) */}
      {!hasActiveTransfers && (
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

      {/* 4. Footer info */}
      <div className="flex items-center justify-between text-xs text-slate-400 mb-4">
        <span>
          Last sync: {syncStats?.lastSync ? formatRelativeTime(syncStats.lastSync) : 'Never'}
        </span>
        <span>
          {syncStats?.syncFolderCount ?? 0} folders{totalPending > 0 ? ` • ${totalPending} pending` : ''}
        </span>
      </div>

      {/* 5. Sync Folders list */}
      <div className="border-t border-white/10 pt-3">
        {foldersLoading ? (
          <div className="space-y-2">
            {[...Array(3)].map((_, i) => (
              <div key={i} className="h-8 bg-slate-700/30 rounded animate-pulse" />
            ))}
          </div>
        ) : folders.length === 0 ? (
          <p className="text-sm text-slate-500 py-4 text-center">No sync folders configured</p>
        ) : (
          <div className="space-y-2">
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

        {/* 6. Manage link */}
        {folders.length > 0 && (
          <div className="mt-3 pt-2 border-t border-white/10">
            <a href="#/sync" className="text-xs text-slate-400 hover:text-white transition-colors">
              Manage folders →
            </a>
          </div>
        )}
      </div>

      {/* 7. Error banner */}
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
