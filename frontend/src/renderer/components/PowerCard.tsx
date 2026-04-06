import React, { useState, useCallback } from 'react';
import { Zap } from 'lucide-react';
import { useVisibilityPolling } from '../hooks/useVisibilityPolling';
import { getCached, setCache } from '../hooks/ipcCache';

interface PowerDevice {
  device_id: number;
  name: string;
  watts: number;
}

interface PowerData {
  currentPower: number;
  energyToday: number;
  trendDelta: number;
  deviceCount: number;
  maxPower: number;
  devices?: PowerDevice[];
  dev_mode?: boolean;
}

const CACHE_KEY = 'power_data';

export const PowerCard: React.FC = () => {
  const cached = getCached<PowerData>(CACHE_KEY);
  const [powerData, setPowerData] = useState<PowerData | null>(cached ?? null);
  const [loading, setLoading] = useState(!cached);

  const fetchPowerData = useCallback(async () => {
    try {
      const response = await window.electronAPI.sendBackendCommand({
        type: 'get_power_monitoring',
      });

      if (response?.success) {
        setPowerData(response.data);
        setCache(CACHE_KEY, response.data);
        setLoading(false);
      }
    } catch (error) {
      console.error('Failed to fetch power data:', error);
      setLoading(false);
    }
  }, []);

  useVisibilityPolling(fetchPowerData, 10000);

  if (loading || !powerData) {
    return (
      <div className="rounded-xl border border-white/10 bg-gradient-to-br from-amber-500/10 to-orange-600/10 p-4 backdrop-blur-sm">
        <div className="flex items-center space-x-2">
          <div className="rounded-lg bg-amber-500/20 p-1.5">
            <Zap className="h-4 w-4 text-amber-400" />
          </div>
          <h3 className="text-sm font-medium text-slate-300">Power</h3>
        </div>
        <p className="mt-3 text-sm text-slate-400">Loading...</p>
      </div>
    );
  }

  const progress = powerData.maxPower > 0
    ? Math.min((powerData.currentPower / powerData.maxPower) * 100, 100)
    : 0;

  return (
    <div className="rounded-xl border border-white/10 bg-gradient-to-br from-amber-500/10 to-orange-600/10 p-4 backdrop-blur-sm hover:border-amber-500/30 hover:shadow-lg hover:shadow-amber-500/20 transition-all">
      <div className="flex items-center space-x-2">
        <div className="rounded-lg bg-gradient-to-br from-amber-500/20 to-orange-500/20 p-1.5">
          <Zap className="h-4 w-4 text-amber-400" />
        </div>
        <h3 className="text-sm font-medium text-slate-300">Power</h3>
      </div>

      {/* Total Power */}
      <div className="mt-3">
        <p className="text-3xl font-bold text-white">
          {powerData.currentPower.toFixed(1)}
          <span className="ml-1 text-lg font-normal text-slate-400">W</span>
        </p>
        <p className="mt-1 text-xs text-slate-400">
          {powerData.deviceCount} device{powerData.deviceCount !== 1 ? 's' : ''} monitored
        </p>
      </div>

      {/* Per-device breakdown */}
      {powerData.devices && powerData.devices.length > 0 && (
        <div className="mt-2 space-y-1">
          {powerData.devices.map((device) => (
            <div key={device.device_id} className="flex items-center justify-between text-xs">
              <span className="text-slate-400 truncate mr-2">{device.name}</span>
              <span className="font-medium text-white flex-shrink-0">
                {device.watts.toFixed(1)} W
              </span>
            </div>
          ))}
        </div>
      )}

      {/* Progress Bar */}
      <div className="mt-3 h-1.5 w-full overflow-hidden rounded-full bg-slate-800">
        <div
          className="h-full rounded-full bg-gradient-to-r from-amber-500 to-orange-500 transition-all duration-500"
          style={{ width: `${Math.max(0, Math.min(100, progress))}%` }}
        />
      </div>
    </div>
  );
};
