import React from 'react';
import { CheckCircle2, XCircle, Play, Loader2 } from 'lucide-react';
import type { BosTestSuiteResponse } from '../lib/api';

interface TestSuitePanelProps {
  result: BosTestSuiteResponse | null;
  running: boolean;
  onRunTests: () => void;
}

export default function TestSuitePanel({ result, running, onRunTests }: TestSuitePanelProps) {
  return (
    <div className="flex-1 overflow-y-auto bg-[#0B0F17] px-10 py-8">
      <div className="max-w-4xl mx-auto space-y-6">
        <div className="flex items-start justify-between gap-6 border-b border-slate-800/80 pb-6">
          <div>
            <h1 className="text-xl font-bold text-white tracking-tight">
              Automatski testovi BOS v0.1 kompajlera
            </h1>
            <p className="text-sm text-slate-400 mt-1">
              Pokreće izvorni C++17 testni skup (<code className="font-mono text-slate-300">bin/bos_tests</code>) koji provjerava lekser, parser, semantički analizator, IR generator, standardnu biblioteku, binarni bajtkod (.bosb) i VM.
            </p>
          </div>
          <button
            onClick={onRunTests}
            disabled={running}
            className="inline-flex items-center gap-2 px-4 py-2 text-xs font-semibold rounded-md bg-amber-500 text-slate-950 hover:bg-amber-400 disabled:opacity-60 transition-colors whitespace-nowrap shrink-0 cursor-pointer"
          >
            {running ? <Loader2 className="w-4 h-4 animate-spin" /> : <Play className="w-4 h-4" />}
            <span>{running ? 'Pokretanje testova...' : 'Pokreni sve testove'}</span>
          </button>
        </div>

        {!result && !running && (
          <div className="rounded-lg border border-slate-800/90 bg-[#0E131F] p-10 text-center space-y-3">
            <p className="text-sm text-slate-300">
              Testovi još nisu pokrenuti u ovoj sesiji.
            </p>
            <p className="text-xs text-slate-500">
              Kliknite &ldquo;Pokreni sve testove&rdquo; da izvršite svih 21 jediničnih i integracijskih testova nad C++17 kompajlerom.
            </p>
          </div>
        )}

        {result && (
          <div className="space-y-4">
            <div className="flex items-center justify-between text-sm px-1">
              <div className="flex items-center gap-3 text-slate-300">
                <span className={result.ok ? 'text-emerald-400 font-semibold' : 'text-red-400 font-semibold'}>
                  {result.ok ? 'Svi testovi prošli' : 'Neki testovi nisu prošli'}
                </span>
                <span className="text-slate-600" aria-hidden="true">·</span>
                <span className="font-mono tabular-nums text-slate-300">
                  {result.passed} / {result.total} prošlo
                </span>
              </div>
              <span className="text-xs font-mono text-slate-500">bin/bos_tests --json</span>
            </div>

            <div className="rounded-lg border border-slate-800/90 bg-[#0E131F] overflow-hidden">
              <table className="w-full text-left border-collapse">
                <thead>
                  <tr className="border-b border-slate-800 text-xs text-slate-400">
                    <th className="px-4 py-2.5 font-medium w-28">Status</th>
                    <th className="px-4 py-2.5 font-medium">Testni slučaj</th>
                  </tr>
                </thead>
                <tbody>
                  {result.tests.map((t, idx) => (
                    <tr
                      key={idx}
                      className="border-b border-slate-800/50 last:border-b-0 hover:bg-slate-800/30 transition-colors"
                    >
                      <td className="px-4 py-2.5 align-top">
                        {t.passed ? (
                          <span className="inline-flex items-center gap-1.5 text-xs font-medium text-emerald-400">
                            <CheckCircle2 className="w-3.5 h-3.5 shrink-0" />
                            <span>PROŠAO</span>
                          </span>
                        ) : (
                          <span className="inline-flex items-center gap-1.5 text-xs font-medium text-red-400">
                            <XCircle className="w-3.5 h-3.5 shrink-0" />
                            <span>PAO</span>
                          </span>
                        )}
                      </td>
                      <td className="px-4 py-2.5">
                        <div className="text-sm text-slate-200">{t.name}</div>
                        {!t.passed && t.details && (
                          <pre className="mt-2 text-xs font-mono text-red-300 whitespace-pre-wrap">
                            {t.details}
                          </pre>
                        )}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
