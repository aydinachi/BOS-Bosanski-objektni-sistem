export interface BosFile {
  name: string;
  size: number;
  updatedAt: string;
  content: string;
}

export interface BosToken {
  type: string;
  lexeme: string;
  line: number;
  column: number;
}

export interface BosDiagnostic {
  level: string;
  stage: string;
  code: string;
  file: string;
  line: number;
  column: number;
  message: string;
  formatted: string;
}

export interface BosExecuteResponse {
  ok: boolean;
  command: 'pokreni' | 'prevedi' | 'provjeri';
  backend: string;
  elapsedMs: number;
  stepsExecuted: number;
  output: string;
  compileNote: string;
  artifactName?: string;
  bytecodeSize?: number;
  bytecodeHexPreview?: string;
  ast: string;
  ir: string;
  tokens: BosToken[];
  diagnostics: BosDiagnostic[];
  error?: string;
}

export interface BosTestCase {
  name: string;
  passed: boolean;
  details: string;
}

export interface BosTestSuiteResponse {
  ok: boolean;
  passed: number;
  total: number;
  tests: BosTestCase[];
  error?: string;
}

export interface BosDocsResponse {
  ok: boolean;
  specification: string;
  architecture: string;
}

export async function fetchFiles(): Promise<BosFile[]> {
  const res = await fetch('/api/files');
  const data = await res.json();
  if (!data.ok) throw new Error(data.error || 'Greška pri učitavanju datoteka.');
  return data.files;
}

export async function saveFile(name: string, content: string): Promise<void> {
  const res = await fetch(`/api/files/${encodeURIComponent(name)}`, {
    method: 'PUT',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ content }),
  });
  const data = await res.json();
  if (!data.ok) throw new Error(data.error || 'Greška pri snimanju datoteke.');
}

export async function deleteFile(name: string): Promise<void> {
  const res = await fetch(`/api/files/${encodeURIComponent(name)}`, {
    method: 'DELETE',
  });
  const data = await res.json();
  if (!data.ok) throw new Error(data.error || 'Greška pri brisanju datoteke.');
}

export async function executeBos(
  action: 'pokreni' | 'prevedi' | 'provjeri',
  filename: string,
  code: string,
  stdin: string
): Promise<BosExecuteResponse> {
  const res = await fetch('/api/bos/execute', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ action, filename, code, stdin }),
  });
  return res.json();
}

export async function runTestSuite(): Promise<BosTestSuiteResponse> {
  const res = await fetch('/api/bos/tests', { method: 'POST' });
  return res.json();
}

export async function fetchDocs(): Promise<BosDocsResponse> {
  const res = await fetch('/api/docs');
  return res.json();
}

export async function fetchVersion(): Promise<string> {
  const res = await fetch('/api/version');
  const data = await res.json();
  return data.versionText || 'BOS 0.1.0';
}
