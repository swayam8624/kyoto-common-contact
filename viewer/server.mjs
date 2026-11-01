import { spawn } from 'node:child_process';
import { createServer } from 'node:http';
import { readFile } from 'node:fs/promises';
import { extname, join, normalize } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('.', import.meta.url));
const executable = process.env.CCP_SERVICE ?? join(root, '../build/ccp_service');
const core = spawn(executable, [], { stdio: ['pipe', 'pipe', 'inherit'] });
let nextId = 1;
let buffer = '';
const pending = new Map();
let snapshot = null;
core.stdout.setEncoding('utf8');
core.stdout.on('data', chunk => {
  buffer += chunk;
  const lines = buffer.split('\n'); buffer = lines.pop() ?? '';
  for (const line of lines) {
    if (!line) continue;
    const item = pending.values().next().value;
    try { const result = JSON.parse(line); snapshot = result.snapshot ?? snapshot; item?.resolve(result); }
    catch (error) { item?.reject(error); }
    if (item) pending.delete(item.id);
  }
});
core.on('exit', code => { for (const item of pending.values()) item.reject(new Error(`CCP service exited (${code})`)); });

function send(command) {
  const id = nextId++;
  return new Promise((resolve, reject) => {
    pending.set(id, { id, resolve, reject });
    core.stdin.write(JSON.stringify({ id, ...command }) + '\n');
  });
}

const mime = { '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.css': 'text/css; charset=utf-8' };
const server = createServer(async (req, res) => {
  try {
    if (req.url === '/api/state') {
      if (!snapshot) await send({ command: 'status' });
      res.writeHead(200, { 'content-type': 'application/json', 'cache-control': 'no-store' });
      res.end(JSON.stringify(snapshot)); return;
    }
    if (req.url?.startsWith('/api/command')) {
      const url = new URL(req.url, 'http://localhost');
      const command = url.searchParams.get('command');
      const scene = url.searchParams.get('scene');
      const mode = url.searchParams.get('mode');
      const payload = command === 'reset' ? { command, scene }
        : command === 'broadPhase' || command === 'queryMode' ? { command, mode }
        : { command };
      const result = await send(payload);
      res.writeHead(result.ok ? 200 : 400, { 'content-type': 'application/json', 'cache-control': 'no-store' });
      res.end(JSON.stringify(result)); return;
    }
    const pathname = req.url === '/' ? '/index.html' : decodeURIComponent(req.url ?? '/');
    const file = normalize(join(root, pathname));
    if (!file.startsWith(root)) { res.writeHead(403); res.end(); return; }
    const body = await readFile(file);
    res.writeHead(200, { 'content-type': mime[extname(file)] ?? 'application/octet-stream' }); res.end(body);
  } catch (error) { res.writeHead(500, { 'content-type': 'text/plain' }); res.end(String(error)); }
});
server.listen(Number(process.env.PORT ?? 4173), '127.0.0.1', () => console.log(`CCP viewer: http://127.0.0.1:${process.env.PORT ?? 4173}`));
process.on('SIGINT', () => { server.close(); core.kill('SIGTERM'); process.exit(0); });
