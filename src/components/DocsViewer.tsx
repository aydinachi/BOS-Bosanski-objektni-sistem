import React, { useState } from 'react';
import { BookOpen, Cpu, Terminal, Copy, Check } from 'lucide-react';

interface DocsViewerProps {
  specification: string;
  architecture: string;
  versionText: string;
  onInsertSnippet: (filename: string, code: string) => void;
}

interface MarkdownBlock {
  type: 'h1' | 'h2' | 'h3' | 'code' | 'table' | 'list' | 'paragraph' | 'hr';
  content: string;
  lang?: string;
  rows?: string[][];
  items?: string[];
}

function parseMarkdown(md: string): MarkdownBlock[] {
  const lines = md.split('\n');
  const blocks: MarkdownBlock[] = [];
  let i = 0;

  while (i < lines.length) {
    const line = lines[i];
    if (line.trim() === '') {
      i++;
      continue;
    }
    if (line.trim() === '---') {
      blocks.push({ type: 'hr', content: '' });
      i++;
      continue;
    }
    if (line.startsWith('```')) {
      const lang = line.slice(3).trim();
      const codeLines: string[] = [];
      i++;
      while (i < lines.length && !lines[i].startsWith('```')) {
        codeLines.push(lines[i]);
        i++;
      }
      i++; // skip closing ```
      blocks.push({ type: 'code', content: codeLines.join('\n'), lang });
      continue;
    }
    if (line.startsWith('# ')) {
      blocks.push({ type: 'h1', content: line.slice(2).trim() });
      i++;
      continue;
    }
    if (line.startsWith('## ')) {
      blocks.push({ type: 'h2', content: line.slice(3).trim() });
      i++;
      continue;
    }
    if (line.startsWith('### ')) {
      blocks.push({ type: 'h3', content: line.slice(4).trim() });
      i++;
      continue;
    }
    if (line.includes('|') && i + 1 < lines.length && lines[i + 1].trim().startsWith('|')) {
      const rows: string[][] = [];
      while (i < lines.length && lines[i].includes('|')) {
        const raw = lines[i].trim();
        if (!/^\|[\s\-:|]+\|$/.test(raw)) {
          const cells = raw
            .replace(/^\|/, '')
            .replace(/\|$/, '')
            .split('|')
            .map((c) => c.trim());
          rows.push(cells);
        }
        i++;
      }
      blocks.push({ type: 'table', content: '', rows });
      continue;
    }
    if (line.trimStart().startsWith('- ') || /^\d+\.\s/.test(line.trimStart())) {
      const items: string[] = [];
      while (
        i < lines.length &&
        (lines[i].trimStart().startsWith('- ') || /^\d+\.\s/.test(lines[i].trimStart()))
      ) {
        items.push(lines[i].trimStart().replace(/^(-\s|\d+\.\s)/, ''));
        i++;
      }
      blocks.push({ type: 'list', content: '', items });
      continue;
    }

    const paraLines: string[] = [line];
    i++;
    while (
      i < lines.length &&
      lines[i].trim() !== '' &&
      !lines[i].startsWith('#') &&
      !lines[i].startsWith('```') &&
      !lines[i].trimStart().startsWith('- ') &&
      lines[i].trim() !== '---'
    ) {
      paraLines.push(lines[i]);
      i++;
    }
    blocks.push({ type: 'paragraph', content: paraLines.join(' ') });
  }
  return blocks;
}

function renderInline(text: string): React.ReactNode[] {
  const parts = text.split(/(`[^`]+`|\*\*[^*]+\*\*)/g);
  return parts.map((part, idx) => {
    if (part.startsWith('`') && part.endsWith('`')) {
      return (
        <code
          key={idx}
          className="font-mono text-[13px] px-1.5 py-0.5 rounded bg-slate-800/90 text-amber-300"
        >
          {part.slice(1, -1)}
        </code>
      );
    }
    if (part.startsWith('**') && part.endsWith('**')) {
      return (
        <strong key={idx} className="font-semibold text-slate-100">
          {part.slice(2, -2)}
        </strong>
      );
    }
    return <React.Fragment key={idx}>{part}</React.Fragment>;
  });
}

export default function DocsViewer({
  specification,
  architecture,
  versionText,
  onInsertSnippet,
}: DocsViewerProps) {
  const [docTab, setDocTab] = useState<'spec' | 'arch' | 'cli'>('spec');
  const [copiedIdx, setCopiedIdx] = useState<number | null>(null);

  const activeText = docTab === 'spec' ? specification : architecture;
  const blocks = parseMarkdown(activeText);

  const copyCode = (text: string, idx: number) => {
    navigator.clipboard?.writeText(text);
    setCopiedIdx(idx);
    setTimeout(() => setCopiedIdx(null), 1500);
  };

  return (
    <div className="flex-1 flex overflow-hidden bg-[#0B0F17]">
      {/* Left Docs Navigation */}
      <aside className="w-60 shrink-0 border-r border-slate-800/80 bg-[#0E131F] p-4 flex flex-col gap-6">
        <div>
          <p className="text-xs font-medium text-slate-400 mb-3">Dokumentacija</p>
          <div className="flex flex-col gap-1">
            <button
              onClick={() => setDocTab('spec')}
              className={`flex items-center gap-2.5 px-3 py-2 text-sm rounded-md text-left transition-colors whitespace-nowrap ${
                docTab === 'spec'
                  ? 'bg-amber-500/15 text-amber-300 font-medium'
                  : 'text-slate-300 hover:bg-slate-800/60 hover:text-white'
              }`}
            >
              <BookOpen className="w-4 h-4 shrink-0" />
              <span>Specifikacija v0.1</span>
            </button>
            <button
              onClick={() => setDocTab('arch')}
              className={`flex items-center gap-2.5 px-3 py-2 text-sm rounded-md text-left transition-colors whitespace-nowrap ${
                docTab === 'arch'
                  ? 'bg-amber-500/15 text-amber-300 font-medium'
                  : 'text-slate-300 hover:bg-slate-800/60 hover:text-white'
              }`}
            >
              <Cpu className="w-4 h-4 shrink-0" />
              <span>Arhitektura kompajlera</span>
            </button>
            <button
              onClick={() => setDocTab('cli')}
              className={`flex items-center gap-2.5 px-3 py-2 text-sm rounded-md text-left transition-colors whitespace-nowrap ${
                docTab === 'cli'
                  ? 'bg-amber-500/15 text-amber-300 font-medium'
                  : 'text-slate-300 hover:bg-slate-800/60 hover:text-white'
              }`}
            >
              <Terminal className="w-4 h-4 shrink-0" />
              <span>CLI i gradnja</span>
            </button>
          </div>
        </div>

        <div className="border-t border-slate-800/80 pt-4">
          <p className="text-xs font-medium text-slate-400 mb-2">Verzija kompajlera</p>
          <pre className="text-[11px] font-mono text-slate-400 whitespace-pre-wrap leading-relaxed">
            {versionText}
          </pre>
        </div>
      </aside>

      {/* Main Content */}
      <div className="flex-1 overflow-y-auto px-10 py-8">
        <div className="max-w-3xl">
          {docTab === 'cli' ? (
            <div className="space-y-6">
              <h1 className="text-2xl font-bold text-white tracking-tight">
                BOS Komandna linija (CLI) i gradnja iz izvora
              </h1>
              <p className="text-sm text-slate-300 leading-relaxed">
                BOS kompajler je napisan u čistom C++17 bez vanjskih biblioteka i gradi se pomoću{' '}
                <code className="font-mono text-amber-300 bg-slate-800/80 px-1.5 py-0.5 rounded">g++</code> i{' '}
                <code className="font-mono text-amber-300 bg-slate-800/80 px-1.5 py-0.5 rounded">make</code>.
              </p>

              <div className="space-y-3">
                <h2 className="text-base font-semibold text-white">Gradnja i pokretanje testova</h2>
                <pre className="p-4 rounded-lg bg-[#0E131F] border border-slate-800/90 font-mono text-xs text-slate-200 overflow-x-auto leading-relaxed">
{`make              # Gradi bin/bos kompajler
make test         # Gradi i pokreće svih 21 C++17 automatskih testova
make clean        # Briše build/ i bin/ direktorije`}
                </pre>
              </div>

              <div className="space-y-3">
                <h2 className="text-base font-semibold text-white">Zvanične CLI naredbe</h2>
                <pre className="p-4 rounded-lg bg-[#0E131F] border border-slate-800/90 font-mono text-xs text-slate-200 overflow-x-auto leading-relaxed">
{`./bin/bos pokreni examples/dobrodosli.bos       # Pokreće izvor na BOS bajtkod VM-u
./bin/bos prevedi examples/dobrodosli.bos       # Prevodi u binarni BOS bajtkod (.bosb)
./bin/bos pokreni examples/dobrodosli.bosb      # Izvršava već prevedeni .bosb fajl
./bin/bos provjeri examples/dobrodosli.bos      # Samo leksička, sintaksna i semantička provjera
./bin/bos verzija                               # Informacije o verziji i backendu
./bin/bos pomoc                                 # Prikazuje pomoć na bosanskom jeziku`}
                </pre>
              </div>

              <div className="space-y-3">
                <h2 className="text-base font-semibold text-white">Opcije za inspekciju kompajlera</h2>
                <pre className="p-4 rounded-lg bg-[#0E131F] border border-slate-800/90 font-mono text-xs text-slate-200 overflow-x-auto leading-relaxed">
{`./bin/bos provjeri examples/dobrodosli.bos --tokens --ast --ir
./bin/bos pokreni examples/dobrodosli.bos --json`}
                </pre>
              </div>
            </div>
          ) : (
            <div className="space-y-5">
              {blocks.map((block, idx) => {
                if (block.type === 'h1') {
                  return (
                    <h1 key={idx} className="text-2xl font-bold text-white tracking-tight">
                      {block.content}
                    </h1>
                  );
                }
                if (block.type === 'h2') {
                  return (
                    <h2
                      key={idx}
                      className="text-lg font-semibold text-slate-100 pt-4 border-t border-slate-800/70"
                    >
                      {block.content}
                    </h2>
                  );
                }
                if (block.type === 'h3') {
                  return (
                    <h3 key={idx} className="text-sm font-semibold text-amber-300 pt-2">
                      {block.content}
                    </h3>
                  );
                }
                if (block.type === 'hr') {
                  return <hr key={idx} className="border-slate-800/70 my-4" />;
                }
                if (block.type === 'paragraph') {
                  return (
                    <p key={idx} className="text-sm text-slate-300 leading-relaxed">
                      {renderInline(block.content)}
                    </p>
                  );
                }
                if (block.type === 'list' && block.items) {
                  return (
                    <ul key={idx} className="list-disc pl-5 space-y-1.5 text-sm text-slate-300">
                      {block.items.map((item, ii) => (
                        <li key={ii} className="leading-relaxed">
                          {renderInline(item)}
                        </li>
                      ))}
                    </ul>
                  );
                }
                if (block.type === 'table' && block.rows && block.rows.length > 0) {
                  const [header, ...body] = block.rows;
                  return (
                    <div
                      key={idx}
                      className="overflow-x-auto rounded-lg border border-slate-800/90 bg-[#0E131F]"
                    >
                      <table className="w-full text-left border-collapse text-xs">
                        <thead>
                          <tr className="border-b border-slate-800 text-slate-400">
                            {header.map((cell, ci) => (
                              <th key={ci} className="px-4 py-2.5 font-medium">
                                {renderInline(cell)}
                              </th>
                            ))}
                          </tr>
                        </thead>
                        <tbody>
                          {body.map((row, ri) => (
                            <tr
                              key={ri}
                              className="border-b border-slate-800/50 last:border-b-0 text-slate-200"
                            >
                              {row.map((cell, ci) => (
                                <td key={ci} className="px-4 py-2.5 align-top leading-relaxed">
                                  {renderInline(cell)}
                                </td>
                              ))}
                            </tr>
                          ))}
                        </tbody>
                      </table>
                    </div>
                  );
                }
                if (block.type === 'code') {
                  const isBos = block.lang === 'bos';
                  return (
                    <div
                      key={idx}
                      className="rounded-lg border border-slate-800/90 bg-[#0E131F] overflow-hidden"
                    >
                      <div className="flex items-center justify-between px-3.5 py-2 border-b border-slate-800/80 text-xs text-slate-400">
                        <span className="font-mono">{block.lang || 'kod'}</span>
                        <div className="flex items-center gap-3">
                          {isBos && (
                            <button
                              onClick={() =>
                                onInsertSnippet(`primjer_${idx}.bos`, block.content + '\n')
                              }
                              className="text-amber-400 hover:text-amber-300 font-medium transition-colors"
                            >
                              Otvori u editoru
                            </button>
                          )}
                          <button
                            onClick={() => copyCode(block.content, idx)}
                            className="inline-flex items-center gap-1 text-slate-400 hover:text-slate-200 transition-colors"
                          >
                            {copiedIdx === idx ? (
                              <>
                                <Check className="w-3.5 h-3.5 text-emerald-400" />
                                <span>Kopirano</span>
                              </>
                            ) : (
                              <>
                                <Copy className="w-3.5 h-3.5" />
                                <span>Kopiraj</span>
                              </>
                            )}
                          </button>
                        </div>
                      </div>
                      <pre className="p-4 font-mono text-xs text-slate-200 overflow-x-auto leading-relaxed">
                        {block.content}
                      </pre>
                    </div>
                  );
                }
                return null;
              })}
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
