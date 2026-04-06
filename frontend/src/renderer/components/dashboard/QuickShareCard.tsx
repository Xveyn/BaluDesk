import { Share2, FolderUp, FolderDown } from 'lucide-react';

export function QuickShareCard() {
  return (
    <div className="rounded-xl border border-white/10 bg-gradient-to-br from-blue-500/10 to-indigo-600/10 p-4 backdrop-blur-sm transition-all hover:border-blue-500/30 hover:shadow-lg hover:shadow-blue-500/20">
      <div className="flex items-center space-x-2 mb-3">
        <div className="rounded-lg bg-gradient-to-br from-blue-500/20 to-indigo-500/20 p-1.5">
          <Share2 className="h-4 w-4 text-blue-400" />
        </div>
        <h3 className="text-sm font-medium text-slate-300">Quick Share</h3>
      </div>

      <div className="space-y-2">
        <div className="flex items-center justify-between">
          <div className="flex items-center space-x-1.5">
            <FolderUp className="h-3.5 w-3.5 text-blue-400" />
            <span className="text-xs text-slate-400">Shared by me</span>
          </div>
          <span className="text-sm font-semibold text-white">0</span>
        </div>
        <div className="flex items-center justify-between">
          <div className="flex items-center space-x-1.5">
            <FolderDown className="h-3.5 w-3.5 text-indigo-400" />
            <span className="text-xs text-slate-400">Shared with me</span>
          </div>
          <span className="text-sm font-semibold text-white">0</span>
        </div>
      </div>

      <div className="mt-3 pt-2 border-t border-white/10">
        <p className="text-xs text-slate-500 text-center">Coming soon</p>
      </div>
    </div>
  );
}
