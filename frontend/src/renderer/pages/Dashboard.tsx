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
