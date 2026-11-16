import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import {mkdtemp, readFile, rm, writeFile, access} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import {resolve, dirname, join, delimiter} from 'node:path';
import {fileURLToPath} from 'node:url';
import WebSocket from 'ws';

const binary = resolve(process.argv[2]), assets = resolve(process.argv[3]);
const rxc = process.env.THE_CREXX_RXC ?? resolve(dirname(fileURLToPath(import.meta.url)), '../../../CREXX/cmake-build-debug/bin/rxc');
try { await access(rxc); } catch { console.log('SKIP native parser status: rxc unavailable'); process.exit(77); }
const root = await mkdtemp(join(tmpdir(), 'THE native parser status '));
const sample = join(root,'sample.crexx'), profile = join(root,'profile.the');
const original = "say 'saved'\n";
await writeFile(sample,original);
await writeFile(profile,"options levelb\naddress the\n" + [
  'set sdslh rxc rxc --syntaxhighlight', 'set autocolor *.crexx rxc', 'set coloring on auto',
  'set statusline bottom', 'set statopt off *', 'set statopt on pmsg.1 15 38',
  'set clock on', 'set hexdisplay on', 'set color cberror white on firebrick',
  'set color pmsgerror white on firebrick',
].map(command => `'${command}';\n`).join(''));
const server = spawn(binary,['--driver','web',sample],{env:{...process.env,
  THE_REXX_INTERPRETER:'CREXX', THE_WEB_ROOT:assets, THE_WEB_WORKSPACE:root,
  THE_WEB_PROFILE:profile, THE_HOME_DIR:join(dirname(binary),'release'),
  THE_DRIVER_PATH:dirname(binary), THE_CREXX_CACHE_DIR:join(root,'cache'),
  THE_TOOL_CONFIG_DIR:join(root,'tools'), PATH:dirname(rxc) + delimiter + process.env.PATH,
},stdio:['ignore','pipe','pipe']});
let stdout='',stderr='',snapshot,socket,id=0,lastRequest=0;
server.stdout.on('data',data=>{stdout+=data;}); server.stderr.on('data',data=>{stderr+=data;});
async function until(check,label) {
  const deadline=Date.now()+15000;
  while(Date.now()<deadline) {
    if(check()) return snapshot;
    if(server.exitCode!==null) throw new Error(`${label}: server exited ${server.exitCode}\n${stderr}`);
    if(socket?.readyState===WebSocket.OPEN && Date.now()-lastRequest>100) {
      lastRequest=Date.now();socket.send(JSON.stringify({v:1,id:++id,type:'snapshot'}));
    }
    await new Promise(resolve=>setTimeout(resolve,10));
  }
  throw new Error(`${label}\n${JSON.stringify(snapshot)}\n${stderr}`);
}
const send=message=>socket.send(JSON.stringify({v:1,id:++id,...message}));
try {
  await until(()=>stdout.includes('THE web UI: '),'native server URL');
  const url=new URL(stdout.split('THE web UI: ')[1].split(/\r?\n/)[0]);
  url.protocol='ws:'; url.pathname='/ws'; socket=new WebSocket(url);
  socket.on('message',data=>{const message=JSON.parse(data);if(message.screen_rows)snapshot=message;});
  await new Promise((resolve,reject)=>{socket.once('open',resolve);socket.once('error',reject);});
  await until(()=>snapshot?.buffer.path.endsWith('sample.crexx'),'initial native snapshot');
  send({type:'command',command:'set eolout lf'});
  send({type:'command',command:'cursor goto 1 1'});
  send({type:'command',command:"replace say 'unterminated"});
  await until(()=>snapshot?.buffer.dirty && snapshot.diagnostics?.some(d=>d.severity==='ERROR'),'native real quote diagnostic');
  send({type:'command',command:'cursor goto 1 5'});
  await until(()=>snapshot?.focus.cell===4 && snapshot.status_pmsg?.severity==='ERROR','native PMSG before first key');
  for(const cols of [40,160,40,80]) {
    send({type:'resize',rows:24,cols});
    const state=await until(()=>snapshot?.viewport.cols===cols && snapshot.status_pmsg?.severity==='ERROR','native narrow PMSG');
    const message=state.status_pmsg;
    const visible=Array.from(state.status_line).slice(message.start,message.start+message.length).join('');
    assert.equal(visible,Array.from(message.text).slice(0,message.length).join(''));
    assert.match(visible,/quote|string|unterminated|input/i,'narrow native PMSG contains the explanation');
    assert.ok(!visible.includes('RXC-') && !visible.includes('LL_COMPILER'));
    assert.equal(state.focus.cell,4); assert.equal(state.buffer.dirty,1);
    assert.equal(state.screen_rows.find(row=>row.role==='file'&&row.line===1).t,"say 'unterminated");
    assert.equal(await readFile(sample,'utf8'),original,'rendering and resizing do not save');
  }
  send({type:'command',command:'cursor goto 1 1'});
  await until(()=>snapshot?.focus.cell===0 && snapshot.status_pmsg===null,'native cursor leave clears PMSG');
  assert.ok(snapshot.status_line.includes('U+73'),'hex field resumes when PMSG no longer occupies it');
  send({type:'command',command:"replace say 'saved'"});
  await until(()=>!snapshot?.diagnostics?.length && snapshot?.status_pmsg===null,'native quote repair');
  send({type:'action',action:'file.save'}); send({type:'action',action:'file.close'});
  await new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(new Error('native shutdown')),5000);server.once('exit',code=>{clearTimeout(timer);code===0?resolve():reject(new Error(stderr));});});
  assert.equal(await readFile(sample,'utf8'),original);
  console.log('PASS native parser status: no-key startup, 40/160 columns, useful PMSG, cursor leave/repair and unchanged saved bytes');
} finally {
  socket?.close();
  if(server.exitCode===null) {server.kill();await new Promise(resolve=>server.once('exit',resolve));}
  await rm(root,{recursive:true,force:true});
}
