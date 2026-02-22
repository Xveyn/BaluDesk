import { Server } from 'lucide-react';
import { useNasServices } from '../../hooks/useNasServices';

export function ServicesPanel() {
  const { services, loading, error } = useNasServices();

  return (
    <div className="rounded-xl border border-white/10 bg-gradient-to-br from-violet-500/10 to-violet-600/10 p-4 backdrop-blur-sm hover:border-violet-500/30 hover:shadow-lg hover:shadow-violet-500/20 transition-all">
      <div className="flex items-center space-x-2 mb-3">
        <div className="rounded-lg bg-gradient-to-br from-violet-500/20 to-violet-600/20 p-1.5">
          <Server className="h-4 w-4 text-violet-400" />
        </div>
        <h3 className="text-sm font-medium text-slate-300">Services</h3>
      </div>

      {loading ? (
        <div className="space-y-2">
          {[...Array(4)].map((_, i) => (
            <div key={i} className="h-5 bg-slate-700/30 rounded animate-pulse" />
          ))}
        </div>
      ) : error ? (
        <p className="text-xs text-slate-500">{error}</p>
      ) : services.length === 0 ? (
        <p className="text-xs text-slate-500">No services detected</p>
      ) : (
        <div className="space-y-1.5">
          {services.map((service) => (
            <div
              key={service.name}
              className="flex items-center justify-between py-1"
            >
              <span className="text-xs text-slate-300">{service.name}</span>
              <span
                className={`px-2 py-0.5 rounded-full text-[10px] font-medium ${
                  service.status === 'running'
                    ? 'bg-emerald-500/20 text-emerald-300'
                    : service.status === 'stopped'
                      ? 'bg-red-500/20 text-red-300'
                      : 'bg-slate-700/30 text-slate-400'
                }`}
              >
                {service.status.charAt(0).toUpperCase() + service.status.slice(1)}
              </span>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}
