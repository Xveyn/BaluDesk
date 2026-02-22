import { Wifi, ArrowUp, ArrowDown } from 'lucide-react';
import { useNetworkStats } from '../../hooks/useNetworkStats';
import { formatBytes } from '../../../lib/formatters';

const formatSpeed = (bytesPerSecond: number): string => {
  if (bytesPerSecond < 1024) return `${bytesPerSecond.toFixed(0)} B/s`;
  if (bytesPerSecond < 1024 * 1024) return `${(bytesPerSecond / 1024).toFixed(1)} KB/s`;
  return `${(bytesPerSecond / (1024 * 1024)).toFixed(1)} MB/s`;
};

export function NetworkWidget() {
  const { stats, loading, error } = useNetworkStats();

  return (
    <div className="rounded-xl border border-white/10 bg-gradient-to-br from-sky-500/10 to-sky-600/10 p-4 backdrop-blur-sm hover:border-sky-500/30 hover:shadow-lg hover:shadow-sky-500/20 transition-all">
      <div className="flex items-center space-x-2 mb-3">
        <div className="rounded-lg bg-gradient-to-br from-sky-500/20 to-sky-600/20 p-1.5">
          <Wifi className="h-4 w-4 text-sky-400" />
        </div>
        <h3 className="text-sm font-medium text-slate-300">Network</h3>
      </div>

      {loading ? (
        <div className="space-y-3">
          <div className="h-5 bg-slate-700/30 rounded animate-pulse" />
          <div className="h-5 bg-slate-700/30 rounded animate-pulse" />
        </div>
      ) : error ? (
        <p className="text-xs text-slate-500">{error}</p>
      ) : stats ? (
        <div className="space-y-2">
          <div className="flex items-center justify-between">
            <div className="flex items-center space-x-1.5">
              <ArrowUp className="h-3.5 w-3.5 text-emerald-400" />
              <span className="text-xs text-slate-400">Upload</span>
            </div>
            <span className="text-sm font-semibold text-white">
              {formatSpeed(stats.uploadSpeed)}
            </span>
          </div>
          <div className="flex items-center justify-between">
            <div className="flex items-center space-x-1.5">
              <ArrowDown className="h-3.5 w-3.5 text-purple-400" />
              <span className="text-xs text-slate-400">Download</span>
            </div>
            <span className="text-sm font-semibold text-white">
              {formatSpeed(stats.downloadSpeed)}
            </span>
          </div>
          <div className="mt-2 pt-2 border-t border-white/10">
            <div className="flex items-center justify-between">
              <span className="text-xs text-slate-400">Today</span>
              <span className="text-xs text-slate-300">
                {formatBytes(stats.totalUpToday + stats.totalDownToday)}
              </span>
            </div>
          </div>
        </div>
      ) : (
        <p className="text-xs text-slate-500">No data available</p>
      )}
    </div>
  );
}
