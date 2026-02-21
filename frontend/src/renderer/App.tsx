import { useState, useEffect } from 'react';
import { BrowserRouter, Routes, Route, Navigate } from 'react-router-dom';
import { Toaster } from 'react-hot-toast';
import toast from 'react-hot-toast';
import Login from './pages/Login';
import Dashboard from './pages/Dashboard';
import Sync from './pages/Sync';
import FileExplorer from './pages/FileExplorer';
import Conflicts from './pages/Conflicts';
import { RemoteServersPage } from './pages/RemoteServers';
import SettingsPanel from './components/SettingsPanel';
import ActivityLog from './components/ActivityLog';
import MainLayout from './components/MainLayout';

interface User {
  username: string;
  serverUrl?: string;
  selectedProfileId?: number;
}

export default function App() {
  const [user, setUser] = useState<User | null>(null);
  const [loading, setLoading] = useState(true);
  const [conflictCount, setConflictCount] = useState(0);

  useEffect(() => {
    const checkAuth = async () => {
      try {
        const response = await window.electronAPI.sendBackendCommand({
          type: 'check_stored_tokens',
        });
        if (response?.status === 'authenticated') {
          setUser({
            username: response.username || '',
            serverUrl: response.serverUrl || '',
          });
        }
      } catch {
        // needs_pairing or error → user stays null → redirected to /login
      } finally {
        setLoading(false);
      }
    };
    checkAuth();
  }, []);

  // Listen to backend events (conflicts + auth_required)
  useEffect(() => {
    const handleBackendMessage = (message: any) => {
      if (message.type === 'conflicts_updated') {
        setConflictCount(message.data?.conflicts?.length || 0);
      } else if (message.type === 'conflict_detected') {
        setConflictCount((prev) => prev + 1);
      } else if (message.type === 'auth_required') {
        // Token expired and refresh failed — force re-login
        setUser(null);
        toast.error('Session abgelaufen. Bitte erneut koppeln.');
      }
    };

    window.electronAPI?.onBackendMessage(handleBackendMessage);

    return () => {
      window.electronAPI?.removeBackendListener?.(handleBackendMessage);
    };
  }, []);

  const handleLogin = (userData: User) => {
    setUser(userData);
  };

  const handleLogout = async () => {
    try {
      await window.electronAPI.sendBackendCommand({ type: 'logout' });
    } catch {
      // Logout from UI regardless of backend response
    }
    setUser(null);
  };

  if (loading) {
    return (
      <div className="flex h-screen items-center justify-center bg-slate-950">
        <div className="text-slate-400">Loading...</div>
      </div>
    );
  }

  return (
    <BrowserRouter>
      <Toaster
        position="top-right"
        toastOptions={{
          className: '',
          style: {
            background: '#1e293b',
            color: '#f1f5f9',
            border: '1px solid #334155',
          },
        }}
      />
      <Routes>
        <Route
          path="/login"
          element={user ? <Navigate to="/" replace /> : <Login onLogin={handleLogin} />}
        />
        
        {/* Protected routes with MainLayout */}
        <Route
          path="/"
          element={
            user ? (
              <MainLayout user={user} onLogout={handleLogout} conflictCount={conflictCount}>
                <Dashboard user={user} onLogout={handleLogout} />
              </MainLayout>
            ) : (
              <Navigate to="/login" replace />
            )
          }
        />
        
        <Route
          path="/sync"
          element={
            user ? (
              <MainLayout user={user} onLogout={handleLogout} conflictCount={conflictCount}>
                <Sync />
              </MainLayout>
            ) : (
              <Navigate to="/login" replace />
            )
          }
        />
        
        <Route
          path="/files"
          element={
            user ? (
              <MainLayout user={user} onLogout={handleLogout} conflictCount={conflictCount}>
                <FileExplorer />
              </MainLayout>
            ) : (
              <Navigate to="/login" replace />
            )
          }
        />

        <Route
          path="/conflicts"
          element={
            user ? (
              <MainLayout user={user} onLogout={handleLogout} conflictCount={conflictCount}>
                <Conflicts />
              </MainLayout>
            ) : (
              <Navigate to="/login" replace />
            )
          }
        />

        <Route
          path="/remote-servers"
          element={
            user ? (
              <MainLayout user={user} onLogout={handleLogout} conflictCount={conflictCount}>
                <RemoteServersPage />
              </MainLayout>
            ) : (
              <Navigate to="/login" replace />
            )
          }
        />

        <Route
          path="/settings"
          element={
            user ? (
              <MainLayout user={user} onLogout={handleLogout} conflictCount={conflictCount}>
                <SettingsPanel onClose={() => window.history.back()} />
              </MainLayout>
            ) : (
              <Navigate to="/login" replace />
            )
          }
        />

        <Route
          path="/activity-log"
          element={
            user ? (
              <MainLayout user={user} onLogout={handleLogout} conflictCount={conflictCount}>
                <ActivityLog onClose={() => window.history.back()} />
              </MainLayout>
            ) : (
              <Navigate to="/login" replace />
            )
          }
        />
      </Routes>
    </BrowserRouter>
  );
}
