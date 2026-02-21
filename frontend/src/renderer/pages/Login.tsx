import { useState, useEffect } from 'react';
import toast from 'react-hot-toast';
import { ServerSelector } from '../components/Login/ServerSelector';
import { DevicePairingScreen } from '../components/Login/DevicePairingScreen';
import { useDevicePairing } from '../hooks/useDevicePairing';
import type { RemoteServerProfile } from '../types/RemoteServerProfile';

interface LoginProps {
  onLogin: (user: any) => void;
}

type ViewState = 'server-select' | 'pairing';

export default function Login({ onLogin }: LoginProps) {
  const [serverUrl, setServerUrl] = useState('http://localhost');
  const [selectedProfileId, setSelectedProfileId] = useState<number | null>(null);
  const [useServerSelection, setUseServerSelection] = useState(true);
  const [view, setView] = useState<ViewState>('server-select');

  const { state: pairingState, startPairing, cancelPairing } = useDevicePairing();

  const handleSelectProfile = (profile: RemoteServerProfile) => {
    setSelectedProfileId(profile.id);
    const url = `http://${profile.sshHost}`;
    setServerUrl(url);
  };

  const handleConnect = () => {
    if (!serverUrl.trim()) {
      toast.error('Bitte Server-URL eingeben oder Server auswaehlen');
      return;
    }
    setView('pairing');
    startPairing(serverUrl);
  };

  const handleCancel = () => {
    cancelPairing();
    setView('server-select');
  };

  const handleRetry = () => {
    startPairing(serverUrl);
  };

  // When pairing is approved, notify parent
  useEffect(() => {
    if (pairingState.phase === 'approved') {
      // Small delay to show success animation
      const timer = setTimeout(async () => {
        // Fetch the stored token info from the backend to get username
        try {
          const response = await window.electronAPI.sendBackendCommand({
            type: 'check_stored_tokens',
          });
          const username = response?.username || '';
          onLogin({
            username,
            serverUrl,
            selectedProfileId,
          });
          toast.success('Erfolgreich verbunden!');
        } catch {
          onLogin({
            username: '',
            serverUrl,
            selectedProfileId,
          });
        }
      }, 800);
      return () => clearTimeout(timer);
    }
  }, [pairingState.phase, serverUrl, selectedProfileId, onLogin]);

  return (
    <div className="relative flex min-h-screen items-center justify-center overflow-hidden text-slate-100">
      {/* Animated background gradients */}
      <div className="pointer-events-none absolute inset-0">
        <div className="absolute -left-24 top-[-120px] h-[420px] w-[420px] rounded-full bg-sky-500/10 blur-3xl" />
        <div className="absolute right-[-120px] top-[18%] h-[460px] w-[460px] rounded-full bg-sky-500/10 blur-[140px]" />
        <div className="absolute left-[45%] bottom-[-180px] h-[340px] w-[340px] rounded-full bg-sky-500/5 blur-[120px]" />
      </div>

      <div className="relative z-10 w-full max-w-md px-4 sm:px-6">
        <div className="card border border-slate-800 bg-slate-900/55 p-6 sm:p-10">
          <div className="flex flex-col items-center text-center">
            <div className="glow-ring h-14 w-14 sm:h-16 sm:w-16">
              <div className="flex h-12 w-12 sm:h-14 sm:w-14 items-center justify-center rounded-full bg-slate-950 p-[2px] shadow-xl">
                <svg
                  className="h-8 w-8 text-sky-500"
                  fill="none"
                  viewBox="0 0 24 24"
                  stroke="currentColor"
                >
                  <path
                    strokeLinecap="round"
                    strokeLinejoin="round"
                    strokeWidth={2}
                    d="M3 15a4 4 0 004 4h9a5 5 0 10-.1-9.999 5.002 5.002 0 10-9.78 2.096A4.001 4.001 0 003 15z"
                  />
                </svg>
              </div>
            </div>
            <h1 className="mt-5 sm:mt-6 text-2xl sm:text-3xl font-semibold tracking-wide text-slate-100">
              BaluDesk
            </h1>
            <p className="mt-2 text-sm text-slate-400">Desktop Sync Client</p>
          </div>

          {view === 'server-select' ? (
            <div className="mt-8 sm:mt-10 space-y-4 sm:space-y-5">
              {/* Server Selection */}
              {useServerSelection ? (
                <ServerSelector
                  selectedProfileId={selectedProfileId}
                  onSelectProfile={handleSelectProfile}
                  onManualMode={() => {
                    setUseServerSelection(false);
                    setSelectedProfileId(null);
                  }}
                />
              ) : (
                <div className="space-y-2">
                  <label
                    htmlFor="serverUrl"
                    className="text-xs font-medium uppercase tracking-[0.2em] text-slate-400"
                  >
                    Server URL
                  </label>
                  <input
                    type="text"
                    id="serverUrl"
                    className="input"
                    value={serverUrl}
                    onChange={(e) => setServerUrl(e.target.value)}
                    placeholder="http://192.168.1.100"
                  />
                  <button
                    type="button"
                    onClick={() => setUseServerSelection(true)}
                    className="text-xs text-slate-400 hover:text-slate-300"
                  >
                    Oder gespeicherten Server waehlen
                  </button>
                </div>
              )}

              <button
                onClick={handleConnect}
                className="btn btn-primary w-full mt-5 sm:mt-6 touch-manipulation active:scale-[0.98]"
              >
                Verbinden
              </button>
            </div>
          ) : (
            <div className="mt-8 sm:mt-10">
              <DevicePairingScreen
                state={pairingState}
                onCancel={handleCancel}
                onRetry={handleRetry}
              />
            </div>
          )}

          <div className="mt-4 sm:mt-6 text-center text-[10px] sm:text-[11px] uppercase tracking-[0.3em] sm:tracking-[0.35em] text-slate-500">
            Desktop Client v1.0.0 - Status{' '}
            <span className="text-sky-400">Ready</span>
          </div>
        </div>
      </div>
    </div>
  );
}
