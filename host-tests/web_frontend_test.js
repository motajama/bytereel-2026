const fs = require('fs');
const vm = require('vm');
const assert = require('assert');
const {execFileSync}=require('child_process');
const validator=process.env.BR_SOURCE_VALIDATOR;
assert(validator,'host-tests/run.sh must provide the compiled production source/bytecode validator');

const header = fs.readFileSync('firmware/bytereel_2026/web_ui.h', 'utf8');
const html = header.match(/R"BRHTML\(([\s\S]*?)\)BRHTML";/)[1];
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
class Element {
  constructor(id='') { this.id=id;this.textContent='';this.value='';this.innerHTML='';this.className='';this.dataset={};this.children=[];this.handlers={};this.checked=true;this.scrollTop=0;this.scrollHeight=100;this.classList={add:x=>{if(!this.className.includes(x))this.className+=' '+x;},remove:x=>{this.className=this.className.replace(x,'');}}; }
  replaceChildren(...xs){this.children=xs;}
  addEventListener(name,fn){this.handlers[name]=fn;}
  append(x){this.children.push(x);}
}
const elements={};
const document={getElementById:id=>elements[id]||(elements[id]=new Element(id)),createElement:tag=>new Element(tag)};
const states=Array.from({length:8},(_,slot)=>({slot,name:'SLOT '+(slot+1),state:'EMPTY',revision:0,videoBytes:null,audioBytes:null,bankCapacity:256}));
const records=new Map(),calls=[];let failSlots=false,failLog=false,hangRequests=false,delaySlot0=false,resolveSlot0=null,delayAssembly=false,resolveAssembly=null,logResponses=[],responseSequence=0;
const reply=(status,data)=>{const requestId='test-'+(++responseSequence);return {status,ok:status>=200&&status<300,headers:{get:n=>n==='X-byteREEL-Request-Id'?requestId:null},text:async()=>typeof data==='string'?data:JSON.stringify(data)}};
async function fetch(url,options={}){
  calls.push({url,method:options.method||'GET',body:options.body,headers:options.headers||{}});
  if(hangRequests)return new Promise((resolve,reject)=>options.signal.addEventListener('abort',()=>{const e=Error('aborted');e.name='AbortError';reject(e)}));
  if(url==='/api/v1/slots'&&failSlots)throw Error('offline');
  if(url.startsWith('/api/v1/log')&&failLog)throw Error('offline');
  if(url==='/api/v1/status')return reply(200,{project:'byteREEL 2026',bootId:7,firmwareVersion:'test-fw',uiVersion:'ui-2026.11.19-r2'});
  if(url.startsWith('/api/v1/log?'))return reply(200,logResponses.length?logResponses.shift():{bootId:7,oldest:1,next:0,lost:false,entries:[]});
  if(url==='/api/v1/instructions')return reply(200,{instructions:[]});
  if(url==='/api/v1/slots')return reply(200,states);
  if(url==='/api/v1/assemble'&&options.method==='POST'){
    const q=JSON.parse(options.body);let out;try{out=JSON.parse(execFileSync(validator,['assemble'],{input:JSON.stringify(q),encoding:'utf8'}))}catch(e){const detail=String(e.stdout||e.stderr||e.message);return reply(400,{code:'ASSEMBLY_ERROR',phase:'ASSEMBLER',message:detail})}if(out&&out.ok===false)return reply(400,{code:'ASSEMBLY_ERROR',phase:'ASSEMBLER',message:out.error});if(delayAssembly)return new Promise(resolve=>{resolveAssembly=()=>resolve(reply(200,out))});return reply(200,out);
  }
  const m=url.match(/^\/api\/v1\/slots\/(\d+)$/);
  if(m){const id=Number(m[1]),r=records.get(id);
    if(!options.method||options.method==='GET'){if(id===0&&delaySlot0&&r)return new Promise(resolve=>{resolveSlot0=()=>resolve(reply(200,{revision:r.revision,slot:r.record}))});return r?reply(200,{revision:r.revision,slot:r.record}):reply(404,{code:'SLOT_EMPTY',message:'Slot je prázdný'})}
    const q=JSON.parse(options.body||'{}');
    if(options.method==='PUT'){
      const checked=JSON.parse(execFileSync(validator,['validate',String(id)],{input:options.body,encoding:'utf8'}));if(!checked.ok)return reply(400,{code:'SLOT_INVALID',phase:'SOURCE_BYTECODE_VALIDATION',message:'pole '+checked.field+': '+checked.reason});
      const actual=r?r.revision:0;if(q.expectedRevision!==actual)return reply(409,{code:'REVISION_CONFLICT',message:'Revision konflikt'});
      const revision=actual+1;records.set(id,{revision,record:q.slot});Object.assign(states[id],{state:'OK',revision,name:q.slot.name,videoBytes:q.slot.video.length,audioBytes:q.slot.audio.length});return reply(200,{ok:true,revision});
    }
    if(options.method==='DELETE'){
      const actual=r?r.revision:0;if(q.expectedRevision!==actual)return reply(409,{code:'REVISION_CONFLICT',message:'Revision konflikt'});
      records.delete(id);Object.assign(states[id],{state:'EMPTY',revision:0,videoBytes:null,audioBytes:null});return reply(200,{ok:true,revision:0});
    }
  }
  return reply(404,{code:'NOT_FOUND',message:'Neznámá cesta'});
}
const timers=[];const sandbox={document,window:{addEventListener(){}},fetch,confirm:()=>true,AbortController,setTimeout:f=>{timers.push(f);return timers.length;},clearTimeout:()=>{},setInterval:()=>1,console:{log(){},error(){}},Number,String,JSON,Array,Error,Promise,Map,Math};
vm.runInNewContext(script,sandbox,{filename:'byteREEL UI'});
const tick=()=>new Promise(resolve=>setImmediate(resolve));
(async()=>{
  for(let i=0;i<6;i++)await tick();
  assert.strictEqual(document.getElementById('cards').children.length,8,'eight card click targets must be registered');
  assert.strictEqual(typeof document.getElementById('cards').children[0].onclick,'function');
  await document.getElementById('cards').children[0].onclick();await tick();
  assert(document.getElementById('editor').className.includes('open'));
  assert.strictEqual(document.getElementById('rev').textContent,'revision 0 · PRÁZDNÝ');
  document.getElementById('asrc').value='NOT_AN_OPCODE 00';document.getElementById('asrc').handlers.input();await document.getElementById('aasm').onclick();assert(document.getElementById('msg').textContent.includes('HTTP 400')&&document.getElementById('msg').textContent.includes('requestId test-')&&document.getElementById('msg').textContent.includes('line 1'),'assembler rejection must show request HTTP status, ID and source line');
  document.getElementById('asrc').value='FREQ 18\nWAVE 00\nVOLUME 40\nGATE 01\nJMP 00';document.getElementById('asrc').handlers.input();
  await document.getElementById('aasm').onclick();await tick();
  assert(document.getElementById('audio').value.startsWith('10181100'),'frontend assemble must call the C++ ISA assembler; got '+document.getElementById('audio').value.slice(0,24)+' msg='+document.getElementById('msg').textContent);
  await document.getElementById('save').onclick();await tick();
  assert(records.has(0),'first empty-slot save must PUT the selected zero-based slot');
  assert.strictEqual(records.get(0).record.slot,0);assert.strictEqual(records.get(0).revision,1);assert.strictEqual(records.get(0).record.audio.sourceMatches,true);
  // Reproduce the reported route: open saved slot 1, then empty slot 2.
  await document.getElementById('cards').children[1].onclick();await tick();
  assert.strictEqual(document.getElementById('asrc').value,'','slot 1 source must not leak into the empty slot 2 editor');
  document.getElementById('asrc').value='PFREQ 18\nPDUR 00\nPPLAY 00\nJMP 06';document.getElementById('asrc').handlers.input();
  await document.getElementById('aasm').onclick();await tick();
  assert(document.getElementById('audio').value.startsWith('8018810082003006'),'required PFREQ/PDUR/PPLAY reproduction must assemble to the specified bytes');
  await document.getElementById('save').onclick();await tick();
  assert(records.has(1),'slot 2 first save must use route index 1');assert.strictEqual(records.get(1).record.slot,1);assert.strictEqual(records.get(1).revision,1);
  assert.strictEqual(records.get(1).record.audio.hex.slice(0,16),'8018810082003006');assert.strictEqual(records.get(1).record.audio.sourceMatches,true);assert.strictEqual(records.get(1).record.video.source,null,'empty untouched bank has no foreign/stale source');
  const slot2Put=calls.slice().reverse().find(x=>x.url==='/api/v1/slots/1'&&x.method==='PUT');const slot2Payload=JSON.parse(slot2Put.body);assert.strictEqual(slot2Put.headers['Content-Type'],'application/json');assert.strictEqual(slot2Payload.expectedRevision,0);assert.strictEqual(slot2Payload.slot.audio.source,'PFREQ 18\nPDUR 00\nPPLAY 00\nJMP 06');assert.strictEqual(slot2Payload.slot.audio.hex.length,512);assert.strictEqual(slot2Payload.slot.audio.length,8);assert.strictEqual(slot2Payload.slot.audio.sourceMatches,true);assert.strictEqual(slot2Payload.slot.video.source,null);
  assert(calls.some(x=>x.url==='/api/v1/assemble'&&x.method==='POST'));
  assert(calls.some(x=>x.url==='/api/v1/slots/1'&&x.method==='PUT'));
  assert.strictEqual(states[1].state,'OK','successful slot 2 PUT must refresh its card');
  await document.getElementById('cards').children[1].onclick();await tick();
  assert.strictEqual(document.getElementById('asrc').value,'PFREQ 18\nPDUR 00\nPPLAY 00\nJMP 06');
  assert.strictEqual(document.getElementById('audio').value,records.get(1).record.audio.hex,'reopening slot 2 must restore the matching source and padded bytes');
  const panelLoad=JSON.parse(execFileSync(validator,['panel-load'],{input:JSON.stringify({slot:records.get(1).record}),encoding:'utf8'}));assert(panelLoad.ok&&panelLoad.audioFirst===0x80&&panelLoad.videoFirst===0,'panel LOAD publish helper must receive the exact saved slot 2 pair');
  delaySlot0=true;const oldOpen=sandbox.openSlot(0);await tick();const newOpen=sandbox.openSlot(1);await tick();assert(resolveSlot0,'older slot request should be pending');resolveSlot0();delaySlot0=false;await Promise.all([oldOpen,newOpen]);assert.strictEqual(document.getElementById('editorTitle').textContent,'Slot 2');assert(document.getElementById('asrc').value.includes('PFREQ 18'),'late slot 1 response must not overwrite slot 2');
  const slot2RevisionBeforeRace=records.get(1).revision;document.getElementById('asrc').value='FREQ 18\nWAVE 00\nVOLUME C0\nGATE 01\nJMP 00';document.getElementById('asrc').handlers.input();delayAssembly=true;const pendingSave=sandbox.saveSlot();await tick();assert(resolveAssembly,'save should be waiting for the assembler');const switchDuringAssembly=sandbox.openSlot(0);await tick();resolveAssembly();delayAssembly=false;await Promise.all([pendingSave,switchDuringAssembly]);assert.strictEqual(records.get(1).revision,slot2RevisionBeforeRace+1,'slot switch is blocked until the in-flight save completes');assert.strictEqual(document.getElementById('editorTitle').textContent,'Slot 2');await sandbox.openSlot(0);await sandbox.openSlot(1);await tick();
  // HEX edits retain the old source explicitly as stale and are accepted as HEX-only data.
  document.getElementById('audio').value='00181100'+document.getElementById('audio').value.slice(8);document.getElementById('audio').handlers.input();
  await document.getElementById('save').onclick();await tick();assert.strictEqual(records.get(1).record.audio.sourceMatches,false);assert(records.get(1).record.audio.source.includes('VOLUME C0'));
  // Every UI slot maps to its own zero-based HTTP route and can be initialized.
  for(let id=2;id<8;id++){await document.getElementById('cards').children[id].onclick();await tick();await document.getElementById('save').onclick();await tick();assert(records.has(id),'slot '+(id+1)+' should save to route '+id);assert.strictEqual(records.get(id).record.slot,id)}
  // A panel SAVE has no current assembler source and must remain valid.
  const panelRecord=JSON.parse(JSON.stringify(records.get(1).record));panelRecord.audio.hex='00181100'+panelRecord.audio.hex.slice(8);panelRecord.audio.sourceMatches=false;
  const panelPut=JSON.stringify({expectedRevision:records.get(1).revision,slot:panelRecord});const panelCheck=JSON.parse(execFileSync(validator,['validate','1'],{input:panelPut,encoding:'utf8'}));assert(panelCheck.ok,'panel-saved bank with explicitly stale source must pass handler validation');assert(panelRecord.audio.source.includes('VOLUME C0'),'panel SAVE metadata must preserve the stale source for later recovery');
  // This is the exact pair consumed by panel LOAD; confirm the untouched VIDEO bank and selected AUDIO data survive.
  assert.strictEqual(records.get(1).record.video.hex,panelRecord.video.hex);assert.strictEqual(panelRecord.audio.hex.slice(0,8),'00181100');
  await document.getElementById('cards').children[1].onclick();await tick();assert.strictEqual(document.getElementById('rev').textContent,'revision 3');
  await document.getElementById('delete').onclick();await tick();await tick();assert(!records.has(1),'delete must affect only selected slot');
  assert(calls.some(x=>x.url==='/api/v1/slots/1'&&x.method==='DELETE'));
  failSlots=true;await sandbox.refreshSlots();assert(document.getElementById('globalError').textContent.includes('síťový výpadek'));
  hangRequests=true;const pending=sandbox.requestJson('Status test','/api/v1/status');await tick();timers[timers.length-1]();await assert.rejects(pending,/HTTP timeout po 3000 ms/);hangRequests=false;
  failSlots=false;failLog=true;await sandbox.pollLog();assert(document.getElementById('monitorError').textContent.includes('síťový výpadek'));
  failLog=false;logResponses.push({bootId:7,oldest:1,next:2,lost:false,entries:[{seq:1,ms:10,type:'MODE_CHANGE',text:'RUN=STOP'},{seq:2,ms:20,type:'RAM_WRITE',text:'old=00 new=01'}]});await sandbox.pollLog();const once=document.getElementById('logs').textContent;assert(once.includes('1 10ms [MODE_CHANGE]')&&once.includes('2 20ms [RAM_WRITE]'));
  logResponses.push({bootId:7,oldest:1,next:2,lost:false,entries:[{seq:1,ms:10,type:'MODE_CHANGE',text:'RUN=STOP'},{seq:2,ms:20,type:'RAM_WRITE',text:'old=00 new=01'}]});await sandbox.pollLog();assert.strictEqual(document.getElementById('logs').textContent,once,'same cursor page must not duplicate entries');
  logResponses.push({bootId:8,oldest:9,next:9,lost:true,entries:[{seq:9,ms:2,type:'BOOT',text:'new boot'}]});await sandbox.pollLog();assert(document.getElementById('logs').textContent.includes('restart zařízení; nový bootId 8')&&document.getElementById('logs').textContent.includes('[část záznamů se ztratila; pokračuji od 9]')&&document.getElementById('logs').textContent.includes('9 2ms [BOOT]'));
  assert(document.getElementById('editor').className.includes('open'),'monitor failure must not close/block the slot editor');
  assert(calls.every(x=>x.headers['X-byteREEL-Client-Request-Id']&&x.headers['X-byteREEL-Client-Request-Id'].startsWith('ui-')),'each frontend request must carry a correlatable request ID');
  console.log('PASS frontend-to-C++ validation integration: slot 1 -> empty slot 2, delayed open/assembly guards, live C++ assembly and source/bytecode validation, padded payload, repeat/read, HEX/panel stale-source preservation, indices 1..8, DELETE and monitor errors/cursors');
})().catch(e=>{console.error(e);process.exitCode=1;});
