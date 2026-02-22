/**
 * Formatting utilities for common data types
 */

/**
 * Format bytes to human-readable size (B, KB, MB, GB, TB)
 */
export const formatBytes = (bytes: number): string => {
  if (!bytes || Number.isNaN(bytes)) return '0 B';
  const k = 1024;
  const units = ['B', 'KB', 'MB', 'GB', 'TB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  const value = bytes / Math.pow(k, i);
  return `${value.toFixed(value >= 100 ? 1 : 2)} ${units[i]}`;
};

/**
 * Format bytes with a simpler calculation (rounded)
 */
export const formatSize = (bytes?: number): string => {
  if (!bytes) return 'Unknown';
  const units = ['B', 'KB', 'MB', 'GB', 'TB'];
  let size = bytes;
  let unitIndex = 0;
  while (size >= 1024 && unitIndex < units.length - 1) {
    size /= 1024;
    unitIndex++;
  }
  return `${size.toFixed(2)} ${units[unitIndex]}`;
};

/**
 * Format uptime in seconds to human-readable format
 */
export const formatUptime = (seconds: number): string => {
  const days = Math.floor(seconds / 86400);
  const hours = Math.floor((seconds % 86400) / 3600);
  const mins = Math.floor((seconds % 3600) / 60);

  if (days > 0) {
    return `${days}d ${hours}h`;
  } else if (hours > 0) {
    return `${hours}h ${mins}m`;
  } else {
    return `${mins}m`;
  }
};

/**
 * Format ISO date string to localized format
 */
export const formatDate = (dateString: string): string => {
  try {
    return new Date(dateString).toLocaleString();
  } catch {
    return dateString;
  }
};

/**
 * Format ISO date string to short format (YYYY-MM-DD)
 */
export const formatDateShort = (dateString: string): string => {
  try {
    const date = new Date(dateString);
    return date.toISOString().split('T')[0];
  } catch {
    return dateString;
  }
};

/**
 * Format ISO date string to time only (HH:MM:SS)
 */
export const formatTime = (dateString: string): string => {
  try {
    const date = new Date(dateString);
    return date.toLocaleTimeString();
  } catch {
    return dateString;
  }
};

/**
 * Format bytes per second to human-readable speed (B/s, KB/s, MB/s)
 */
export const formatSpeed = (bytesPerSecond: number): string => {
  if (bytesPerSecond < 1024) return `${bytesPerSecond.toFixed(0)} B/s`;
  if (bytesPerSecond < 1024 * 1024) return `${(bytesPerSecond / 1024).toFixed(1)} KB/s`;
  return `${(bytesPerSecond / (1024 * 1024)).toFixed(1)} MB/s`;
};

/**
 * Format timestamp to relative time string (e.g., "2m ago", "1h ago")
 */
export const formatRelativeTime = (timestamp: string): string => {
  if (!timestamp) return 'Never';
  const now = Date.now();

  let msTimestamp: number;

  // Check if purely numeric (Unix timestamp)
  if (/^\d+$/.test(timestamp.trim())) {
    const ts = parseInt(timestamp, 10);
    if (isNaN(ts) || ts === 0) return 'Never';
    msTimestamp = ts < 10000000000 ? ts * 1000 : ts;
  } else {
    // ISO date string (e.g. "2026-02-22 14:35:21" from SQLite)
    const parsed = new Date(timestamp).getTime();
    if (isNaN(parsed)) return 'Never';
    msTimestamp = parsed;
  }

  const diff = now - msTimestamp;
  if (diff < 0) return 'just now';
  if (diff < 60000) return 'just now';
  if (diff < 3600000) return `${Math.floor(diff / 60000)}m ago`;
  if (diff < 86400000) return `${Math.floor(diff / 3600000)}h ago`;
  return `${Math.floor(diff / 86400000)}d ago`;
};

/**
 * Extract file/folder name from a path
 */
export const getFileName = (path: string): string => {
  const parts = path.replace(/\\/g, '/').split('/').filter(Boolean);
  return parts[parts.length - 1] || path;
};

/**
 * Format Unix timestamp (seconds or milliseconds) to localized date/time
 */
export const formatTimestamp = (timestamp: string | number): string => {
  if (!timestamp) return 'Never';

  try {
    // Convert to number if it's a string
    const ts = typeof timestamp === 'string' ? parseInt(timestamp, 10) : timestamp;

    if (Number.isNaN(ts) || ts === 0) return 'Never';

    // Check if it's in seconds (< year 2100 in milliseconds)
    // Unix timestamps in seconds are ~10 digits, in ms are ~13 digits
    const tsInMs = ts < 10000000000 ? ts * 1000 : ts;

    const date = new Date(tsInMs);

    // Check if date is valid
    if (Number.isNaN(date.getTime())) return 'Invalid date';

    return date.toLocaleString();
  } catch {
    return String(timestamp);
  }
};
