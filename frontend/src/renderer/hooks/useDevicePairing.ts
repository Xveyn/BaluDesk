import { useState, useRef, useCallback } from 'react';

export interface PairingState {
  phase: 'idle' | 'requesting' | 'waiting' | 'approved' | 'denied' | 'expired' | 'error';
  userCode: string | null;
  verificationUrl: string | null;
  secondsRemaining: number;
  errorMessage: string | null;
}

interface DeviceCodeResponse {
  device_code: string;
  user_code: string;
  verification_url: string;
  expires_in: number;
  interval: number;
}

interface PollResponse {
  status: 'authorization_pending' | 'approved' | 'denied' | 'expired';
  access_token?: string;
  refresh_token?: string;
  user?: {
    username: string;
    id: number;
  };
}

export function useDevicePairing() {
  const [state, setState] = useState<PairingState>({
    phase: 'idle',
    userCode: null,
    verificationUrl: null,
    secondsRemaining: 0,
    errorMessage: null,
  });

  const countdownRef = useRef<ReturnType<typeof setInterval> | null>(null);
  const pollingRef = useRef<ReturnType<typeof setInterval> | null>(null);
  const cancelledRef = useRef(false);

  const cleanup = useCallback(() => {
    if (countdownRef.current) {
      clearInterval(countdownRef.current);
      countdownRef.current = null;
    }
    if (pollingRef.current) {
      clearInterval(pollingRef.current);
      pollingRef.current = null;
    }
  }, []);

  const cancelPairing = useCallback(() => {
    cancelledRef.current = true;
    cleanup();
    setState({
      phase: 'idle',
      userCode: null,
      verificationUrl: null,
      secondsRemaining: 0,
      errorMessage: null,
    });
  }, [cleanup]);

  const startPairing = useCallback(async (serverUrl: string) => {
    // Reset state
    cancelledRef.current = false;
    cleanup();

    // Normalize URL: ensure http:// prefix (curl defaults to HTTPS without scheme)
    let normalizedUrl = serverUrl.trim();
    if (normalizedUrl && !normalizedUrl.match(/^https?:\/\//i)) {
      normalizedUrl = `http://${normalizedUrl}`;
    }

    setState(prev => ({ ...prev, phase: 'requesting', errorMessage: null }));

    try {
      // 1. Get device info from backend
      const deviceInfoResponse = await window.electronAPI.sendBackendCommand({
        type: 'get_device_info',
      });

      const deviceInfo = deviceInfoResponse?.data || {};
      const deviceId = deviceInfo.deviceId || 'unknown';
      const deviceName = deviceInfo.deviceName || 'BaluDesk';
      const platform = deviceInfo.platform || 'unknown';

      // 2. Request device code from server (via backend to avoid CORS)
      const codeResponse = await window.electronAPI.sendBackendCommand({
        type: 'request_device_code',
        data: {
          serverUrl: normalizedUrl,
          deviceId,
          deviceName,
          platform,
        },
      });

      if (!codeResponse?.success) {
        throw new Error(codeResponse?.error || codeResponse?.message || 'Failed to request device code');
      }

      const data: DeviceCodeResponse = codeResponse.data;
      const { device_code, user_code, verification_url, expires_in, interval } = data;

      if (cancelledRef.current) return;

      // 3. Update state with code and URL
      setState({
        phase: 'waiting',
        userCode: user_code,
        verificationUrl: verification_url,
        secondsRemaining: expires_in,
        errorMessage: null,
      });

      // 4. Start countdown timer
      countdownRef.current = setInterval(() => {
        setState(prev => {
          const newSeconds = prev.secondsRemaining - 1;
          if (newSeconds <= 0) {
            cleanup();
            return { ...prev, phase: 'expired', secondsRemaining: 0 };
          }
          return { ...prev, secondsRemaining: newSeconds };
        });
      }, 1000);

      // 5. Start polling loop
      const pollInterval = Math.max(interval, 2) * 1000; // Minimum 2 seconds

      pollingRef.current = setInterval(async () => {
        if (cancelledRef.current) {
          cleanup();
          return;
        }

        try {
          const pollResult = await window.electronAPI.sendBackendCommand({
            type: 'poll_device_code',
            data: {
              serverUrl: normalizedUrl,
              deviceCode: device_code,
            },
          });

          if (!pollResult?.success) {
            // Backend error — keep polling (might be transient)
            return;
          }

          const pollData: PollResponse = pollResult.data;

          if (pollData.status === 'expired') {
            cleanup();
            setState(prev => ({ ...prev, phase: 'expired' }));
            return;
          }

          if (cancelledRef.current) return;

          switch (pollData.status) {
            case 'authorization_pending':
              // Keep polling
              break;

            case 'approved': {
              cleanup();
              // Send tokens to C++ backend for secure storage
              const accessToken = pollData.access_token || '';
              const refreshToken = pollData.refresh_token || '';
              const username = pollData.user?.username || '';

              await window.electronAPI.sendBackendCommand({
                type: 'set_tokens',
                data: {
                  serverUrl: normalizedUrl,
                  accessToken,
                  refreshToken,
                  username,
                },
              });

              setState(prev => ({
                ...prev,
                phase: 'approved',
              }));
              break;
            }

            case 'denied':
              cleanup();
              setState(prev => ({ ...prev, phase: 'denied' }));
              break;
          }
        } catch {
          // Network error — keep polling (server might be temporarily unreachable)
          console.warn('[DevicePairing] Poll network error, retrying...');
        }
      }, pollInterval);

    } catch (err: any) {
      if (cancelledRef.current) return;
      cleanup();
      setState({
        phase: 'error',
        userCode: null,
        verificationUrl: null,
        secondsRemaining: 0,
        errorMessage: err.message || 'Failed to start pairing',
      });
    }
  }, [cleanup]);

  return { state, startPairing, cancelPairing };
}
