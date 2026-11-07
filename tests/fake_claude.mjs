export async function listSessions(){return [{sessionId:'claude-1',summary:'Claude test'}]}
export async function getSessionMessages(){return [{type:'user',message:{content:'прошлый вопрос'}},{type:'assistant',message:{content:[{type:'text',text:'прошлый ответ'}]}}]}
export async function* query({options}){
  yield {type:'system',subtype:'init',session_id:'claude-1'};
  const result=await options.canUseTool('testTool',{target:'test'});
  if(result.behavior!=='deny')throw Error('Approval must be denied explicitly in this test');
  yield {type:'stream_event',event:{type:'content_block_delta',delta:{type:'text_delta',text:'ответ Claude'}}};
  yield {type:'result',is_error:false};
}
