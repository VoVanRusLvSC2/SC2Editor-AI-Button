import test from 'node:test';
import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import {createInterface} from 'node:readline';
import {readFileSync} from 'node:fs';
import path from 'node:path';

const root=process.cwd();
const catalog=JSON.parse(readFileSync(path.join(root,'bridge/locales.json'),'utf8'));
const locales=['deDE','esES','esMX','frFR','itIT','koKR','plPL','ptBR','zhCN','zhTW'];
const decode=value=>Buffer.from(value||'','base64').toString('utf8');

test('all language catalogs contain the same complete non-empty set of messages',()=>{
  assert.deepEqual(Object.keys(catalog).sort(),[...locales].sort());
  const reference=Object.keys(catalog.deDE).sort();assert(reference.length>=100);
  for(const locale of locales){
    assert.deepEqual(Object.keys(catalog[locale]).sort(),reference,locale);
    for(const [key,value] of Object.entries(catalog[locale])){
      assert(value.trim(),`${locale}: empty translation for ${key}`);
      assert(!/QZX(?:ITEM|KEEP)/i.test(value),`${locale}: translation placeholder in ${key}`);
    }
    assert(catalog[locale]['You: '].endsWith(' '),`${locale}: user prefix needs a trailing space`);
  }
});

test('catalogs cover the source UI and bridge translation keys',()=>{
  const cpp=readFileSync(path.join(root,'src/ai_button.cpp'),'utf8');
  const bridge=readFileSync(path.join(root,'bridge/main.mjs'),'utf8');
  const cppPattern=/Tr\(ctx,L"(?:\\.|[^"\\])*",L"((?:\\.|[^"\\])*)"\)/g;
  const jsPattern=/tr\('(?:\\.|[^'\\])*','((?:\\.|[^'\\])*)'\)/g;
  const keys=[];
  for(const match of cpp.matchAll(cppPattern))keys.push(JSON.parse('"'+match[1]+'"'));
  for(const match of bridge.matchAll(jsPattern))keys.push(JSON.parse('"'+match[1].replace(/\\'/g,"'").replace(/"/g,'\\"')+'"'));
  assert(keys.length>=100,'expected all UI and bridge translations');
  for(const locale of locales)for(const key of keys)assert(Object.hasOwn(catalog[locale],key),`${locale}: missing ${key}`);
});

test('the bridge uses each locale and preserves English and Russian',async()=>{
  const cases=[...locales,'enUS','ruRU'];
  await Promise.all(cases.map(locale=>new Promise((resolve,reject)=>{
    const child=spawn(process.execPath,[path.join(root,'bridge/main.mjs'),root,'codex',locale],{
      stdio:['pipe','pipe','pipe'],windowsHide:true,
      env:{...process.env,SC2_AI_CODEX_PATH:process.execPath,SC2_AI_CODEX_EXTRA_ARGS:JSON.stringify([path.join(root,'tests/fake_lock_codex.mjs')])}
    });
    let status='',finished=false;const timeout=setTimeout(()=>finish(Error(`${locale}: timed out`)),15000);
    function finish(error){if(finished)return;finished=true;clearTimeout(timeout);child.stdin.end();if(error){child.kill();reject(error);}else resolve();}
    child.on('error',finish);child.on('exit',code=>{if(!finished)finish(Error(`${locale}: bridge exited ${code}`));});
    createInterface({input:child.stdout}).on('line',line=>{
      const [kind,...fields]=line.split('\t');const value=decode(fields[0]);
      if(kind==='status')status=value;
      if(kind==='ready'&&value==='1'){
        const expected=locale==='enUS'?'Codex connected':locale==='ruRU'?'Codex подключён':catalog[locale]['Codex connected'];
        if(status!==expected)finish(Error(`${locale}: expected ${expected}, got ${status}`));else finish();
      }
    });
  })));
});
