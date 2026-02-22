import { CheckCircle2, RefreshCw, Pause, AlertTriangle, ArrowUp, ArrowDown } from 'lucide-react';
import { formatSpeed, formatRelativeTime } from '../../../lib/formatters';
import { SyncStats } from '../../hooks/useSyncStatus';

interface SyncOverviewCardProps {
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

export function SyncOverviewCard({ syncStats, syncError, loading }: SyncOverviewCardProps) {
  if (loading) {
    return (
      <div className="rounded-xl border border-white/10 bg-gradient-to-br from-emerald-500/10 to-emerald-600/10 p-5 backdrop-blur-sm">
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

  return (
    <div className="rounded-xl border border-white/10 bg-gradient-to-br from-emerald-500/10 to-emerald-600/10 p-5 backdrop-blur-sm transition-all hover:border-emerald-500/30">
      {/* Status Row */}
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

        {/* Upload / Download counters */}
        <div className="flex items-center space-x-4">
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
        </div>
      </div>

      {/* Progress Bar */}
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

      {/* Footer */}
      <div className="flex items-center justify-between text-xs text-slate-400">
        <span>
          Last sync: {syncStats?.lastSync ? formatRelativeTime(syncStats.lastSync) : 'Never'}
        </span>
        <span>
          {syncStats?.syncFolderCount ?? 0} folders {totalPending > 0 ? `• ${totalPending} pending` : ''}
        </span>
      </div>

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
