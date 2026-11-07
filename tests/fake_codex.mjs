import {createInterface} from 'node:readline';
import path from 'node:path';
const reply=(id,result)=>process.stdout.write(JSON.stringify({id,result})+'\n');
const event=(method,params)=>process.stdout.write(JSON.stringify({method,params})+'\n');
let pendingApproval=false;
createInterface({input:process.stdin}).on('line',line=>{
  const m=JSON.parse(line);
  if(m.id===901 && m.result){if(m.result.answers.q1.answers[0]!=='вариант')process.exit(8);process.stdout.write(JSON.stringify({id:900,method:'item/commandExecution/requestApproval',params:{threadId:'test-1',command:'example'}})+'\n');return;}
  if(m.id===900 && m.result){if(m.result.decision!=='decline')process.exit(7);event('item/agentMessage/delta',{threadId:'test-1',delta:'ответ'});event('turn/completed',{threadId:'test-1',turn:{status:'completed'}});return;}
  if(m.id===undefined)return;
  switch(m.method){
    case 'initialize':reply(m.id,{});break;
    case 'account/read':reply(m.id,{account:{type:'chatgpt'}});break;
    case 'model/list':reply(m.id,{data:[{model:'test-model',displayName:'Test',isDefault:true}]});break;
    case 'mcpServerStatus/list':reply(m.id,{data:[{name:'sc2editor',tools:{one:{}}}]});break;
    case 'thread/list':reply(m.id,{data:[{id:'test-1',name:'Тест',status:{type:'idle'}}]});break;
    case 'thread/turns/list':reply(m.id,{data:[{items:[{type:'userMessage',content:[{text:'[Контекст SC2 Editor]\nINTERNAL MAP CONTEXT\n\n[Сообщение пользователя]\nвопрос'}]},{type:'agentMessage',text:'история'},{type:'commandExecution',command:'example '+ 'x'.repeat(400),status:'completed'}]}]});break;
    case 'thread/resume':reply(m.id,{thread:{id:'test-1'}});break;
    case 'thread/start':reply(m.id,{thread:{id:'test-1'}});break;
    case 'turn/start':{const hasImage=m.params.input[0].text.includes('другая карта'),expected=hasImage?process.env.SC2_TEST_MAP2:process.env.SC2_TEST_MAP;if(m.params.cwd!==path.dirname(expected) || !m.params.input[0].text.includes(JSON.stringify(expected)) || !m.params.input[0].text.includes('windowId="123"'))process.exit(9);if(hasImage && (m.params.input[1]?.type!=='localImage'||m.params.input[1]?.path!==process.env.SC2_TEST_IMAGE || m.params.input[2]?.type!=='localImage' || m.params.input[2]?.path!==process.env.SC2_TEST_IMAGE2 || m.params.input.length!==3))process.exit(10);reply(m.id,{turn:{id:'turn-1'}});pendingApproval=true;setTimeout(()=>{if(pendingApproval)process.stdout.write(JSON.stringify({id:901,method:'item/tool/requestUserInput',params:{threadId:'test-1',questions:[{id:'q1',question:'Выбор?',options:[]}]}})+'\n')},10);break;}
    case 'turn/interrupt':reply(m.id,{});break;
    default:reply(m.id,{});
  }
});
