import { useEffect, useState } from 'react';
import { FolderSync, RefreshCw, ArrowUpDown, ArrowUp, ArrowDown } from 'lucide-react';
import { formatRelativeTime, getFileName } from '../../../lib/formatters';

interface SyncFolder {
  id: string;
  local_path: string;
  remote_path: string;
  status: string;
  enabled: boolean;
  size: number;
  last_sync: string;
  sync_direction: string;
}

function getStatusBadge(status: string) {
  switch (status) {
    case 'syncing':
      return { label: 'Syncing', className: 'bg-emerald-500/20 text-emerald-300' };
    case 'paused':
      return { label: 'Paused', className: 'bg-amber-500/20 text-amber-300' };
    case 'error':
      return { label: 'Error', className: 'bg-red-500/20 text-red-300' };
    default:
      return { label: 'Synced', className: 'bg-blue-500/20 text-blue-300' };
  }
}

function getDirectionInfo(direction: string) {
  switch (direction) {
    case 'upload':
      return { icon: ArrowUp, label: 'Upload', className: 'text-emerald-400' };
    case 'download':
      return { icon: ArrowDown, label: 'Download', className: 'text-purple-400' };
    default:
      return { icon: ArrowUpDown, label: 'Bidirectional', className: 'text-blue-400' };
  }
}

export function SyncFoldersCard() {
  const [folders, setFolders] = useState<SyncFolder[]>([]);
  const [loading, setLoading] = useState(true);

  const fetchFolders = async () => {
    try {
      const response = await window.electronAPI.sendBackendCommand({ type: 'get_folders' });
      if (response?.folders) {
        setFolders(response.folders);
      }
    } catch (err) {
      console.error('Failed to fetch folders:', err);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchFolders();
    const interval = setInterval(fetchFolders, 15000);
    return () => clearInterval(interval);
  }, []);

  const handleSyncAll = async () => {
    try {
      await window.electronAPI.sendBackendCommand({ type: 'trigger_sync', data: {} });
    } catch (err) {
      console.error('Failed to trigger sync:', err);
    }
  };

  return (
    <div className="rounded-xl border border-white/10 bg-gradient-to-br from-orange-500/10 to-orange-600/10 p-4 backdrop-blur-sm transition-all hover:border-orange-500/30">
      <div className="flex items-center justify-between mb-3">
        <div className="flex items-center space-x-2">
          <div className="rounded-lg bg-orange-500/20 p-1.5">
            <FolderSync className="h-4 w-4 text-orange-400" />
          </div>
          <h3 className="text-sm font-medium text-slate-300">Sync Folders</h3>
        </div>
        <button
          onClick={handleSyncAll}
          className="flex items-center space-x-1 px-2 py-1 rounded-lg bg-white/5 hover:bg-white/10 text-xs text-slate-400 hover:text-white transition-all"
          title="Sync all folders"
        >
          <RefreshCw className="h-3 w-3" />
          <span>Sync All</span>
        </button>
      </div>

      {loading ? (
        <div className="space-y-2">
          {[...Array(3)].map((_, i) => (
            <div key={i} className="h-8 bg-slate-700/30 rounded animate-pulse" />
          ))}
        </div>
      ) : folders.length === 0 ? (
        <p className="text-sm text-slate-500 py-4 text-center">No sync folders configured</p>
      ) : (
        <div className="space-y-2">
          {folders.map((folder) => {
            const badge = getStatusBadge(folder.status);
            const direction = getDirectionInfo(folder.sync_direction);
            const DirIcon = direction.icon;
            return (
              <div
                key={folder.id}
                className="flex items-center justify-between py-1.5 px-2 rounded-lg bg-white/5"
              >
                <div className="flex items-center space-x-2 min-w-0 flex-1">
                  <FolderSync className="h-3.5 w-3.5 text-orange-400 flex-shrink-0" />
                  <span className="text-xs text-white truncate" title={folder.local_path}>
                    {getFileName(folder.local_path)}
                  </span>
                </div>
                <div className="flex items-center space-x-2 flex-shrink-0 ml-2">
                  <span className={`px-2 py-0.5 rounded-full text-[10px] font-medium ${badge.className}`}>
                    {badge.label}
                  </span>
                  <span title={direction.label}>
                    <DirIcon className={`h-3 w-3 ${direction.className}`} />
                  </span>
                  <span className="text-[10px] text-slate-500 w-14 text-right">
                    {formatRelativeTime(folder.last_sync)}
                  </span>
                </div>
              </div>
            );
          })}
        </div>
      )}

      {folders.length > 0 && (
        <div className="mt-3 pt-2 border-t border-white/10">
          <a href="#/sync" className="text-xs text-slate-400 hover:text-white transition-colors">
            Manage folders →
          </a>
        </div>
      )}
    </div>
  );
}
