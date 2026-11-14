// Check that prompts include the active editor identity and map context.
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

// Exercise the actual side-effect-free prompt builder without launching providers.
const source = readFileSync(new URL('../bridge/main.mjs', import.meta.url), 'utf8');
const start = source.search(/function mapPrompt\s*\(\s*text\s*\)\s*\{/);
const boundary = source.slice(start).search(/\nconst claudeDirs\s*=/);
const end = boundary < 0 ? -1 : start + boundary;
assert.ok(start >= 0 && end > start, 'Prompt builder boundary');
const prompt = (mapContext, text) =>
  vm.runInNewContext(source.slice(start, end) + '\nmapPrompt(text)', { mapContext, text });

test('dirty-map connectivity tests do not route through save/checkpoint import', () => {
  const result = prompt(
    { path: 'K:/Maps/Active.SC2Map', dirty: true, inferred: true, windowId: '3934182', processId: '27008' },
    'test',
  );
  assert.ok(result.includes('"K:/Maps/Active.SC2Map"'));
  assert.ok(result.includes('windowId="3934182", processId="27008"'));
  assert.ok(result.includes('Do not ask the user to save or call live.connect merely to test connectivity'));
  assert.ok(result.includes('live.memory.read/set'));
  assert.ok(result.includes('matching Data window from the same process/document'));
  assert.ok(result.includes('saving will not expose missing tools'));
  assert.ok(result.includes('live.connect is ONLY the separately selected saved-checkpoint workflow'));
  assert.ok(
    result.includes(
      'Never create, export or open another map/checkpoint/Save As copy unless the user explicitly requests a copy',
    ),
  );
  assert.ok(result.includes('live.document.save'));
  assert.ok(result.includes('do not silently substitute checkpoint authoring'));
  assert.ok(result.endsWith('[Сообщение пользователя]\ntest'));
});

test('unknown document identity stays unresolved and the user message is retained', () => {
  const result = prompt(
    {
      path: '',
      caption: 'Terrain - [Untitled]',
      dirty: false,
      inferred: false,
      windowId: '99',
      processId: '42',
    },
    'show status',
  );
  assert.ok(result.includes('The exact open-map path is unresolved'));
  assert.ok(result.includes('"Terrain - [Untitled]"'));
  assert.ok(result.endsWith('[Сообщение пользователя]\nshow status'));
  assert.ok(result.includes('not live Terrain, triggers or object creation'));
});
