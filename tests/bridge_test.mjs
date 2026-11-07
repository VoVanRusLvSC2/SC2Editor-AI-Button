import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import {createInterface} from 'node:readline';
import path from 'node:path';
const root=process.cwd(),exe=process.execPath;
const child=spawn(exe,[path.join(root,'bridge/main.mjs')],{windowsHide:true,stdio:['pipe','pipe','inherit'],env:{...process.env,SC2_AI_CODEX_PATH:exe,SC2_AI_CODEX_EXTRA_ARGS:JSON.stringify([path.join(root,'tests/fake_codex.mjs')]),SC2_AI_CLAUDE_MODULE:path.join(root,'tests/fake_claude.mjs')}});
const seen=[],waiters=[];const decode=x=>Buffer.from(x||'','base64').toString('utf8');
createInterface({input:child.stdout}).on('line',line=>{const [kind,...v]=line.split('\t');const e={kind,fields:v.map(decode)};seen.push(e);for(const w of [...waiters])if(w.pred(e)){waiters.splice(waiters.indexOf(w),1);w.resolve(e)}});
const wait=(pred,from=0)=>{const found=seen.slice(from).find(pred);if(found)return Promise.resolve(found);return new Promise((resolve,reject)=>{const w={pred,resolve};waiters.push(w);setTimeout(()=>{const i=waiters.indexOf(w);if(i>=0){waiters.splice(i,1);reject(Error('Timed out waiting for event'))}},8000).unref()})};
const send=(cmd,...fields)=>child.stdin.write([cmd,...fields.map(x=>Buffer.from(x).toString('base64'))].join('\t')+'\n');
try{
 await wait(e=>e.kind==='ready'&&e.fields[0]==='1');assert(seen.some(e=>e.kind==='thread'&&e.fields[0]==='test-1'));assert(seen.some(e=>e.kind==='model'&&e.fields[0]==='test-model'));assert(seen.some(e=>e.kind==='mcp'&&e.fields[0].includes('подключён')));
 let from=seen.length;send('open','test-1');await wait(e=>e.kind==='selected'&&e.fields[0]==='test-1',from);assert(seen.slice(from).some(e=>e.kind==='history'&&e.fields[0].includes('история')));
 from=seen.length;send('send','привет');await wait(e=>e.kind==='question',from);send('answer','вариант');let a=await wait(e=>e.kind==='approval',from);send('approve',a.fields[0],'no');await wait(e=>e.kind==='busy'&&e.fields[0]==='0',from);assert(seen.slice(from).some(e=>e.kind==='delta'&&e.fields[0]==='ответ'));
 from=seen.length;send('provider','claude');await wait(e=>e.kind==='ready'&&e.fields[0]==='1',from);assert(seen.slice(from).some(e=>e.kind==='thread'&&e.fields[0]==='claude-1'));
 from=seen.length;send('open','claude-1');await wait(e=>e.kind==='selected'&&e.fields[0]==='claude-1',from);assert(seen.slice(from).some(e=>e.kind==='history'&&e.fields[0].includes('прошлый ответ')));
 from=seen.length;send('send','привет Claude');a=await wait(e=>e.kind==='approval',from);send('approve',a.fields[0],'no');await wait(e=>e.kind==='busy'&&e.fields[0]==='0',from);assert(seen.slice(from).some(e=>e.kind==='delta'&&e.fields[0]==='ответ Claude'));
 console.log('PASS: Codex and Claude sessions, history, streamed replies, user questions, MCP status, and explicit approval denial');
}finally{child.stdin.end();}
