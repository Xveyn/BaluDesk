import { useState } from 'react';
import { Server, Power, Moon, Sun, Wifi } from 'lucide-react';
import { formatUptime } from '../../../lib/formatters';

type ServerStatus = 'online' | 'sleeping' | 'offline' | 'unknown';
type PowerAction = 'wol' | 'wake' | 'soft_sleep' | 'suspend';

interface ServerStatusStripProps {
  systemInfo: {
    uptime: number;
    serverUptime?: number;
  } | null;
  loading: boolean;
}

function deriveStatus(loading: boolean, systemInfo: ServerStatusStripProps['systemInfo']): ServerStatus {
  if (loading) return 'unknown';
  if (systemInfo) return 'online';
  return 'offline';
}

export function ServerStatusStrip({ systemInfo, loading }: ServerStatusStripProps) {
  const [actionInProgress, setActionInProgress] = useState(false);

  const status = deriveStatus(loading, systemInfo);

  const uptimeToShow: number | null = (() => {
    if (!systemInfo) return null;
    const sv = systemInfo.serverUptime;
    return typeof sv === 'number' && sv > 0 ? sv : systemInfo.uptime;
  })();

  const handlePowerAction = async (action: PowerAction) => {
    setActionInProgress(true);
    try {
      await window.electronAPI.sendBackendCommand({
        type: 'power_action',
        data: { action },
      });
    } catch (error) {
      console.error(`Power action '${action}' failed:`, error);
    } finally {
      setActionInProgress(false);
    }
  };

  const statusConfig: Record<ServerStatus, { dot: string; label: string }> = {
    online:  { dot: 'bg-emerald-400',                  label: 'Online'  },
    sleeping:{ dot: 'bg-amber-400 animate-pulse',       label: 'Sleeping'},
    offline: { dot: 'bg-red-400',                       label: 'Offline' },
    unknown: { dot: 'bg-slate-500 animate-pulse',       label: 'Unknown' },
  };

  const { dot, label } = statusConfig[status];

  const btnBase =
    'flex items-center gap-1 px-2 py-1 rounded-lg bg-white/5 text-xs text-slate-400 transition-all disabled:opacity-50';

  return (
    <div className="rounded-xl border border-white/10 bg-gradient-to-br from-slate-500/10 to-slate-600/10 px-4 py-3 backdrop-blur-sm transition-all hover:border-slate-500/30">
      <div className="flex items-center justify-between gap-4">

        {/* Left: icon + status + uptime */}
        <div className="flex items-center gap-3 min-w-0">
          <div className="rounded-lg bg-slate-700/50 p-1.5 shrink-0">
            <Server className="h-4 w-4 text-slate-400" />
          </div>

          <div className={`h-2 w-2 rounded-full shrink-0 ${dot}`} />

          <span className="text-sm font-medium text-slate-300">{label}</span>

          {status === 'online' && uptimeToShow !== null && (
            <span className="text-xs text-slate-500 hidden sm:inline">
              Up {formatUptime(uptimeToShow)}
            </span>
          )}
        </div>

        {/* Right: power action buttons */}
        <div className="flex items-center gap-2 shrink-0">
          {status === 'online' && (
            <>
              <button
                className={`${btnBase} hover:bg-indigo-500/20 hover:text-indigo-300`}
                disabled={actionInProgress}
                onClick={() => handlePowerAction('soft_sleep')}
                title="Soft Sleep"
              >
                <Moon className="h-3.5 w-3.5" />
                <span className="hidden sm:inline">Soft Sleep</span>
              </button>
              <button
                className={`${btnBase} hover:bg-rose-500/20 hover:text-rose-300`}
                disabled={actionInProgress}
                onClick={() => handlePowerAction('suspend')}
                title="Suspend"
              >
                <Power className="h-3.5 w-3.5" />
                <span className="hidden sm:inline">Suspend</span>
              </button>
            </>
          )}

          {status === 'sleeping' && (
            <>
              <button
                className={`${btnBase} hover:bg-sky-500/20 hover:text-sky-300`}
                disabled={actionInProgress}
                onClick={() => handlePowerAction('wol')}
                title="Wake on LAN"
              >
                <Wifi className="h-3.5 w-3.5" />
                <span className="hidden sm:inline">WOL</span>
              </button>
              <button
                className={`${btnBase} hover:bg-amber-500/20 hover:text-amber-300`}
                disabled={actionInProgress}
                onClick={() => handlePowerAction('wake')}
                title="Wake"
              >
                <Sun className="h-3.5 w-3.5" />
                <span className="hidden sm:inline">Wake</span>
              </button>
            </>
          )}

          {status === 'offline' && (
            <button
              className={`${btnBase} hover:bg-sky-500/20 hover:text-sky-300`}
              disabled={actionInProgress}
              onClick={() => handlePowerAction('wol')}
              title="Wake on LAN"
            >
              <Wifi className="h-3.5 w-3.5" />
              <span className="hidden sm:inline">WOL</span>
            </button>
          )}
        </div>
      </div>
    </div>
  );
}
