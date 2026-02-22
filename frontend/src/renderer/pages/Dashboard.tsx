import { Activity } from 'lucide-react';
import { useSyncStatus } from '../hooks/useSyncStatus';
import { SyncOverviewCard } from '../components/dashboard/SyncOverviewCard';
import { SyncFoldersCard } from '../components/dashboard/SyncFoldersCard';
import { ActiveTransfersCard } from '../components/dashboard/ActiveTransfersCard';
import { RecentActivityCard } from '../components/dashboard/RecentActivityCard';
import { NasDetailsPanel } from '../components/dashboard/NasDetailsPanel';

interface DashboardProps {
  user: { username: string; serverUrl?: string };
  onLogout: () => void;
}

function getStatusSubtitle(status: string | undefined, pendingUploads: number, pendingDownloads: number): string {
  const total = pendingUploads + pendingDownloads;
  switch (status) {
    case 'syncing':
      return total > 0 ? `${total} files syncing...` : 'Syncing...';
    case 'paused':
      return 'Sync paused';
    case 'error':
      return 'Sync error occurred';
    default:
      return 'All files are synced';
  }
}

export default function Dashboard({ user: _user, onLogout: _onLogout }: DashboardProps) {
  const { syncStats, systemInfo, raidStatus, loading, syncError } = useSyncStatus();

  const subtitle = getStatusSubtitle(
    syncStats?.status,
    syncStats?.pendingUploads || 0,
    syncStats?.pendingDownloads || 0
  );

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
        <p className="mt-2 text-slate-400">{subtitle}</p>
      </div>

      {/* Row 1: Sync Overview (full width) */}
      <SyncOverviewCard syncStats={syncStats} syncError={syncError} loading={loading} />

      {/* Row 2: Sync Folders (full width) */}
      <SyncFoldersCard />

      {/* Row 3: Active Transfers + Recent Activity (2 columns) */}
      <div className="grid gap-6 md:grid-cols-2">
        <ActiveTransfersCard syncStats={syncStats} />
        <RecentActivityCard />
      </div>

      {/* Row 4: NAS Details (collapsible, closed by default) */}
      <NasDetailsPanel systemInfo={systemInfo} raidStatus={raidStatus} loading={loading} />
    </div>
  );
}
