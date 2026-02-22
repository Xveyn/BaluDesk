import { useState } from 'react';
import { ChevronRight, Server } from 'lucide-react';
import { formatBytes, formatUptime } from '../../../lib/formatters';
import { getMemoryPercent } from '../../../lib/calculations';
import { QuickStatsGrid } from './QuickStatsGrid';
import { NetworkWidget } from './NetworkWidget';
import { ServicesPanel } from './ServicesPanel';
import { RaidStatusCard } from './RaidStatusCard';
import { PowerCard } from '../PowerCard';
import { SystemInfo, RaidStatus } from '../../hooks/useSyncStatus';

interface NasDetailsPanelProps {
  systemInfo: SystemInfo | null;
  raidStatus: RaidStatus | null;
  loading: boolean;
}

function getSummaryLine(systemInfo: SystemInfo | null): string {
  if (!systemInfo) return 'No data available';

  const parts: string[] = [];
  parts.push(`CPU ${systemInfo.cpu.usage.toFixed(0)}%`);

  const memPercent = getMemoryPercent(systemInfo.memory);
  parts.push(`RAM ${memPercent.toFixed(0)}%`);

  parts.push(`Disk ${formatBytes(systemInfo.disk.used)} / ${formatBytes(systemInfo.disk.total)}`);

  const serverU = systemInfo.serverUptime;
  const uptimeToShow = (typeof serverU === 'number' && serverU > 0) ? serverU : systemInfo.uptime;
  parts.push(`Up ${formatUptime(uptimeToShow)}`);

  return parts.join(' | ');
}

export function NasDetailsPanel({ systemInfo, raidStatus, loading }: NasDetailsPanelProps) {
  const [isOpen, setIsOpen] = useState(false);

  return (
    <div className="rounded-xl border border-white/10 bg-white/[0.02] backdrop-blur-sm overflow-hidden">
      {/* Header (always visible) */}
      <button
        onClick={() => setIsOpen(!isOpen)}
        className="w-full flex items-center justify-between px-4 py-3 hover:bg-white/5 transition-colors"
      >
        <div className="flex items-center space-x-3">
          <div className="rounded-lg bg-slate-700/50 p-1.5">
            <Server className="h-4 w-4 text-slate-400" />
          </div>
          <span className="text-sm font-medium text-slate-300">NAS Details</span>
          <span className="text-xs text-slate-500 hidden sm:inline">
            {!loading && getSummaryLine(systemInfo)}
          </span>
        </div>
        <ChevronRight
          className={`h-4 w-4 text-slate-500 transition-transform duration-200 ${
            isOpen ? 'rotate-90' : ''
          }`}
        />
      </button>

      {/* Expandable content */}
      <div
        className={`transition-all duration-300 ease-in-out overflow-hidden ${
          isOpen ? 'max-h-[2000px] opacity-100' : 'max-h-0 opacity-0'
        }`}
      >
        <div className="px-4 pb-4 space-y-4 border-t border-white/5">
          {/* System Stats Grid */}
          <div className="pt-4">
            <QuickStatsGrid systemInfo={systemInfo} loading={loading} />
          </div>

          {/* Network + Services + Power */}
          <div className="grid gap-4 md:grid-cols-3">
            <NetworkWidget />
            <ServicesPanel />
            <PowerCard />
          </div>

          {/* RAID Status (only shows if RAID data exists) */}
          <RaidStatusCard raidStatus={raidStatus} />
        </div>
      </div>
    </div>
  );
}
