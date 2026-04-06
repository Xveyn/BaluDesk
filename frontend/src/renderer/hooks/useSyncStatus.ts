import { useEffect, useState, useCallback } from 'react';
import { BackendMessage } from '../../lib/types';
import { getCached, setCache } from './ipcCache';

export interface SyncStats {
  status: string;
  uploadSpeed: number;
  downloadSpeed: number;
  pendingUploads: number;
  pendingDownloads: number;
  lastSync: string;
  syncFolderCount?: number;
  currentFile?: string;
  totalFiles?: number;
  processedFiles?: number;
  currentFileSize?: number;
  currentFileTransferred?: number;
  currentFilePercent?: number;
}

export interface SystemInfo {
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
  dev_mode?: boolean;
}

export interface RaidDevice {
  name: string;
  state: string;
}

export interface RaidArray {
  name: string;
  level: string;
  status: string;
  size_bytes: number;
  resync_progress?: number;
  devices: RaidDevice[];
}

export interface RaidStatus {
  arrays: RaidArray[];
  dev_mode?: boolean;
}

export function useSyncStatus() {
  const cachedSync = getCached<SyncStats>('sync_stats');
  const cachedSystem = getCached<SystemInfo>('system_info');
  const cachedRaid = getCached<RaidStatus>('raid_status');
  const hasCached = !!(cachedSync || cachedSystem);
  const [syncStats, setSyncStats] = useState<SyncStats | null>(cachedSync ?? null);
  const [systemInfo, setSystemInfo] = useState<SystemInfo | null>(cachedSystem ?? null);
  const [raidStatus, setRaidStatus] = useState<RaidStatus | null>(cachedRaid ?? null);
  const [loading, setLoading] = useState(!hasCached);
  const [syncError, setSyncError] = useState<string | null>(null);

  const fetchData = useCallback(async () => {
    setLoading(true);
    try {
      // Fetch all three in parallel instead of sequentially
      const [syncResponse, sysResponse, raidResponse] = await Promise.all([
        window.electronAPI.sendBackendCommand({ type: 'get_sync_state' }).catch(err => {
          console.error('Failed to fetch sync state:', err);
          return null;
        }),
        window.electronAPI.sendBackendCommand({ type: 'get_system_info' }).catch(err => {
          console.error('Failed to fetch system info:', err);
          return null;
        }),
        window.electronAPI.sendBackendCommand({ type: 'get_raid_status' }).catch(err => {
          console.error('Failed to fetch RAID status:', err);
          return null;
        }),
      ]);

      // Process sync state
      if (syncResponse?.success && syncResponse.data) {
        setSyncStats(syncResponse.data);
        setCache('sync_stats', syncResponse.data);
      } else if (syncResponse) {
        const legacy = syncResponse as any;
        if (legacy.type === 'sync_state') {
          const mapped: SyncStats = {
            status: legacy.status || 'idle',
            uploadSpeed: legacy.upload_speed ?? legacy.uploadSpeed ?? 0,
            downloadSpeed: legacy.download_speed ?? legacy.downloadSpeed ?? 0,
            pendingUploads: legacy.pendingUploads ?? legacy.pending_uploads ?? 0,
            pendingDownloads: legacy.pendingDownloads ?? legacy.pending_downloads ?? 0,
            lastSync: legacy.last_sync ?? legacy.lastSync ?? ''
          };
          if ((legacy.syncFolderCount ?? legacy.sync_folder_count) !== undefined) {
            mapped.syncFolderCount = legacy.syncFolderCount ?? legacy.sync_folder_count;
          }
          setSyncStats(prev => {
            const merged = prev ? { ...prev, ...mapped } : mapped;
            setCache('sync_stats', merged);
            return merged;
          });
        }
      }

      // Process system info
      if (sysResponse?.success) {
        const raw = sysResponse.data as any;
        const normalized: any = { ...raw };
        if (raw?.cpu) {
          normalized.cpu = {
            usage: raw.cpu.usage ?? 0,
            cores: raw.cpu.cores ?? raw.cpu.coreCount ?? 0,
            frequency_mhz: raw.cpu.frequency_mhz ?? raw.cpu.frequency ?? null,
            model: raw.cpu.model ?? null,
          };
        }
        if (raw?.disk) {
          normalized.disk = {
            total: raw.disk.total ?? raw.disk.total_bytes ?? 0,
            used: raw.disk.used ?? (raw.disk.total - (raw.disk.available ?? raw.disk.free ?? 0)),
            available: raw.disk.available ?? raw.disk.free ?? null,
          };
        }
        if (typeof raw?.uptime === 'number') {
          let uptimeSeconds = raw.uptime;
          if (uptimeSeconds > 1e12) uptimeSeconds = Math.floor(uptimeSeconds / 1000);
          normalized.uptime = uptimeSeconds;
        }
        if (typeof raw?.serverUptime === 'number') {
          let sUptime = raw.serverUptime;
          if (sUptime > 1e12) sUptime = Math.floor(sUptime / 1000);
          normalized.serverUptime = sUptime;
        }
        setSystemInfo(normalized as SystemInfo);
        setCache('system_info', normalized as SystemInfo);
      }

      // Process RAID status
      if (raidResponse?.success) {
        setRaidStatus(raidResponse.data);
        setCache('raid_status', raidResponse.data);
      }
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    const handleMessage = (message: BackendMessage) => {
      if (message.type === 'sync_stats') {
        setSyncStats(prev => prev ? { ...prev, ...message.data } : message.data);
      } else if (message.type === 'sync_state_update') {
        setSyncStats(prev => prev ? { ...prev, ...message.data } : message.data);
      } else if (message.type === 'sync_error') {
        const errorMsg = message.data?.message || 'Sync error';
        setSyncError(errorMsg);
        setTimeout(() => setSyncError(null), 15000);
      } else if (message.type === 'sync_state') {
        if ((message as any).success && (message as any).data) {
          setSyncStats(prev => prev ? { ...prev, ...(message as any).data } : (message as any).data);
        } else if ((message as any).status) {
          const legacy = message as any;
          const mapped: SyncStats = {
            status: legacy.status || 'idle',
            uploadSpeed: legacy.upload_speed ?? legacy.uploadSpeed ?? 0,
            downloadSpeed: legacy.download_speed ?? legacy.downloadSpeed ?? 0,
            pendingUploads: legacy.pendingUploads ?? legacy.pending_uploads ?? 0,
            pendingDownloads: legacy.pendingDownloads ?? legacy.pending_downloads ?? 0,
            lastSync: legacy.last_sync ?? legacy.lastSync ?? ''
          };
          if ((legacy.syncFolderCount ?? legacy.sync_folder_count) !== undefined) {
            mapped.syncFolderCount = legacy.syncFolderCount ?? legacy.sync_folder_count;
          }
          setSyncStats(prev => prev ? { ...prev, ...mapped } : mapped);
        }
      }
    };

    window.electronAPI.onBackendMessage(handleMessage);
    fetchData();

    return () => {
      window.electronAPI.removeBackendListener(handleMessage);
    };
  }, [fetchData]);

  return { syncStats, systemInfo, raidStatus, loading, syncError, refetch: fetchData };
}
