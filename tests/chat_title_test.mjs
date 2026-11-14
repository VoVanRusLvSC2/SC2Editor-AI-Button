// Check display titles derived from provider names and user messages.
import test from 'node:test';
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { createInterface } from 'node:readline';
import path from 'node:path';
import { chatTitle, compactTitle, firstUserTitle, isContextTitle } from '../bridge/chat_titles.mjs';
const context =
  '[Контекст SC2 Editor]\nDocument in this SC2 Editor window: private map\n\n[Сообщение пользователя]\nExplain my triggers';
test('titles hide internal map context and use the actual user message', () => {
  assert.equal(compactTitle(context), 'Explain my triggers');
  assert.equal(chatTitle('[Контекст SC2 Editor] Document ...', '', 'Editor chat'), 'Editor chat');
  assert(isContextTitle('  [Контекст SC2 Editor] Document ...'));
  assert.equal(chatTitle('Existing title', 'other', 'Untitled'), 'Existing title');
  assert.equal(
    firstUserTitle([{ items: [{ type: 'userMessage', content: [{ text: context }] }] }]),
    'Explain my triggers',
  );
});
test('bridge resolves old context titles from the first user turn without editing provider metadata', async () => {
  const root = process.cwd(),
    child = spawn(process.execPath, [path.join(root, 'bridge/main.mjs'), root, 'codex', 'enUS'], {
      stdio: ['pipe', 'pipe', 'pipe'],
      windowsHide: true,
      env: {
        ...process.env,
        SC2_AI_CODEX_PATH: process.execPath,
        SC2_AI_CODEX_EXTRA_ARGS: JSON.stringify([path.join(root, 'tests/fake_codex.mjs')]),
        SC2_TEST_CONTEXT_TITLE: '1',
      },
    });
  await new Promise((resolve, reject) => {
    const timer = setTimeout(() => finish(Error('Timed out waiting for title')), 10000);
    let initial = false,
      done = false;
    function finish(error) {
      if (done) return;
      done = true;
      clearTimeout(timer);
      child.stdin.end();
      if (error) {
        child.kill();
        reject(error);
      } else resolve();
    }
    child.on('error', finish);
    child.on('exit', (code) => {
      if (!done) finish(Error(`bridge exited ${code}`));
    });
    createInterface({ input: child.stdout }).on('line', (line) => {
      const [kind, ...raw] = line.split('\t'),
        fields = raw.map((v) => Buffer.from(v || '', 'base64').toString('utf8'));
      if (kind === 'thread' && fields[0] === 'test-1') {
        initial = true;
        if (fields[1].includes('Document')) finish(Error('Leaked internal context'));
      }
      if (kind === 'thread_title' && fields[0] === 'test-1') {
        if (!initial || fields[1] !== 'вопрос') finish(Error('Wrong recovered title'));
        else finish();
      }
    });
  });
});
