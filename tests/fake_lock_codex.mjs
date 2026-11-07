import {createInterface} from 'node:readline';
import {unlinkSync} from 'node:fs';
const reply=(id,result)=>process.stdout.write(JSON.stringify({id,result})+'\n');
const event=(method,params)=>process.stdout.write(JSON.stringify({method,params})+'\n');
let resumes=0;
createInterface({input:process.stdin}).on('line',line=>{
 const m=JSON.parse(line);if(m.id===undefined)return;
 switch(m.method){
  case 'initialize':reply(m.id,{});break;
  case 'account/read':reply(m.id,{account:{type:'chatgpt'}});break;
  case 'model/list':reply(m.id,{data:[{model:'test',displayName:'Test',isDefault:true}]});break;
  case 'mcpServerStatus/list':reply(m.id,{data:[]});break;
  case 'thread/list':reply(m.id,{data:[{id:'locked-test',name:'Locked test',status:{type:'idle'}}]});break;
  case 'thread/turns/list':reply(m.id,{data:[]});break;
  case 'thread/resume':if(++resumes===1)process.stdout.write(JSON.stringify({id:m.id,error:{code:-32000,message:'thread locked-test already has an active writer'}})+'\n');else reply(m.id,{thread:{id:'locked-test'}});break;
  case 'turn/start':{const image=m.params.input.find(x=>x.type==='localImage');if(!image)process.exit(3);reply(m.id,{turn:{id:'turn-1'}});setTimeout(()=>{try{unlinkSync(image.path)}catch{}event('turn/completed',{threadId:'locked-test',turn:{status:'completed'}})},300);break;}
  default:reply(m.id,{});
 }
});
