import { useState, useCallback } from 'react';
import { useVisibilityPolling } from './useVisibilityPolling';
import { getCached, setCache } from './ipcCache';

export interface NetworkStats {
  uploadSpeed: number;
  downloadSpeed: number;
  totalUpToday: number;
  totalDownToday: number;
}

const CACHE_KEY = 'network_stats';

export function useNetworkStats() {
  const cached = getCached<NetworkStats>(CACHE_KEY);
  const [stats, setStats] = useState<NetworkStats | null>(cached ?? null);
  const [loading, setLoading] = useState(!cached);
  const [error, setError] = useState<string | null>(null);

  const fetchStats = useCallback(async () => {
    try {
      const response = await window.electronAPI.sendBackendCommand({
        type: 'get_network_stats'
      });
      if (response?.success && response.data) {
        setStats(response.data);
        setCache(CACHE_KEY, response.data);
        setError(null);
      } else if (response?.error) {
        setError(response.error);
      }
    } catch (err) {
      setError('Failed to fetch network stats');
      console.error('Failed to fetch network stats:', err);
    } finally {
      setLoading(false);
    }
  }, []);

  useVisibilityPolling(fetchStats, 10000);

  return { stats, loading, error, refetch: fetchStats };
}
