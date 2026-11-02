import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { pathToFileURL } from "node:url";
import { resolve } from "node:path";
import { checkCommandPolicy, checkEditorCommands } from "./command-policy.mjs";

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
const policyText = Array.from({length: 80}, (_, index) => `safe ${index + 1}\n`).join('');
editor.FS.writeFile('/workspace/Policy.txt', policyText);
editor.FS.chdir('/workspace');
const hasCrexx = editor.ccall('the_wasm_has_crexx','number',[],[],{});
if (hasCrexx) editor.FS.writeFile('/workspace/pfx.rxbin', editor.FS.readFile('/app/paneldemo.rxbin'));
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
const initialSnapshot = await until((s) => s.buffer?.path === '/workspace/Welcome.txt', 'startup');
const initialConsoleColours = initialSnapshot.console_colours;
assert.equal(Object.keys(initialConsoleColours ?? {}).length, 8, 'Wasm startup exposes all console colour areas');
const workbenchHello = messages.find((m) => m.type === 'hello');
assert.equal(workbenchHello?.driver, 'wasm');
assert.match(workbenchHello.workbenchSession, /^[0-9a-f]{24}$/);
let workbenchSequence = workbenchHello.workbenchNextRequest;
const nextWorkbenchRequest = () => `${workbenchHello.workbenchSession}.${workbenchSequence++}`;
const workbenchPage = (section) => messages.filter((m) => m.type === 'workbench' && m.operation === 'list' && m.data?.section === section).at(-1)?.data;
const workbenchResult = (requestId) => messages.filter((m) => m.type === 'workbench' && m.operation === 'result' && m.requestId === requestId && m.state === 'complete').at(-1);
assert.ok(workbenchPage('actions')?.records.some((action) => action.id === 'the.file.save'), 'Wasm publishes builtin actions');
const preservedWorkbenchEditor = { ...initialSnapshot.focus, dirty: initialSnapshot.buffer.dirty };
send({type:'command',command:'workbench begin owner user.wasm'});
send({type:'command',command:'workbench action put user.save label "Safe Save" builtin file.save'});
send({type:'command',command:'workbench menu put user.tools label "Tools" order 50'});
send({type:'command',command:'workbench item put user.tools user.saveitem label "Check / <img>" action user.save'});
send({type:'command',command:'workbench commit'});
await until(() => workbenchPage('items')?.records.some((item) => item.id === 'user.saveitem'), 'Wasm profile menu published');
const firstAction = workbenchPage('actions').records.find((action) => action.id === 'user.save');
assert.ok(firstAction?.generation > 0);
assert.equal(workbenchPage('menus').records.find((menu) => menu.id === 'user.tools')?.label, 'Tools');
const firstSaveRequest = nextWorkbenchRequest();
send({type:'workbench',version:1,operation:'invoke',requestId:firstSaveRequest,action:'user.save',expectGeneration:firstAction.generation});
await until(() => !!workbenchResult(firstSaveRequest), 'Wasm semantic action result');
assert.equal(workbenchResult(firstSaveRequest)?.code, 'ok');
send({type:'workbench',version:1,operation:'invoke',requestId:firstSaveRequest,action:'the.edit.undo',expectGeneration:firstAction.generation});
await until(() => workbenchResult(firstSaveRequest)?.code === 'request-id-conflict', 'Wasm receipt payload conflict');
send({type:'command',command:'workbench begin owner user.wasm'});
send({type:'command',command:'workbench action put user.save label "Replaced Save" builtin file.save'});
send({type:'command',command:'workbench item put user.tools user.saveitem label "Replaced Save" action user.save'});
send({type:'command',command:'workbench commit'});
await until(() => workbenchPage('items')?.records.some((item) => item.id === 'user.saveitem' && item.label === 'Replaced Save'), 'Wasm menu replacement');
const secondAction = workbenchPage('actions').records.find((action) => action.id === 'user.save');
assert.ok(secondAction.generation > firstAction.generation);
const staleRequest = nextWorkbenchRequest();
send({type:'workbench',version:1,operation:'invoke',requestId:staleRequest,action:'user.save',expectGeneration:firstAction.generation});
await until(() => !!workbenchResult(staleRequest), 'Wasm stale generation result');
assert.equal(workbenchResult(staleRequest)?.code, 'stale-generation');
const nulRequest = nextWorkbenchRequest();
send({type:'workbench',version:1,operation:'invoke',requestId:nulRequest,action:'user.save\0hidden',expectGeneration:secondAction.generation});
await until(() => !!workbenchResult(nulRequest), 'Wasm NUL operand refusal');
assert.equal(workbenchResult(nulRequest)?.code, 'invalid-operand');
const builtinSaveGeneration = workbenchPage('actions').records.find((action) => action.id === 'the.file.save').generation;
for (let index = 0; index < 257; index++) {
  const requestId = nextWorkbenchRequest();
  send({type:'workbench',version:1,operation:'invoke',requestId,action:'the.file.save',expectGeneration:builtinSaveGeneration});
  await until(() => !!workbenchResult(requestId), `Wasm rolling receipt ${index}`);
  assert.equal(workbenchResult(requestId)?.code, 'ok');
}
send({type:'workbench',version:1,operation:'invoke',requestId:firstSaveRequest,action:'user.save',expectGeneration:firstAction.generation});
await until(() => workbenchResult(firstSaveRequest)?.code === 'receipt-unknown', 'evicted receipt cannot execute');
send({type:'command',command:'workbench begin owner user.wasm'});
send({type:'command',command:'workbench item remove user.tools user.saveitem'});
send({type:'command',command:'workbench action remove user.save'});
send({type:'command',command:'workbench menu remove user.tools'});
send({type:'command',command:'workbench commit'});
await until(() => !workbenchPage('menus')?.records.some((menu) => menu.id === 'user.tools'), 'Wasm menu removal');
assert.deepEqual({ ...snapshots.at(-1).focus, dirty: snapshots.at(-1).buffer.dirty }, preservedWorkbenchEditor,
  'Wasm menu definitions preserve editor focus, caret and dirty state');
const policyRevision = workbenchPage('actions').revision;
const saveReason = () => workbenchPage('actions')?.records.find((action) => action.id === 'the.file.save')?.reason;
assert.equal(saveReason(), 'ok');
editor.FS.writeFile('/app/Readonly-workbench.txt', 'read only policy\n');
send({type:'action',action:'file.open',argument:'/app/Readonly-workbench.txt'});
await until((s) => s.buffer?.path === '/app/Readonly-workbench.txt' && saveReason() === 'policy-denied', 'Wasm same-revision readonly action refresh');
assert.equal(workbenchPage('actions').revision, policyRevision, 'reason refresh keeps definition revision');
send({type:'action',action:'file.close'});
await until((s) => s.buffer?.path === '/workspace/Welcome.txt' && saveReason() === 'ok', 'Wasm same-revision writable action refresh');
send({type: 'action', action: 'file.open', argument: 'Policy.txt'});
await until(s => s.buffer?.path === '/workspace/Policy.txt', 'policy buffer');
const policyRequest = async (message, predicate = () => true) => {
  const before = snapshots.length;
  send(message);
  return until(s => snapshots.length > before && predicate(s), JSON.stringify(message));
};
await checkCommandPolicy(policyRequest, 2);
await checkEditorCommands(policyRequest);
assert.equal(editor.FS.readFile('/workspace/Command contract.txt', {encoding: 'utf8'}), 'alpha beta\ngamma\n');
assert.equal(editor.FS.readFile('/workspace/Policy.txt', {encoding: 'utf8'}), policyText);
assert.equal(editor.FS.analyzePath('/workspace/policy-escape.txt').exists, false);
send({type: 'action', action: 'file.close'});
await until(s => s.buffer?.path === '/workspace/Welcome.txt', 'return from policy buffer');
assert.deepEqual(snapshots.at(-1).console_colours, initialConsoleColours, 'file switch restores the current file console colours');
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
// Closing the final browser file must leave a usable workspace. Choose a
// fresh pathname even if an Untitled file already exists in browser storage.
editor.FS.writeFile('/workspace/Untitled-1.txt', 'existing untitled\n');
await policyRequest({type: 'command', command: 'cursor goto 1 1'});
await policyRequest({type: 'command', command: 'set insertmode on'});
await policyRequest({type: 'text', text: 'DISCARD '}, s => s.buffer.dirty);
const afterLastQquit = await policyRequest({type: 'command', command: 'qquit'});
assert.equal(afterLastQquit.buffer.path, '/workspace/Untitled-2.txt');
assert.equal(afterLastQquit.buffer.lines, 0);
assert.equal(afterLastQquit.buffer.dirty, 0);
assert.equal(afterLastQquit.buffers.length, 1);
assert.equal(editor.FS.readFile('/workspace/Welcome.txt', {encoding: 'utf8'}), 'é界initial\n');
assert.equal(editor.FS.readFile('/workspace/Untitled-1.txt', {encoding: 'utf8'}), 'existing untitled\n');
await policyRequest({type: 'command', command: 'input usable blank buffer'});
await policyRequest({type: 'command', command: 'save'}, s => !s.buffer.dirty);
assert.equal(editor.FS.readFile('/workspace/Untitled-2.txt', {encoding: 'utf8'}), 'usable blank buffer\n');
const afterLastQuit = await policyRequest({type: 'action', action: 'file.close'});
assert.equal(afterLastQuit.buffer.path, '/workspace/Untitled-3.txt');
assert.equal(afterLastQuit.buffer.lines, 0);
assert.equal(afterLastQuit.buffers.length, 1);
console.log(`PASS Wasm core: ${hasCrexx ? 'cREXX profile/ADDRESS/READ' : 'command profile'}, Unicode editing, editor commands, durable save and usable last-file close`);
process.exit(0);
