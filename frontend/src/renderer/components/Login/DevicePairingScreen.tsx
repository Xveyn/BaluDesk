import React from 'react';
import type { PairingState } from '../../hooks/useDevicePairing';

interface DevicePairingScreenProps {
  state: PairingState;
  onCancel: () => void;
  onRetry: () => void;
}

function formatTime(seconds: number): string {
  const m = Math.floor(seconds / 60);
  const s = seconds % 60;
  return `${m.toString().padStart(2, '0')}:${s.toString().padStart(2, '0')}`;
}

function formatCode(code: string): string {
  // Format as 3-3 grouping (e.g., "847 293")
  if (code.length === 6) {
    return `${code.slice(0, 3)} ${code.slice(3)}`;
  }
  return code;
}

export const DevicePairingScreen: React.FC<DevicePairingScreenProps> = ({
  state,
  onCancel,
  onRetry,
}) => {
  const handleCopyCode = async () => {
    if (state.userCode) {
      try {
        await navigator.clipboard.writeText(state.userCode);
      } catch {
        // Fallback: ignore clipboard errors
      }
    }
  };

  const handleOpenUrl = () => {
    if (state.verificationUrl) {
      window.electronAPI?.openExternal(state.verificationUrl);
    }
  };

  // Phase: requesting
  if (state.phase === 'requesting') {
    return (
      <div className="flex flex-col items-center space-y-4 py-6">
        <div className="h-8 w-8 animate-spin rounded-full border-2 border-sky-500 border-t-transparent" />
        <p className="text-sm text-slate-400">Verbindung wird hergestellt...</p>
      </div>
    );
  }

  // Phase: waiting
  if (state.phase === 'waiting') {
    return (
      <div className="flex flex-col items-center space-y-6 py-4">
        {/* User Code Display */}
        <div className="text-center">
          <p className="text-xs font-medium uppercase tracking-[0.2em] text-slate-400 mb-3">
            Kopplungscode
          </p>
          <div className="flex items-center justify-center gap-3">
            <span className="font-mono text-4xl sm:text-5xl font-bold tracking-[0.15em] text-sky-400">
              {state.userCode ? formatCode(state.userCode) : '--- ---'}
            </span>
          </div>
          <button
            onClick={handleCopyCode}
            className="mt-3 inline-flex items-center gap-1.5 rounded-lg border border-slate-700 bg-slate-800/50 px-3 py-1.5 text-xs text-slate-300 transition-colors hover:border-slate-600 hover:bg-slate-700/50"
          >
            <svg className="h-3.5 w-3.5" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
              <rect x="9" y="9" width="13" height="13" rx="2" ry="2" />
              <path d="M5 15H4a2 2 0 01-2-2V4a2 2 0 012-2h9a2 2 0 012 2v1" />
            </svg>
            Code kopieren
          </button>
        </div>

        {/* Verification URL */}
        {state.verificationUrl && (
          <div className="text-center">
            <p className="text-xs text-slate-400 mb-2">
              Gib den Code auf deinem BaluHost-Server ein:
            </p>
            <button
              onClick={handleOpenUrl}
              className="text-sm text-sky-400 underline underline-offset-2 hover:text-sky-300 transition-colors"
            >
              {state.verificationUrl}
            </button>
          </div>
        )}

        {/* Countdown + Spinner */}
        <div className="flex flex-col items-center gap-3">
          <div className="flex items-center gap-2 text-slate-400">
            <div className="h-4 w-4 animate-spin rounded-full border-2 border-slate-500 border-t-sky-500" />
            <span className="text-sm">Warte auf Bestaetigung...</span>
          </div>
          <span className="font-mono text-sm text-slate-500">
            {formatTime(state.secondsRemaining)}
          </span>
        </div>

        {/* Cancel Button */}
        <button
          onClick={onCancel}
          className="mt-2 rounded-lg border border-slate-700 px-4 py-2 text-sm text-slate-400 transition-colors hover:border-slate-600 hover:text-slate-300"
        >
          Abbrechen
        </button>
      </div>
    );
  }

  // Phase: approved
  if (state.phase === 'approved') {
    return (
      <div className="flex flex-col items-center space-y-4 py-6">
        <div className="flex h-12 w-12 items-center justify-center rounded-full bg-green-500/20">
          <svg className="h-6 w-6 text-green-400" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
            <path strokeLinecap="round" strokeLinejoin="round" d="M5 13l4 4L19 7" />
          </svg>
        </div>
        <p className="text-sm font-medium text-green-400">Erfolgreich gekoppelt!</p>
        <p className="text-xs text-slate-400">Verbindung wird hergestellt...</p>
      </div>
    );
  }

  // Phase: denied
  if (state.phase === 'denied') {
    return (
      <div className="flex flex-col items-center space-y-4 py-6">
        <div className="flex h-12 w-12 items-center justify-center rounded-full bg-red-500/20">
          <svg className="h-6 w-6 text-red-400" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
            <path strokeLinecap="round" strokeLinejoin="round" d="M6 18L18 6M6 6l12 12" />
          </svg>
        </div>
        <p className="text-sm font-medium text-red-400">Kopplung abgelehnt</p>
        <p className="text-xs text-slate-400">Die Anfrage wurde auf dem Server abgelehnt.</p>
        <button
          onClick={onRetry}
          className="btn btn-primary mt-2"
        >
          Erneut versuchen
        </button>
      </div>
    );
  }

  // Phase: expired
  if (state.phase === 'expired') {
    return (
      <div className="flex flex-col items-center space-y-4 py-6">
        <div className="flex h-12 w-12 items-center justify-center rounded-full bg-amber-500/20">
          <svg className="h-6 w-6 text-amber-400" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
            <circle cx="12" cy="12" r="10" />
            <polyline points="12 6 12 12 16 14" />
          </svg>
        </div>
        <p className="text-sm font-medium text-amber-400">Code abgelaufen</p>
        <p className="text-xs text-slate-400">Der Kopplungscode ist abgelaufen.</p>
        <button
          onClick={onRetry}
          className="btn btn-primary mt-2"
        >
          Erneut versuchen
        </button>
      </div>
    );
  }

  // Phase: error
  if (state.phase === 'error') {
    return (
      <div className="flex flex-col items-center space-y-4 py-6">
        <div className="flex h-12 w-12 items-center justify-center rounded-full bg-red-500/20">
          <svg className="h-6 w-6 text-red-400" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
            <circle cx="12" cy="12" r="10" />
            <line x1="12" y1="8" x2="12" y2="12" />
            <line x1="12" y1="16" x2="12.01" y2="16" />
          </svg>
        </div>
        <p className="text-sm font-medium text-red-400">Fehler</p>
        {state.errorMessage && (
          <p className="text-xs text-slate-400 text-center max-w-xs">{state.errorMessage}</p>
        )}
        <button
          onClick={onRetry}
          className="btn btn-primary mt-2"
        >
          Erneut versuchen
        </button>
      </div>
    );
  }

  // Phase: idle (shouldn't normally be visible in pairing view)
  return null;
};
