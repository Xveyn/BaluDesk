import { useState, useEffect, useRef } from 'react';
import { Folder, FolderOpen, FolderPlus, ChevronRight, ChevronDown, X, Check, Loader2 } from 'lucide-react';
import toast from 'react-hot-toast';
import { BackendResponse } from '../../lib/types';

declare const window: any;

type SyncDirectionType = 'push' | 'pull' | 'bidirectional';

interface RemoteFolderBrowserProps {
  isOpen: boolean;
  onClose: () => void;
  onSelect: (remotePath: string, syncDirection: string) => void;
}

interface RemoteFolder {
  name: string;
  path: string;
  size: number;
  is_directory: boolean;
  modified_at: string;
}

interface FolderNode {
  folder: RemoteFolder;
  children: FolderNode[];
  expanded: boolean;
  loading: boolean;
  loaded: boolean;
}

export default function RemoteFolderBrowser({ isOpen, onClose, onSelect }: RemoteFolderBrowserProps) {
  const [rootNodes, setRootNodes] = useState<FolderNode[]>([]);
  const [loading, setLoading] = useState(false);
  const [selectedPath, setSelectedPath] = useState<string | null>(null);
  const [syncDirection, setSyncDirection] = useState<SyncDirectionType>('bidirectional');
  const [showNewFolder, setShowNewFolder] = useState(false);
  const [newFolderName, setNewFolderName] = useState('');
  const [creatingFolder, setCreatingFolder] = useState(false);
  const newFolderInputRef = useRef<HTMLInputElement>(null);

  useEffect(() => {
    if (isOpen) {
      loadFolder('/');
    } else {
      setRootNodes([]);
      setSelectedPath(null);
      setSyncDirection('bidirectional');
      setShowNewFolder(false);
      setNewFolderName('');
    }
  }, [isOpen]);

  const loadFolder = async (path: string) => {
    if (path === '/') {
      setLoading(true);
    }

    try {
      const response: BackendResponse = await window.electronAPI.sendBackendCommand({
        type: 'list_remote_folders',
        data: { path },
      });

      if (response.success && response.data?.folders) {
        const folders: RemoteFolder[] = response.data.folders;

        if (path === '/') {
          const nodes: FolderNode[] = folders.map((f) => ({
            folder: f,
            children: [],
            expanded: false,
            loading: false,
            loaded: false,
          }));
          setRootNodes(nodes);
        } else {
          // Update the expanded node's children
          setRootNodes((prev) => updateNodeChildren(prev, path, folders));
        }
      } else {
        toast.error(response.error || 'Failed to load folders');
      }
    } catch (err) {
      console.error('Error loading remote folders:', err);
      toast.error('Failed to connect to server');
    } finally {
      if (path === '/') {
        setLoading(false);
      }
    }
  };

  const handleCreateFolder = async () => {
    const name = newFolderName.trim();
    if (!name) return;

    const parentPath = selectedPath || '/';
    setCreatingFolder(true);

    try {
      const response: BackendResponse = await window.electronAPI.sendBackendCommand({
        type: 'create_folder',
        data: { path: parentPath, name },
      });

      if (response.success) {
        toast.success(`Folder "${name}" created`);
        setShowNewFolder(false);
        setNewFolderName('');

        // Reload parent folder to show new entry
        if (parentPath === '/') {
          await loadFolder('/');
        } else {
          // Mark parent as not-loaded so it re-fetches
          setRootNodes((prev) => markNodeUnloaded(prev, parentPath));
          await loadFolder(parentPath);
        }

        // Select the newly created folder
        const newPath = parentPath === '/' ? `/${name}` : `${parentPath}/${name}`;
        setSelectedPath(newPath);
      } else {
        toast.error(response.error || 'Failed to create folder');
      }
    } catch (err) {
      console.error('Error creating folder:', err);
      toast.error('Failed to create folder');
    } finally {
      setCreatingFolder(false);
    }
  };

  const markNodeUnloaded = (nodes: FolderNode[], path: string): FolderNode[] => {
    return nodes.map((node) => {
      if (node.folder.path === path) {
        return { ...node, loaded: false, expanded: false, children: [] };
      }
      return { ...node, children: markNodeUnloaded(node.children, path) };
    });
  };

  const updateNodeChildren = (
    nodes: FolderNode[],
    parentPath: string,
    children: RemoteFolder[]
  ): FolderNode[] => {
    return nodes.map((node) => {
      if (node.folder.path === parentPath) {
        return {
          ...node,
          children: children.map((f) => ({
            folder: f,
            children: [],
            expanded: false,
            loading: false,
            loaded: false,
          })),
          expanded: true,
          loading: false,
          loaded: true,
        };
      }
      if (node.children.length > 0) {
        return {
          ...node,
          children: updateNodeChildren(node.children, parentPath, children),
        };
      }
      return node;
    });
  };

  const toggleExpand = async (path: string) => {
    const node = findNode(rootNodes, path);
    if (!node) return;

    if (node.expanded) {
      // Collapse
      setRootNodes((prev) => setNodeExpanded(prev, path, false));
    } else {
      if (!node.loaded) {
        // Set loading state
        setRootNodes((prev) => setNodeLoading(prev, path, true));
        await loadFolder(path);
      } else {
        setRootNodes((prev) => setNodeExpanded(prev, path, true));
      }
    }
  };

  const findNode = (nodes: FolderNode[], path: string): FolderNode | null => {
    for (const node of nodes) {
      if (node.folder.path === path) return node;
      const found = findNode(node.children, path);
      if (found) return found;
    }
    return null;
  };

  const setNodeExpanded = (nodes: FolderNode[], path: string, expanded: boolean): FolderNode[] => {
    return nodes.map((node) => {
      if (node.folder.path === path) {
        return { ...node, expanded };
      }
      return { ...node, children: setNodeExpanded(node.children, path, expanded) };
    });
  };

  const setNodeLoading = (nodes: FolderNode[], path: string, isLoading: boolean): FolderNode[] => {
    return nodes.map((node) => {
      if (node.folder.path === path) {
        return { ...node, loading: isLoading };
      }
      return { ...node, children: setNodeLoading(node.children, path, isLoading) };
    });
  };

  const handleConfirm = () => {
    if (selectedPath) {
      onSelect(selectedPath, syncDirection);
      onClose();
    }
  };

  const renderNode = (node: FolderNode, depth: number = 0) => {
    const isSelected = selectedPath === node.folder.path;

    return (
      <div key={node.folder.path}>
        <div
          className={`flex items-center space-x-2 px-3 py-2 rounded-lg cursor-pointer transition-colors ${
            isSelected
              ? 'bg-blue-500/20 border border-blue-500/40 text-blue-300'
              : 'hover:bg-slate-800 text-slate-300'
          }`}
          style={{ paddingLeft: `${depth * 20 + 12}px` }}
          onClick={() => setSelectedPath(node.folder.path)}
          onDoubleClick={() => toggleExpand(node.folder.path)}
        >
          {/* Expand/Collapse toggle */}
          <button
            onClick={(e) => {
              e.stopPropagation();
              toggleExpand(node.folder.path);
            }}
            className="flex-shrink-0 p-0.5 rounded hover:bg-slate-700 transition-colors"
          >
            {node.loading ? (
              <Loader2 className="h-4 w-4 text-slate-500 animate-spin" />
            ) : node.expanded ? (
              <ChevronDown className="h-4 w-4 text-slate-500" />
            ) : (
              <ChevronRight className="h-4 w-4 text-slate-500" />
            )}
          </button>

          {/* Folder icon */}
          {node.expanded ? (
            <FolderOpen className="h-4 w-4 text-amber-400 flex-shrink-0" />
          ) : (
            <Folder className="h-4 w-4 text-amber-400 flex-shrink-0" />
          )}

          {/* Folder name */}
          <span className="text-sm truncate">{node.folder.name}</span>
        </div>

        {/* Children */}
        {node.expanded && node.children.length > 0 && (
          <div>
            {node.children.map((child) => renderNode(child, depth + 1))}
          </div>
        )}

        {/* Empty state */}
        {node.expanded && node.loaded && node.children.length === 0 && (
          <div
            className="text-xs text-slate-600 italic px-3 py-1"
            style={{ paddingLeft: `${(depth + 1) * 20 + 12}px` }}
          >
            No subfolders
          </div>
        )}
      </div>
    );
  };

  if (!isOpen) return null;

  return (
    <div className="fixed inset-0 bg-black/60 flex items-center justify-center z-50 p-4">
      <div className="bg-slate-900 rounded-xl border border-slate-800 w-full max-w-lg shadow-2xl flex flex-col max-h-[80vh]">
        {/* Header */}
        <div className="border-b border-slate-800 p-5 flex items-center justify-between flex-shrink-0">
          <div>
            <h2 className="text-lg font-bold text-white">Select Remote Folder</h2>
            <p className="text-sm text-slate-400 mt-1">Choose a server folder to sync with</p>
          </div>
          <button
            onClick={onClose}
            className="rounded-lg p-2 text-slate-400 hover:bg-slate-800 hover:text-slate-200 transition-colors"
          >
            <X className="h-5 w-5" />
          </button>
        </div>

        {/* Direction Picker */}
        <div className="border-b border-slate-800 px-5 py-3">
          <label className="block text-xs font-medium text-slate-400 mb-2">Sync Direction</label>
          <div className="grid grid-cols-3 gap-2">
            {([
              { value: 'push' as SyncDirectionType, label: '↑ Upload to NAS' },
              { value: 'bidirectional' as SyncDirectionType, label: '↕ Bidirectional' },
              { value: 'pull' as SyncDirectionType, label: '↓ Download from NAS' },
            ]).map((opt) => (
              <button
                key={opt.value}
                onClick={() => setSyncDirection(opt.value)}
                className={`rounded-lg border px-2 py-1.5 text-xs font-medium transition-all ${
                  syncDirection === opt.value
                    ? 'border-blue-500 bg-blue-500/20 text-blue-300'
                    : 'border-slate-700 bg-slate-800 text-slate-400 hover:border-slate-600'
                }`}
              >
                {opt.label}
              </button>
            ))}
          </div>
        </div>

        {/* Folder Tree */}
        <div className="flex-1 overflow-y-auto p-4 min-h-[200px]">
          {loading ? (
            <div className="flex flex-col items-center justify-center py-12 space-y-3">
              <Loader2 className="h-8 w-8 text-blue-400 animate-spin" />
              <p className="text-sm text-slate-400">Loading server folders...</p>
            </div>
          ) : rootNodes.length === 0 ? (
            <div className="flex flex-col items-center justify-center py-12 space-y-3">
              <Folder className="h-8 w-8 text-slate-600" />
              <p className="text-sm text-slate-400">No folders found on server</p>
            </div>
          ) : (
            <div className="space-y-0.5">
              {rootNodes.map((node) => renderNode(node))}
            </div>
          )}
        </div>

        {/* Selected path info */}
        {selectedPath && (
          <div className="border-t border-slate-800 px-5 py-3 bg-slate-950/50">
            <p className="text-xs text-slate-400">
              Selected: <span className="text-blue-400 font-mono">{selectedPath}</span>
            </p>
          </div>
        )}

        {/* New Folder Inline */}
        {showNewFolder && (
          <div className="border-t border-slate-800 px-5 py-3 flex items-center space-x-2">
            <FolderPlus className="h-4 w-4 text-amber-400 flex-shrink-0" />
            <input
              ref={newFolderInputRef}
              type="text"
              value={newFolderName}
              onChange={(e) => setNewFolderName(e.target.value)}
              onKeyDown={(e) => {
                if (e.key === 'Enter') handleCreateFolder();
                if (e.key === 'Escape') { setShowNewFolder(false); setNewFolderName(''); }
              }}
              placeholder="Folder name..."
              className="flex-1 bg-slate-800 border border-slate-700 rounded-lg px-3 py-1.5 text-sm text-white placeholder-slate-500 focus:outline-none focus:border-blue-500"
              autoFocus
            />
            <button
              onClick={handleCreateFolder}
              disabled={!newFolderName.trim() || creatingFolder}
              className="rounded-lg bg-blue-500 px-3 py-1.5 text-xs text-white font-medium hover:bg-blue-600 transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
            >
              {creatingFolder ? <Loader2 className="h-3.5 w-3.5 animate-spin" /> : 'Create'}
            </button>
            <button
              onClick={() => { setShowNewFolder(false); setNewFolderName(''); }}
              className="rounded-lg p-1.5 text-slate-400 hover:bg-slate-800 hover:text-slate-200 transition-colors"
            >
              <X className="h-3.5 w-3.5" />
            </button>
          </div>
        )}

        {/* Footer */}
        <div className="border-t border-slate-800 p-5 flex items-center justify-between flex-shrink-0">
          <button
            onClick={() => { setShowNewFolder(true); setTimeout(() => newFolderInputRef.current?.focus(), 50); }}
            className={`flex items-center space-x-1.5 rounded-lg px-3 py-2 text-sm text-slate-400 hover:bg-slate-800 hover:text-slate-200 transition-colors font-medium ${showNewFolder ? 'invisible' : ''}`}
          >
            <FolderPlus className="h-4 w-4" />
            <span>New Folder</span>
          </button>
          <div className="flex items-center space-x-3">
            <button
              onClick={onClose}
              className="rounded-lg px-4 py-2 text-slate-400 hover:bg-slate-800 hover:text-slate-200 transition-colors font-medium"
            >
              Cancel
            </button>
            <button
              onClick={handleConfirm}
              disabled={!selectedPath}
              className="flex items-center space-x-2 rounded-lg bg-gradient-to-r from-blue-500 to-blue-600 px-5 py-2 text-white font-medium hover:shadow-lg hover:shadow-blue-500/30 transition-all disabled:opacity-50 disabled:cursor-not-allowed"
            >
              <Check className="h-4 w-4" />
              <span>Select Folder</span>
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}
