import React, { useEffect, useRef, useState, useCallback } from 'react';
import Editor, { type OnMount } from '@monaco-editor/react';
import {
  Play,
  Hammer,
  CheckCheck,
  Save,
  FileCode2,
  Plus,
  Trash2,
  X,
  Terminal as TerminalIcon,
  AlertTriangle,
  Binary,
  GitBranch,
  List,
  Loader2,
  Keyboard,
} from 'lucide-react';
import { registerBosLanguage, monaco } from './lib/bosLanguage';
import {
  fetchFiles,
  saveFile,
  deleteFile,
  executeBos,
  runTestSuite,
  fetchDocs,
  fetchVersion,
  type BosFile,
  type BosExecuteResponse,
  type BosTestSuiteResponse,
  type BosDiagnostic,
} from './lib/api';
import DocsViewer from './components/DocsViewer';
import TestSuitePanel from './components/TestSuitePanel';

registerBosLanguage();

type TopView = 'editor' | 'docs' | 'tests';
type BottomTab = 'terminal' | 'diagnostics' | 'ir' | 'ast' | 'tokens';

export default function App() {
  const [view, setView] = useState<TopView>('editor');
  const [files, setFiles] = useState<BosFile[]>([]);
  const [openTabs, setOpenTabs] = useState<string[]>([]);
  const [activeFile, setActiveFile] = useState<string>('');
  const [dirtyFiles, setDirtyFiles] = useState<Record<string, boolean>>({});
  const [fileContents, setFileContents] = useState<Record<string, string>>({});

  const [creatingFile, setCreatingFile] = useState(false);
  const [newFileName, setNewFileName] = useState('');
  const [fileError, setFileError] = useState<string | null>(null);

  const [stdinValue, setStdinValue] = useState('');
  const [showStdin, setShowStdin] = useState(false);

  const [runningAction, setRunningAction] = useState<'pokreni' | 'prevedi' | 'provjeri' | null>(null);
  const [execResult, setExecResult] = useState<BosExecuteResponse | null>(null);
  const [bottomTab, setBottomTab] = useState<BottomTab>('terminal');

  const [testResult, setTestResult] = useState<BosTestSuiteResponse | null>(null);
  const [runningTests, setRunningTests] = useState(false);

  const [docs, setDocs] = useState({ specification: '', architecture: '' });
  const [versionText, setVersionText] = useState('BOS 0.1.0');

  const editorRef = useRef<monaco.editor.IStandaloneCodeEditor | null>(null);

  // Initial data load
  useEffect(() => {
    (async () => {
      try {
        const [loadedFiles, loadedDocs, ver] = await Promise.all([
          fetchFiles(),
          fetchDocs(),
          fetchVersion(),
        ]);
        setFiles(loadedFiles);
        const contentsMap: Record<string, string> = {};
        for (const f of loadedFiles) contentsMap[f.name] = f.content;
        setFileContents(contentsMap);

        const preferred =
          loadedFiles.find((f) => f.name === 'dobrodosli.bos')?.name ||
          loadedFiles[0]?.name ||
          '';
        if (preferred) {
          setOpenTabs([preferred]);
          setActiveFile(preferred);
        }
        if (loadedDocs.ok) {
          setDocs({
            specification: loadedDocs.specification,
            architecture: loadedDocs.architecture,
          });
        }
        setVersionText(ver);
      } catch (err) {
        setFileError(err instanceof Error ? err.message : 'Greška pri inicijalizaciji.');
      }
    })();
  }, []);

  // Sync Monaco inline diagnostic markers whenever execResult changes
  useEffect(() => {
    const editor = editorRef.current;
    if (!editor) return;
    const model = editor.getModel();
    if (!model) return;

    if (!execResult || !execResult.diagnostics.length) {
      monaco.editor.setModelMarkers(model, 'bos', []);
      return;
    }

    const markers: monaco.editor.IMarkerData[] = execResult.diagnostics.map((d) => ({
      severity:
        d.level === 'upozorenje'
          ? monaco.MarkerSeverity.Warning
          : monaco.MarkerSeverity.Error,
      message: `${d.stage}${d.code ? ` [${d.code}]` : ''}: ${d.message}`,
      startLineNumber: Math.max(1, d.line),
      startColumn: Math.max(1, d.column),
      endLineNumber: Math.max(1, d.line),
      endColumn: Math.max(2, d.column + 8),
    }));
    monaco.editor.setModelMarkers(model, 'bos', markers);
  }, [execResult, activeFile]);

  const openFile = useCallback((name: string) => {
    setOpenTabs((prev) => (prev.includes(name) ? prev : [...prev, name]));
    setActiveFile(name);
    setView('editor');
  }, []);

  const closeTab = (name: string, e: React.MouseEvent) => {
    e.stopPropagation();
    setOpenTabs((prev) => {
      const next = prev.filter((t) => t !== name);
      if (activeFile === name) {
        setActiveFile(next[next.length - 1] || '');
      }
      return next;
    });
  };

  const handleSaveCurrent = useCallback(async () => {
    if (!activeFile) return;
    const content = fileContents[activeFile] ?? '';
    try {
      await saveFile(activeFile, content);
      setDirtyFiles((prev) => ({ ...prev, [activeFile]: false }));
      setFileError(null);
    } catch (err) {
      setFileError(err instanceof Error ? err.message : 'Greška pri snimanju.');
    }
  }, [activeFile, fileContents]);

  const handleAction = useCallback(
    async (action: 'pokreni' | 'prevedi' | 'provjeri') => {
      if (!activeFile) return;
      setView('editor');
      setRunningAction(action);
      setFileError(null);
      const code = fileContents[activeFile] ?? '';

      // Auto-save dirty file before running
      if (dirtyFiles[activeFile]) {
        try {
          await saveFile(activeFile, code);
          setDirtyFiles((prev) => ({ ...prev, [activeFile]: false }));
        } catch {
          // Proceed even if save failed
        }
      }

      try {
        const res = await executeBos(action, activeFile, code, stdinValue);
        setExecResult(res);
        if (!res.ok && res.diagnostics.length > 0) {
          setBottomTab('diagnostics');
        } else {
          setBottomTab('terminal');
        }
      } catch (err) {
        setFileError(err instanceof Error ? err.message : 'Greška pri komunikaciji sa kompajlerom.');
      } finally {
        setRunningAction(null);
      }
    },
    [activeFile, fileContents, dirtyFiles, stdinValue]
  );

  // Global keyboard shortcuts (Ctrl/Cmd+S, Ctrl/Cmd+Enter)
  useEffect(() => {
    const onKeyDown = (e: KeyboardEvent) => {
      if ((e.metaKey || e.ctrlKey) && e.key.toLowerCase() === 's') {
        e.preventDefault();
        handleSaveCurrent();
      } else if ((e.metaKey || e.ctrlKey) && e.key === 'Enter') {
        e.preventDefault();
        handleAction('pokreni');
      }
    };
    window.addEventListener('keydown', onKeyDown);
    return () => window.removeEventListener('keydown', onKeyDown);
  }, [handleSaveCurrent, handleAction]);

  const handleCreateFile = async (e: React.FormEvent) => {
    e.preventDefault();
    let trimmed = newFileName.trim();
    if (!trimmed) return;
    if (!trimmed.endsWith('.bos')) trimmed += '.bos';

    const template = `// ${trimmed}\nispisi "Pozdrav iz ${trimmed}!"\n`;
    try {
      await saveFile(trimmed, template);
      const refreshed = await fetchFiles();
      setFiles(refreshed);
      setFileContents((prev) => ({ ...prev, [trimmed]: template }));
      setDirtyFiles((prev) => ({ ...prev, [trimmed]: false }));
      openFile(trimmed);
      setCreatingFile(false);
      setNewFileName('');
      setFileError(null);
    } catch (err) {
      setFileError(err instanceof Error ? err.message : 'Neispravan naziv datoteke.');
    }
  };

  const handleDeleteFile = async (name: string, e: React.MouseEvent) => {
    e.stopPropagation();
    try {
      await deleteFile(name);
      const refreshed = await fetchFiles();
      setFiles(refreshed);
      setOpenTabs((prev) => {
        const next = prev.filter((t) => t !== name);
        if (activeFile === name) {
          setActiveFile(next[next.length - 1] || refreshed[0]?.name || '');
        }
        return next;
      });
    } catch (err) {
      setFileError(err instanceof Error ? err.message : 'Greška pri brisanju.');
    }
  };

  const handleRunTests = async () => {
    setRunningTests(true);
    try {
      const res = await runTestSuite();
      setTestResult(res);
    } finally {
      setRunningTests(false);
    }
  };

  const handleInsertSnippet = async (filename: string, code: string) => {
    try {
      await saveFile(filename, code);
      const refreshed = await fetchFiles();
      setFiles(refreshed);
      setFileContents((prev) => ({ ...prev, [filename]: code }));
      openFile(filename);
    } catch (err) {
      setFileError(err instanceof Error ? err.message : 'Greška pri kreiranju primjera.');
    }
  };

  const jumpToDiagnostic = (diag: BosDiagnostic) => {
    const editor = editorRef.current;
    if (!editor) return;
    editor.revealLineInCenter(diag.line);
    editor.setPosition({ lineNumber: diag.line, column: diag.column });
    editor.focus();
  };

  const handleEditorMount: OnMount = (editor) => {
    editorRef.current = editor;
  };

  const activeCode = activeFile ? fileContents[activeFile] ?? '' : '';
  const diagCount = execResult?.diagnostics?.length ?? 0;

  return (
    <div className="h-screen w-screen flex flex-col overflow-hidden bg-[#0B0F17] text-slate-100">
      {/* Top Bar — 3-Zone Contract */}
      <header className="h-12 shrink-0 flex items-center justify-between gap-8 px-5 border-b border-slate-800/80 bg-[#0E131F]">
        <button
          onClick={() => setView('editor')}
          className="text-base font-bold tracking-tight text-white whitespace-nowrap shrink-0 cursor-pointer"
        >
          BOS Studio
        </button>

        <nav className="flex items-center gap-6 text-sm font-medium">
          <button
            onClick={() => setView('editor')}
            className={`py-1 transition-colors whitespace-nowrap shrink-0 cursor-pointer ${
              view === 'editor'
                ? 'text-amber-400 border-b border-amber-400'
                : 'text-slate-400 hover:text-slate-100'
            }`}
          >
            Editor
          </button>
          <button
            onClick={() => setView('docs')}
            className={`py-1 transition-colors whitespace-nowrap shrink-0 cursor-pointer ${
              view === 'docs'
                ? 'text-amber-400 border-b border-amber-400'
                : 'text-slate-400 hover:text-slate-100'
            }`}
          >
            Specifikacija
          </button>
          <button
            onClick={() => {
              setView('tests');
              if (!testResult && !runningTests) handleRunTests();
            }}
            className={`py-1 transition-colors whitespace-nowrap shrink-0 cursor-pointer ${
              view === 'tests'
                ? 'text-amber-400 border-b border-amber-400'
                : 'text-slate-400 hover:text-slate-100'
            }`}
          >
            Testovi
          </button>
        </nav>

        <div className="flex items-center shrink-0">
          <button
            onClick={() => handleAction('pokreni')}
            disabled={!activeFile || runningAction !== null}
            className="inline-flex items-center gap-2 px-4 py-1.5 text-xs font-semibold rounded-md bg-amber-500 text-slate-950 hover:bg-amber-400 disabled:opacity-50 transition-colors whitespace-nowrap shrink-0 cursor-pointer"
          >
            {runningAction === 'pokreni' ? (
              <Loader2 className="w-3.5 h-3.5 animate-spin" />
            ) : (
              <Play className="w-3.5 h-3.5 fill-current" />
            )}
            <span>Pokreni</span>
          </button>
        </div>
      </header>

      {/* Main Content Area */}
      {view === 'docs' ? (
        <DocsViewer
          specification={docs.specification}
          architecture={docs.architecture}
          versionText={versionText}
          onInsertSnippet={handleInsertSnippet}
        />
      ) : view === 'tests' ? (
        <TestSuitePanel
          result={testResult}
          running={runningTests}
          onRunTests={handleRunTests}
        />
      ) : (
        <div className="flex-1 flex overflow-hidden">
          {/* Left File Explorer Sidebar */}
          <aside className="w-60 shrink-0 flex flex-col border-r border-slate-800/80 bg-[#0E131F]">
            <div className="h-10 px-4 flex items-center justify-between border-b border-slate-800/70">
              <span className="text-xs font-semibold text-slate-400">Radni prostor</span>
              <button
                onClick={() => {
                  setCreatingFile((v) => !v);
                  setFileError(null);
                }}
                title="Nova .bos datoteka"
                className="p-1 rounded text-slate-400 hover:text-white hover:bg-slate-800 transition-colors cursor-pointer"
              >
                <Plus className="w-4 h-4" />
              </button>
            </div>

            {creatingFile && (
              <form
                onSubmit={handleCreateFile}
                className="p-2.5 border-b border-slate-800/70 bg-[#121827] space-y-2"
              >
                <input
                  type="text"
                  value={newFileName}
                  onChange={(e) => setNewFileName(e.target.value)}
                  placeholder="novi_program.bos"
                  autoFocus
                  className="w-full px-2.5 py-1.5 text-xs font-mono bg-[#0B0F17] border border-slate-700 rounded text-slate-100 placeholder:text-slate-500 focus:outline-none focus:border-amber-500"
                />
                <div className="flex items-center justify-end gap-2">
                  <button
                    type="button"
                    onClick={() => setCreatingFile(false)}
                    className="px-2.5 py-1 text-xs text-slate-400 hover:text-slate-200 cursor-pointer"
                  >
                    Odustani
                  </button>
                  <button
                    type="submit"
                    className="px-2.5 py-1 text-xs font-medium bg-amber-500 text-slate-950 rounded hover:bg-amber-400 cursor-pointer"
                  >
                    Kreiraj
                  </button>
                </div>
              </form>
            )}

            {fileError && (
              <div className="px-3 py-2 text-xs text-red-300 bg-red-950/40 border-b border-red-900/50">
                {fileError}
              </div>
            )}

            <div className="flex-1 overflow-y-auto py-2">
              {files.map((f) => {
                const isActive = f.name === activeFile;
                const isDirty = !!dirtyFiles[f.name];
                return (
                  <div
                    key={f.name}
                    onClick={() => openFile(f.name)}
                    className={`group flex items-center justify-between px-4 py-2 text-xs cursor-pointer transition-colors ${
                      isActive
                        ? 'bg-slate-800/80 text-white font-medium'
                        : 'text-slate-400 hover:bg-slate-800/40 hover:text-slate-200'
                    }`}
                  >
                    <div className="flex items-center gap-2 min-w-0">
                      <FileCode2
                        className={`w-3.5 h-3.5 shrink-0 ${
                          isActive ? 'text-amber-400' : 'text-slate-500'
                        }`}
                      />
                      <span className="font-mono truncate">
                        {f.name}
                        {isDirty ? ' *' : ''}
                      </span>
                    </div>
                    {files.length > 1 && (
                      <button
                        onClick={(e) => handleDeleteFile(f.name, e)}
                        title="Obriši datoteku"
                        className="opacity-0 group-hover:opacity-100 p-0.5 text-slate-500 hover:text-red-400 transition-opacity cursor-pointer"
                      >
                        <Trash2 className="w-3.5 h-3.5" />
                      </button>
                    )}
                  </div>
                );
              })}
            </div>

            <div className="px-4 py-3 border-t border-slate-800/70 text-[11px] text-slate-500 space-y-1">
              <div>BOS v0.1 · C++17 kompajler</div>
              <div className="font-mono text-slate-500">Ctrl+Enter pokreni · Ctrl+S snimi</div>
            </div>
          </aside>

          {/* Center Editor + Bottom Workbench */}
          <main className="flex-1 flex flex-col min-w-0 overflow-hidden">
            {/* Editor Tabs & Workspace Actions Toolbar */}
            <div className="h-10 shrink-0 flex items-center justify-between border-b border-slate-800/80 bg-[#0E131F] px-2">
              <div className="flex items-center overflow-x-auto min-w-0 h-full">
                {openTabs.map((tab) => {
                  const isActive = tab === activeFile;
                  return (
                    <div
                      key={tab}
                      onClick={() => setActiveFile(tab)}
                      className={`h-full flex items-center gap-2 px-3.5 text-xs font-mono border-r border-slate-800/70 cursor-pointer transition-colors whitespace-nowrap shrink-0 ${
                        isActive
                          ? 'bg-[#0E131F] text-amber-300 border-b-2 border-b-amber-400'
                          : 'bg-[#0B0F17]/60 text-slate-400 hover:text-slate-200'
                      }`}
                    >
                      <span>
                        {tab}
                        {dirtyFiles[tab] ? ' *' : ''}
                      </span>
                      <button
                        onClick={(e) => closeTab(tab, e)}
                        className="text-slate-500 hover:text-slate-200 cursor-pointer"
                        aria-label={`Zatvori ${tab}`}
                      >
                        <X className="w-3 h-3" />
                      </button>
                    </div>
                  );
                })}
              </div>

              <div className="flex items-center gap-1.5 pl-3 shrink-0">
                <button
                  onClick={() => handleAction('provjeri')}
                  disabled={!activeFile || runningAction !== null}
                  className="inline-flex items-center gap-1.5 px-3 py-1 text-xs font-medium rounded text-slate-300 hover:text-white hover:bg-slate-800 disabled:opacity-50 transition-colors whitespace-nowrap cursor-pointer"
                >
                  {runningAction === 'provjeri' ? (
                    <Loader2 className="w-3.5 h-3.5 animate-spin" />
                  ) : (
                    <CheckCheck className="w-3.5 h-3.5 text-sky-400" />
                  )}
                  <span>Provjeri</span>
                </button>
                <button
                  onClick={() => handleAction('prevedi')}
                  disabled={!activeFile || runningAction !== null}
                  className="inline-flex items-center gap-1.5 px-3 py-1 text-xs font-medium rounded text-slate-300 hover:text-white hover:bg-slate-800 disabled:opacity-50 transition-colors whitespace-nowrap cursor-pointer"
                >
                  {runningAction === 'prevedi' ? (
                    <Loader2 className="w-3.5 h-3.5 animate-spin" />
                  ) : (
                    <Hammer className="w-3.5 h-3.5 text-amber-400" />
                  )}
                  <span>Prevedi (.bosb)</span>
                </button>
                <button
                  onClick={handleSaveCurrent}
                  disabled={!activeFile || !dirtyFiles[activeFile]}
                  className="inline-flex items-center gap-1.5 px-3 py-1 text-xs font-medium rounded text-slate-300 hover:text-white hover:bg-slate-800 disabled:opacity-40 transition-colors whitespace-nowrap cursor-pointer"
                >
                  <Save className="w-3.5 h-3.5" />
                  <span>Snimi</span>
                </button>
                <button
                  onClick={() => setShowStdin((s) => !s)}
                  className={`inline-flex items-center gap-1.5 px-3 py-1 text-xs font-medium rounded transition-colors whitespace-nowrap cursor-pointer ${
                    showStdin
                      ? 'bg-slate-800 text-amber-300'
                      : 'text-slate-400 hover:text-white hover:bg-slate-800'
                  }`}
                >
                  <Keyboard className="w-3.5 h-3.5" />
                  <span>Ulaz (stdin)</span>
                </button>
              </div>
            </div>

            {/* Optional Stdin Input Strip for `ucitaj` */}
            {showStdin && (
              <div className="shrink-0 px-4 py-2.5 bg-[#121827] border-b border-slate-800/80 flex items-start gap-3">
                <span className="text-xs text-slate-400 pt-1 whitespace-nowrap">
                  Standardni ulaz za <code className="font-mono text-amber-300">ucitaj</code>:
                </span>
                <textarea
                  value={stdinValue}
                  onChange={(e) => setStdinValue(e.target.value)}
                  rows={2}
                  placeholder="Unesite linije ulaza (svaki ucitaj čita jednu liniju)..."
                  className="flex-1 px-2.5 py-1.5 text-xs font-mono bg-[#0B0F17] border border-slate-700 rounded text-slate-100 placeholder:text-slate-500 focus:outline-none focus:border-amber-500 resize-none"
                />
              </div>
            )}

            {/* Monaco Editor Canvas */}
            <div className="flex-1 min-h-0 bg-[#0E131F]">
              {activeFile ? (
                <Editor
                  height="100%"
                  language="bos"
                  theme="bos-dark"
                  path={activeFile}
                  value={activeCode}
                  onMount={handleEditorMount}
                  onChange={(val) => {
                    const nextVal = val ?? '';
                    setFileContents((prev) => ({ ...prev, [activeFile]: nextVal }));
                    setDirtyFiles((prev) => ({ ...prev, [activeFile]: true }));
                  }}
                  options={{
                    fontFamily: "'JetBrains Mono', monospace",
                    fontSize: 13.5,
                    lineHeight: 22,
                    minimap: { enabled: false },
                    scrollBeyondLastLine: false,
                    automaticLayout: true,
                    tabSize: 4,
                    insertSpaces: true,
                    padding: { top: 14, bottom: 14 },
                    renderLineHighlight: 'line',
                    smoothScrolling: true,
                    cursorBlinking: 'smooth',
                  }}
                />
              ) : (
                <div className="h-full flex items-center justify-center text-sm text-slate-500">
                  Odaberite ili kreirajte .bos datoteku u lijevom panelu.
                </div>
              )}
            </div>

            {/* Bottom Compiler Workbench Panel */}
            <section className="h-72 shrink-0 flex flex-col border-t border-slate-800/90 bg-[#0B0F17]">
              <div className="h-9 shrink-0 flex items-center justify-between px-4 border-b border-slate-800/80 bg-[#0E131F]">
                <div className="flex items-center gap-1">
                  <button
                    onClick={() => setBottomTab('terminal')}
                    className={`inline-flex items-center gap-1.5 px-3 py-1 text-xs font-medium rounded transition-colors cursor-pointer ${
                      bottomTab === 'terminal'
                        ? 'bg-slate-800 text-white'
                        : 'text-slate-400 hover:text-slate-200'
                    }`}
                  >
                    <TerminalIcon className="w-3.5 h-3.5" />
                    <span>Terminal</span>
                  </button>
                  <button
                    onClick={() => setBottomTab('diagnostics')}
                    className={`inline-flex items-center gap-1.5 px-3 py-1 text-xs font-medium rounded transition-colors cursor-pointer ${
                      bottomTab === 'diagnostics'
                        ? 'bg-slate-800 text-white'
                        : 'text-slate-400 hover:text-slate-200'
                    }`}
                  >
                    <AlertTriangle
                      className={`w-3.5 h-3.5 ${diagCount > 0 ? 'text-red-400' : ''}`}
                    />
                    <span>
                      Dijagnostika{diagCount > 0 ? ` (${diagCount})` : ''}
                    </span>
                  </button>
                  <button
                    onClick={() => setBottomTab('ir')}
                    className={`inline-flex items-center gap-1.5 px-3 py-1 text-xs font-medium rounded transition-colors cursor-pointer ${
                      bottomTab === 'ir'
                        ? 'bg-slate-800 text-white'
                        : 'text-slate-400 hover:text-slate-200'
                    }`}
                  >
                    <Binary className="w-3.5 h-3.5" />
                    <span>IR Bajtkod</span>
                  </button>
                  <button
                    onClick={() => setBottomTab('ast')}
                    className={`inline-flex items-center gap-1.5 px-3 py-1 text-xs font-medium rounded transition-colors cursor-pointer ${
                      bottomTab === 'ast'
                        ? 'bg-slate-800 text-white'
                        : 'text-slate-400 hover:text-slate-200'
                    }`}
                  >
                    <GitBranch className="w-3.5 h-3.5" />
                    <span>AST</span>
                  </button>
                  <button
                    onClick={() => setBottomTab('tokens')}
                    className={`inline-flex items-center gap-1.5 px-3 py-1 text-xs font-medium rounded transition-colors cursor-pointer ${
                      bottomTab === 'tokens'
                        ? 'bg-slate-800 text-white'
                        : 'text-slate-400 hover:text-slate-200'
                    }`}
                  >
                    <List className="w-3.5 h-3.5" />
                    <span>
                      Tokeni{execResult ? ` (${execResult.tokens.length})` : ''}
                    </span>
                  </button>
                </div>

                {execResult && (
                  <div className="flex items-center gap-2 text-[11px] font-mono text-slate-400 tabular-nums">
                    <span className={execResult.ok ? 'text-emerald-400' : 'text-red-400'}>
                      {execResult.ok ? 'OK' : 'GREŠKA'}
                    </span>
                    <span aria-hidden="true">·</span>
                    <span>bos {execResult.command}</span>
                    <span aria-hidden="true">·</span>
                    <span>{execResult.elapsedMs.toFixed(2)} ms</span>
                    {execResult.command === 'pokreni' && execResult.stepsExecuted > 0 && (
                      <>
                        <span aria-hidden="true">·</span>
                        <span>{execResult.stepsExecuted} VM instr.</span>
                      </>
                    )}
                  </div>
                )}
              </div>

              <div className="flex-1 overflow-y-auto p-4 font-mono text-xs">
                {!execResult ? (
                  <div className="text-slate-500">
                    Pokrenite <span className="text-slate-300">Pokreni</span>,{' '}
                    <span className="text-slate-300">Prevedi (.bosb)</span> ili{' '}
                    <span className="text-slate-300">Provjeri</span> da vidite stvarni izlaz C++17 BOS kompajlera.
                  </div>
                ) : bottomTab === 'terminal' ? (
                  <div className="space-y-3">
                    {execResult.command === 'provjeri' && execResult.ok && (
                      <div className="text-emerald-400">
                        Provjera uspješna: datoteka &apos;{activeFile}&apos; nema leksičkih, sintaksnih ni semantičkih grešaka.
                      </div>
                    )}

                    {execResult.command === 'prevedi' && execResult.ok && (
                      <div className="space-y-2.5">
                        <pre className="text-emerald-300 whitespace-pre-wrap leading-relaxed">
                          {execResult.compileNote}
                        </pre>
                        {execResult.bytecodeSize && execResult.bytecodeSize > 0 && (
                          <div className="p-3 rounded bg-[#0E131F] border border-slate-800/80 space-y-1.5">
                            <div className="text-slate-400">
                              Artifakt: <span className="text-slate-200">{execResult.artifactName}</span>{' '}
                              · Veličina: <span className="text-amber-300">{execResult.bytecodeSize} bajtova</span>{' '}
                              · Zaglavlje: <span className="text-slate-200">BOSB v1</span>
                            </div>
                            <div className="text-[11px] text-slate-400 break-all">
                              {execResult.bytecodeHexPreview}
                            </div>
                          </div>
                        )}
                      </div>
                    )}

                    {execResult.command === 'pokreni' && (
                      <>
                        {execResult.output ? (
                          <pre className="text-slate-100 whitespace-pre-wrap leading-relaxed">
                            {execResult.output}
                          </pre>
                        ) : execResult.ok ? (
                          <div className="text-slate-500">
                            (Program je završen bez ispisa na standardni izlaz.)
                          </div>
                        ) : null}
                      </>
                    )}

                    {!execResult.ok && execResult.diagnostics.length > 0 && (
                      <div className="space-y-1.5 pt-1">
                        {execResult.diagnostics.map((d, i) => (
                          <div key={i} className="text-red-400 whitespace-pre-wrap">
                            {d.formatted}
                          </div>
                        ))}
                      </div>
                    )}
                  </div>
                ) : bottomTab === 'diagnostics' ? (
                  execResult.diagnostics.length === 0 ? (
                    <div className="text-emerald-400">
                      Nema pronađenih grešaka ni upozorenja.
                    </div>
                  ) : (
                    <div className="space-y-1.5">
                      {execResult.diagnostics.map((d, idx) => (
                        <button
                          key={idx}
                          onClick={() => jumpToDiagnostic(d)}
                          className="w-full text-left px-3 py-2 rounded bg-[#0E131F] border border-slate-800/80 hover:border-red-500/50 transition-colors flex items-start justify-between gap-4 cursor-pointer"
                        >
                          <div className="space-y-0.5">
                            <div className="text-red-400 font-medium">
                              {d.stage} {d.code ? `[${d.code}]` : ''}
                            </div>
                            <div className="text-slate-200 font-sans text-xs">{d.message}</div>
                          </div>
                          <span className="text-slate-400 whitespace-nowrap shrink-0">
                            {d.file}:{d.line}:{d.column}
                          </span>
                        </button>
                      ))}
                    </div>
                  )
                ) : bottomTab === 'ir' ? (
                  execResult.ir ? (
                    <pre className="text-slate-200 whitespace-pre leading-relaxed">
                      {execResult.ir}
                    </pre>
                  ) : (
                    <div className="text-slate-500">
                      Bajtkod IR nije generisan (ispravite greške u kodu).
                    </div>
                  )
                ) : bottomTab === 'ast' ? (
                  execResult.ast ? (
                    <pre className="text-slate-200 whitespace-pre leading-relaxed">
                      {execResult.ast}
                    </pre>
                  ) : (
                    <div className="text-slate-500">AST nije dostupan.</div>
                  )
                ) : (
                  <div className="overflow-x-auto">
                    <table className="w-full text-left border-collapse">
                      <thead>
                        <tr className="border-b border-slate-800 text-slate-400">
                          <th className="py-1.5 pr-4 font-medium w-24">Pozicija</th>
                          <th className="py-1.5 pr-4 font-medium w-40">Tip tokena</th>
                          <th className="py-1.5 font-medium">Leksema</th>
                        </tr>
                      </thead>
                      <tbody>
                        {execResult.tokens.map((t, i) => (
                          <tr
                            key={i}
                            className="border-b border-slate-800/40 text-slate-200 hover:bg-slate-800/30"
                          >
                            <td className="py-1 pr-4 text-slate-400">
                              {t.line}:{t.column}
                            </td>
                            <td className="py-1 pr-4 text-sky-400">{t.type}</td>
                            <td className="py-1 text-amber-300">{t.lexeme}</td>
                          </tr>
                        ))}
                      </tbody>
                    </table>
                  </div>
                )}
              </div>
            </section>
          </main>
        </div>
      )}
    </div>
  );
}
