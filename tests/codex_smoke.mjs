import {spawn} from 'node:child_process';import {createInterface} from 'node:readline';
const child=spawn(process.execPath,['bridge/main.mjs'],{stdio:['pipe','pipe','inherit'],windowsHide:true,env:{...process.env,SC2_AI_TEST_EPHEMERAL:'1'}});
let submitted=false,hasReply=false,finished=false;const send=(c,...v)=>child.stdin.write([c,...v.map(x=>Buffer.from(x).toString('base64'))].join('\t')+'\n');
createInterface({input:child.stdout}).on('line',line=>{const [k,...encoded]=line.split('\t');const v=encoded.map(x=>Buffer.from(x,'base64').toString());
 if(k==='error'){console.error('ERROR:',v[0]);finished=true;child.stdin.end();process.exitCode=1;}
 if(k==='ready'&&v[0]==='1'&&!submitted){submitted=true;send('send','Ответь одним словом: готово. Не используй инструменты.');}
 if(k==='approval'){send('approve',v[0],'no');}
 if(k==='delta'&&v[0])hasReply=true;
 if(k==='busy'&&v[0]==='0'&&submitted){finished=true;console.log(hasReply?'PASS: real Codex app-server ephemeral thread streamed an answer':'FAIL: no streamed answer');process.exitCode=hasReply?0:1;child.stdin.end();}
});
setTimeout(()=>{if(!finished){console.error('ERROR: smoke test timeout');process.exitCode=1;child.kill()}},90000).unref();
