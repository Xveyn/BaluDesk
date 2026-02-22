import { Activity } from 'lucide-react';
import { formatTimestamp } from '../../lib/formatters';
import { useSyncStatus } from '../hooks/useSyncStatus';
import { QuickStatsGrid } from '../components/dashboard/QuickStatsGrid';
import { SyncActivityCard } from '../components/dashboard/SyncActivityCard';
import { SyncFoldersCard } from '../components/dashboard/SyncFoldersCard';
import { NetworkWidget } from '../components/dashboard/NetworkWidget';
import { ServicesPanel } from '../components/dashboard/ServicesPanel';
import { RaidStatusCard } from '../components/dashboard/RaidStatusCard';
import { RecentActivityCard } from '../components/dashboard/RecentActivityCard';
import { PowerCard } from '../components/PowerCard';

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
        <p className="mt-2 text-slate-400">
          Monitor your NAS and sync status • Last sync: {syncStats?.lastSync ? formatTimestamp(syncStats.lastSync) : 'Never'}
        </p>
      </div>

      {/* Quick Stats: CPU, RAM, Disk, Uptime */}
      <QuickStatsGrid systemInfo={systemInfo} loading={loading} />

      {/* Sync Overview: Activity + Folders */}
      <div className="grid gap-6 md:grid-cols-2">
        <SyncActivityCard syncStats={syncStats} syncError={syncError} />
        <SyncFoldersCard />
      </div>

      {/* Monitoring: Network, Services, Power */}
      <div className="grid gap-6 md:grid-cols-3">
        <NetworkWidget />
        <ServicesPanel />
        <PowerCard />
      </div>

      {/* RAID + Activity Feed */}
      <div className="grid gap-6 md:grid-cols-2">
        <RaidStatusCard raidStatus={raidStatus} />
        <RecentActivityCard />
      </div>
    </div>
  );
}
