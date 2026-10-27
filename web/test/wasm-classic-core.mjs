import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {ByteInput} from '../vm/byte-input.mjs';
import {FramedInput} from '../vm/classic-host.mjs';

const directory = resolve(process.argv[2] ?? 'build-wasm-classic');
const {default: createEditor} = await import(pathToFileURL(`${directory}/crxthe_wasm.mjs`));
const {default: createVM} = await import(pathToFileURL(`${directory}/site/vm/crxthe_rxvm.mjs`));
const input = new ByteInput(), replies = new FramedInput();
const vm = await createVM({noInitialRun: true,
  vmStdinRead: n => input.read(n), vmStreamWrite: (fd, bytes) => {if (fd === 1) replies.write(bytes);},
});
assert.equal(vm._the_vm_create(), 0);
const image = await readFile(`${directory}/site/vm/modules/dslsh-classic.rxbin`);
const pointer = vm._malloc(image.length);
vm.HEAPU8.set(image, pointer);
assert.equal(vm._the_vm_load(pointer, image.length), 0); vm._free(pointer);
const args = vm._malloc(12);
vm.HEAPU32[args >>> 2] = args + 4;
vm.HEAPU8.set(new TextEncoder().encode('--stdio\0'), args + 4);
const parserRun = vm.ccall('the_vm_run', 'number', ['number', 'number'], [1, args], {async: true});
const snapshots = [], jobs = [], forgotten = [], requests = [], messages = [];
let wake;
const data = await readFile(`${directory}/crxthe_wasm.data`);
const editor = await createEditor({noInitialRun: true,
  wasmBinary: await readFile(`${directory}/crxthe_wasm.wasm`),
  getPreloadedPackage: () => data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength),
  sendEditorMessage: message => {messages.push(message);if (message.screen_rows) snapshots.push(message);},
  requestHighlight: job => jobs.push(job), forgetHighlight: id => forgotten.push(id),
  nextEditorRequest: () => requests.length ? Promise.resolve(requests.shift()) : new Promise(resolve => {wake = resolve;}),
  flushWorkspace: async () => {},
});
const source = "/* λ */\nsay '😀' 42\nsay reverse('ab')\n";
editor.FS.mkdir('/workspace'); editor.FS.chdir('/workspace');
editor.FS.writeFile('/workspace/Welcome.txt', 'welcome\n');
editor.FS.writeFile('/workspace/Alpha.rexx', source);
editor.FS.writeFile('/workspace/Beta.rexx', "say 'second'\n");
editor.FS.writeFile('/workspace/Refused.rexx', "options levelc srcmap\nsay 'mapped'\n");
editor.FS.writeFile('/workspace/Empty.rexx', '');
for (const [name, value] of Object.entries({HOME: '/home/web_user', THE_HOME_DIR: '/app/',
  THE_WEB_WORKSPACE: '/workspace', THE_WEB_READONLY_ROOTS: '/app', THE_WEB_PROFILE: '/app/browser.rxbin',
  THE_WEB_MACRO_PATH: '/app:/workspace', THE_REXX_INTERPRETER: 'CREXX'}))
  editor.ccall('setenv', 'number', ['string','string','number'], [name,value,1], {});
const run = editor.ccall('the_wasm_main', 'number', [], [], {async: true});
const send = message => {if (wake) {const resume = wake; wake = undefined; resume(message);} else requests.push(message);};
async function until(check, label) {
  const end = Date.now() + 10000;
  while (Date.now() < end) {
    const latest = snapshots.at(-1);
    if (latest && check(latest)) return latest;
    await new Promise(resolve => setTimeout(resolve, 10));
  }
  throw new Error(`${label}: ${JSON.stringify(snapshots.at(-1))}`);
}
async function parse(job) {
  const payload = `I|${job.revision}|${job.lines}|656469746f72|${Buffer.from(job.source).toString('hex')}|`;
  input.write(new TextEncoder().encode(payload.length.toString(16).padStart(8,'0') + payload));
  return {type:'parser-result', id:job.id, revision:job.revision, payload:await replies.reply(), failed:0};
}
const latestJob = () => jobs.at(-1);
await until(s => s.buffer.path.endsWith('Welcome.txt'), 'startup');
send({type:'action',action:'file.open',argument:'/workspace/Alpha.rexx'});
await until(s => s.buffer.path.endsWith('Alpha.rexx') && s.parser?.state === 'pending', 'Alpha pending');
const initial = latestJob(); send(await parse(initial));
let snapshot = await until(s => s.parser?.state === 'ready', 'initial parse');
const row = snapshot.screen_rows.find(r => r.line === 2);
assert.ok(row.s.some(([start, length, style]) => start === 4 && length === 3 && style === 'string'), JSON.stringify(row));
assert.ok(row.s.some(([start, length, style]) => start === 8 && length === 2 && style === 'number'), JSON.stringify(row));
assert.equal(snapshot.buffer.dirty, 0);
assert.equal(snapshot.diagnostics?.length ?? 0, 0);
send({type:'command',command:'cursor goto 2 1'});
send({type:'text',text:'x'});
await until(s => s.parser.state === 'pending' && s.parser.revision > initial.revision, 'first edit');
const staleJob = latestJob();
send({type:'text',text:'y'});
await until(s => s.parser.revision > staleJob.revision, 'second edit');
const currentJob = latestJob();
send(await parse(staleJob));
snapshot = await until(s => s.parser.stale > 0, 'stale result rejected');
assert.equal(snapshot.parser.applied, 0);
assert.equal(snapshot.parser.state, 'pending');
assert.equal(snapshot.buffer.dirty, 1);
assert.equal(editor.FS.readFile('/workspace/Alpha.rexx',{encoding:'utf8'}), source);
if (editor.ccall('the_wasm_has_interpreter','number',[],[],{})) {
  send({type:'run-source',id:91});
  await until(()=>messages.some(m=>m.type==='run-source'&&m.request===91),'run source capture');
  const captured=messages.find(m=>m.type==='run-source'&&m.request===91);
  assert.equal(new TextDecoder().decode(Uint8Array.from(captured.source)),source.replace("say '😀'", "xysay '😀'"));
  assert.equal(captured.filename,'/workspace/Alpha.rexx');
  assert.equal(captured.id,snapshots.at(-1).source.id);
  assert.equal(captured.revision,snapshots.at(-1).source.revision);
  assert.equal(snapshots.at(-1).buffer.dirty,1);
  assert.equal(snapshots.at(-1).focus.line,2);
  assert.equal(snapshots.at(-1).focus.cell,2);
  assert.equal(editor.FS.readFile('/workspace/Alpha.rexx',{encoding:'utf8'}),source);
}
send(await parse(currentJob));
await until(s => s.parser.state === 'ready' && s.parser.applied === currentJob.revision, 'current result');
send({type:'action',action:'edit.undo'});
await until(s => s.parser.revision > currentJob.revision, 'undo working record');
let job = latestJob(); send(await parse(job));
await until(s => s.parser.state === 'ready' && !s.buffer.dirty, 'undo parse');
send({type:'text',text:'['});
await until(s => s.parser.revision > job.revision, 'syntax damage');
job = latestJob(); send(await parse(job));
snapshot = await until(s => s.parser.state === 'ready' && s.diagnostics?.some(d => d.code === 'CLASSIC_INPUT'), 'located diagnostic');
assert.equal(snapshot.diagnostics[0].line, 2);
assert.equal(snapshot.diagnostics[0].severity, 'ERROR');
send({type:'action',action:'edit.undo'});
await until(s => s.parser.revision > job.revision, 'repair syntax');
job = latestJob(); send(await parse(job));
await until(s => s.parser.state === 'ready' && !(s.diagnostics?.length), 'diagnostics cleared');
send({type:'action',action:'file.open',argument:'/workspace/Beta.rexx'});
await until(s => s.buffer.path.endsWith('Beta.rexx') && s.parser.id !== initial.id, 'buffer identity');
const beta = latestJob();
send({...await parse(beta), payload:'999999999999999999999|'});
await until(s => s.parser.state === 'refused', 'malformed reply refused safely');
send({type:'parser-restart'});
await until(s => s.parser.state === 'pending' && s.parser.revision > beta.revision, 'restart');
const restarted = latestJob();
send({type:'action',action:'file.close'});
await until(s => !s.buffer.path.endsWith('Beta.rexx'), 'close pending document');
assert.ok(forgotten.includes(beta.id));
send(await parse(restarted));
send({type:'snapshot'});
await until(s => s.parser.id !== beta.id, 'closed result discarded');
send({type:'action',action:'file.open',argument:'/workspace/Refused.rexx'});
await until(s => s.buffer.path.endsWith('Refused.rexx') && s.parser.state === 'pending', 'SRCMAP source');
send(await parse(latestJob()));
snapshot = await until(s => s.parser.state === 'refused', 'SRCMAP refusal');
assert.ok(snapshot.screen_rows.filter(r => r.role === 'file').every(r => !r.s?.length));
send({type:'action',action:'file.close'});
await until(s => !s.buffer.path.endsWith('Refused.rexx'), 'close refused source');
send({type:'action',action:'file.open',argument:'/workspace/Empty.rexx'});
await until(s => s.buffer.path.endsWith('Empty.rexx') && s.parser.state === 'pending', 'empty source');
send(await parse(latestJob()));
await until(s => s.parser.state === 'ready', 'empty parse root');
send({type:'action',action:'file.close'});
await until(s => !s.buffer.path.endsWith('Empty.rexx'), 'close empty source');
for (let i = 0; i < 2; i++) {send({type:'action',action:'file.save'}); send({type:'action',action:'file.close'});}
assert.equal(await Promise.race([run, new Promise((_, reject) => setTimeout(() => reject(new Error('shutdown')),3000))]), 0);
assert.equal(editor.FS.readFile('/workspace/Alpha.rexx', {encoding:'utf8'}), source);
input.close(); assert.equal(await parserRun, 0); assert.equal(vm._the_vm_program_rc(), 0);
vm._the_vm_destroy(); vm._free(args);
console.log('PASS Level L Wasm editor: actual parser, Unicode spans, pending-record edits, stale rejection, diagnostics/repair, buffer identity, refusal, restart and close');
