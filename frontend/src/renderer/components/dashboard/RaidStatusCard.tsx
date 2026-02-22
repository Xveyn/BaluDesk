import { Zap } from 'lucide-react';
import { formatBytes } from '../../../lib/formatters';

interface RaidDevice {
  name: string;
  state: string;
}

interface RaidArray {
  name: string;
  level: string;
  status: string;
  size_bytes: number;
  resync_progress?: number;
  devices: RaidDevice[];
}

interface RaidStatus {
  arrays: RaidArray[];
  dev_mode?: boolean;
}

interface RaidStatusCardProps {
  raidStatus: RaidStatus | null;
}

export function RaidStatusCard({ raidStatus }: RaidStatusCardProps) {
  if (!raidStatus || !raidStatus.arrays || raidStatus.arrays.length === 0) {
    return null;
  }

  return (
    <div className="space-y-3">
      <div className="flex items-center space-x-2">
        <div className="rounded-lg bg-gradient-to-br from-red-500 to-red-600 p-2">
          <Zap className="h-4 w-4 text-white" />
        </div>
        <h3 className="text-sm font-semibold text-white">RAID Status</h3>
      </div>

      <div className="space-y-3">
        {raidStatus.arrays.map((array) => (
          <div
            key={array.name}
            className={`rounded-xl border p-4 transition-all ${
              array.status === 'optimal'
                ? 'border-emerald-500/30 bg-emerald-500/10'
                : array.status === 'degraded'
                  ? 'border-amber-500/30 bg-amber-500/10'
                  : array.status === 'rebuilding'
                    ? 'border-blue-500/30 bg-blue-500/10'
                    : 'border-slate-700/50 bg-slate-900/30'
            }`}
          >
            <div className="flex items-start justify-between mb-3">
              <div>
                <p className="font-semibold text-white">
                  {array.name} - RAID{array.level.replace('RAID', '')}
                </p>
                <p className="text-sm text-slate-400">
                  {formatBytes(array.size_bytes)}
                  {array.status === 'rebuilding' &&
                    ` (${array.resync_progress?.toFixed(1) || 0}% synced)`}
                </p>
              </div>
              <span
                className={`px-3 py-1 rounded-full text-xs font-medium ${
                  array.status === 'optimal'
                    ? 'bg-emerald-500/30 text-emerald-200'
                    : array.status === 'degraded'
                      ? 'bg-amber-500/30 text-amber-200'
                      : array.status === 'rebuilding'
                        ? 'bg-blue-500/30 text-blue-200'
                        : 'bg-slate-700/30 text-slate-300'
                }`}
              >
                {array.status.charAt(0).toUpperCase() + array.status.slice(1)}
              </span>
            </div>

            <div className="space-y-2">
              {array.devices.map((device) => (
                <div
                  key={device.name}
                  className={`text-xs rounded px-2 py-1 ${
                    device.state === 'active'
                      ? 'bg-emerald-500/20 text-emerald-300'
                      : device.state === 'failed'
                        ? 'bg-red-500/20 text-red-300'
                        : device.state === 'spare'
                          ? 'bg-blue-500/20 text-blue-300'
                          : 'bg-slate-700/20 text-slate-300'
                  }`}
                >
                  {device.name} ({device.state})
                </div>
              ))}
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}
