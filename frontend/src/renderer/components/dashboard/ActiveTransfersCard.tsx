import { ArrowUpDown, ArrowUp, ArrowDown } from 'lucide-react';
import { formatSpeed, formatBytes } from '../../../lib/formatters';
import { SyncStats } from '../../hooks/useSyncStatus';

interface ActiveTransfersCardProps {
  syncStats: SyncStats | null;
}

export function ActiveTransfersCard({ syncStats }: ActiveTransfersCardProps) {
  const isSyncing = syncStats?.status === 'syncing';
  const hasFileProgress = isSyncing && syncStats?.currentFile && (syncStats?.currentFileSize ?? 0) > 0;
  const hasOverallProgress = isSyncing && (syncStats?.totalFiles ?? 0) > 0;
  const hasActivity = hasFileProgress || hasOverallProgress;

  return (
    <div className="rounded-xl border border-white/10 bg-gradient-to-br from-blue-500/10 to-blue-600/10 p-4 backdrop-blur-sm transition-all hover:border-blue-500/30">
      <div className="flex items-center space-x-2 mb-3">
        <div className="rounded-lg bg-blue-500/20 p-1.5">
          <ArrowUpDown className="h-4 w-4 text-blue-400" />
        </div>
        <h3 className="text-sm font-medium text-slate-300">Active Transfers</h3>
      </div>

      {!hasActivity ? (
        <div className="py-6 text-center">
          <p className="text-sm text-slate-500">No active transfers</p>
        </div>
      ) : (
        <div className="space-y-3">
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

          {/* Current file name only (no size info) */}
          {isSyncing && syncStats?.currentFile && !hasFileProgress && (
            <p className="text-xs text-slate-500 truncate" title={syncStats.currentFile}>
              {syncStats.currentFile}
            </p>
          )}
        </div>
      )}
    </div>
  );
}
