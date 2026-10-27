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
for (const [name, value] of Object.entries({
  HOME: '/home/web_user', THE_HOME_DIR: '/app/', THE_WEB_WORKSPACE: '/workspace',
  THE_WEB_READONLY_ROOTS: '/app',
  THE_WEB_PROFILE: '/app/browser.commands',
  THE_REXX_INTERPRETER: 'NONE',
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
send({type: 'key', key: 'F1'});
await until((s) => s.buffer.path.endsWith('THE_Help.txt'), 'read-only bundled help');
send({type: 'action', action: 'file.close'});
await until((s) => s.buffer.path === '/workspace/Welcome.txt', 'return from help');
send({ type: 'action', action: 'file.close' });
assert.equal(await Promise.race([run, new Promise((_, reject) => setTimeout(() => reject(new Error('shutdown timeout')), 3000))]), 0);
console.log('PASS Wasm core: command profile, Unicode editing, save callback and clean shutdown');
