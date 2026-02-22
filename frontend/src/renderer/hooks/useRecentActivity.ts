import { useEffect, useState, useCallback } from 'react';

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

export function useRecentActivity(limit: number = 8) {
  const [activities, setActivities] = useState<ActivityLog[]>([]);
  const [loading, setLoading] = useState(true);

  const fetchActivities = useCallback(async () => {
    try {
      const response = await window.electronAPI.sendBackendCommand({
        type: 'get_activity_logs',
        data: { limit }
      });
      if (response?.success && response.data?.logs) {
        setActivities(response.data.logs);
      } else if (response?.logs) {
        setActivities(response.logs);
      }
    } catch (err) {
      console.error('Failed to fetch activity logs:', err);
    } finally {
      setLoading(false);
    }
  }, [limit]);

  useEffect(() => {
    fetchActivities();
    const interval = setInterval(fetchActivities, 15000);
    return () => clearInterval(interval);
  }, [fetchActivities]);

  return { activities, loading, refetch: fetchActivities };
}
