// Exercise file-change reporting and undo through the bridge protocol.
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { createInterface } from 'node:readline';
import { mkdtempSync, mkdirSync, writeFileSync, readFileSync, rmSync } from 'node:fs';
import path from 'node:path';
import os from 'node:os';
const claude = process.argv.includes('--claude'),
  thread = claude ? 'claude-change' : 'test-1';
const root = process.cwd(),
  dir = mkdtempSync(path.join(os.tmpdir(), 'sc2-ai-bridge-change-')),
  work = path.join(dir, 'work'),
  state = path.join(dir, 'state');
mkdirSync(work);
const map = path.join(work, 'Test.SC2Map');
writeFileSync(map, '');
writeFileSync(path.join(work, 'changes.txt'), 'before\n');
const child = spawn(
  process.execPath,
  [path.join(root, 'bridge/main.mjs'), work, claude ? 'claude' : 'codex', 'enUS'],
  {
    windowsHide: true,
    stdio: ['pipe', 'pipe', 'inherit'],
    env: {
      ...process.env,
      SC2_AI_STATE_DIR: state,
      CODEX_HOME: dir,
      ANTHROPIC_API_KEY: 'test-key',
      SC2_AI_CLAUDE_MODULE: path.join(root, 'tests/fake_claude_changes.mjs'),
      SC2_TEST_CHANGE_DIR: work,
      ...(process.argv.includes('--no-diff') ? { SC2_TEST_NO_DIFF: '1' } : {}),
      SC2_AI_CODEX_PATH: process.execPath,
      SC2_AI_CODEX_EXTRA_ARGS: JSON.stringify([path.join(root, 'tests/fake_codex.mjs')]),
    },
  },
);
const seen = [],
  waiters = [];
createInterface({ input: child.stdout }).on('line', (line) => {
  const [kind, ...fields] = line.split('\t');
  const e = { kind, fields: fields.map((x) => Buffer.from(x, 'base64').toString('utf8')) };
  seen.push(e);
  for (const waiter of [...waiters])
    if (waiter.pred(e)) {
      waiters.splice(waiters.indexOf(waiter), 1);
      waiter.resolve(e);
    }
});
const wait = (pred, from = 0) => {
  const found = seen.slice(from).find(pred);
  if (found) return Promise.resolve(found);
  return new Promise((resolve, reject) => {
    const item = { pred, resolve };
    waiters.push(item);
    setTimeout(() => {
      const at = waiters.indexOf(item);
      if (at >= 0) {
        waiters.splice(at, 1);
        reject(Error('Timeout: ' + JSON.stringify(seen.at(-1))));
      }
    }, 15000).unref();
  });
};
const send = (cmd, ...args) =>
  child.stdin.write([cmd, ...args.map((v) => Buffer.from(v).toString('base64'))].join('\t') + '\n');
try {
  await wait((x) => x.kind === 'ready' && x.fields[0] === '1');
  send('map', map, '0', 'Test.SC2Map', '123', '456');
  let from = seen.length;
  send('open', thread);
  await wait((x) => x.kind === 'selected' && x.fields[0] === thread, from);
  from = seen.length;
  send('send', 'change a file');
  const change = await wait((x) => x.kind === 'change' && x.fields[1] === 'changes.txt', from);
  await wait((x) => x.kind === 'busy' && x.fields[0] === '0', from);
  const id = change.fields[0];
  assert.equal(readFileSync(path.join(work, 'changes.txt'), 'utf8'), 'after\n');
  from = seen.length;
  send('change_view', id);
  const diff = await wait((x) => x.kind === 'change_diff', from);
  assert(diff.fields[2].includes('-before') && diff.fields[2].includes('+after'));
  writeFileSync(path.join(work, 'changes.txt'), 'after\nlater\n');
  from = seen.length;
  send('change_undo', id);
  const error = await wait((x) => x.kind === 'error', from);
  assert(error.fields[0].includes('changed after this response'));
  assert.equal(readFileSync(path.join(work, 'changes.txt'), 'utf8'), 'after\nlater\n');
  writeFileSync(path.join(work, 'changes.txt'), 'after\n');
  from = seen.length;
  send('change_undo', id);
  await wait((x) => x.kind === 'change' && x.fields[0] === id && x.fields[4] === '1', from);
  assert.equal(readFileSync(path.join(work, 'changes.txt'), 'utf8'), 'before\n');
  from = seen.length;
  send('open', thread);
  await wait((x) => x.kind === 'change' && x.fields[0] === id && x.fields[4] === '1', from);
  console.log(
    'PASS:',
    claude ? 'Claude' : 'Codex',
    'change list, preview, protected undo and restored history',
  );
} finally {
  child.stdin.end();
  await new Promise((r) => child.once('exit', r));
  rmSync(dir, { recursive: true, force: true });
}
