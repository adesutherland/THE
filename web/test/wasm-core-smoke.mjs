import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { pathToFileURL } from "node:url";
import { resolve } from "node:path";

const directory = resolve(process.argv[2] ?? "../build-wasm");
const { default: createEditor } = await import(pathToFileURL(`${directory}/crxthe_wasm.mjs`));
const data = await readFile(`${directory}/crxthe_wasm.data`);
const snapshots = [];
const messages = [];
const requests = [];
let wake;
let saves = 0;
const editor = await createEditor({
  noInitialRun: true,
  wasmBinary: await readFile(`${directory}/crxthe_wasm.wasm`),
  getPreloadedPackage: () => data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength),
  sendEditorMessage(message) { messages.push(message); if (message.screen_rows) snapshots.push(message); },
  nextEditorRequest() { return requests.length ? Promise.resolve(requests.shift()) : new Promise((resolve) => { wake = resolve; }); },
  async flushWorkspace() { saves++; },
});
editor.FS.mkdir('/workspace');
editor.FS.writeFile('/workspace/Welcome.txt', 'initial\n');
editor.FS.chdir('/workspace');
const hasCrexx = editor.ccall('the_wasm_has_crexx','number',[],[],{});
for (const [name, value] of Object.entries({
  HOME: '/home/web_user', THE_HOME_DIR: '/app/', THE_WEB_WORKSPACE: '/workspace',
  THE_WEB_READONLY_ROOTS: '/app',
  THE_WEB_PROFILE: hasCrexx ? '/app/browser.rxbin' : '/app/browser.commands',
  THE_WEB_MACRO_PATH: '/app:/workspace',
  THE_REXX_INTERPRETER: hasCrexx ? 'CREXX' : 'NONE',
})) editor.ccall('setenv', 'number', ['string', 'string', 'number'], [name, value, 1], {});
const run = editor.ccall('the_wasm_main', 'number', [], [], { async: true });
function send(request) {
  if (wake) { const resume = wake; wake = undefined; resume(request); }
  else requests.push(request);
}
async function until(check, label) {
  const end = Date.now() + 10000;
  while (Date.now() < end) {
    const snapshot = snapshots.at(-1);
    if (snapshot && check(snapshot)) return snapshot;
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  throw new Error(`Timeout: ${label}\n${JSON.stringify(snapshots.at(-1))}\n${JSON.stringify(messages.slice(-3))}`);
}
await until((s) => s.buffer?.path === '/workspace/Welcome.txt', 'startup');
assert.equal(messages.find((m) => m.type === 'hello')?.driver, 'wasm');
send({ type: 'command', command: 'cursor goto 1 1' });
send({ type: 'text', text: 'é界' });
await until((s) => s.buffer?.dirty && s.screen_rows.some((r) => r.t?.includes('é界initial')), 'UTF-8 editing');
send({ type: 'action', action: 'file.save' });
await until((s) => !s.buffer.dirty, 'save');
assert.ok(saves > 0);
assert.equal(editor.FS.readFile('/workspace/Welcome.txt', { encoding: 'utf8' }), 'é界initial\n');
send({type: 'command', command: 'input prefix-probe'});
await until((s) => s.buffer.lines === 2, 'input line');
send({type: 'key', key: 'F4'});
await until((s) => s.focus.zone === 'prefix', 'prefix focus');
send({type: 'text', text: 'd'});
await until((s) => s.screen_rows.some((r) => r.p?.trim() === 'd'), 'pending prefix');
send({type: 'key', key: 'enter'});
await until((s) => s.buffer.lines === 1, 'prefix delete');
send({type: 'action', action: 'file.save'});
await until((s) => !s.buffer.dirty, 'save prefix result');
assert.equal(editor.FS.readFile('/workspace/Welcome.txt', {encoding: 'utf8'}), 'é界initial\n');
if (hasCrexx) {
  send({type: 'key', key: 'C-f'});
  await until(s => s.command === 'locate /' && s.focus.zone === 'command', 'cREXX quickfind callback');
  send({type:'action', action:'edit.undo'});
  await until(s => s.command === '', 'clear macro command');
  send({type:'key',key:'F6'});
  send({type:'command',command:'cursor goto 1 1'});
  send({type:'key',key:'F7'});
  await until(s => !!s.panel && s.panel.fields.some(f => f.text.includes('Guest')), 'cREXX READ panel');
  send({type:'text',text:'Ada'});
  await until(s => s.panel?.fields.some(f => f.changed && f.text.startsWith('Ada')), 'editable READ field');
  send({type:'key',key:'enter'});
  const submitted = await until(s => !s.panel && s.status.includes('PANEL_RC 0') && s.status.includes('Ada'), 'cREXX READ stem exchange/resume');
  assert.equal(submitted.focus.zone, 'filearea');
  assert.equal(submitted.focus.line, 1);
  assert.equal(submitted.focus.cell, 0);
  assert.equal(submitted.buffer.dirty, 0);
  send({type:'key',key:'F7'});
  await until(s => !!s.panel, 'repeated cREXX READ');
  send({type:'key',key:'esc'});
  await until(s => !s.panel && s.status.includes('PANEL_RC'), 'cREXX READ cancel/resume');
  const eofRow = snapshots.at(-1).screen_rows.find(r => r.role === 'eof');
  send({type:'hit',target:'filearea',line:eofRow.line,row:eofRow.r,cell:0});
  await until(s => s.focus.line === 2, 'EOF focus');
  send({type:'key',key:'F7'});
  await until(s => !!s.panel, 'READ from EOF');
  send({type:'key',key:'enter'});
  const eof = await until(s => !s.panel && s.status.includes('PANEL_RC 0') && s.focus.line === 2, 'READ restores EOF');
  assert.equal(eof.focus.line, 2);
  assert.equal(editor.FS.readFile('/workspace/Welcome.txt',{encoding:'utf8'}),'é界initial\n');
}
send({type: 'key', key: 'F1'});
await until((s) => s.buffer.path.endsWith('THE_Help.txt'), 'read-only bundled help');
send({type: 'action', action: 'file.close'});
await until((s) => s.buffer.path === '/workspace/Welcome.txt', 'return from help');
send({ type: 'action', action: 'file.close' });
assert.equal(await Promise.race([run, new Promise((_, reject) => setTimeout(() => reject(new Error('shutdown timeout')), 3000))]), 0);
console.log(`PASS Wasm core: ${hasCrexx ? 'cREXX profile/ADDRESS/READ' : 'command profile'}, Unicode editing, save callback and clean shutdown`);
