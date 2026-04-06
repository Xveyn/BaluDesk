import { useState, useCallback } from 'react';
import { useVisibilityPolling } from './useVisibilityPolling';
import { getCached, setCache } from './ipcCache';

export interface ActivityLog {
  id: string;
  timestamp: string;
  activity_type: string;
  file_path: string;
  folder_id: string;
  details: string;
  file_size: number;
  status: string;
}

const CACHE_KEY = 'recent_activity';

export function useRecentActivity(limit: number = 8) {
  const cached = getCached<ActivityLog[]>(CACHE_KEY);
  const [activities, setActivities] = useState<ActivityLog[]>(cached ?? []);
  const [loading, setLoading] = useState(!cached);

  const fetchActivities = useCallback(async () => {
    try {
      const response = await window.electronAPI.sendBackendCommand({
        type: 'get_activity_logs',
        data: { limit }
      });
      if (response?.success && response.data?.logs) {
        setActivities(response.data.logs);
        setCache(CACHE_KEY, response.data.logs);
      } else if (response?.logs) {
        setActivities(response.logs);
        setCache(CACHE_KEY, response.logs);
      }
    } catch (err) {
      console.error('Failed to fetch activity logs:', err);
    } finally {
      setLoading(false);
    }
  }, [limit]);

  useVisibilityPolling(fetchActivities, 15000);

  return { activities, loading, refetch: fetchActivities };
}
