import {spawn, execFileSync} from 'node:child_process';
import {createInterface} from 'node:readline';
import {existsSync, readdirSync, statSync} from 'node:fs';
import path from 'node:path';
import {pathToFileURL, fileURLToPath} from 'node:url';

// A private stdio channel connects this process only to the editor extension.
// Data fields are base64-encoded UTF-8; stdout never contains debug output.
const b64 = s => Buffer.from(String(s ?? ''), 'utf8').toString('base64');
const un64 = s => Buffer.from(s || '', 'base64').toString('utf8');
const emit = (kind, ...values) => process.stdout.write([kind, ...values.map(b64)].join('\t') + '\n');
const cwd = process.argv[2] && existsSync(process.argv[2]) ? process.argv[2] : process.cwd();
let provider = 'codex', model = '', selected = '', running = null, generation = 0;
let mapContext={path:'',dirty:false,caption:'',windowId:'',processId:'',inferred:false},mcpStatus='SC2 MCP: проверка...';
function showContext(){
  const name=mapContext.path?path.basename(mapContext.path):'';
  emit('mcp',mcpStatus+' · '+(name?`карта: ${name}${mapContext.inferred?' (по истории редактора)':''}${mapContext.dirty?' (есть несохранённые изменения)':''}`:'карта не определена'));
}
function mapPrompt(text){
  const map=mapContext.path
    ? `Документ этого окна SC2 Editor: ${JSON.stringify(mapContext.path)}. ${mapContext.inferred?'Путь восстановлен по сокращённому заголовку и истории редактора; проверь привязку к окну через MCP. ':''}${mapContext.dirty?'В редакторе есть несохранённые изменения; сохранённый файл может отличаться.':'Это путь к сохранённому файлу; наличие несохранённых изменений необходимо проверять в редакторе.'}`
    : `Точный путь открытой карты не определён (заголовок: ${JSON.stringify(mapContext.caption)}). Не выбирай другую карту по догадке; попроси точный путь.`;
  return `[Контекст SC2 Editor]\n${map}\nОкно редактора: windowId=${JSON.stringify(mapContext.windowId)}, processId=${JSON.stringify(mapContext.processId)}. Работай с этой картой по умолчанию. SC2 MCP может быть настроен на другую рабочую папку: не считай её текущей картой. Найди и включи через modkit.tools/modkit.enable_tools инструменты live.windows и live.connect, если они доступны, и используй путь и windowId этого редактора для подключения; учитывай сохранение и ограничения MCP. Если live API отсутствует или привязка не подтверждена, сообщи об этом. Не утверждай, что изменения применены к открытому редактору, пока это не проверено.\n\n[Сообщение пользователя]\n${text}`;
}
const claudeDirs=new Map();
let codex, claude, aborter, nextApproval = 1, questionRequest = null; const approvals = new Map();
function findCodex(){
  if(process.env.SC2_AI_CODEX_PATH && existsSync(process.env.SC2_AI_CODEX_PATH))return process.env.SC2_AI_CODEX_PATH;
  const root=path.join(process.env.LOCALAPPDATA || '', 'OpenAI','Codex','bin');
  try { const candidates=readdirSync(root).map(x=>path.join(root,x,'codex.exe')).filter(existsSync);
    candidates.sort((a,b)=>statSync(b).mtimeMs-statSync(a).mtimeMs);if(candidates.length)return candidates[0]; } catch {}
  try { const values=execFileSync('where.exe',['codex.exe'],{encoding:'utf8',windowsHide:true}).trim().split(/\r?\n/); return values.find(existsSync); }catch{return null;}
}
const codexPath=findCodex();
class AppServer{
  constructor(exe){this.proc=spawn(exe,[...(process.env.SC2_AI_CODEX_EXTRA_ARGS ? JSON.parse(process.env.SC2_AI_CODEX_EXTRA_ARGS) : []),'app-server','--listen','stdio://'],{stdio:['pipe','pipe','pipe'],windowsHide:true});this.id=1;this.pending=new Map();this.ready=false;
    createInterface({input:this.proc.stdout}).on('line',line=>this.onLine(line));
    this.proc.stderr.on('data',()=>{});
    this.proc.on('error',e=>this.failed('Не удалось запустить Codex: '+e.message));
    this.proc.on('exit',()=>this.failed('Codex app-server завершил работу.'));
  }
  failed(message){if(this.closed)return;this.ready=false;for(const p of this.pending.values())p.reject(Error(message));this.pending.clear();if(provider==='codex')emit('error',message);}
  send(msg){if(!this.proc.stdin.destroyed)this.proc.stdin.write(JSON.stringify(msg)+'\n');}
  request(method,params={}){return new Promise((resolve,reject)=>{const id=this.id++;this.pending.set(id,{resolve,reject});this.send({id,method,params});setTimeout(()=>{if(this.pending.delete(id))reject(Error('Нет ответа от Codex: '+method));},30000).unref();});}
  onLine(line){let m;try{m=JSON.parse(line)}catch{return;}
    if(m.id!==undefined && !m.method){const p=this.pending.get(m.id);if(!p)return;this.pending.delete(m.id);m.error?p.reject(Error(m.error.message || 'Ошибка Codex')):p.resolve(m.result);return;}
    if(m.id!==undefined && m.method){this.serverRequest(m);return;}
    if(provider!=='codex')return;
    const p=m.params || {};if(p.threadId && p.threadId!==selected)return;
    if(m.method==='item/agentMessage/delta')emit('delta',p.delta||'');
    if(m.method==='item/started' && p.item?.type==='mcpToolCall')emit('event',`MCP: ${p.item.server} / ${p.item.tool}`);
    if(m.method==='item/started' && p.item?.type==='commandExecution')emit('event','Выполняется команда');
    if(m.method==='turn/completed'){running=null;emit('busy','0');const turn=p.turn||{};emit('status',turn.status==='completed'?'Готово':`Завершено: ${turn.status||'неизвестно'}`);if(turn.error?.message)emit('error',turn.error.message);}
    if(m.method==='error' && p.error?.message)emit('error',p.error.message);
  }
  serverRequest(m){
    const supported=['item/commandExecution/requestApproval','item/fileChange/requestApproval'];
    if(supported.includes(m.method)){
      const id=String(nextApproval++), details=JSON.stringify(m.params,null,2).slice(0,12000);
      approvals.set(id,yes=>this.send({id:m.id,result:{decision:yes?'accept':'decline'}}));
      emit('approval',id,`Запрос Codex: ${m.method}\n\n${details}`);return;
    }
    if(m.method==='item/permissions/requestApproval'){
      const id=String(nextApproval++);approvals.set(id,yes=>this.send({id:m.id,result:{permissions:yes?(m.params.permissions||{}):{},scope:'turn'}}));
      emit('approval',id,`Codex запрашивает дополнительные права:\n\n${JSON.stringify(m.params,null,2).slice(0,12000)}`);return;
    }
    if(m.method==='item/tool/requestUserInput'){
      const questions=m.params?.questions||[];
      if(!questions.length || questions.some(q=>q.isSecret) || questionRequest){this.send({id:m.id,error:{code:-32601,message:'This input request is not supported.'}});emit('error','Секретный или повторный запрос ввода пока не поддерживается.');return;}
      questionRequest={requestId:m.id,questions,index:0,answers:{}};askNext();return;
    }
    if(m.method==='mcpServer/elicitation/request'){
      this.send({id:m.id,result:{action:'decline'}});emit('error','Форма подтверждения MCP пока не поддерживается и была отклонена.');return;
    }
    // Unsupported interactive requests are rejected explicitly; never auto-approve.
    this.send({id:m.id,error:{code:-32601,message:'This client does not support this interactive request.'}});
    emit('error',`Этот запрос Codex пока не поддерживается: ${m.method}`);
  }
  async start(){await this.request('initialize',{clientInfo:{name:'sc2_editor_ai',title:'SC2 Editor AI',version:'0.8.0'},capabilities:{}});this.send({method:'initialized',params:{}});this.ready=true;}
  close(){this.closed=true;try{this.proc.stdin.end();this.proc.kill()}catch{}}
}
function askNext(){
  if(!questionRequest)return;
  const q=questionRequest.questions[questionRequest.index];
  if(!q){codex.send({id:questionRequest.requestId,result:{answers:questionRequest.answers}});questionRequest=null;return;}
  const choices=(q.options||[]).map(o=>`• ${o.label}: ${o.description}`).join('\n');
  emit('question',q.question+(choices?'\n'+choices:''));
}
async function codexConnect(gen){
  if(!codexPath)throw Error('Codex не найден. Установите приложение Codex или задайте SC2_AI_CODEX_PATH.');
  if(!codex || !codex.ready){codex?.close();codex=new AppServer(codexPath);await codex.start();}
  const [account,models,mcp]=await Promise.all([codex.request('account/read',{refreshToken:false}),codex.request('model/list',{limit:100}),codex.request('mcpServerStatus/list',{limit:100})]);
  if(gen!==generation || provider!=='codex')return;
  emit('status',account.account?'Codex подключён':'Войдите в Codex');
  emit('models_begin');for(const m of models.data||[])emit('model',m.model||m.id,m.displayName||m.model||m.id,m.isDefault?'1':'0');emit('models_end');
  const server=(mcp.data||[]).find(x=>x.name==='sc2editor');mcpStatus=server?(server.toolsError?'SC2 MCP: ошибка подключения':(Object.keys(server.tools||{}).length?'SC2 MCP подключён':'SC2 MCP настроен')):'SC2 MCP не найден в настройках Codex';showContext();
  await listCodex(gen);if(gen===generation && provider==='codex')emit('ready','1');
}
async function listCodex(gen=generation){
  emit('threads_begin');let cursor=null,count=0;do{const r=await codex.request('thread/list',{limit:100,cursor,sortKey:'updated_at'});
    if(gen!==generation || provider!=='codex')return;
    for(const t of r.data||[]){emit('thread',t.id,t.name||t.preview?.replace(/\s+/g,' ').slice(0,90)||'Без названия',t.status?.type||'');count++;}cursor=r.nextCursor;
  }while(cursor && count<1000);emit('threads_end',cursor?'Есть дополнительные чаты':'');
}
function itemText(item){if(item.type==='userMessage')return 'Вы: '+(item.content||[]).map(c=>c.text||'').join('');
  if(item.type==='agentMessage')return 'AI: '+(item.text||'');
  if(item.type==='mcpToolCall')return `[MCP: ${item.server} / ${item.tool} — ${item.status}]`;
  if(item.type==='commandExecution')return `[Команда: ${item.command||''} — ${item.status}]`;
  if(item.type==='fileChange')return `[Изменения файлов: ${item.status}]`;return '';}
async function openCodex(id,gen){
  const result=await codex.request('thread/turns/list',{threadId:id,limit:100,itemsView:'full',sortDirection:'desc'});
  if(gen!==generation)return;
  const lines=[];for(const turn of [...(result.data||[])].reverse())for(const item of turn.items||[]){let text=itemText(item);if(text)lines.push(text);}
  let history=lines.join('\r\n\r\n');if(history.length>200000)history='[Показана последняя часть истории]\r\n'+history.slice(-200000);
  emit('history',history);if(result.nextCursor)emit('event','Показаны последние 100 сообщений диалога.');
}
async function sendCodex(text){
  running={provider:'codex',turnId:null,threadId:selected,stopRequested:false};
  const turnCwd=mapContext.path?path.dirname(mapContext.path):cwd;
  if(!selected){const r=await codex.request('thread/start',{cwd:turnCwd,model:model||null,approvalPolicy:'on-request',approvalsReviewer:'user',...(process.env.SC2_AI_TEST_EPHEMERAL==='1'?{ephemeral:true,sandbox:'read-only'}:{})});selected=r.thread.id;emit('selected',selected);await listCodex();}
  else await codex.request('thread/resume',{threadId:selected,excludeTurns:true,approvalPolicy:'on-request',approvalsReviewer:'user'});
  emit('line','Вы: '+text);emit('line','AI: ');emit('busy','1');
  const r=await codex.request('turn/start',{threadId:selected,cwd:turnCwd,input:[{type:'text',text:mapPrompt(text),text_elements:[]}],model:model||null});if(running?.provider==='codex'){running.turnId=r.turn.id;running.threadId=selected;if(running.stopRequested)await codex.request('turn/interrupt',{threadId:selected,turnId:r.turn.id});}
}
function sc2Config(){
  if(!codexPath)return undefined;
  try{const config=JSON.parse(execFileSync(codexPath,['mcp','get','sc2editor','--json'],{encoding:'utf8',windowsHide:true,timeout:8000,stdio:['ignore','pipe','ignore']}));const t=config.transport;
    if(!config.enabled || !t)return undefined;
    if(t.type==='stdio'){
      const env={...process.env,...(t.env||{})};for(const n of t.env_vars||[])if(process.env[n]!==undefined)env[n]=process.env[n];
      if(t.cwd){const wrapper=path.join(path.dirname(fileURLToPath(import.meta.url)),'mcp_spawn.mjs');
        env.SC2_AI_MCP_SPEC=Buffer.from(JSON.stringify({command:t.command,args:t.args||[],cwd:t.cwd})).toString('base64');
        return {sc2editor:{type:'stdio',command:process.execPath,args:[wrapper],env}};}
      return {sc2editor:{type:'stdio',command:t.command,args:t.args||[],env}};
    }
    if((t.type==='http'||t.type==='streamable_http') && t.url)return {sc2editor:{type:'http',url:t.url,headers:t.http_headers||{}}};
  }catch{}return undefined;
}
async function claudeConnect(gen){
  try{claude=await import(process.env.SC2_AI_CLAUDE_MODULE?pathToFileURL(process.env.SC2_AI_CLAUDE_MODULE).href:'@anthropic-ai/claude-agent-sdk');}catch{throw Error('Claude Agent SDK не установлен. Выполните npm ci в папке bridge.');}
  let loggedIn=!!process.env.ANTHROPIC_API_KEY;
  if(!loggedIn && process.platform==='win32')try{
    const bin=path.join(path.dirname(fileURLToPath(import.meta.url)),'node_modules','@anthropic-ai','claude-agent-sdk-win32-x64','claude.exe');
    const result=JSON.parse(execFileSync(bin,['auth','status','--json'],{encoding:'utf8',windowsHide:true,timeout:12000,stdio:['ignore','pipe','ignore']}));loggedIn=!!result.loggedIn;
  }catch(e){try{loggedIn=!!JSON.parse(e.stdout||'{}').loggedIn}catch{}}
  if(gen!==generation || provider!=='claude')return;
  emit('status',loggedIn?'Claude авторизован':'Claude: нужен вход');
  emit('models_begin');for(const [id,name] of [['','По умолчанию'],['sonnet','Sonnet'],['opus','Opus'],['haiku','Haiku']])emit('model',id,name,id===''?'1':'0');emit('models_end');
  mcpStatus=sc2Config()?'SC2 MCP настроен для Claude':'SC2 MCP не найден в настройках Codex';showContext();await listClaude(gen);if(gen===generation && provider==='claude')emit('ready','1');
}
async function listClaude(gen=generation){emit('threads_begin');const sessions=await claude.listSessions({limit:200});if(gen!==generation || provider!=='claude')return;for(const s of sessions){if(s.cwd)claudeDirs.set(s.sessionId,s.cwd);emit('thread',s.sessionId,s.customTitle||s.summary||s.firstPrompt||'Без названия','');}emit('threads_end');}
async function openClaude(id,gen){const msgs=await claude.getSessionMessages(id,{limit:200});if(gen!==generation)return;const lines=[];
  for(const m of msgs){const parts=typeof m.message?.content==='string'?[{text:m.message.content}]:m.message?.content||[];const text=parts.filter(x=>x.type==='text'||typeof x.text==='string').map(x=>x.text).join('\n');if(text)lines.push((m.type==='user'?'Вы: ':'AI: ')+text);}
  let history=lines.join('\r\n\r\n');if(history.length>200000)history='[Показана последняя часть истории]\r\n'+history.slice(-200000);emit('history',history);}
async function sendClaude(text){
  const current=selected;aborter=new AbortController();emit('line','Вы: '+text);emit('line','AI: ');emit('busy','1');running={provider:'claude'};
  let streamed=false,failed=false;const options={cwd:(current&&claudeDirs.get(current))||(mapContext.path?path.dirname(mapContext.path):cwd),abortController:aborter,includePartialMessages:true,permissionMode:'default',settingSources:['user','project'],
    ...(current?{resume:current}:{}),...(model?{model}:{}),...(sc2Config()?{mcpServers:sc2Config()}:{}),
    canUseTool:async(toolName,input)=>{const id=String(nextApproval++);return await new Promise(resolve=>{approvals.set(id,yes=>resolve(yes?{behavior:'allow',updatedInput:input}:{behavior:'deny',message:'Отклонено пользователем'}));emit('approval',id,`Запрос Claude: ${toolName}\n\n${JSON.stringify(input,null,2).slice(0,12000)}`);});}};
  try{for await(const message of claude.query({prompt:mapPrompt(text),options})){
    if(message.type==='system' && message.subtype==='init' && message.session_id && !selected){selected=message.session_id;emit('selected',selected);}
    if(message.type==='stream_event' && message.event?.type==='content_block_delta' && message.event.delta?.type==='text_delta'){emit('delta',message.event.delta.text);streamed=true;}
    if(message.type==='assistant' && !streamed){for(const part of message.message?.content||[])if(part.type==='text')emit('delta',part.text);}
    if(message.type==='result' && message.is_error){failed=true;emit('error',(message.errors||[]).join('\n')||'Claude завершился с ошибкой.');}
  }if(!failed)emit('status','Готово');await listClaude();}finally{running=null;aborter=null;emit('busy','0');}
}
async function connect(which){const gen=++generation;provider=which;selected='';model='';emit('ready','0');emit('clear');emit('threads_begin');emit('threads_end');emit('status','Подключение...');emit('provider',which);try{if(which==='codex')await codexConnect(gen);else await claudeConnect(gen);}catch(e){if(gen===generation)throw e;}}
async function handle(line){const [cmd,...args]=line.split('\t');const a=args.map(un64);
  if(cmd==='provider'){if(running)throw Error('Остановите текущий ответ перед переключением.');await connect(a[0]==='claude'?'claude':'codex');}
  else if(cmd==='map'){mapContext={path:a[0]||'',dirty:a[1]==='1',caption:a[2]||'',windowId:a[3]||'',processId:a[4]||'',inferred:a[5]==='1'};showContext();}
  else if(cmd==='refresh'){if(provider==='codex')await listCodex();else await listClaude();}
  else if(cmd==='open'){if(running)throw Error('Остановите ответ перед выбором чата.');selected=a[0];let g=++generation;emit('busy','1');try{provider==='codex'?await openCodex(selected,g):await openClaude(selected,g);}finally{if(g===generation)emit('busy','0');}if(g===generation)emit('selected',selected);}
  else if(cmd==='new'){if(running)throw Error('Остановите текущий ответ.');selected='';generation++;emit('clear');emit('selected','');}
  else if(cmd==='model')model=a[0];
  else if(cmd==='answer' && questionRequest){const q=questionRequest.questions[questionRequest.index++];questionRequest.answers[q.id]={answers:[a[0]||'']};askNext();}
  else if(cmd==='send'){if(running)throw Error('Дождитесь завершения ответа.');if(!a[0]?.trim())return;provider==='codex'?await sendCodex(a[0]):await sendClaude(a[0]);}
  else if(cmd==='stop'){if(questionRequest){codex.send({id:questionRequest.requestId,error:{code:-32800,message:'Input cancelled by user.'}});questionRequest=null;}if(running?.provider==='codex'){if(running.turnId)await codex.request('turn/interrupt',{threadId:running.threadId,turnId:running.turnId});else running.stopRequested=true;}else aborter?.abort();}
  else if(cmd==='approve'){const f=approvals.get(a[0]);if(f){approvals.delete(a[0]);f(a[1]==='yes');}}
}
createInterface({input:process.stdin}).on('line',line=>{handle(line).catch(e=>{running=null;emit('busy','0');emit('error',e.message||String(e));});}).on('close',()=>{for(const f of approvals.values())f(false);aborter?.abort();codex?.close();process.exit(0);});
process.on('uncaughtException',e=>emit('error',e.message||String(e)));
connect(process.argv[3]==='claude'?'claude':'codex').catch(e=>emit('error',e.message||String(e)));
