const fs = require('fs');
const vm = require('vm');
const assert = require('assert');

const header = fs.readFileSync('firmware/bytereel_2026/web_ui.h', 'utf8');
const html = header.match(/R"BRHTML\(([\s\S]*?)\)BRHTML";/)[1];
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
class Element {
  constructor(id='') { this.id=id;this.textContent='';this.value='';this.innerHTML='';this.className='';this.dataset={};this.children=[];this.handlers={};this.checked=true;this.scrollTop=0;this.scrollHeight=100;this.classList={add:x=>{if(!this.className.includes(x))this.className+=' '+x;},remove:x=>{this.className=this.className.replace(x,'');}}; }
  replaceChildren(...xs){this.children=xs;}
  append(x){this.children.push(x);}
}
const elements={};
const document={getElementById:id=>elements[id]||(elements[id]=new Element(id)),createElement:tag=>new Element(tag)};
const states=Array.from({length:8},(_,slot)=>({slot,name:'SLOT '+(slot+1),state:'EMPTY',revision:0,videoBytes:null,audioBytes:null,bankCapacity:256}));
const records=new Map(),calls=[];let failSlots=false,failLog=false,logResponses=[];
const reply=(status,data)=>({status,ok:status>=200&&status<300,text:async()=>typeof data==='string'?data:JSON.stringify(data)});
async function fetch(url,options={}){
  calls.push({url,method:options.method||'GET',body:options.body});
  if(url==='/api/v1/slots'&&failSlots)throw Error('offline');
  if(url.startsWith('/api/v1/log')&&failLog)throw Error('offline');
  if(url==='/api/v1/status')return reply(200,{project:'byteREEL 2026',bootId:7,firmwareVersion:'test-fw',uiVersion:'ui-2026.11.19-r2'});
  if(url.startsWith('/api/v1/log?'))return reply(200,logResponses.length?logResponses.shift():{bootId:7,oldest:1,next:0,lost:false,entries:[]});
  if(url==='/api/v1/instructions')return reply(200,{instructions:[]});
  if(url==='/api/v1/slots')return reply(200,states);
  if(url==='/api/v1/assemble'&&options.method==='POST'){
    const q=JSON.parse(options.body);return reply(200,{bank:q.bank,hex:'0000'.padEnd(512,'0'),bytes:2,labels:{}});
  }
  const m=url.match(/^\/api\/v1\/slots\/(\d+)$/);
  if(m){const id=Number(m[1]),r=records.get(id);
    if(!options.method||options.method==='GET')return r?reply(200,{revision:r.revision,slot:r.record}):reply(404,{code:'SLOT_EMPTY',message:'Slot je prázdný'});
    const q=JSON.parse(options.body||'{}');
    if(options.method==='PUT'){
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
const timers=[];const sandbox={document,window:{addEventListener(){}},fetch,confirm:()=>true,setTimeout:f=>{timers.push(f);return timers.length;},setInterval:()=>1,console:{log(){},error(){}},Number,String,JSON,Array,Error,Promise,Map,Math};
vm.runInNewContext(script,sandbox,{filename:'byteREEL UI'});
const tick=()=>new Promise(resolve=>setImmediate(resolve));
(async()=>{
  for(let i=0;i<6;i++)await tick();
  assert.strictEqual(document.getElementById('cards').children.length,8,'eight card click targets must be registered');
  assert.strictEqual(typeof document.getElementById('cards').children[0].onclick,'function');
  await document.getElementById('cards').children[0].onclick();await tick();
  assert(document.getElementById('editor').className.includes('open'));
  assert.strictEqual(document.getElementById('rev').textContent,'revision 0 · PRÁZDNÝ');
  await document.getElementById('save').onclick();await tick();await tick();
  assert(records.has(0),'first empty-slot save must PUT the selected zero-based slot');
  assert.strictEqual(records.get(0).record.slot,0);assert.strictEqual(records.get(0).revision,1);
  document.getElementById('vsrc').value='NOP';
  await document.getElementById('save').onclick();await tick();await tick();
  assert.strictEqual(records.get(0).revision,2,'repeat save must use latest revision');
  assert(calls.some(x=>x.url==='/api/v1/assemble'&&x.method==='POST'));
  assert(calls.some(x=>x.url==='/api/v1/slots/0'&&x.method==='PUT'));
  assert.strictEqual(states[0].state,'OK','successful PUT must refresh card state');
  await document.getElementById('cards').children[0].onclick();await tick();
  assert.strictEqual(document.getElementById('rev').textContent,'revision 2');
  await document.getElementById('delete').onclick();await tick();await tick();
  assert(!records.has(0),'delete must affect only selected slot');
  assert(calls.some(x=>x.url==='/api/v1/slots/0'&&x.method==='DELETE'));
  failSlots=true;await sandbox.refreshSlots();assert(document.getElementById('globalError').textContent.includes('síťový výpadek'));
  failSlots=false;failLog=true;await sandbox.pollLog();assert(document.getElementById('monitorError').textContent.includes('síťový výpadek'));
  failLog=false;logResponses.push({bootId:7,oldest:1,next:2,lost:false,entries:[{seq:1,ms:10,type:'MODE_CHANGE',text:'RUN=STOP'},{seq:2,ms:20,type:'RAM_WRITE',text:'old=00 new=01'}]});await sandbox.pollLog();const once=document.getElementById('logs').textContent;assert(once.includes('1 10ms [MODE_CHANGE]')&&once.includes('2 20ms [RAM_WRITE]'));
  logResponses.push({bootId:7,oldest:1,next:2,lost:false,entries:[{seq:1,ms:10,type:'MODE_CHANGE',text:'RUN=STOP'},{seq:2,ms:20,type:'RAM_WRITE',text:'old=00 new=01'}]});await sandbox.pollLog();assert.strictEqual(document.getElementById('logs').textContent,once,'same cursor page must not duplicate entries');
  logResponses.push({bootId:8,oldest:9,next:9,lost:true,entries:[{seq:9,ms:2,type:'BOOT',text:'new boot'}]});await sandbox.pollLog();assert(document.getElementById('logs').textContent.includes('restart zařízení; nový bootId 8')&&document.getElementById('logs').textContent.includes('[část záznamů se ztratila; pokračuji od 9]')&&document.getElementById('logs').textContent.includes('9 2ms [BOOT]'));
  assert(document.getElementById('editor').className.includes('open'),'monitor failure must not close/block the slot editor');
  console.log('PASS frontend integration adapter: page JS, eight card listeners, GET-empty/revision 0, assemble, PUT, GET-revision, DELETE, visible errors, cursor dedupe, loss and boot reset');
})().catch(e=>{console.error(e);process.exitCode=1;});
