import { useState, useCallback } from 'react';
import { useVisibilityPolling } from './useVisibilityPolling';
import { getCached, setCache } from './ipcCache';

export interface NasService {
  name: string;
  status: 'running' | 'stopped' | 'unknown';
}

const CACHE_KEY = 'nas_services';

export function useNasServices() {
  const cached = getCached<NasService[]>(CACHE_KEY);
  const [services, setServices] = useState<NasService[]>(cached ?? []);
  const [loading, setLoading] = useState(!cached);
  const [error, setError] = useState<string | null>(null);

  const fetchServices = useCallback(async () => {
    try {
      const response = await window.electronAPI.sendBackendCommand({
        type: 'get_services_status'
      });
      if (response?.success && response.data?.services) {
        setServices(response.data.services);
        setCache(CACHE_KEY, response.data.services);
        setError(null);
      } else if (response?.error) {
        setError(response.error);
      }
    } catch (err) {
      setError('Failed to fetch services status');
      console.error('Failed to fetch services status:', err);
    } finally {
      setLoading(false);
    }
  }, []);

  useVisibilityPolling(fetchServices, 30000);

  return { services, loading, error, refetch: fetchServices };
}
