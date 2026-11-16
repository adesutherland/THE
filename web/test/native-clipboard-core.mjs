// Real native WebSocket adapter; system clipboard permissions are tested in UI.
import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import {mkdtemp,readFile,writeFile,rm} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import {resolve,join} from 'node:path';
import {createInterface} from 'node:readline';
import WebSocket from 'ws';

const [binary,assets]=process.argv.slice(2).map(value=>resolve(value));
const directory=await mkdtemp(join(tmpdir(),'the-native-clipboard-'));
const source=join(directory,'sample.txt');
const original=Buffer.from(['alpha','beta',''].join(process.platform==='win32'?'\r\n':'\n'));
await writeFile(source,original);
const server=spawn(binary,['--driver','web','-n',source],{cwd:directory,
  env:{...process.env,THE_REXX_INTERPRETER:'NONE',THE_WEB_ROOT:assets,
    THE_WEB_WORKSPACE:directory,THE_TOOL_CONFIG_DIR:join(directory,'config')},stdio:['ignore','pipe','pipe']});
let socket,errors='',serial=0;
const messages=[];
server.stderr.on('data',chunk=>{errors+=chunk;});
async function until(check,label){
  const end=Date.now()+10000;
  while(Date.now()<end){const result=await check();if(result)return result;await new Promise(resolve=>setTimeout(resolve,10));}
  throw Error(`${label}: ${errors}\n${JSON.stringify(messages.slice(-3))}`);
}
try{
  const url=await new Promise((resolve,reject)=>{
    const timer=setTimeout(()=>reject(Error('No native web URL: '+errors)),10000);
    const lines=createInterface({input:server.stdout});
    lines.on('line',line=>{if(line.startsWith('THE web UI: ')){clearTimeout(timer);resolve(line.slice(12).trim());}});
    server.once('exit',code=>{if(code){clearTimeout(timer);reject(Error('Native web exited '+code+': '+errors));}});
  });
  const ws=new URL(url);ws.protocol='ws:';ws.pathname='/ws';
  socket=new WebSocket(ws);
  socket.on('message',bytes=>messages.push(JSON.parse(bytes.toString())));
  await new Promise((resolve,reject)=>{socket.once('open',resolve);socket.once('error',reject);});
  await until(()=>messages.find(message=>message.screen_rows),'initial snapshot');
  async function action(action,argument){
    const id=++serial;socket.send(JSON.stringify({id,type:'action',action,argument}));
    await until(()=>messages.find(message=>message.type==='ack'&&message.id===id),'action '+action);
  }
  async function clipboard(operation,extra={},requestId='clipboard-'+(++serial),escapedUnicode=false){
    let json=JSON.stringify({type:'clipboard',version:1,operation,requestId,...extra});
    if(escapedUnicode)json=json.replace(/[\u007f-\uffff]/g,c=>'\\u'+c.charCodeAt(0).toString(16).padStart(4,'0'));
    socket.send(json);
    return until(()=>messages.find(message=>message.type==='clipboard'&&message.requestId===requestId),'clipboard '+operation);
  }
  const id=++serial;
  socket.send(JSON.stringify({id,type:'command',command:'cursor goto 1 1'}));
  await until(()=>messages.find(message=>message.screen_rows&&message.focus?.line===1),'file caret');
  assert.equal((await clipboard('selectAll')).rc,0);
  const escaped='"\n'.repeat(64);
  const copied=await clipboard('copy',{},escaped);
  assert.equal(copied.rc,0);assert.equal(copied.requestId,escaped);
  assert.equal(copied.text,'alpha\nbeta');
  assert.equal((await clipboard('cut',{token:copied.token})).rc,0);
  assert.equal((await clipboard('undo')).rc,0);
  await action('file.save');
  await until(async()=>(await readFile(source)).equals(original),'Undo exact saved bytes');
  const captured=await clipboard('capture');
  assert.equal(captured.rc,0);
  const large=Array.from({length:1300},(_,i)=>`${i}: 界Ω ${'x'.repeat(90)}`).join('\n');
  const pasted=await clipboard('paste',{token:captured.token,text:large});
  assert.equal(pasted.rc,0,JSON.stringify(pasted));
  assert.equal((await clipboard('undo')).rc,0);
  assert.equal((await clipboard('copy')).text,'alpha\nbeta');
  const unicodeCapture=await clipboard('capture');
  const escapedReply=await clipboard('paste',{token:unicodeCapture.token,text:'Ωé🙂\nJSON Unicode'},'escaped-Ω🙂',true);
  assert.equal(escapedReply.rc,0,JSON.stringify(escapedReply));
  assert.equal(escapedReply.requestId,'escaped-Ω🙂');
  assert.equal((await clipboard('selectAll')).rc,0);
  assert.equal((await clipboard('copy')).text,'Ωé🙂\nJSON Unicode');
  assert.equal((await clipboard('undo')).rc,0);
  const invalidCapture=await clipboard('capture');
  assert.notEqual((await clipboard('paste',{token:invalidCapture.token,text:'bad\u0000data'})).rc,0);
  assert.notEqual((await clipboard('paste',{token:invalidCapture.token,text:'bad\ud800data'})).rc,0);
  assert.notEqual((await clipboard('paste',{token:invalidCapture.token,text:'x'.repeat(1024*1024+1)})).rc,0);
  assert.equal((await clipboard('paste',{token:invalidCapture.token,text:''})).rc,0);
  assert.equal((await clipboard('copy')).text,'alpha\nbeta');
  const stale=await clipboard('copy');
  await clipboard('capture');
  assert.equal((await clipboard('cut',{token:stale.token})).code,'stale-revision');
  await action('file.save');
  await until(async()=>(await readFile(source)).equals(original),'refused requests preserve exact bytes');
  socket.send(JSON.stringify({type:'action',action:'file.close',id:++serial}));
  await until(()=>server.exitCode!==null,'native web close');
  assert.equal(server.exitCode,0,errors);
  console.log('PASS native clipboard bridge: escaped receipts, Unicode large paste/Undo, stale/invalid/oversize refusal and exact saved bytes');
}finally{
  socket?.close();
  if(server.exitCode===null){server.kill('SIGTERM');await new Promise(resolve=>server.once('exit',resolve));}
  await rm(directory,{recursive:true,force:true,maxRetries:3});
}
