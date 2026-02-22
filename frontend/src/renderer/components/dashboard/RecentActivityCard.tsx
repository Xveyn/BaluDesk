import { FileText, Upload, Download, AlertTriangle, Trash2, Clock } from 'lucide-react';
import { useRecentActivity } from '../../hooks/useRecentActivity';
import { formatRelativeTime, getFileName } from '../../../lib/formatters';

function getActivityIcon(type: string) {
  switch (type) {
    case 'upload': return <Upload className="h-3.5 w-3.5 text-emerald-400" />;
    case 'download': return <Download className="h-3.5 w-3.5 text-purple-400" />;
    case 'conflict': return <AlertTriangle className="h-3.5 w-3.5 text-amber-400" />;
    case 'delete': return <Trash2 className="h-3.5 w-3.5 text-red-400" />;
    default: return <FileText className="h-3.5 w-3.5 text-slate-400" />;
  }
}

function getActivityLabel(type: string): string {
  switch (type) {
    case 'upload': return 'uploaded';
    case 'download': return 'downloaded';
    case 'conflict': return 'conflict';
    case 'delete': return 'deleted';
    case 'modify': return 'modified';
    default: return type;
  }
}

export function RecentActivityCard() {
  const { activities, loading } = useRecentActivity(8);

  return (
    <div className="space-y-3">
      <div className="flex items-center justify-between">
        <div className="flex items-center space-x-2">
          <div className="rounded-lg bg-gradient-to-br from-indigo-500 to-indigo-600 p-2">
            <Clock className="h-4 w-4 text-white" />
          </div>
          <h3 className="text-sm font-semibold text-white">Recent Activity</h3>
        </div>
        <a href="#/activity-log" className="text-xs text-slate-400 hover:text-white transition-colors">
          View all →
        </a>
      </div>

      {loading ? (
        <div className="space-y-2">
          {[...Array(4)].map((_, i) => (
            <div key={i} className="h-8 bg-slate-700/30 rounded animate-pulse" />
          ))}
        </div>
      ) : activities.length === 0 ? (
        <div className="rounded-xl border border-white/10 bg-white/5 p-6 text-center">
          <p className="text-sm text-slate-500">No recent activity</p>
        </div>
      ) : (
        <div className="rounded-xl border border-white/10 bg-white/5 overflow-hidden">
          {activities.map((activity, index) => (
            <div
              key={activity.id || index}
              className={`flex items-center justify-between px-3 py-2 hover:bg-white/5 transition-colors ${
                index !== activities.length - 1 ? 'border-b border-white/5' : ''
              }`}
            >
              <div className="flex items-center space-x-2 min-w-0 flex-1">
                {getActivityIcon(activity.activity_type)}
                <span className="text-xs text-white truncate" title={activity.file_path}>
                  {getFileName(activity.file_path)}
                </span>
                <span className="text-[10px] text-slate-500">
                  {getActivityLabel(activity.activity_type)}
                </span>
              </div>
              <span className="text-[10px] text-slate-500 flex-shrink-0 ml-2">
                {formatRelativeTime(activity.timestamp)}
              </span>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}
