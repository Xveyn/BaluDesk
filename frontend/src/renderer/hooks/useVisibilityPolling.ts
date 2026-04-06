import { useEffect, useRef } from 'react';

/**
 * Runs a callback on an interval, but pauses when the document is hidden.
 * Also calls the callback immediately on mount.
 */
export function useVisibilityPolling(callback: () => void, intervalMs: number) {
  const savedCallback = useRef(callback);
  savedCallback.current = callback;

  useEffect(() => {
    savedCallback.current();

    let timer: ReturnType<typeof setInterval> | null = null;

    const start = () => {
      if (!timer) {
        timer = setInterval(() => savedCallback.current(), intervalMs);
      }
    };

    const stop = () => {
      if (timer) {
        clearInterval(timer);
        timer = null;
      }
    };

    const onVisibilityChange = () => {
      if (document.hidden) {
        stop();
      } else {
        savedCallback.current();
        start();
      }
    };

    if (!document.hidden) {
      start();
    }

    document.addEventListener('visibilitychange', onVisibilityChange);

    return () => {
      stop();
      document.removeEventListener('visibilitychange', onVisibilityChange);
    };
  }, [intervalMs]);
}
