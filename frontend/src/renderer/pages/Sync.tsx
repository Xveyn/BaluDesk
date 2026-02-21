import { useEffect, useState } from 'react';
import { FolderSync, FolderPlus, Trash2, Settings, CheckCircle, Circle, Clock, RefreshCw } from 'lucide-react';
import toast from 'react-hot-toast';
import { formatSize } from '../../lib/formatters';
import { BackendMessage, BackendResponse } from '../../lib/types';
import RemoteFolderBrowser from '../components/RemoteFolderBrowser';

// Type assertion for Electron API
declare const window: any;

type SyncDirectionType = 'push' | 'pull' | 'bidirectional';

interface SyncFolder {
  id: string;
  localPath: string;
  remotePath: string;
  enabled: boolean;
  status?: string;
  size?: number;
  lastSync?: string;
  syncDirection: SyncDirectionType;
  conflictResolution?: string;
}

// Format a relative time string from a timestamp
function formatLastSync(timestamp?: string): string {
  if (!timestamp) return 'Never';

  // Try parsing as ISO date or unix timestamp
  let date: Date;
  if (/^\d+$/.test(timestamp)) {
    date = new Date(parseInt(timestamp, 10) * 1000);
  } else {
    date = new Date(timestamp);
  }

  if (isNaN(date.getTime())) return 'Never';

  const now = new Date();
  const diffMs = now.getTime() - date.getTime();
  const diffSec = Math.floor(diffMs / 1000);
  const diffMin = Math.floor(diffSec / 60);
  const diffHours = Math.floor(diffMin / 60);
  const diffDays = Math.floor(diffHours / 24);

  if (diffSec < 60) return 'Just now';
  if (diffMin < 60) return `${diffMin}m ago`;
  if (diffHours < 24) return `${diffHours}h ago`;
  if (diffDays < 7) return `${diffDays}d ago`;
  return date.toLocaleDateString();
}

// Status badge component
function StatusBadge({ status }: { status?: string }) {
  switch (status) {
    case 'syncing':
      return (
        <span className="rounded-full px-3 py-1 text-xs font-medium bg-blue-500/20 text-blue-400 animate-pulse">
          Syncing...
        </span>
      );
    case 'error':
      return (
        <span className="rounded-full px-3 py-1 text-xs font-medium bg-red-500/20 text-red-400">
          Error
        </span>
      );
    case 'paused':
      return (
        <span className="rounded-full px-3 py-1 text-xs font-medium bg-slate-700 text-slate-400">
          Paused
        </span>
      );
    default:
      return (
        <span className="rounded-full px-3 py-1 text-xs font-medium bg-green-500/20 text-green-400">
          Synced
        </span>
      );
  }
}

// Direction badge component
function DirectionBadge({ direction }: { direction: SyncDirectionType }) {
  switch (direction) {
    case 'push':
      return (
        <span className="rounded-full px-2.5 py-0.5 text-xs font-medium bg-violet-500/20 text-violet-400">
          ↑ Upload
        </span>
      );
    case 'pull':
      return (
        <span className="rounded-full px-2.5 py-0.5 text-xs font-medium bg-cyan-500/20 text-cyan-400">
          ↓ Download
        </span>
      );
    default:
      return (
        <span className="rounded-full px-2.5 py-0.5 text-xs font-medium bg-green-500/20 text-green-400">
          ↕ Sync
        </span>
      );
  }
}

export default function Sync() {
  const [folders, setFolders] = useState<SyncFolder[]>([]);
  const [loading, setLoading] = useState(false);
  const [showSettingsModal, setShowSettingsModal] = useState(false);
  const [selectedFolder, setSelectedFolder] = useState<SyncFolder | null>(null);
  const [syncDirection, setSyncDirection] = useState<SyncDirectionType>('bidirectional');
  const [conflictResolution, setConflictResolution] = useState('ask');

  // Sync trigger states
  const [syncingAll, setSyncingAll] = useState(false);
  const [syncingFolders, setSyncingFolders] = useState<Set<string>>(new Set());

  // Remote folder browser state
  const [showRemoteBrowser, setShowRemoteBrowser] = useState(false);
  const [pendingLocalPath, setPendingLocalPath] = useState<string | null>(null);

  useEffect(() => {
    // Listen to backend messages — stable reference for targeted cleanup
    const handleMessage = (message: BackendMessage) => {
      if (message.type === 'sync_folders') {
        setFolders(message.data);
      }
    };

    window.electronAPI.onBackendMessage(handleMessage);

    // Request initial data
    fetchSyncFolders();

    return () => {
      window.electronAPI.removeBackendListener(handleMessage);
    };
  }, []);

  const fetchSyncFolders = async () => {
    setLoading(true);
    try {
      const response: BackendResponse = await window.electronAPI.sendBackendCommand({
        type: 'get_folders',
      });
      if (response.success || response.folders) {
        const folderData = response.folders || response.data || [];
        const transformedFolders = folderData.map((folder: any) => ({
          id: folder.id,
          localPath: folder.local_path,
          remotePath: folder.remote_path,
          enabled: folder.enabled,
          status: folder.status,
          size: folder.size,
          lastSync: folder.last_sync,
          syncDirection: folder.sync_direction || 'bidirectional',
          conflictResolution: folder.conflict_resolution || 'ask',
        }));
        setFolders(transformedFolders);
      }
    } catch (err) {
      console.error('Failed to fetch sync folders:', err);
      toast.error('Failed to load sync folders');
    } finally {
      setLoading(false);
    }
  };

  const handleAddFolder = async () => {
    try {
      const localPath = await window.electronAPI.selectFolder({
        defaultPath: '',
      });

      if (localPath) {
        // Store local path and open remote folder browser
        setPendingLocalPath(localPath);
        setShowRemoteBrowser(true);
      }
    } catch (err) {
      console.error('Error selecting folder:', err);
      toast.error('Error selecting folder');
    }
  };

  const handleRemoteSelect = async (remotePath: string, direction: string = 'bidirectional') => {
    if (!pendingLocalPath) return;

    try {
      const response: BackendResponse = await window.electronAPI.sendBackendCommand({
        type: 'add_sync_folder',
        payload: {
          local_path: pendingLocalPath,
          remote_path: remotePath,
          sync_direction: direction,
        }
      });

      if (response.success) {
        toast.success('Folder added successfully');
        fetchSyncFolders();
      } else {
        toast.error(response.error || 'Failed to add folder');
      }
    } catch (err) {
      console.error('Error adding folder:', err);
      toast.error('Error adding folder');
    } finally {
      setPendingLocalPath(null);
    }
  };

  const handleToggleFolder = async (folderId: string, enabled: boolean) => {
    try {
      const commandType = enabled ? 'pause_sync' : 'resume_sync';
      const response = await window.electronAPI.sendBackendCommand({
        type: commandType,
        payload: {
          folder_id: folderId,
        }
      });

      if (response.success) {
        toast.success(`Sync ${!enabled ? 'resumed' : 'paused'}`);
        fetchSyncFolders();
      } else {
        toast.error(response.error || 'Failed to update folder');
      }
    } catch (err) {
      console.error('Error toggling folder:', err);
      toast.error('Error toggling folder');
    }
  };

  const openFolderSettings = (folder: SyncFolder) => {
    setSelectedFolder(folder);
    setSyncDirection(folder.syncDirection || 'bidirectional');
    setConflictResolution(folder.conflictResolution || 'ask');
    setShowSettingsModal(true);
  };

  const handleSaveFolderSettings = async () => {
    if (!selectedFolder) return;

    try {
        const response: BackendResponse = await window.electronAPI.sendBackendCommand({
        type: 'update_sync_folder',
        payload: {
          folder_id: selectedFolder.id,
          sync_direction: syncDirection,
          conflict_resolution: conflictResolution
        }
      });

      if (response.success) {
        toast.success('Folder settings saved');
        setShowSettingsModal(false);
        fetchSyncFolders();
      } else {
        toast.error(response.error || 'Failed to save folder settings');
      }
    } catch (err) {
      console.error('Error saving folder settings:', err);
      toast.error('Error saving folder settings');
    }
  };

  const handleRemoveFolder = async (folderId: string) => {
    if (confirm('Are you sure you want to remove this sync folder?')) {
      try {
      const response: BackendResponse = await window.electronAPI.sendBackendCommand({
          type: 'remove_sync_folder',
          payload: {
            folder_id: folderId,
          }
        });

        if (response.success) {
          toast.success('Folder removed');
          fetchSyncFolders();
        } else {
          toast.error(response.error || 'Failed to remove folder');
        }
      } catch (err) {
        console.error('Error removing folder:', err);
        toast.error('Error removing folder');
      }
    }
  };

  const handleTriggerSync = async (folderId?: string) => {
    if (folderId) {
      setSyncingFolders((prev) => new Set(prev).add(folderId));
    } else {
      setSyncingAll(true);
    }

    try {
      const response: BackendResponse = await window.electronAPI.sendBackendCommand({
        type: 'trigger_sync',
        payload: folderId ? { folder_id: folderId } : {},
      });

      if (response.success) {
        toast.success(folderId ? 'Sync gestartet' : 'Sync für alle Ordner gestartet');
      } else {
        toast.error(response.error || 'Sync konnte nicht gestartet werden');
      }
    } catch (err) {
      console.error('Error triggering sync:', err);
      toast.error('Error triggering sync');
    } finally {
      // Reset spinning state after 2s — actual status comes via events
      setTimeout(() => {
        if (folderId) {
          setSyncingFolders((prev) => {
            const next = new Set(prev);
            next.delete(folderId);
            return next;
          });
        } else {
          setSyncingAll(false);
        }
      }, 2000);
    }
  };

  return (
    <div className="space-y-6">
      {/* Page Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-3xl font-bold text-white flex items-center space-x-3">
            <div className="rounded-lg bg-gradient-to-br from-orange-500 to-orange-600 p-3">
              <FolderSync className="h-6 w-6 text-white" />
            </div>
            <span>Sync Folders</span>
          </h1>
          <p className="mt-2 text-slate-400">Manage folders to synchronize with your server</p>
        </div>
        <div className="flex items-center space-x-3">
          <button
            onClick={() => handleTriggerSync()}
            disabled={folders.length === 0 || syncingAll}
            className="flex items-center space-x-2 rounded-lg bg-gradient-to-r from-emerald-500 to-emerald-600 px-5 py-3 font-medium text-white shadow-lg shadow-emerald-500/30 transition-all hover:shadow-xl hover:shadow-emerald-500/40 hover:scale-105 disabled:opacity-50 disabled:cursor-not-allowed"
          >
            <RefreshCw className={`h-5 w-5 ${syncingAll ? 'animate-spin' : ''}`} />
            <span>Sync All</span>
          </button>
          <button
            onClick={handleAddFolder}
            disabled={loading}
            className="flex items-center space-x-2 rounded-lg bg-gradient-to-r from-blue-500 to-blue-600 px-6 py-3 font-medium text-white shadow-lg shadow-blue-500/30 transition-all hover:shadow-xl hover:shadow-blue-500/40 hover:scale-105 disabled:opacity-50 disabled:cursor-not-allowed"
          >
            <FolderPlus className="h-5 w-5" />
            <span>Add Folder</span>
          </button>
        </div>
      </div>

      {/* Folders List */}
      <div className="rounded-xl border border-white/10 bg-white/5 p-6 backdrop-blur-sm">
        {loading ? (
          <div className="flex justify-center py-12">
            <div className="text-slate-400">Loading folders...</div>
          </div>
        ) : folders.length === 0 ? (
          <div className="rounded-xl border border-dashed border-slate-700 bg-slate-950/50 py-12 text-center">
            <FolderSync className="mx-auto h-12 w-12 text-slate-600" />
            <p className="mt-4 text-slate-400 text-lg font-medium">No sync folders configured</p>
            <p className="mt-2 text-sm text-slate-500">Click "Add Folder" to start syncing files</p>
          </div>
        ) : (
          <div className="space-y-3">
            {folders.map((folder) => (
              <div
                key={folder.id}
                className="group rounded-lg border border-slate-800 bg-slate-950/50 p-4 transition-all hover:border-slate-700 hover:bg-slate-950"
              >
                <div className="flex items-center justify-between">
                  {/* Left side: Folder info */}
                  <div className="flex items-center space-x-4 flex-1">
                    <button
                      onClick={() => handleToggleFolder(folder.id, folder.status !== 'paused')}
                      className="flex-shrink-0 text-slate-400 hover:text-slate-200 transition-colors"
                    >
                      {folder.status === 'paused' ? (
                        <Circle className="h-6 w-6" />
                      ) : (
                        <CheckCircle className="h-6 w-6 text-green-500" />
                      )}
                    </button>

                    <div className="flex-1 min-w-0">
                      <p className="font-medium text-white break-all">
                        {folder.localPath}
                      </p>
                      <p className="text-sm text-slate-400 mt-1 break-all">
                        Syncs to: {folder.remotePath}
                      </p>
                      <div className="flex items-center space-x-4 mt-2">
                        <span className="text-xs text-slate-500">
                          Size: {formatSize(folder.size)}
                        </span>
                        <span className="text-xs text-slate-500 flex items-center space-x-1">
                          <Clock className="h-3 w-3" />
                          <span>Last sync: {formatLastSync(folder.lastSync)}</span>
                        </span>
                      </div>
                    </div>
                  </div>

                  {/* Right side: Direction + Status badge & Actions */}
                  <div className="flex items-center space-x-3 flex-shrink-0">
                    <DirectionBadge direction={folder.syncDirection} />
                    <StatusBadge status={folder.status} />

                    <div className="flex items-center space-x-2 opacity-0 group-hover:opacity-100 transition-opacity">
                      <button
                        onClick={() => handleTriggerSync(folder.id)}
                        disabled={folder.status === 'paused' || syncingFolders.has(folder.id)}
                        className="rounded-lg p-2 text-slate-400 hover:bg-emerald-500/20 hover:text-emerald-400 transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
                        title="Sync Now"
                      >
                        <RefreshCw className={`h-4 w-4 ${syncingFolders.has(folder.id) ? 'animate-spin' : ''}`} />
                      </button>
                      <button
                        onClick={() => openFolderSettings(folder)}
                        className="rounded-lg p-2 text-slate-400 hover:bg-slate-800 hover:text-slate-200 transition-colors"
                        title="Settings"
                      >
                        <Settings className="h-4 w-4" />
                      </button>
                      <button
                        onClick={() => handleRemoveFolder(folder.id)}
                        className="rounded-lg p-2 text-slate-400 hover:bg-red-500/20 hover:text-red-400 transition-colors"
                        title="Remove"
                      >
                        <Trash2 className="h-4 w-4" />
                      </button>
                    </div>
                  </div>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Info Box */}
      <div className="rounded-xl border border-slate-800 bg-slate-900/30 p-4">
        <p className="text-sm text-slate-400">
          <span className="font-medium text-slate-300">Tip:</span> Only active folders will be synced. Click the circle icon to pause/resume sync for a folder.
        </p>
      </div>

      {/* Remote Folder Browser Modal */}
      <RemoteFolderBrowser
        isOpen={showRemoteBrowser}
        onClose={() => {
          setShowRemoteBrowser(false);
          setPendingLocalPath(null);
        }}
        onSelect={handleRemoteSelect}
      />

      {/* Folder Settings Modal */}
      {showSettingsModal && selectedFolder && (
        <div className="fixed inset-0 bg-black/50 flex items-center justify-center z-50 p-4">
          <div className="bg-slate-900 rounded-xl border border-slate-800 w-full max-w-md shadow-xl">
            {/* Header */}
            <div className="border-b border-slate-800 p-6">
              <h2 className="text-xl font-bold text-white">Folder Settings</h2>
              <p className="mt-2 text-sm text-slate-400">{selectedFolder.remotePath}</p>
            </div>

            {/* Content */}
            <div className="p-6 space-y-6">
              {/* Sync Direction */}
              <div>
                <label className="block text-sm font-medium text-slate-300 mb-3">
                  Sync Direction
                </label>
                <div className="grid grid-cols-3 gap-2">
                  {([
                    { value: 'push' as SyncDirectionType, label: '↑ Upload', desc: 'Local → NAS' },
                    { value: 'bidirectional' as SyncDirectionType, label: '↕ Bidirectional', desc: 'Both ways' },
                    { value: 'pull' as SyncDirectionType, label: '↓ Download', desc: 'NAS → Local' },
                  ]).map((opt) => (
                    <button
                      key={opt.value}
                      onClick={() => setSyncDirection(opt.value)}
                      className={`rounded-lg border px-3 py-2.5 text-center transition-all ${
                        syncDirection === opt.value
                          ? 'border-blue-500 bg-blue-500/20 text-blue-300'
                          : 'border-slate-700 bg-slate-800 text-slate-400 hover:border-slate-600 hover:text-slate-300'
                      }`}
                    >
                      <div className="text-sm font-medium">{opt.label}</div>
                      <div className="text-xs mt-0.5 opacity-70">{opt.desc}</div>
                    </button>
                  ))}
                </div>
              </div>

              {/* Conflict Resolution — only for bidirectional */}
              {syncDirection === 'bidirectional' && (
              <div>
                <label className="block text-sm font-medium text-slate-300 mb-2">
                  Conflict Resolution
                </label>
                <select
                  value={conflictResolution}
                  onChange={(e) => setConflictResolution(e.target.value)}
                  className="w-full rounded-lg bg-slate-800 border border-slate-700 px-3 py-2 text-white hover:border-slate-600 focus:border-blue-500 focus:outline-none focus:ring-1 focus:ring-blue-500 transition-colors"
                >
                  <option value="ask">Ask (recommended)</option>
                  <option value="skip">Skip conflicting files</option>
                  <option value="overwrite_local">Overwrite local version</option>
                  <option value="overwrite_remote">Overwrite remote version</option>
                </select>
                <p className="mt-2 text-xs text-slate-500">
                  Choose what to do when the same file is modified in both locations.
                </p>
              </div>
              )}

              {syncDirection !== 'bidirectional' && (
                <p className="text-xs text-slate-500 bg-slate-800/50 rounded-lg p-3">
                  {syncDirection === 'push'
                    ? 'In Upload mode, local files always overwrite the server version. No conflicts possible.'
                    : 'In Download mode, server files always overwrite the local version. No conflicts possible.'}
                </p>
              )}
            </div>

            {/* Footer */}
            <div className="border-t border-slate-800 p-6 flex items-center justify-end space-x-3">
              <button
                onClick={() => setShowSettingsModal(false)}
                className="rounded-lg px-4 py-2 text-slate-400 hover:bg-slate-800 hover:text-slate-200 transition-colors font-medium"
              >
                Cancel
              </button>
              <button
                onClick={handleSaveFolderSettings}
                className="rounded-lg bg-gradient-to-r from-blue-500 to-blue-600 px-4 py-2 text-white font-medium hover:shadow-lg hover:shadow-blue-500/30 transition-all"
              >
                Save Settings
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
