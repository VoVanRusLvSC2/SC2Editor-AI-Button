import test from 'node:test';
import assert from 'node:assert/strict';
import {createMcpFreshener} from '../bridge/mcp_refresh.mjs';

test('one refresh per runtime fingerprint, queued before the turn without restarting apps',async()=>{
 let stamp='old';const calls=[];
 const refresh=createMcpFreshener(async(method,params)=>{calls.push([method,params]);},()=>stamp);
 assert.equal(await refresh(),true);assert.equal(await refresh(),false);
 stamp='updated';assert.equal(await refresh(),true);
 assert.deepEqual(calls,[['config/mcpServer/reload',null],['config/mcpServer/reload',null]]);
});
test('a rejected reload is never recorded as loaded; unknown transport has no invented freshness',async()=>{
 let fail=true,attempts=0;
 const refresh=createMcpFreshener(async()=>{attempts++;if(fail)throw Error('reload refused');},()=> 'one');
 await assert.rejects(refresh(),/reload refused/);fail=false;
 assert.equal(await refresh(),true);assert.equal(attempts,2);
 assert.equal(await createMcpFreshener(()=>assert.fail('unexpected reload'),()=>null)(),false);
});
