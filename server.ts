import express from 'express';
import { createServer as createViteServer } from 'vite';
import { spawn } from 'child_process';
import crypto from 'crypto';
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const ROOT_DIR = __dirname;
const BOS_BIN = path.join(ROOT_DIR, 'bin', 'bos');
const BOS_TESTS_BIN = path.join(ROOT_DIR, 'bin', 'bos_tests');
const EXAMPLES_DIR = path.join(ROOT_DIR, 'examples');
const DOCS_DIR = path.join(ROOT_DIR, 'docs');
const WORKSPACE_DIR = path.join(ROOT_DIR, 'workspace');
const SANDBOX_DIR = '/tmp/bos_sandbox';
const RUNS_DIR = '/tmp/bos_runs';

function ensureDirs() {
  fs.mkdirSync(WORKSPACE_DIR, { recursive: true });
  fs.mkdirSync(SANDBOX_DIR, { recursive: true });
  fs.mkdirSync(RUNS_DIR, { recursive: true });

  // Seed workspace with example .bos files if empty
  const existing = fs.readdirSync(WORKSPACE_DIR).filter((f) => f.endsWith('.bos'));
  if (existing.length === 0 && fs.existsSync(EXAMPLES_DIR)) {
    for (const file of fs.readdirSync(EXAMPLES_DIR)) {
      if (file.endsWith('.bos')) {
        fs.copyFileSync(path.join(EXAMPLES_DIR, file), path.join(WORKSPACE_DIR, file));
      }
    }
  }
}

const VALID_FILENAME_RE = /^[a-zA-Z0-9_\-čćžšđČĆŽŠĐ]+\.bos$/;

function resolveWorkspaceFile(rawName: string): string | null {
  if (!rawName || typeof rawName !== 'string') return null;
  const trimmed = rawName.trim();
  if (!VALID_FILENAME_RE.test(trimmed)) return null;
  const resolved = path.resolve(WORKSPACE_DIR, trimmed);
  const baseWithSep = path.resolve(WORKSPACE_DIR) + path.sep;
  if (!resolved.startsWith(baseWithSep)) return null;
  return resolved;
}

interface ExecResult {
  stdout: string;
  stderr: string;
  exitCode: number;
  timedOut: boolean;
}

function runBinary(
  binPath: string,
  args: string[],
  stdinData = '',
  timeoutMs = 5000
): Promise<ExecResult> {
  return new Promise((resolve) => {
    const child = spawn(binPath, args, {
      cwd: ROOT_DIR,
      stdio: ['pipe', 'pipe', 'pipe'],
      shell: false,
    });

    let stdout = '';
    let stderr = '';
    let timedOut = false;

    const timer = setTimeout(() => {
      timedOut = true;
      child.kill('SIGKILL');
    }, timeoutMs);

    child.stdout.on('data', (chunk: Buffer) => {
      if (stdout.length < 2 * 1024 * 1024) stdout += chunk.toString('utf8');
    });
    child.stderr.on('data', (chunk: Buffer) => {
      if (stderr.length < 512 * 1024) stderr += chunk.toString('utf8');
    });

    child.on('error', (err) => {
      clearTimeout(timer);
      resolve({ stdout, stderr: err.message, exitCode: 1, timedOut: false });
    });

    child.on('close', (code) => {
      clearTimeout(timer);
      resolve({ stdout, stderr, exitCode: code ?? 1, timedOut });
    });

    if (stdinData) {
      child.stdin.write(stdinData, 'utf8');
    }
    child.stdin.end();
  });
}

async function startServer() {
  ensureDirs();

  const app = express();
  app.use(express.json({ limit: '256kb' }));

  // Compiler version info
  app.get('/api/version', async (_req, res) => {
    if (!fs.existsSync(BOS_BIN)) {
      return res.status(503).json({
        ok: false,
        error: 'BOS kompajler binarni fajl (bin/bos) još nije izgrađen.',
      });
    }
    const r = await runBinary(BOS_BIN, ['verzija']);
    res.json({ ok: r.exitCode === 0, versionText: r.stdout.trim() });
  });

  // List workspace .bos files
  app.get('/api/files', (_req, res) => {
    try {
      ensureDirs();
      const names = fs
        .readdirSync(WORKSPACE_DIR)
        .filter((f) => f.endsWith('.bos'))
        .sort();
      const files = names.map((name) => {
        const full = path.join(WORKSPACE_DIR, name);
        const stat = fs.statSync(full);
        return {
          name,
          size: stat.size,
          updatedAt: stat.mtime.toISOString(),
          content: fs.readFileSync(full, 'utf8'),
        };
      });
      res.json({ ok: true, files });
    } catch (err) {
      res.status(500).json({ ok: false, error: 'Greška pri čitanju radnog prostora.' });
    }
  });

  // Save or create a workspace .bos file
  app.put('/api/files/:name', (req, res) => {
    const filePath = resolveWorkspaceFile(req.params.name);
    if (!filePath) {
      return res.status(400).json({
        ok: false,
        error: 'Neispravan naziv datoteke. Koristite slova, brojeve, "_" ili "-" sa nastavkom .bos.',
      });
    }
    const content = typeof req.body?.content === 'string' ? req.body.content : '';
    try {
      fs.writeFileSync(filePath, content, 'utf8');
      res.json({ ok: true, name: path.basename(filePath) });
    } catch {
      res.status(500).json({ ok: false, error: 'Greška pri snimanju datoteke.' });
    }
  });

  // Delete a workspace .bos file
  app.delete('/api/files/:name', (req, res) => {
    const filePath = resolveWorkspaceFile(req.params.name);
    if (!filePath) {
      return res.status(400).json({ ok: false, error: 'Neispravan naziv datoteke.' });
    }
    if (!fs.existsSync(filePath)) {
      return res.status(404).json({ ok: false, error: 'Datoteka ne postoji.' });
    }
    try {
      fs.unlinkSync(filePath);
      res.json({ ok: true });
    } catch {
      res.status(500).json({ ok: false, error: 'Greška pri brisanju datoteke.' });
    }
  });

  // Execute, Compile (.bosb), or Check a BOS program via the real C++17 compiler
  app.post('/api/bos/execute', async (req, res) => {
    if (!fs.existsSync(BOS_BIN)) {
      return res.status(503).json({
        ok: false,
        error: 'BOS kompajler (bin/bos) nije dostupan.',
      });
    }

    const action = req.body?.action;
    if (action !== 'pokreni' && action !== 'prevedi' && action !== 'provjeri') {
      return res.status(400).json({ ok: false, error: 'Nepoznata akcija kompajlera.' });
    }

    const code = typeof req.body?.code === 'string' ? req.body.code : '';
    const stdinData = typeof req.body?.stdin === 'string' ? req.body.stdin : '';
    const rawName = typeof req.body?.filename === 'string' ? req.body.filename : 'program.bos';
    const safeDisplayName = VALID_FILENAME_RE.test(rawName.trim()) ? rawName.trim() : 'program.bos';

    const runId = crypto.randomBytes(8).toString('hex');
    const runDir = path.join(RUNS_DIR, runId);
    fs.mkdirSync(runDir, { recursive: true });
    const srcPath = path.join(runDir, safeDisplayName);
    const outPath = path.join(runDir, safeDisplayName.replace(/\.bos$/, '.bosb'));

    try {
      fs.writeFileSync(srcPath, code, 'utf8');
      const args = [action, srcPath, '--json', '--sandbox-dir', SANDBOX_DIR];
      if (action === 'prevedi') {
        args.push('-o', outPath);
      }

      const execRes = await runBinary(BOS_BIN, args, stdinData, 6000);
      if (execRes.timedOut) {
        return res.json({
          ok: false,
          command: action,
          backend: 'BOS Bytecode VM v0.1 (.bosb)',
          elapsedMs: 6000,
          stepsExecuted: 0,
          output: '',
          compileNote: '',
          ast: '',
          ir: '',
          tokens: [],
          diagnostics: [
            {
              level: 'greška',
              stage: 'Greška izvršavanja',
              code: 'R998',
              file: safeDisplayName,
              line: 1,
              column: 1,
              message: 'Izvršavanje programa je prekinuto zbog vremenskog ograničenja (6s).',
              formatted: `${safeDisplayName}:1:1: Greška izvršavanja [R998]: Izvršavanje programa je prekinuto zbog vremenskog ograničenja (6s).`,
            },
          ],
        });
      }

      let parsed: Record<string, unknown>;
      try {
        parsed = JSON.parse(execRes.stdout);
      } catch {
        return res.status(500).json({
          ok: false,
          error: 'Neočekivan izlaz kompajlera.',
          rawStderr: execRes.stderr,
        });
      }

      // Normalize temp path in diagnostics to the user's logical filename
      if (Array.isArray(parsed.diagnostics)) {
        parsed.diagnostics = parsed.diagnostics.map((d: Record<string, unknown>) => ({
          ...d,
          file: safeDisplayName,
          formatted:
            typeof d.formatted === 'string'
              ? d.formatted.split(srcPath).join(safeDisplayName)
              : d.formatted,
        }));
      }
      if (typeof parsed.compileNote === 'string') {
        parsed.compileNote = parsed.compileNote
          .split(outPath)
          .join(safeDisplayName.replace(/\.bos$/, '.bosb'));
      }

      // Attach compiled .bosb bytecode binary size & hex header preview
      let bytecodeSize = 0;
      let bytecodeHexPreview = '';
      if (action === 'prevedi' && fs.existsSync(outPath)) {
        const buf = fs.readFileSync(outPath);
        bytecodeSize = buf.length;
        bytecodeHexPreview = Array.from(buf.subarray(0, 64))
          .map((b) => b.toString(16).padStart(2, '0').toUpperCase())
          .join(' ');
      }

      res.json({
        ...parsed,
        bytecodeSize,
        bytecodeHexPreview,
        artifactName:
          action === 'prevedi' && parsed.ok ? safeDisplayName.replace(/\.bos$/, '.bosb') : '',
      });
    } finally {
      fs.rmSync(runDir, { recursive: true, force: true });
    }
  });

  // Run real C++ automated test suite
  app.post('/api/bos/tests', async (_req, res) => {
    if (!fs.existsSync(BOS_TESTS_BIN)) {
      return res.status(503).json({ ok: false, error: 'Testni binarni fajl nije pronađen.' });
    }
    const r = await runBinary(BOS_TESTS_BIN, ['--json'], '', 10000);
    try {
      const data = JSON.parse(r.stdout);
      res.json({ ok: r.exitCode === 0, ...data });
    } catch {
      res.status(500).json({ ok: false, error: 'Greška pri pokretanju testova.', stderr: r.stderr });
    }
  });

  // Documentation endpoint
  app.get('/api/docs', (_req, res) => {
    try {
      const specPath = path.join(DOCS_DIR, 'SPECIFICATION.md');
      const archPath = path.join(DOCS_DIR, 'ARCHITECTURE.md');
      res.json({
        ok: true,
        specification: fs.existsSync(specPath) ? fs.readFileSync(specPath, 'utf8') : '',
        architecture: fs.existsSync(archPath) ? fs.readFileSync(archPath, 'utf8') : '',
      });
    } catch {
      res.status(500).json({ ok: false, error: 'Greška pri učitavanju dokumentacije.' });
    }
  });

  const isProd = process.env.NODE_ENV === 'production';
  if (!isProd) {
    const vite = await createViteServer({
      server: { middlewareMode: true },
      appType: 'spa',
    });
    app.use(vite.middlewares);
  } else {
    const distDir = path.join(ROOT_DIR, 'dist');
    app.use(express.static(distDir));
    app.get('*', (_req, res) => {
      res.sendFile(path.join(distDir, 'index.html'));
    });
  }

  const PORT = Number(process.env.PORT) || 3000;
  app.listen(PORT, '0.0.0.0', () => {
    console.log(`BOS Studio server running on http://0.0.0.0:${PORT}`);
  });
}

startServer();
