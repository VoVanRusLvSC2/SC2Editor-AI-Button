import {spawn} from 'node:child_process';
const spec=JSON.parse(Buffer.from(process.env.SC2_AI_MCP_SPEC||'','base64').toString('utf8'));
const env={...process.env};delete env.SC2_AI_MCP_SPEC;
const child=spawn(spec.command,spec.args,{cwd:spec.cwd,env,stdio:'inherit',windowsHide:true});
child.on('error',()=>process.exit(1));child.on('exit',(code,signal)=>process.exit(code??(signal?1:0)));
process.on('SIGTERM',()=>child.kill());
