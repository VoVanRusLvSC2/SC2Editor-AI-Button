// Test recorded patches, path validation, and safe per-file undo.
import assert from 'node:assert/strict';
import { mkdtempSync, writeFileSync, readFileSync, rmSync, mkdirSync } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { recordTurnDiff, listChanges, changeDiff, revertChange } from '../bridge/file_changes.mjs';
import { snapshotWorkspace, observedDiff, textPatch } from '../bridge/workspace_snapshot.mjs';
const root = mkdtempSync(path.join(os.tmpdir(), 'sc2-ai-changes-test-'));
try {
  const work = path.join(root, 'work'),
    state = path.join(root, 'state');
  mkdirSync(work);
  const file = path.join(work, 'example.txt'),
    old = 'user content\nold\n',
    updated = 'user content\nnew\n';
  writeFileSync(file, updated);
  const patch =
    'diff --git a/example.txt b/example.txt\n--- a/example.txt\n+++ b/example.txt\n@@ -1,2 +1,2 @@\n user content\n-old\n+new\n';
  const items = recordTurnDiff(state, 'thread-a', 'turn-a', work, patch);
  assert.equal(items.length, 1);
  const id = items[0].id;
  assert.equal(listChanges(state, 'thread-a')[0].path, 'example.txt');
  assert.equal(changeDiff(state, 'thread-a', id), patch);
  assert.equal(listChanges(state, 'thread-b').length, 0);
  writeFileSync(file, updated + 'later work\n');
  assert.throws(() => revertChange(state, 'thread-a', id), /changed/);
  assert.equal(readFileSync(file, 'utf8'), updated + 'later work\n');
  writeFileSync(file, updated);
  revertChange(state, 'thread-a', id);
  assert.equal(readFileSync(file, 'utf8'), old);
  assert(listChanges(state, 'thread-a')[0].reverted);
  assert.throws(() => revertChange(state, 'thread-a', id), /already/);
  const created = path.join(work, 'created.txt');
  writeFileSync(created, 'hello\n');
  const add =
    'diff --git a/created.txt b/created.txt\nnew file mode 100644\n--- /dev/null\n+++ b/created.txt\n@@ -0,0 +1 @@\n+hello\n';
  const added = recordTurnDiff(state, 'thread-a', 'turn-b', work, add);
  assert.equal(added.length, 1);
  revertChange(state, 'thread-a', added[0].id);
  assert.throws(() => readFileSync(created));
  const traversal =
    'diff --git a/../outside.txt b/../outside.txt\n--- a/../outside.txt\n+++ b/../outside.txt\n@@ -1 +1 @@\n-old\n+new\n';
  assert.equal(recordTurnDiff(state, 'thread-a', 'turn-c', work, traversal).length, 0);
  const binary = 'diff --git a/bin b/bin\nGIT binary patch\nliteral 4\n';
  assert.equal(recordTurnDiff(state, 'thread-a', 'turn-d', work, binary).length, 0);
  const before = await snapshotWorkspace(work);
  const unicode = path.join(work, 'тест.txt');
  writeFileSync(unicode, 'one\r\ntwo\r\n');
  writeFileSync(file, 'shell change\n');
  const after = await snapshotWorkspace(work),
    observed = observedDiff(work, before, after);
  const detected = recordTurnDiff(state, 'thread-a', 'turn-shell', work, observed);
  assert.equal(detected.length, 2);
  const existing = detected.find((x) => x.relative === 'example.txt'),
    newFile = detected.find((x) => x.relative === 'тест.txt');
  assert(existing && newFile);
  revertChange(state, 'thread-a', existing.id);
  assert.equal(readFileSync(file, 'utf8'), old);
  revertChange(state, 'thread-a', newFile.id);
  assert.throws(() => readFileSync(unicode));
  const cr = path.join(work, 'crlf.txt');
  writeFileSync(cr, 'first\r\nafter\r\n');
  const crPatch = textPatch(
    'crlf.txt',
    Buffer.from('first\r\nbefore\r\n'),
    Buffer.from('first\r\nafter\r\n'),
  );
  const crItem = recordTurnDiff(state, 'thread-a', 'turn-crlf', work, crPatch);
  assert.equal(crItem.length, 1);
  revertChange(state, 'thread-a', crItem[0].id);
  assert.equal(readFileSync(cr, 'utf8'), 'first\r\nbefore\r\n');
  const deleted = path.join(work, 'deleted.txt');
  const deletePatch = textPatch('deleted.txt', Buffer.from('no ending newline'), null);
  const deletion = recordTurnDiff(state, 'thread-a', 'turn-delete', work, deletePatch);
  assert.equal(deletion.length, 1);
  revertChange(state, 'thread-a', deletion[0].id);
  assert.equal(readFileSync(deleted, 'utf8'), 'no ending newline');
  const partial = { files: new Map(), limited: true };
  assert.equal(observedDiff(work, partial, after), '');
  console.log(
    'PASS: per-turn changes, bounded snapshots, Unicode, CRLF, isolated chats, safe undo and unsafe patches',
  );
} finally {
  rmSync(root, { recursive: true, force: true });
}
