import {readFileSync} from 'node:fs';
export async function listSessions(){return [{sessionId:'claude-1',summary:'Claude test'}]}
export async function getSessionMessages(){return [{type:'user',message:{content:'прошлый вопрос'}},{type:'assistant',message:{content:[{type:'text',text:'прошлый ответ'}]}}]}
export async function* query({prompt,options}){
  if(typeof prompt!=='string'){
    let message;for await(const item of prompt)message=item;
    const images=message?.message?.content?.filter(x=>x.type==='image')||[];
    if(images.length!==2 || images.some((image,i)=>image.source?.media_type!=='image/png' || image.source.data!==readFileSync(i?process.env.SC2_TEST_IMAGE2:process.env.SC2_TEST_IMAGE).toString('base64')))throw Error('Missing or reordered Claude image content');
    prompt=message.message.content.find(x=>x.type==='text')?.text||'';
  }else throw Error('Image must be passed as a structured Claude input');
  if(!prompt.includes(JSON.stringify(process.env.SC2_TEST_MAP2)) || !prompt.includes('windowId="123"'))throw Error('Missing active map context');
  yield {type:'system',subtype:'init',session_id:'claude-1'};
  const result=await options.canUseTool('testTool',{target:'test'});
  if(result.behavior!=='deny')throw Error('Approval must be denied explicitly in this test');
  yield {type:'stream_event',event:{type:'content_block_delta',delta:{type:'text_delta',text:'ответ Claude'}}};
  yield {type:'result',is_error:false};
}
