import { Cpu, HardDrive, Clock } from 'lucide-react';
import { formatBytes, formatUptime } from '../../../lib/formatters';
import { getMemoryPercent, getDiskPercent } from '../../../lib/calculations';

interface SystemInfo {
  cpu: {
    usage: number;
    cores: number;
    frequency_mhz?: number | null;
    model?: string | null;
  };
  memory: {
    total: number;
    used: number;
    free: number;
    speed_mts?: number | null;
    type?: string | null;
  };
  disk: {
    total: number;
    used: number;
    free: number;
  };
  uptime: number;
  serverUptime?: number;
}

interface QuickStatsGridProps {
  systemInfo: SystemInfo | null;
  loading: boolean;
}

export function QuickStatsGrid({ systemInfo, loading }: QuickStatsGridProps) {
  if (loading) {
    return (
      <div className="grid gap-4 grid-cols-2 lg:grid-cols-4">
        {[...Array(4)].map((_, i) => (
          <div
            key={i}
            className="rounded-xl border border-white/10 bg-white/5 p-4 backdrop-blur-sm"
          >
            <div className="space-y-3">
              <div className="flex items-center justify-between">
                <div className="h-4 w-16 bg-slate-700 rounded animate-pulse" />
                <div className="h-7 w-7 bg-slate-700 rounded animate-pulse" />
              </div>
              <div className="h-8 w-20 bg-slate-700 rounded animate-pulse" />
            </div>
          </div>
        ))}
      </div>
    );
  }

  if (!systemInfo) {
    return (
      <div className="rounded-xl border border-red-500/30 bg-red-500/10 p-6">
        <p className="text-sm text-red-400">
          Unable to load system information. Please try refreshing.
        </p>
      </div>
    );
  }

  const memPercent = getMemoryPercent(systemInfo.memory);
  const diskPercent = getDiskPercent(systemInfo.disk);
  const serverU = systemInfo.serverUptime;
  const uptimeToShow = (typeof serverU === 'number' && serverU > 0) ? serverU : systemInfo.uptime;

  return (
    <div className="grid gap-4 grid-cols-2 lg:grid-cols-4">
      {/* CPU */}
      <div className="group relative overflow-hidden rounded-xl border border-white/10 bg-gradient-to-br from-cyan-500/10 to-cyan-600/10 p-4 backdrop-blur-sm transition-all hover:border-cyan-500/30 hover:shadow-lg hover:shadow-cyan-500/20">
        <div className="space-y-2">
          <div className="flex items-center justify-between">
            <p className="text-sm font-medium text-slate-400">CPU</p>
            <div className="rounded-lg bg-cyan-500/20 p-1.5">
              <Cpu className="h-4 w-4 text-cyan-400" />
            </div>
          </div>
          <p className="text-3xl font-bold text-white">
            {systemInfo.cpu.usage.toFixed(1)}%
          </p>
          <div className="space-y-1">
            <p className="text-xs text-slate-400">
              {systemInfo.cpu.cores} cores
            </p>
            {systemInfo.cpu.frequency_mhz && (
              <p className="text-xs text-slate-400">
                {systemInfo.cpu.frequency_mhz.toFixed(0)} MHz
              </p>
            )}
          </div>
        </div>
      </div>

      {/* RAM */}
      <div className="group relative overflow-hidden rounded-xl border border-white/10 bg-gradient-to-br from-pink-500/10 to-pink-600/10 p-4 backdrop-blur-sm transition-all hover:border-pink-500/30 hover:shadow-lg hover:shadow-pink-500/20">
        <div className="space-y-2">
          <div className="flex items-center justify-between">
            <p className="text-sm font-medium text-slate-400">RAM</p>
            <div className="rounded-lg bg-pink-500/20 p-1.5">
              <div className="h-4 w-4 bg-pink-400 rounded" />
            </div>
          </div>
          <p className="text-3xl font-bold text-white">
            {memPercent.toFixed(1)}%
          </p>
          <div className="space-y-1">
            <div className="h-1.5 w-full bg-slate-700 rounded-full overflow-hidden">
              <div
                className="h-full bg-gradient-to-r from-pink-500 to-pink-600 transition-all"
                style={{ width: `${Math.min(memPercent, 100)}%` }}
              />
            </div>
            <p className="text-xs text-slate-400">
              {formatBytes(systemInfo.memory.used)} / {formatBytes(systemInfo.memory.total)}
            </p>
          </div>
        </div>
      </div>

      {/* Disk */}
      <div className="group relative overflow-hidden rounded-xl border border-white/10 bg-gradient-to-br from-amber-500/10 to-amber-600/10 p-4 backdrop-blur-sm transition-all hover:border-amber-500/30 hover:shadow-lg hover:shadow-amber-500/20">
        <div className="space-y-2">
          <div className="flex items-center justify-between">
            <p className="text-sm font-medium text-slate-400">Disk</p>
            <div className="rounded-lg bg-amber-500/20 p-1.5">
              <HardDrive className="h-4 w-4 text-amber-400" />
            </div>
          </div>
          <p className="text-3xl font-bold text-white">
            {diskPercent.toFixed(1)}%
          </p>
          <div className="space-y-1">
            <div className="h-1.5 w-full bg-slate-700 rounded-full overflow-hidden">
              <div
                className="h-full bg-gradient-to-r from-amber-500 to-amber-600 transition-all"
                style={{ width: `${Math.min(diskPercent, 100)}%` }}
              />
            </div>
            <p className="text-xs text-slate-400">
              {formatBytes(systemInfo.disk.used)} / {formatBytes(systemInfo.disk.total)}
            </p>
          </div>
        </div>
      </div>

      {/* Uptime */}
      <div className="group relative overflow-hidden rounded-xl border border-white/10 bg-gradient-to-br from-lime-500/10 to-lime-600/10 p-4 backdrop-blur-sm transition-all hover:border-lime-500/30 hover:shadow-lg hover:shadow-lime-500/20">
        <div className="space-y-2">
          <div className="flex items-center justify-between">
            <p className="text-sm font-medium text-slate-400">Uptime</p>
            <div className="rounded-lg bg-lime-500/20 p-1.5">
              <Clock className="h-4 w-4 text-lime-400" />
            </div>
          </div>
          <p className="text-3xl font-bold text-white">{formatUptime(uptimeToShow)}</p>
          <p className="text-xs text-slate-400">{(uptimeToShow / 86400).toFixed(1)} days</p>
        </div>
      </div>
    </div>
  );
}
