import {writeFileSync} from 'node:fs';import path from 'node:path';
export async function listSessions(){return [{sessionId:'claude-change',summary:'Change test',cwd:process.env.SC2_TEST_CHANGE_DIR}];}
export async function getSessionMessages(){return [];}
export async function* query(){yield {type:'system',subtype:'init',session_id:'claude-change'};writeFileSync(path.join(process.env.SC2_TEST_CHANGE_DIR,'changes.txt'),'after\n');yield {type:'result',is_error:false};}
