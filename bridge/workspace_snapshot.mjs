import { promises as fs } from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';

// This bounded, asynchronous scan catches common text-file changes made by shell commands.
// It is not a filesystem audit and intentionally excludes map archives and large files.
const extensions = new Set([
  '.txt',
  '.md',
  '.galaxy',
  '.xml',
  '.json',
  '.yml',
  '.yaml',
  '.ini',
  '.cfg',
  '.lua',
  '.py',
  '.js',
  '.mjs',
  '.cjs',
  '.ts',
  '.tsx',
  '.jsx',
  '.css',
  '.html',
  '.htm',
  '.cpp',
  '.cc',
  '.c',
  '.h',
  '.hpp',
  '.cs',
  '.java',
  '.rs',
  '.toml',
  '.sh',
  '.ps1',
  '.bat',
  '.cmd',
]);
const skip = new Set([
  '.git',
  '.svn',
  '.hg',
  'node_modules',
  'build',
  'dist',
  'bin',
  'obj',
  'target',
  'vendor',
  '.venv',
  'venv',
]);
const digest = (b) => createHash('sha256').update(b).digest('hex');
const utf8 = new TextDecoder('utf-8', { fatal: true });
// Capture bounded UTF-8 file contents before or after a turn; report when limits were reached.
export async function snapshotWorkspace(root) {
  const result = new Map();
  let bytes = 0,
    visited = 0,
    limited = false,
    cancelled = false;
  const scan = async () => {
    const dirs = [root],
      start = Date.now();
    while (dirs.length && !cancelled) {
      if (Date.now() - start > 250 || visited >= 1200 || bytes >= 16 * 1024 * 1024) {
        limited = true;
        break;
      }
      const current = dirs.shift();
      let entries;
      try {
        entries = await fs.readdir(current, { withFileTypes: true });
      } catch {
        limited = true;
        continue;
      }
      for (const entry of entries) {
        if (cancelled) break;
        if (++visited > 1200 || Date.now() - start > 250) {
          limited = true;
          break;
        }
        if (entry.isSymbolicLink()) continue;
        const name = entry.name,
          full = path.join(current, name);
        if (entry.isDirectory()) {
          if (!name.startsWith('.') && !skip.has(name.toLowerCase())) dirs.push(full);
          continue;
        }
        if (!entry.isFile() || !extensions.has(path.extname(name).toLowerCase())) continue;
        try {
          const stat = await fs.lstat(full);
          if (!stat.isFile() || stat.size > 512 * 1024 || bytes + stat.size > 16 * 1024 * 1024) continue;
          const data = await fs.readFile(full);
          if (cancelled) break;
          if (data.includes(0)) continue;
          utf8.decode(data);
          bytes += data.length;
          result.set(full, { data, hash: digest(data) });
        } catch {
          limited = true;
        }
      }
    }
    return { files: result, limited };
  };
  let timer;
  const timeout = new Promise((resolve) => {
    timer = setTimeout(() => {
      cancelled = true;
      resolve({ files: result, limited: true });
    }, 300);
  });
  const output = await Promise.race([scan(), timeout]);
  clearTimeout(timer);
  return output;
}
function quotedPath(prefix, relative) {
  let out = '"';
  for (const b of Buffer.from(prefix + relative.replace(/\\/g, '/'), 'utf8')) {
    if (b === 34 || b === 92) out += '\\' + String.fromCharCode(b);
    else if (b >= 32 && b < 127) out += String.fromCharCode(b);
    else out += '\\' + b.toString(8).padStart(3, '0');
  }
  return out + '"';
}
function lines(buffer) {
  if (buffer === null) return [];
  const text = utf8.decode(buffer);
  if (!text) return [];
  const values = text.split('\n'),
    ends = text.endsWith('\n');
  if (ends) values.pop();
  return values.map((text, i) => ({ text, ends: ends || i < values.length - 1 }));
}
const same = (a, b) => a.text === b.text && a.ends === b.ends;
function formatLine(prefix, line) {
  return prefix + line.text + '\n' + (line.ends ? '' : '\\ No newline at end of file\n');
}
// Produce a Git-compatible text patch for a changed, added, or deleted text file.
export function textPatch(relative, before, after) {
  if (before === null && after === null) return '';
  const old = lines(before),
    next = lines(after);
  let prefix = 0,
    suffix = 0;
  while (prefix < old.length && prefix < next.length && same(old[prefix], next[prefix])) ++prefix;
  while (
    suffix < old.length - prefix &&
    suffix < next.length - prefix &&
    same(old[old.length - 1 - suffix], next[next.length - 1 - suffix])
  )
    ++suffix;
  if (prefix === old.length && prefix === next.length) return '';
  const contextBefore = Math.min(prefix, 3),
    contextAfter = Math.min(suffix, 3),
    start = prefix - contextBefore;
  const oldEnd = old.length - suffix + contextAfter,
    newEnd = next.length - suffix + contextAfter;
  const oldCount = oldEnd - start,
    newCount = newEnd - start,
    oldStart = oldCount ? start + 1 : start,
    newStart = newCount ? start + 1 : start;
  const a = quotedPath('a/', relative),
    b = quotedPath('b/', relative);
  let patch =
    `diff --git ${a} ${b}\n` +
    (before === null ? 'new file mode 100644\n' : after === null ? 'deleted file mode 100644\n' : '');
  patch += `--- ${before === null ? '/dev/null' : a}\n+++ ${after === null ? '/dev/null' : b}\n@@ -${oldStart},${oldCount} +${newStart},${newCount} @@\n`;
  for (let i = start; i < prefix; ++i) patch += formatLine(' ', old[i]);
  for (let i = prefix; i < old.length - suffix; ++i) patch += formatLine('-', old[i]);
  for (let i = prefix; i < next.length - suffix; ++i) patch += formatLine('+', next[i]);
  for (let i = old.length - suffix; i < oldEnd; ++i) patch += formatLine(' ', old[i]);
  return patch;
}
// Compare snapshots and create patches only for supported files present in the scan.
export function observedDiff(root, before, after) {
  const paths = new Set([...before.files.keys(), ...after.files.keys()]);
  let diff = '',
    files = 0;
  for (const full of paths) {
    const a = before.files.get(full),
      b = after.files.get(full);
    if ((!a && before.limited) || (!b && after.limited) || a?.hash === b?.hash) continue;
    const relative = path.relative(root, full);
    if (!relative || relative.startsWith('..' + path.sep) || path.isAbsolute(relative)) continue;
    const patch = textPatch(relative, a?.data ?? null, b?.data ?? null);
    if (!patch) continue;
    if (Buffer.byteLength(diff) + Buffer.byteLength(patch) > 2 * 1024 * 1024 || ++files > 50) break;
    diff += patch;
  }
  return diff;
}
