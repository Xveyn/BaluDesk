import { ArrowUpDown, ArrowUp, ArrowDown } from 'lucide-react';
import { formatBytes } from '../../../lib/formatters';

interface SyncStats {
  status: string;
  uploadSpeed: number;
  downloadSpeed: number;
  pendingUploads: number;
  pendingDownloads: number;
  lastSync: string;
  syncFolderCount?: number;
  currentFile?: string;
  totalFiles?: number;
  processedFiles?: number;
  currentFileSize?: number;
  currentFileTransferred?: number;
  currentFilePercent?: number;
}

interface SyncActivityCardProps {
  syncStats: SyncStats | null;
  syncError: string | null;
}

const formatSpeed = (bytesPerSecond: number): string => {
  if (bytesPerSecond < 1024) return `${bytesPerSecond.toFixed(0)} B/s`;
  if (bytesPerSecond < 1024 * 1024) return `${(bytesPerSecond / 1024).toFixed(1)} KB/s`;
  return `${(bytesPerSecond / (1024 * 1024)).toFixed(1)} MB/s`;
};

export function SyncActivityCard({ syncStats, syncError }: SyncActivityCardProps) {
  return (
    <div className="group relative overflow-hidden rounded-xl border border-white/10 bg-gradient-to-br from-emerald-500/10 via-purple-500/10 to-purple-600/10 p-4 backdrop-blur-sm transition-all hover:border-emerald-500/30 hover:shadow-lg hover:shadow-emerald-500/20">
      <div className="flex items-center space-x-2 mb-3">
        <div className="rounded-lg bg-gradient-to-br from-emerald-500/20 to-purple-500/20 p-1.5">
          <ArrowUpDown className="h-4 w-4 text-emerald-400" />
        </div>
        <h3 className="text-sm font-medium text-slate-300">Sync Activity</h3>
        {syncStats?.status && (
          <span className={`ml-auto px-2 py-0.5 rounded-full text-xs font-medium ${
            syncStats.status === 'syncing'
              ? 'bg-emerald-500/20 text-emerald-300'
              : syncStats.status === 'paused'
                ? 'bg-amber-500/20 text-amber-300'
                : syncStats.status === 'error'
                  ? 'bg-red-500/20 text-red-300'
                  : 'bg-slate-700/30 text-slate-400'
          }`}>
            {syncStats.status.charAt(0).toUpperCase() + syncStats.status.slice(1)}
          </span>
        )}
      </div>

      <div className="space-y-2">
        {/* Upload Section */}
        <div className="flex items-center justify-between">
          <div className="flex items-center space-x-2">
            <ArrowUp className="h-4 w-4 text-emerald-400" />
            <span className="text-xs text-slate-400">Uploads</span>
          </div>
          <div className="flex items-center space-x-2">
            <span className="text-sm font-semibold text-white">
              {syncStats?.pendingUploads || 0}
            </span>
            {syncStats?.uploadSpeed && syncStats.uploadSpeed > 0 && (
              <span className="text-xs text-emerald-400">
                {formatSpeed(syncStats.uploadSpeed)}
              </span>
            )}
          </div>
        </div>

        {/* Download Section */}
        <div className="flex items-center justify-between">
          <div className="flex items-center space-x-2">
            <ArrowDown className="h-4 w-4 text-purple-400" />
            <span className="text-xs text-slate-400">Downloads</span>
          </div>
          <div className="flex items-center space-x-2">
            <span className="text-sm font-semibold text-white">
              {syncStats?.pendingDownloads || 0}
            </span>
            {syncStats?.downloadSpeed && syncStats.downloadSpeed > 0 && (
              <span className="text-xs text-purple-400">
                {formatSpeed(syncStats.downloadSpeed)}
              </span>
            )}
          </div>
        </div>

        {/* Total Pending */}
        <div className="mt-2 pt-2 border-t border-white/10">
          <div className="flex items-center justify-between">
            <span className="text-xs text-slate-400">Total Pending</span>
            <span className="text-lg font-bold text-white">
              {(syncStats?.pendingUploads || 0) + (syncStats?.pendingDownloads || 0)}
            </span>
          </div>
        </div>

        {/* Sync Error Banner */}
        {syncError && (
          <div className="mt-2 pt-2 border-t border-red-500/30">
            <div className="rounded-lg bg-red-500/10 border border-red-500/20 p-2">
              <p className="text-xs text-red-300">{syncError}</p>
            </div>
          </div>
        )}

        {/* Sync Progress (shown when syncing) */}
        {syncStats?.status === 'syncing' && (syncStats?.totalFiles ?? 0) > 0 && (
          <div className="mt-2 pt-2 border-t border-white/10 space-y-2">
            {/* Overall file progress */}
            <div className="flex items-center justify-between">
              <span className="text-xs text-slate-400">Files</span>
              <span className="text-xs text-slate-300">
                {syncStats.processedFiles || 0} / {syncStats.totalFiles}
              </span>
            </div>
            <div className="h-1.5 bg-slate-700 rounded-full overflow-hidden">
              <div
                className="h-full bg-gradient-to-r from-blue-500 to-blue-600 transition-all duration-300"
                style={{
                  width: `${Math.min(
                    ((syncStats.processedFiles || 0) / (syncStats.totalFiles || 1)) * 100,
                    100
                  )}%`,
                }}
              />
            </div>

            {/* Per-file progress */}
            {syncStats.currentFile && (syncStats.currentFileSize ?? 0) > 0 && (
              <div className="space-y-1">
                <div className="flex items-center justify-between">
                  <p className="text-xs text-slate-400 truncate max-w-[60%]" title={syncStats.currentFile}>
                    {syncStats.currentFile}
                  </p>
                  <div className="flex items-center space-x-2">
                    <span className="text-xs text-slate-300">
                      {formatBytes(syncStats.currentFileTransferred || 0)} / {formatBytes(syncStats.currentFileSize || 0)}
                    </span>
                    {syncStats.uploadSpeed > 0 && (
                      <span className="text-xs text-emerald-400">
                        {formatSpeed(syncStats.uploadSpeed)}
                      </span>
                    )}
                  </div>
                </div>
                <div className="h-1 bg-slate-700 rounded-full overflow-hidden">
                  <div
                    className="h-full bg-gradient-to-r from-emerald-500 to-emerald-400 transition-all duration-150"
                    style={{
                      width: `${Math.min(syncStats.currentFilePercent || 0, 100)}%`,
                    }}
                  />
                </div>
              </div>
            )}

            {/* Current file name only (when no size info available) */}
            {syncStats.currentFile && !(syncStats.currentFileSize && syncStats.currentFileSize > 0) && (
              <p className="text-xs text-slate-500 truncate" title={syncStats.currentFile}>
                {syncStats.currentFile}
              </p>
            )}
          </div>
        )}
      </div>
    </div>
  );
}
