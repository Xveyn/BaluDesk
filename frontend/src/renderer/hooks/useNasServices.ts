import { useEffect, useState, useCallback } from 'react';

export interface NasService {
  name: string;
  status: 'running' | 'stopped' | 'unknown';
}

export function useNasServices() {
  const [services, setServices] = useState<NasService[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  const fetchServices = useCallback(async () => {
    try {
      const response = await window.electronAPI.sendBackendCommand({
        type: 'get_services_status'
      });
      if (response?.success && response.data?.services) {
        setServices(response.data.services);
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

  useEffect(() => {
    fetchServices();
    const interval = setInterval(fetchServices, 15000);
    return () => clearInterval(interval);
  }, [fetchServices]);

  return { services, loading, error, refetch: fetchServices };
}
