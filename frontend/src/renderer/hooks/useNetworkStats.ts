import { useEffect, useState, useCallback } from 'react';

export interface NetworkStats {
  uploadSpeed: number;
  downloadSpeed: number;
  totalUpToday: number;
  totalDownToday: number;
}

export function useNetworkStats() {
  const [stats, setStats] = useState<NetworkStats | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  const fetchStats = useCallback(async () => {
    try {
      const response = await window.electronAPI.sendBackendCommand({
        type: 'get_network_stats'
      });
      if (response?.success && response.data) {
        setStats(response.data);
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

  useEffect(() => {
    fetchStats();
    const interval = setInterval(fetchStats, 10000);
    return () => clearInterval(interval);
  }, [fetchStats]);

  return { stats, loading, error, refetch: fetchStats };
}
