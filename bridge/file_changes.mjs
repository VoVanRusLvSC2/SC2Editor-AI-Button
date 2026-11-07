import {spawnSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {existsSync,lstatSync,readFileSync,realpathSync,mkdirSync,readdirSync,writeFileSync,renameSync} from 'node:fs';
import path from 'node:path';

// Store only the actual per-turn patches and a hash of the post-edit file. No map data is
// read here unless Codex explicitly reported a textual change to that file.
const hash = data => createHash('sha256').update(data).digest('hex');
const validId = id => typeof id==='string' && /^[a-f0-9]{64}$/.test(id);
const folder = (state,thread) => path.join(state,'file-changes',hash(thread));
const inside = (root,target) => {const rel=path.relative(root,target);return !!rel && rel!=='..' && !rel.startsWith('..'+path.sep) && !path.isAbsolute(rel);};
const runGit = (args,cwd,input) => spawnSync('git',['-c','core.autocrlf=false','-c','core.safecrlf=false',...args],{cwd,input,windowsHide:true,timeout:15000,maxBuffer:4*1024*1024});
export function workspaceRoot(cwd){
  const real=realpathSync(cwd);const result=runGit(['rev-parse','--show-toplevel'],real);
  if(result.status===0){const root=result.stdout.toString('utf8').trim();if(root && existsSync(root))return realpathSync(root);}
  return real;
}
function targetPath(root,relative){
  if(typeof relative!=='string' || !relative || path.isAbsolute(relative) || relative.includes('\0'))throw Error('Invalid file path');
  const pieces=relative.split(/[\\/]+/);if(pieces.some(p=>p==='..' || p.toLowerCase()==='.git'))throw Error('Invalid file path');
  const target=path.resolve(root,relative);if(!inside(root,target))throw Error('File is outside the workspace');
  const parent=realpathSync(path.dirname(target));if(parent!==root && !inside(root,parent))throw Error('File is outside the workspace');
  try{if(!lstatSync(target).isFile())throw Error('Not a regular file');}catch(error){if(error.code!=='ENOENT')throw error;}
  return target;
}
function fingerprint(root,relative){
  const target=targetPath(root,relative);
  if(!existsSync(target))return 'missing';
  const info=lstatSync(target);if(info.size>8*1024*1024)throw Error('File is too large');
  return hash(readFileSync(target));
}
function sections(diff){
  if(typeof diff!=='string' || Buffer.byteLength(diff)>2*1024*1024)return [];
  return diff.split(/(?=^diff --git )/m).filter(s=>s.startsWith('diff --git '));
}
function supported(patch){return !/^((?:old|new) mode|rename (?:from|to)|copy (?:from|to)|GIT binary patch|Binary files )/m.test(patch);}
function parsedStat(root,patch,stat){
  const match=/^(\d+)\t(\d+)\t([\s\S]+)$/.exec(stat);if(!match)return null;
  try{const relative=match[3];return {relative,added:Number(match[1]),removed:Number(match[2]),after:fingerprint(root,relative),patch};}catch{return null;}
}
function parseSection(root,patch){
  if(!supported(patch))return null;
  const res=runGit(['apply','--numstat','-z','-'],root,patch);if(res.status!==0)return null;
  const fields=res.stdout.toString('utf8').split('\0');if(fields.length!==2 || fields[1]!=='' || !fields[0])return null;
  return parsedStat(root,patch,fields[0]);
}
function save(file,data){mkdirSync(path.dirname(file),{recursive:true});const temp=file+'.'+process.pid+'.tmp';writeFileSync(temp,JSON.stringify(data),{encoding:'utf8',mode:0o600});renameSync(temp,file);}
export function recordTurnDiff(state,thread,turn,cwd,diff){
  if(!thread || !turn || !cwd)return [];
  let root;try{root=workspaceRoot(cwd);}catch{return [];}
  const patches=sections(diff).filter(supported).slice(0,100);if(!patches.length)return [];
  // A single Git process parses all file names, including quoted Unicode paths. The actual
  // reverse applicability is checked on undo, after the current file fingerprint is verified.
  const stats=runGit(['apply','--numstat','-z','-'],root,patches.join(''));if(stats.status!==0)return [];
  const fields=stats.stdout.toString('utf8').split('\0');if(fields.pop()!=='' || fields.length!==patches.length)return [];
  const entries=[];for(let i=0;i<patches.length;++i){
    const item=parsedStat(root,patches[i],fields[i]);if(!item || entries.some(existing=>existing.relative===item.relative))continue;
    item.id=hash(thread+'\0'+turn+'\0'+item.relative);item.reverted=false;entries.push(item);
  }
  if(!entries.length)return [];
  const file=path.join(folder(state,thread),hash(turn)+'.json');save(file,{version:1,thread,turn,root,created:Date.now(),entries});return entries;
}
function records(state,thread){
  const dir=folder(state,thread);let files;try{files=readdirSync(dir).filter(x=>/^[a-f0-9]{64}\.json$/.test(x));}catch{return [];}
  const out=[];for(const name of files.slice(-200)){try{const file=path.join(dir,name),data=JSON.parse(readFileSync(file,'utf8'));if(data.version===1 && data.thread===thread && Array.isArray(data.entries))out.push({file,data});}catch{}}
  return out.sort((a,b)=>b.data.created-a.data.created);
}
export function listChanges(state,thread){
  if(!thread)return [];const out=[];for(const {data} of records(state,thread))for(const item of data.entries){
    if(validId(item.id))out.push({id:item.id,path:item.relative,added:item.added,removed:item.removed,reverted:!!item.reverted});if(out.length>=100)return out;
  }return out;
}
function findChange(state,thread,id){
  if(!thread || !validId(id))throw Error('Unknown change');
  for(const record of records(state,thread)){const item=record.data.entries.find(entry=>entry.id===id);if(item)return {...record,item};}throw Error('Unknown change');
}
export function changeDiff(state,thread,id){return findChange(state,thread,id).item.patch;}
export function revertChange(state,thread,id){
  const {file,data,item}=findChange(state,thread,id);if(item.reverted)throw Error('This change was already reverted');
  // Reparse the stored patch and compare metadata before applying. An altered file, link,
  // binary patch, rename or path outside this turn's workspace is never rolled back.
  const checked=parseSection(data.root,item.patch);
  if(!checked || checked.relative!==item.relative || checked.after!==item.after)throw Error('The file changed after this response; undo was not performed');
  const check=runGit(['apply','--no-index','--reverse','--check','-'],data.root,item.patch);
  if(check.status!==0)throw Error('The change cannot be safely undone');
  const apply=runGit(['apply','--no-index','--reverse','-'],data.root,item.patch);
  if(apply.status!==0)throw Error('The change could not be undone');
  item.reverted=true;save(file,data);return item.relative;
}
