import { spawn } from "node:child_process";
import { mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import WebSocket from "ws";
import { fileFieldSmoke } from "./wasm-file-fields.mjs";
import { checkNativeToolPolicy } from "./native-tools-policy.mjs";
import { sourceFileRunnerSmoke } from "./wasm-source-file-runner.mjs";
import { wasmEditorCommandsSmoke } from "./wasm-editor-commands.mjs";
import { profileSourcesSmoke, profileActivationStorageFailureSmoke } from "./wasm-profile-sources.mjs";
import { minibasicCandidateSmoke } from "./wasm-minibasic-candidate.mjs";

const pageUrl = process.env.THE_WEB_URL;
const chromium = process.env.CHROMIUM_BIN;
if (!pageUrl || !chromium) throw new Error("THE_WEB_URL and CHROMIUM_BIN are required");

const profile = await mkdtemp(join(tmpdir(), "the-chrome-"));
const chrome = spawn(chromium, [
  "--headless=new",
  "--no-sandbox",
  "--disable-gpu",
  "--no-first-run",
  "--no-default-browser-check",
  "--disable-dev-shm-usage",
  "--remote-debugging-port=0",
  `--user-data-dir=${profile}`,
  "about:blank",
], { stdio: "ignore" });

let socket;
let workerSession;
let nextId = 1;
const pending = new Map();
const exceptions = [];
let acceptUnsavedDialogs = false;

const sleep = (milliseconds) => new Promise((resolve) => setTimeout(resolve, milliseconds));

async function waitFor(check, description, timeout = 8000) {
  const deadline = Date.now() + timeout;
  let lastError;
  while (Date.now() < deadline) {
    try {
      const value = await check();
      if (value) return value;
    } catch (error) {
      lastError = error;
    }
    await sleep(50);
  }
  throw new Error(`timed out waiting for ${description}${lastError ? `: ${lastError.message}` : ""}`);
}

function command(method, params = {}, sessionId = undefined) {
  const id = nextId++;
  return new Promise((resolve, reject) => {
    const timer=setTimeout(()=>{pending.delete(id);reject(new Error('CDP timeout: '+method));},30000);
    pending.set(id, { resolve, reject, method,timer });
    socket.send(JSON.stringify({ id, method, params, sessionId }));
  });
}

async function evaluate(expression) {
  const result = await command("Runtime.evaluate", {
    expression,
    awaitPromise: true,
    returnByValue: true,
  });
  if (result.exceptionDetails) {
    throw new Error(result.exceptionDetails.text || "browser evaluation failed");
  }
  return result.result?.value;
}

async function domKey(key, code, ctrlKey = false, shiftKey = false) {
  await evaluate(`document.activeElement.dispatchEvent(new KeyboardEvent('keydown', {key: ${JSON.stringify(key)}, code: ${JSON.stringify(code)}, ctrlKey: ${ctrlKey}, shiftKey: ${shiftKey}, bubbles: true, cancelable: true}))`);
}
async function consoleEnter(text) {
  await evaluate("document.querySelector('#rexx-input').focus()");
  await waitFor(()=>evaluate("document.activeElement?.id==='rexx-input'"),'console input focus');
  await command('Input.insertText',{text});
  await command('Input.dispatchKeyEvent',{type:'keyDown',key:'Enter',code:'Enter',windowsVirtualKeyCode:13,text:'\r'});
  await command('Input.dispatchKeyEvent',{type:'keyUp',key:'Enter',code:'Enter',windowsVirtualKeyCode:13});
}

async function reloadAcceptingUnsaved(previousGeneration) {
  acceptUnsavedDialogs = true;
  try {
    await command("Page.reload", { ignoreCache: true });
    await waitFor(() => evaluate(`window.__minibasicGeneration !== ${previousGeneration} && window.__snapshot?.buffer?.path === '/workspace/Welcome.txt'`), "browser reload after accepting unsaved changes", 30000);
  } finally {
    acceptUnsavedDialogs = false;
  }
}

async function openWorkspaceFile(name) {
  await evaluate("document.querySelector('[aria-label=\"Open\"]').click()");
  await waitFor(() => evaluate("!!document.querySelector('.root-switcher button')"), "workspace Files roots");
  await evaluate("document.querySelector('.root-switcher button').click()");
  await waitFor(() => evaluate("document.querySelector('.explorer-path > span')?.textContent==='/'"),
    "workspace Files root");
  await waitFor(
    () => evaluate(`[...document.querySelectorAll('.file-list button')].some((node) => node.textContent.trim() === ${JSON.stringify(name)})`),
    `${name} in workspace files`,
  );
  await evaluate(`[...document.querySelectorAll('.file-list button')].find((node) => node.textContent.trim() === ${JSON.stringify(name)}).click()`);
  await waitFor(
    () => evaluate(`document.querySelector('.buffer-tab-current span')?.textContent.trim() === ${JSON.stringify(name)}`),
    `${name} buffer`,
  );
}

async function selectBuffer(name) {
  await evaluate(`[...document.querySelectorAll('.buffer-tab')].find((node) => node.textContent.trim() === ${JSON.stringify(name)}).click()`);
  await waitFor(
    () => evaluate(`document.querySelector('.buffer-tab-current span')?.textContent.trim() === ${JSON.stringify(name)}`),
    `${name} buffer switch`,
  );
}

async function runCurrentFile() {
  await waitFor(() => evaluate("(() => {const file=window.__tools?.currentFile,source=window.__snapshot?.source;return file&&source&&file.sourceId===source.id&&file.revision===source.revision&&!document.querySelector('.file-action-run')?.disabled;})()"), "eligible selected-file Run");
  await evaluate("document.querySelector('.file-action-run').click()");
}

try {
  const portFile = join(profile, "DevToolsActivePort");
  const port = await waitFor(async () => {
    if (chrome.exitCode !== null) throw new Error(`Chrome exited with ${chrome.exitCode}`);
    const contents = await readFile(portFile, "utf8");
    return Number(contents.split("\n", 1)[0]) || 0;
  }, "Chrome DevTools port", 30000);
  const targetResponse = await fetch(
    `http://127.0.0.1:${port}/json/new?${encodeURIComponent("about:blank")}`,
    { method: "PUT" },
  );
  if (!targetResponse.ok) throw new Error(`unable to create Chrome target: ${targetResponse.status}`);
  const target = await targetResponse.json();

  socket = new WebSocket(target.webSocketDebuggerUrl);
  await new Promise((resolve, reject) => {
    socket.once("open", resolve);
    socket.once("error", reject);
  });
  socket.on("message", (data) => {
    const message = JSON.parse(data.toString());
    if (message.id && pending.has(message.id)) {
      const request = pending.get(message.id);
      pending.delete(message.id);clearTimeout(request.timer);
      if (message.error) request.reject(new Error(`${request.method}: ${message.error.message}`));
      else request.resolve(message.result);
    } else if (message.method === "Target.attachedToTarget" && message.params.targetInfo.type === "worker") {
      workerSession = message.params.sessionId;
    } else if (message.method === "Runtime.exceptionThrown") {
      exceptions.push(message.params?.exceptionDetails?.text || "browser exception");
    } else if (message.method === "Page.javascriptDialogOpening" && acceptUnsavedDialogs) {
      void command("Page.handleJavaScriptDialog", { accept: true }).catch(error => exceptions.push(error.message));
    }
  });

  // Creating a target may precede its active renderer on a loaded runner.
  // Attach to a ready blank page before starting the application navigation.
  await waitFor(() => command("Page.getFrameTree"), "active Chrome page", 30000);
  await command("Runtime.enable");
  await command("Page.bringToFront");
  await command("Page.enable");
  await command("Target.setAutoAttach", { autoAttach: true, waitForDebuggerOnStart: false, flatten: true });
  await command("Page.addScriptToEvaluateOnNewDocument", { source: `
    window.__messages = [];
    const OriginalWorker = Worker;
    window.Worker = class extends OriginalWorker {
      constructor(...args) {
        super(...args); if (!String(args[0]).includes('/runner/')) window.__worker = this;
        this.addEventListener('message', ({data}) => {
          window.__messages.push(data);
          if (data.screen_rows) window.__snapshot = data;
          if (data.type==='tools') window.__tools=data;
        });
      }
    };
  ` });
  await command("Page.navigate", { url: pageUrl });
  await waitFor(() => evaluate("window.__snapshot?.buffer?.path === '/workspace/Welcome.txt'"), "Wasm startup", 30000);
  await waitFor(() => evaluate("/F10\\s+Menu/.test(document.querySelector('.key-hints')?.textContent) && /Ctrl-S\\s+Save/.test(document.querySelector('.key-hints')?.textContent)"), "visible packaged profile key guide");
  if (!(await evaluate("typeof window.__snapshot.status_line === 'string' && document.querySelector('.profile-status')?.textContent === window.__snapshot.status_line && !document.querySelector('.editor-details')")))
    throw new Error("native profile status line is not visible under the browser editor");
  const send = (request) => evaluate(`window.__worker.postMessage(${JSON.stringify(request)})`);
  const snapshot = () => evaluate("window.__snapshot");
  const manifest = await (await fetch(new URL('manifest.json', pageUrl))).json();
  const nativePolicyRequest = async request => {
    const before = await evaluate("window.__messages.filter(m=>m.screen_rows).length");
    await send(request);
    await waitFor(() => evaluate(`window.__messages.filter(m=>m.screen_rows).length>${before}`), "native tool refusal snapshot");
    return snapshot();
  };
  const nativePolicyInspect = async section => {
    const before = await evaluate("window.__messages.length");
    await send({type: "workbench", version: 1, operation: "list", section, offset: 0, limit: 8});
    return waitFor(() => evaluate(`window.__messages.slice(${before}).find(m=>m.operation==='list'&&m.data?.section===${JSON.stringify(section.toLowerCase())})?.data`), "native tool refusal inspection");
  };
  await checkNativeToolPolicy(nativePolicyRequest, "unsupported-host", nativePolicyInspect);
  console.log("PASS rebuilt Wasm native TOOL/action/RUN unsupported-host refusals preserve editor");
  if (process.env.THE_WASM_COMMANDS_ONLY === "1") {
    await waitFor(() => !!workerSession, "editor Worker debugger");
    await wasmEditorCommandsSmoke({send, snapshot, evaluate, command, waitFor, workerSession});
    if (exceptions.length) throw new Error(`browser exceptions: ${exceptions.join("; ")}`);
  } else if (process.env.THE_WASM_PROFILE_SOURCES_ONLY === "1") {
    await profileSourcesSmoke({ send, evaluate, command, waitFor, snapshot, hasCrexx: manifest.capabilities.editor_address });
    if (exceptions.length) throw new Error(`browser exceptions: ${exceptions.join("; ")}`);
  } else if (process.env.THE_WASM_MINIBASIC_ONLY === "1") {
    await minibasicCandidateSmoke({pageUrl, manifest, send, snapshot, evaluate, command, waitFor, openWorkspaceFile, consoleEnter, reloadAcceptingUnsaved});
    if (exceptions.length) throw new Error(`browser exceptions: ${exceptions.join("; ")}`);
  } else if (process.env.THE_WASM_FILE_FIELDS_ONLY === "1") {
    await fileFieldSmoke({ send, evaluate, command, waitFor, snapshot });
    if (exceptions.length) throw new Error(`browser exceptions: ${exceptions.join("; ")}`);
  } else {
  await waitFor(() => !!workerSession, "editor Worker debugger");
  await wasmEditorCommandsSmoke({send, snapshot, evaluate, command, waitFor, workerSession});
  await profileSourcesSmoke({ send, evaluate, command, waitFor, snapshot, hasCrexx: manifest.capabilities.editor_address });
  const beforeWorkbench = await snapshot();
  await send({type:'command',command:'workbench begin owner user.browser'});
  await send({type:'command',command:'workbench action put user.save label "Safe Save" builtin file.save'});
  await send({type:'command',command:'workbench menu put user.tools label "Tools" order 50'});
  await send({type:'command',command:'workbench item put user.tools user.saveitem label "Check / <img>" action user.save'});
  await send({type:'command',command:'workbench commit'});
  await evaluate("document.querySelector('.menu-trigger').click()");
  await waitFor(() => evaluate("document.querySelector('[data-menu-id=\"user.tools\"] button span')?.textContent === 'Check / <img>'"), 'Wasm published menu DOM');
  if (await evaluate("!!document.querySelector('.menu-popover img')")) throw Error('Wasm menu label created markup');
  const oldWorkbenchGeneration = +(await evaluate("document.querySelector('[data-menu-id=\"user.tools\"] button').dataset.generation"));
  await evaluate("document.querySelector('[data-menu-id=\"user.tools\"] button').click()");
  await waitFor(() => evaluate("window.__messages.some(m=>m.type==='workbench'&&m.operation==='result'&&m.state==='complete'&&m.code==='ok')"), 'Wasm menu invocation result');
  await send({type:'command',command:'workbench begin owner user.browser'});
  await send({type:'command',command:'workbench action put user.save label "Replaced Save" builtin file.save'});
  await send({type:'command',command:'workbench item put user.tools user.saveitem label "Replaced Save" action user.save'});
  await send({type:'command',command:'workbench commit'});
  await evaluate("document.querySelector('.menu-trigger').click()");
  await waitFor(() => evaluate("document.querySelector('[data-menu-id=\"user.tools\"] button span')?.textContent === 'Replaced Save'"), 'Wasm replaced menu DOM');
  if (+(await evaluate("document.querySelector('[data-menu-id=\"user.tools\"] button').dataset.generation")) <= oldWorkbenchGeneration) throw Error('Wasm action generation did not advance');
  const staleWorkbenchRequest = await evaluate("(() => { const hello=window.__messages.find(m=>m.type==='hello'); const used=window.__messages.filter(m=>m.type==='workbench'&&m.operation==='result').map(m=>Number(m.requestId?.split('.').at(-1))||0); return hello.workbenchSession+'.'+(Math.max(hello.workbenchNextRequest-1,...used)+1); })()");
  await send({type:'workbench',version:1,operation:'invoke',requestId:staleWorkbenchRequest,action:'user.save',expectGeneration:oldWorkbenchGeneration});
  await waitFor(() => evaluate(`window.__messages.some(m=>m.type==='workbench'&&m.operation==='result'&&m.requestId===${JSON.stringify(staleWorkbenchRequest)}&&m.code==='stale-generation')`), 'Wasm stale action refusal');
  await send({type:'command',command:'workbench begin owner user.browser'});
  await send({type:'command',command:'workbench item remove user.tools user.saveitem'});
  await send({type:'command',command:'workbench action remove user.save'});
  await send({type:'command',command:'workbench menu remove user.tools'});
  await send({type:'command',command:'workbench commit'});
  await waitFor(() => evaluate("!document.querySelector('[data-menu-id=\"user.tools\"]')"), 'Wasm removed menu DOM');
  await evaluate("document.querySelector('.menu-trigger').click()");
  const afterWorkbench = await snapshot();
  if (JSON.stringify({focus:afterWorkbench.focus,dirty:afterWorkbench.buffer?.dirty}) !== JSON.stringify({focus:beforeWorkbench.focus,dirty:beforeWorkbench.buffer?.dirty}))
    throw Error('Wasm menu transition changed editor caret or dirty state');
  console.log('PASS browser Wasm shared workbench: add, invoke, replace, stale generation, escaped label and remove');
  if (manifest.capabilities.editor_address) {
    await evaluate("document.querySelector('.input-sink').focus()");
    await domKey('f', 'KeyF', true);
    await waitFor(async () => (await snapshot()).command === 'locate /', 'ADDRESS THE quickfind');
    await send({type: 'action', action: 'edit.undo'});
    await send({type: 'command', command: 'cursor goto 1 1'});
    await evaluate("document.querySelector('.input-sink').focus()");
    await domKey('F7', 'F7');
    await waitFor(async () => !!(await snapshot()).panel, 'cREXX READ panel');
    await command('Input.insertText', {text: 'Ada'});
    await waitFor(async () => (await snapshot()).panel?.fields.some(f => f.changed && f.text.startsWith('Ada')), 'READ field DOM input');
    await domKey('Enter', 'Enter');
    await waitFor(async () => { const s = await snapshot(); return !s.panel && s.status.includes('PANEL_RC 0') && s.status.includes('Ada'); }, 'READ stem result/resume');
    const restored = await snapshot();
    if (restored.buffer.dirty || restored.focus.line !== 1 || restored.focus.cell !== 0)
      throw new Error('Panel changed buffer or failed to restore cursor');
    await domKey('F7', 'F7');
    await waitFor(async () => !!(await snapshot()).panel, 'repeated READ');
    await domKey('Escape', 'Escape');
    await waitFor(async () => { const s = await snapshot(); return !s.panel && s.status.includes('PANEL_RC 4'); }, 'READ cancellation/resume');
  }
  await send({ type: "command", command: "cursor goto 1 1" });
  await send({ type: "text", text: "é界" });
  await waitFor(async () => (await snapshot()).buffer.dirty, "dirty editing");
  await send({ type: "action", action: "file.save" });
  await waitFor(async () => !(await snapshot()).buffer.dirty, "durable save");
  const oldGeneration = await evaluate("window.__generation = Math.random()");
  workerSession = undefined;
  await command("Page.reload", { ignoreCache: true });
  await waitFor(() => evaluate(`window.__generation !== ${oldGeneration} && !!window.__snapshot`), "new Worker after reload", 30000);
  await waitFor(() => evaluate("window.__snapshot?.screen_rows.some(r => r.t.includes('é界Welcome'))"), "saved bytes after reload", 30000);
  await waitFor(() => !!workerSession, "Worker debugger");
  const originalBytes = await evaluate(`new Promise((resolve, reject) => {
    const request = indexedDB.open('/workspace');
    request.onerror = () => reject(request.error);
    request.onsuccess = () => {
      const db = request.result;
      const get = db.transaction('FILE_DATA').objectStore('FILE_DATA').get('/workspace/Welcome.txt');
      get.onsuccess = () => { resolve(Array.from(get.result.contents)); db.close(); };
      get.onerror = () => reject(get.error);
    };
  })`);
  await command("Runtime.evaluate", { expression: `globalThis.originalPut = IDBObjectStore.prototype.put;
    IDBObjectStore.prototype.put = function() { throw new DOMException('test quota refusal', 'QuotaExceededError'); };` }, workerSession);
  await send({ type: "text", text: "unsaved" });
  await waitFor(async () => (await snapshot()).buffer.dirty, "second edit");
  await send({ type: "action", action: "file.save" });
  await waitFor(() => evaluate("window.__messages.some(m => m.type === 'storage' && m.state === 'error')"), "storage refusal");
  if (!(await snapshot()).buffer.dirty) throw new Error("storage failure cleared dirty state");
  const durableBytes = await evaluate(`new Promise((resolve, reject) => {
    const request = indexedDB.open('/workspace');
    request.onerror = () => reject(request.error);
    request.onsuccess = () => {
      const db = request.result;
      const get = db.transaction('FILE_DATA').objectStore('FILE_DATA').get('/workspace/Welcome.txt');
      get.onsuccess = () => { resolve(Array.from(get.result.contents)); db.close(); };
      get.onerror = () => reject(get.error);
    };
  })`);
  if (JSON.stringify(originalBytes) !== JSON.stringify(durableBytes)) throw new Error("failed save modified durable bytes");
  await command("Runtime.evaluate", { expression: "IDBObjectStore.prototype.put = globalThis.originalPut" }, workerSession);
  await send({ type: "action", action: "file.save" });
  await waitFor(async () => !(await snapshot()).buffer.dirty, "save retry");
  // Exercise the real Preact keyboard/action transport and file ring.
  await evaluate("document.querySelector('.input-sink').focus()");
  await domKey("n", "KeyN", true);
  await waitFor(() => evaluate("!!document.querySelector('.path-dialog')"), "New file dialog");
  await evaluate(`(() => {
    const input = document.querySelector('.path-dialog input');
    input.value = 'browser-ui.txt'; input.dispatchEvent(new Event('input', {bubbles:true}));
  })()`);
  await waitFor(() => evaluate("!document.querySelector('.path-dialog button[type=submit]').disabled"), "New file input");
  await evaluate("document.querySelector('.path-dialog').dispatchEvent(new Event('submit', {bubbles:true,cancelable:true}))");
  await waitFor(async () => (await snapshot()).buffer.path.endsWith('/browser-ui.txt'), "new buffer");
  // THE starts an empty file at its command line; INPUT supplies its first record.
  await send({type: "command", command: "input seed"});
  await send({type: "command", command: "cursor goto 1 1"});
  await waitFor(async () => (await snapshot()).focus.zone === 'filearea', "new file record");
  await evaluate("document.querySelector('.input-sink').focus()");
  await command("Input.insertText", { text: "UI α界  " });
  await waitFor(async () => (await snapshot()).buffer.dirty, "DOM typing dirty");
  await evaluate("document.querySelector('[aria-label=Undo]').click()");
  await waitFor(() => evaluate("window.__snapshot.screen_rows.some(r => r.t === 'seed')"), 'restore current line');
  await evaluate("document.querySelector('.input-sink').focus()");
  await send({type: 'command', command: 'cursor goto 1 1'});
  await waitFor(async () => (await snapshot()).focus.cell === 0, 'undo cursor reset');
  await command('Input.insertText', {text: 'UI α界  '});
  await waitFor(() => evaluate("window.__snapshot.screen_rows.some(r => r.t === 'UI α界  seed')"), 'restored line typing');
  await send({type: 'command', command: 'cursor goto 1 1'});
  await waitFor(async () => (await snapshot()).focus.cell === 0, 'selection start');
  await domKey('ArrowRight', 'ArrowRight', false, true);
  await domKey('ArrowRight', 'ArrowRight', false, true);
  await waitFor(async () => { const s = (await snapshot()).selection; return s.active && s.start_cell === 1 && s.end_cell === 3; }, 'DOM Shift selection');
  await domKey('Delete', 'Delete');
  await waitFor(() => evaluate("window.__snapshot.screen_rows.some(r => r.t.includes('α界') && !r.t.includes('UI'))"), 'selection Delete');
  await evaluate("document.querySelector('.input-sink').focus()");
  await command('Input.insertText', {text: 'UI '});
  await waitFor(() => evaluate("window.__snapshot.screen_rows.some(r => r.t.includes('UI α界'))"), 'selection replacement');
  await evaluate("document.querySelector('[aria-label=Save]').click()");
  await waitFor(async () => !(await snapshot()).buffer.dirty, "toolbar save");
  await selectBuffer('Welcome.txt');
  await selectBuffer('browser-ui.txt');
  await command("Emulation.setDeviceMetricsOverride", {width: 900, height: 600, deviceScaleFactor: 1, mobile: false});
  await waitFor(() => evaluate("window.__snapshot.viewport.cols < 150"), "browser resize");
  // A competing tab must fail before mounting or overwriting this workspace.
  const second = await command("Target.createTarget", { url: "about:blank" });
  const attached = await command("Target.attachToTarget", {targetId: second.targetId, flatten: true});
  await command("Page.navigate", { url: pageUrl }, attached.sessionId);
  await waitFor(async () => {
    const result = await command("Runtime.evaluate", {expression:"document.body.innerText.includes('already open in another tab')", returnByValue:true}, attached.sessionId);
    return result.result?.value;
  }, "single-writer workspace lock", 30000);
  await command("Target.closeTarget", {targetId: second.targetId});
  // A new Worker restores all saved files and the explorer can reopen them.
  const generation = await evaluate("window.__generation = Math.random()");
  await command("Page.reload", {ignoreCache:true});
  await waitFor(() => evaluate(`window.__generation !== ${generation} && !!window.__snapshot`), "workspace reopen", 30000);
  await openWorkspaceFile('browser-ui.txt');
  await waitFor(() => evaluate("window.__snapshot.screen_rows.some(r => r.t.includes('UI α') && r.t.includes('界'))"), "DOM bytes persisted");
  if ((await snapshot()).buffer.dirty) throw new Error('restored buffer dirty');
  if (manifest.capabilities.classic_rexx_highlighter) {
    await openWorkspaceFile('ClassicDemo.rexx');
    await waitFor(async () => (await snapshot()).parser?.state === 'ready', 'Level L parser ready', 30000);
    let parsed = await snapshot();
    if (parsed.parser.provider !== 'Level L Classic Rexx' || parsed.parser.applied !== parsed.parser.revision)
      throw new Error('Wrong parser provider or revision');
    const greeting = parsed.screen_rows.find(r => r.line === 3);
    if (!greeting?.s?.some(([a,n,style]) => a === 11 && n === 10 && style === 'string')
        || !greeting.s.some(([a,n,style]) => a === 0 && n === 8 && style === 'identifier'))
      throw new Error(`Classic Unicode/identifier styles missing: ${JSON.stringify(greeting)}`);
    await send({type:'command', command:'cursor goto 3 1'});
    await waitFor(async () => {const s = await snapshot(); return s.focus.line === 3 && s.focus.cell === 0;}, 'Classic edit cursor');
    await evaluate("document.querySelector('.input-sink').focus()");
    await command('Input.insertText', {text:'['});
    await waitFor(async () => {const s = await snapshot(); return s.buffer.dirty && s.parser.state === 'ready' && s.diagnostics?.some(d => d.code === 'CLASSIC_INPUT');}, 'Level L diagnostics');
    parsed = await snapshot();
    const error = parsed.diagnostics.find(d => d.code === 'CLASSIC_INPUT');
    await send({type:'command', command:`cursor goto ${error.line} ${error.column}`});
    await waitFor(async () => {const s = await snapshot(); return s.focus.line === error.line && s.focus.cell === error.column - 1 && s.status_pmsg?.severity === 'ERROR';}, 'cursor-local error status');
    parsed = await snapshot();
    if (!parsed.screen_rows.find(r => r.line === error.line)?.s?.some(([, , style]) => style === 'diagnostic-error'))
      throw new Error('Parser error missing from editor styles');
    if (!await evaluate("!!document.querySelector('.syntax-diagnostic-error') && !!document.querySelector('.status-pmsg-error')"))
      throw new Error('Parser error missing from visible editor or profile status');
    if (await evaluate("!!document.querySelector('.diagnostics') || [...document.querySelectorAll('button')].some(b => b.textContent.startsWith('Diagnostics'))"))
      throw new Error('Browser-only Diagnostics pane or control remains');
    // CURSOR GOTO commits/reloads the working record. SOS UNDO then restores
    // that already-edited record, so repair the located byte explicitly.
    await evaluate("document.querySelector('.input-sink').focus()");
    await domKey('Delete', 'Delete');
    await waitFor(async () => {const s = await snapshot(); return s.screen_rows.find(r => r.line === 3)?.t === greeting.t && s.parser.state === 'ready' && !s.diagnostics?.length;}, 'Level L explicit repair');
    parsed = await snapshot();
    if (parsed.focus.line !== error.line || parsed.focus.cell !== error.column - 1)
      throw new Error('Repair moved the diagnostic caret');
    if (await evaluate("!!document.querySelector('.syntax-diagnostic-error, .status-pmsg-error')"))
      throw new Error('Stale parser error remains after repair');
    await evaluate("document.querySelector('[aria-label=Save]').click()");
    await waitFor(async () => !(await snapshot()).buffer.dirty, 'save repaired Classic source');
    const repairedBytes = await evaluate(`new Promise((resolve,reject) => {
      const request=indexedDB.open('/workspace');request.onerror=()=>reject(request.error);
      request.onsuccess=()=>{
        const db=request.result,get=db.transaction('FILE_DATA').objectStore('FILE_DATA').get('/workspace/ClassicDemo.rexx');
        get.onsuccess=()=>{resolve(Array.from(get.result.contents));db.close();};get.onerror=()=>reject(get.error);
      };
    })`);
    const classicBytes = new Uint8Array(await (await fetch(new URL('examples/ClassicDemo.rexx', pageUrl))).arrayBuffer());
    if (JSON.stringify(repairedBytes) !== JSON.stringify(Array.from(classicBytes)))
      throw new Error('Explicit diagnostic repair changed saved Classic source bytes');
    await send({type:'command', command:'cursor goto 3 1'});
    await waitFor(async () => (await snapshot()).focus.cell === 0, 'rapid edit cursor');
    await evaluate("document.querySelector('.input-sink').focus()");
    await command('Input.insertText', {text:'abc'});
    await waitFor(async () => {const s = await snapshot(); return s.parser.state === 'ready' && s.parser.applied === s.parser.revision && s.screen_rows.some(r => r.t.startsWith('abcgreeting'));}, 'latest coalesced revision');
    await evaluate("document.querySelector('[aria-label=Undo]').click()");
    await waitFor(async () => {const s = await snapshot(); return !s.buffer.dirty && s.parser.state === 'ready';}, 'rapid edit undo');
    await selectBuffer('Welcome.txt');
    if ((await snapshot()).parser?.state !== 'inactive') throw new Error('Classic styles leaked into a text file');
    await selectBuffer('ClassicDemo.rexx');
    await waitFor(async () => (await snapshot()).parser.state === 'ready', 'Classic buffer return');
    const previousRevision = (await snapshot()).parser.revision;
    await evaluate("document.querySelector('.parser-status').click()");
    await waitFor(async () => {const s = await snapshot(); return s.parser.revision > previousRevision && s.parser.state === 'ready';}, 'parser Worker restart', 30000);
    const previousId = (await snapshot()).parser.id;
    await send({type:'action',action:'file.close'});
    await waitFor(async () => !(await snapshot()).buffer.path.endsWith('ClassicDemo.rexx'), 'Classic close');
    await openWorkspaceFile('ClassicDemo.rexx');
    await waitFor(async () => {const s = await snapshot(); return s.parser.id !== previousId && s.parser.state === 'ready';}, 'Classic reopen identity');
    console.log('PASS browser Level L: actual VM/DSLSH styles, Unicode, diagnostics/repair, coalesced edits, switching, restart and reopen');
  }
  if (manifest.capabilities.classic_rexx_interpreter) {
    await command('Emulation.setDeviceMetricsOverride',{width:1200,height:900,deviceScaleFactor:1,mobile:false});
    await openWorkspaceFile('RexxRunner.rexx');
    await waitFor(()=>evaluate("!!document.querySelector('.rexx-dock')"),'split console');
    const initial = await snapshot();
    await runCurrentFile();
    await waitFor(()=>evaluate("document.querySelector('.run-state').textContent==='Waiting for input'"),'interpreter console input',30000);
    await waitFor(()=>evaluate("document.querySelector('.console-output').textContent.includes('What is your name?')"),'prompt before input');
    await send({type:'command',command:'set colour consolebody #123456 on #234567'});
    await waitFor(()=>evaluate("getComputedStyle(document.querySelector('.console-output')).color==='rgb(18, 52, 86)' && getComputedStyle(document.querySelector('.console-output')).backgroundColor==='rgb(35, 69, 103)'"),'SET COLOUR recolours running Wasm console');
    await selectBuffer('Welcome.txt');
    await waitFor(()=>evaluate("getComputedStyle(document.querySelector('.console-output')).color!=='rgb(18, 52, 86)'"),'Wasm file switch changes console palette');
    await selectBuffer('RexxRunner.rexx');
    await waitFor(()=>evaluate("getComputedStyle(document.querySelector('.console-output')).color==='rgb(18, 52, 86)'"),'Wasm return restores console file palette');
    if(!await evaluate("document.querySelector('.run-state').textContent==='Waiting for input'"))throw Error('Colour commands or file switching interrupted Wasm program input');
    await consoleEnter('Zoë');
    await waitFor(()=>evaluate("document.querySelector('.run-state').textContent.includes('Completed · status 0')"),'interpreter completion',30000);
    const text=await evaluate("document.querySelector('.console-output').textContent");
    if (!text.includes('Hello, Zoë')||!text.includes('Total: 55')||!text.includes('INTERPRET works here too.')||!text.includes('Welcome, Zoë!')) throw new Error(`Rexx output wrong: ${text}`);
    const after=await snapshot();
    if (after.buffer.dirty!==initial.buffer.dirty||after.focus.line!==initial.focus.line||after.focus.cell!==initial.focus.cell) throw new Error('Run/input changed editor state');
    // Execute the pending working record; neither Save nor moving to another line is required.
    await send({type:'command',command:'cursor goto 2 1'});
    await send({type:'text',text:"say 'UNSAVED'; "});
    await waitFor(async()=>{const s=await snapshot();return s.buffer.dirty&&s.screen_rows.some(r=>r.t.includes('UNSAVED'));},'unsaved working line');
    await runCurrentFile();
    await waitFor(()=>evaluate("document.querySelector('.run-state').textContent==='Waiting for input'"),'second interpreter input',30000);
    if (!await evaluate("document.querySelector('.console-output').textContent.includes('UNSAVED')")) throw new Error('Run missed unsaved working record');
    await evaluate("[...document.querySelectorAll('.rexx-dock button')].find(b=>b.textContent==='End input').click()");
    await waitFor(()=>evaluate("document.querySelector('.run-state').textContent.includes('Completed')"),'explicit EOF',30000);
    if (!(await snapshot()).buffer.dirty) throw new Error('Run cleared dirty state');
    await send({type:'action',action:'edit.undo'});
    await waitFor(async()=>!(await snapshot()).buffer.dirty,'restore program');
    // Stop a blocked program, then prove its replacement has a fresh input stream.
    await runCurrentFile();
    await waitFor(()=>evaluate("document.querySelector('.run-state').textContent==='Waiting for input'"),'third interpreter input',30000);
    await evaluate("[...document.querySelectorAll('.rexx-dock button')].find(b=>b.textContent==='Stop').click()");
    await waitFor(()=>evaluate("document.querySelector('.run-state').textContent==='Stopped'"),'stopped Worker');
    await runCurrentFile();
    await waitFor(()=>evaluate("document.querySelector('.run-state').textContent==='Waiting for input'"),'restart input',30000);
    await consoleEnter('Ada');
    await waitFor(()=>evaluate("document.querySelector('.run-state').textContent.includes('Completed · status 0')"),'restart completed',30000);
    if (!await evaluate("document.querySelector('.console-output').textContent.includes('Hello, Ada')")) throw new Error('Restart retained old stdin');
    await send({type:'action',action:'file.create',argument:'RunnerProbe.rexx'});
    await waitFor(async()=>(await snapshot()).buffer.path.endsWith('RunnerProbe.rexx'),'probe buffer');
    await send({type:'command',command:"input say arg()||'|'||arg(1)||'|'||arg(2)||'|'||arg(3); exit 7"});
    await waitFor(async()=>(await snapshot()).screen_rows.some(r=>r.t.includes('say arg()')),'argument probe');
    await evaluate("const field=document.querySelector('[aria-label=\"Program arguments\"]');field.value='first\\n\\nlast';field.dispatchEvent(new Event('input',{bubbles:true}))");
    await runCurrentFile();
    await waitFor(()=>evaluate("document.querySelector('.run-state').textContent==='Completed · status 7'"),'argument status',30000);
    if (!await evaluate("document.querySelector('.console-output').textContent.includes('3|first||last')")) throw new Error('Argument positions lost');
    await send({type:'command',command:'replace say 1/0'});
    await waitFor(async()=>(await snapshot()).screen_rows.some(r=>r.t.includes('say 1/0')),'runtime condition probe');
    await runCurrentFile();
    await waitFor(()=>evaluate("document.querySelector('.run-state').textContent==='Runtime condition'"),'typed runtime condition',30000);
    if (!await evaluate("!!document.querySelector('.console-stderr')?.textContent")) throw new Error('Runtime condition missing');
    await send({type:'command',command:'replace parse pull start; do forever; nop; end'});
    await waitFor(async()=>(await snapshot()).screen_rows.some(r=>r.t.includes('do forever')),'infinite program');
    await runCurrentFile();
    await waitFor(()=>evaluate("document.querySelector('.run-state').textContent==='Waiting for input'"),'infinite program input',30000);
    await consoleEnter('go');
    await evaluate("[...document.querySelectorAll('.rexx-dock button')].find(b=>b.textContent==='Stop').click()");
    await waitFor(()=>evaluate("document.querySelector('.run-state').textContent==='Stopped'"),'stop infinite program');
    await send({type:'action',action:'file.save'});
    await waitFor(async()=>!(await snapshot()).buffer.dirty,'editor responds after infinite program');
    await selectBuffer('RexxRunner.rexx');
    const beforeResize=(await snapshot()).viewport.cols;
    await evaluate("document.querySelector('[aria-label=\"Resize auxiliary panes\"]').dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowLeft',bubbles:true,cancelable:true}))");
    await waitFor(async()=>(await snapshot()).viewport.cols<beforeResize,'auxiliary split resize');
    if (manifest.capabilities.classic_rexx_highlighter) await waitFor(async()=>(await snapshot()).parser.state==='ready','highlighting after execution');
    console.log('PASS browser interpreter: split console, actual Classic execution/INTERPRET, Unicode stdin/output, EOF, unsaved record, Stop/restart and independent editor/highlighter');
  }
  if (exceptions.length) throw new Error(`browser exceptions: ${exceptions.join('; ')}`);

  if(manifest.capabilities.classic_rexx_interpreter && manifest.capabilities.crexx_vm){
    const image=Buffer.from(await(await fetch(new URL('examples/ToolHello.rxbin',pageUrl))).arrayBuffer());
    await evaluate("document.querySelector('.tools-toggle').click()");
    await waitFor(() => evaluate("!!document.querySelector('.tools-profiles-pane')?.getClientRects().length"), "explicit program tools pane");
    await evaluate(`(() => {
      const bytes = Uint8Array.from(atob('${image.toString('base64')}'), char => char.charCodeAt(0));
      const file = new File([bytes], 'ToolHello.rxbin', {type:'application/octet-stream'});
      const transfer = new DataTransfer();
      transfer.items.add(file);
      const importer = document.querySelector('.tools-profiles-pane input[type=file][accept=".rxbin"]');
      importer.files = transfer.files;
      importer.dispatchEvent(new Event('change', {bubbles:true}));
    })()`);
    await waitFor(()=>evaluate("window.__messages.some(m=>m.type==='tool-imported'&&m.path.endsWith('/ToolHello.rxbin'))"),'binary RXBIN import');
    await waitFor(()=>evaluate("document.querySelector('[aria-label=\"Run tool\"]')?.value==='rxbin'"),'imported RXBIN selected');
    await evaluate("document.querySelector('.tools-profiles-pane .run-with-tool').click()");
    await waitFor(()=>evaluate("window.__tools?.job.state==='Completed · status 9'"),'RXBIN console run',30000);
    const previousRxbinJob = await evaluate("window.__tools.job.id");
    await send({type:'command',command:'RX RUN "/workspace/ToolHello.rxbin"'});
    await waitFor(()=>evaluate(`window.__tools?.job.id>${previousRxbinJob} && window.__tools.job.state==='Completed · status 9' && window.__tools.chunks.some(c=>c.text.includes('RXBIN program proof'))`),'direct compiled RXBIN and output',30000);
    await send({type:'tool',action:'register',arguments:'hello-program\nprogram\n/workspace/ToolHello.rxbin\n.rxbin\nHello program'});
    await waitFor(()=>evaluate("window.__tools.catalog.some(t=>t.id==='hello-program')"),'tool registration');
    await send({type:'tool',action:'run',tool:'hello-program'});
    await waitFor(()=>evaluate("window.__tools.job.tool==='hello-program'&&!window.__tools.job.running"),'registered RXBIN run',30000);
    console.log('PASS browser RXBIN: console import/run, direct and registered execution with output/status');
    await sourceFileRunnerSmoke({pageUrl, send, snapshot, evaluate, waitFor});
  }

  if(manifest.capabilities.profile_source_compiler){

    if(manifest.capabilities.classic_rexx_highlighter){
      const parser=Buffer.from(await(await fetch(new URL('vm/modules/dslsh-classic.rxbin',pageUrl))).arrayBuffer());
      await send({type:'tool-import',name:'CustomHighlight.rxbin',bytes:parser.toString('base64')});
      await waitFor(()=>evaluate("window.__messages.some(m=>m.type==='tool-imported'&&m.path.endsWith('/CustomHighlight.rxbin'))"),'language RXBIN import');
      await send({type:'tool',action:'register',arguments:'custom-classic\nlanguage\n/workspace/CustomHighlight.rxbin\n.rexx,.rex\nCustom Classic'});
      await openWorkspaceFile('ClassicDemo.rexx');
      await waitFor(()=>evaluate("window.__snapshot.parser?.state==='ready'&&window.__snapshot.parser.providerId==='custom-classic'"),'registered language RXBIN service',30000);
      console.log('PASS browser language registration: imported RXBIN, association and admitted current revision');
    }
    console.log('CHECK browser profile: edit/build');
    await send({type:'tool',action:'profile'});
    await waitFor(()=>evaluate("window.__snapshot?.buffer?.path.endsWith('/user.profile.the')"),'editable user profile');
    await send({type:'action',action:'file.save'});
    await send({type:'tool',action:'build'});
    await waitFor(()=>evaluate("window.__tools?.build?.job.tool==='profile-compiler'&&!window.__tools.build.job.running"),'browser source profile compilation',60000);
    if(await evaluate("window.__tools.build.job.state")!=='Completed · status 0')throw Error('Profile compilation failed: '+await evaluate("JSON.stringify(window.__tools)"));
    console.log('CHECK browser profile: activate');
    await profileActivationStorageFailureSmoke({ send, evaluate, command, waitFor, snapshot });
    await waitFor(()=>evaluate("window.__tools.build.chunks.some(c=>c.text.includes('User profile activated'))"),'new addressed profile output');
    console.log('CHECK browser profile: rejected replacement');
    // A failed replacement must leave the successfully activated artifact selected.
    await send({type:'command',command:'cursor goto 2 1'});
    await send({type:'command',command:'replace options invalid-level'});
    await send({type:'tool',action:'build'});
    await waitFor(()=>evaluate("!window.__tools.build.job.running&&window.__tools.build.job.state!=='Completed · status 0'"),'failed replacement profile',60000);
    console.log('CHECK browser profile: save/reload');
    await send({type:'action',action:'file.save'});
    await waitFor(()=>evaluate("!window.__snapshot.buffer.dirty"),'save rejected profile source');
    await command('Page.reload',{ignoreCache:true});
    await waitFor(()=>evaluate("window.__tools?.build?.chunks.some(c=>c.text.includes('User profile activated'))"),'persisted working profile restore',30000);
    if(manifest.capabilities.classic_rexx_interpreter && manifest.capabilities.crexx_vm &&
        !await evaluate("window.__tools.catalog.some(t=>t.id==='hello-program')"))throw Error('Registry was not restored');
    console.log('PASS browser profiles: fresh Wasm compiler/assembler, ADDRESS THE, console output, activation, failed-build retention and persisted restore');
  }
  if (manifest.capabilities.minibasic_candidate) {
    // The ordinary smoke intentionally registers a separate .bas runner.
    // Qualify the candidate's fresh-file refusal in its own browser profile.
    const child = spawn(process.execPath, [fileURLToPath(import.meta.url)], {
      env: { ...process.env, THE_WASM_MINIBASIC_ONLY: "1" }, stdio: "inherit",
    });
    const status = await new Promise((resolve, reject) => {
      child.once("error", reject);
      child.once("exit", (code, signal) => resolve({ code, signal }));
    });
    if (status.code !== 0) throw new Error(`MiniBASIC candidate browser journey failed: ${JSON.stringify(status)}`);
  }
  if (exceptions.length) throw new Error(`browser exceptions: ${exceptions.join('; ')}`);
  if (process.env.THE_WASM_SCREENSHOT) {
    await command('Page.bringToFront');
    const image = await command('Page.captureScreenshot', {format: 'png'});
    await writeFile(process.env.THE_WASM_SCREENSHOT, Buffer.from(image.data, 'base64'));
  }
  console.log('PASS browser Wasm: persistence/refusal/retry, DOM editing/selection/undo, file ring, resize and tab lock');
  }
} catch(error) {
  if (socket?.readyState===WebSocket.OPEN) {
    try {
      console.error('Browser failure state:',await evaluate("({messages:window.__messages?.slice(-10),body:document.body.innerText.slice(-3000),state:document.querySelector('.run-state')?.textContent,active:document.activeElement?.outerHTML?.slice(0,400),input:document.querySelector('#rexx-input')?.value,output:document.querySelector('.console-output')?.textContent.slice(-4000)})"));
    } catch {}
  }
  throw error;
} finally {
  if (socket?.readyState === WebSocket.OPEN) socket.close();
  if (chrome.exitCode === null) {
    chrome.kill("SIGTERM");
    await new Promise((resolve) => chrome.once("exit", resolve));
  }
  await rm(profile, { recursive: true, force: true, maxRetries: 5, retryDelay: 100 });
}
