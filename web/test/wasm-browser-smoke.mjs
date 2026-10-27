import { spawn } from "node:child_process";
import { mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import WebSocket from "ws";

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
    pending.set(id, { resolve, reject, method });
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

async function openWorkspaceFile(name) {
  await evaluate("document.querySelector('[aria-label=\"Open\"]').click()");
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
      pending.delete(message.id);
      if (message.error) request.reject(new Error(`${request.method}: ${message.error.message}`));
      else request.resolve(message.result);
    } else if (message.method === "Target.attachedToTarget" && message.params.targetInfo.type === "worker") {
      workerSession = message.params.sessionId;
    } else if (message.method === "Runtime.exceptionThrown") {
      exceptions.push(message.params?.exceptionDetails?.text || "browser exception");
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
        super(...args); window.__worker = this;
        this.addEventListener('message', ({data}) => {
          window.__messages.push(data);
          if (data.screen_rows) window.__snapshot = data;
        });
      }
    };
  ` });
  await command("Page.navigate", { url: pageUrl });
  await waitFor(() => evaluate("window.__snapshot?.buffer?.path === '/workspace/Welcome.txt'"), "Wasm startup", 30000);
  const send = (request) => evaluate(`window.__worker.postMessage(${JSON.stringify(request)})`);
  const snapshot = () => evaluate("window.__snapshot");
  const manifest = await (await fetch(new URL('manifest.json', pageUrl))).json();
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
    await evaluate("document.querySelector('[aria-label=Undo]').click()");
    await waitFor(async () => {const s = await snapshot(); return !s.buffer.dirty && s.parser.state === 'ready' && !s.diagnostics?.length;}, 'Level L repair');
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
  if (exceptions.length) throw new Error(`browser exceptions: ${exceptions.join('; ')}`);
  if (process.env.THE_WASM_SCREENSHOT) {
    await command('Page.bringToFront');
    const image = await command('Page.captureScreenshot', {format: 'png'});
    await writeFile(process.env.THE_WASM_SCREENSHOT, Buffer.from(image.data, 'base64'));
  }
  console.log('PASS browser Wasm: persistence/refusal/retry, DOM editing/selection/undo, file ring, resize and tab lock');
} finally {
  if (socket?.readyState === WebSocket.OPEN) socket.close();
  if (chrome.exitCode === null) {
    chrome.kill("SIGTERM");
    await new Promise((resolve) => chrome.once("exit", resolve));
  }
  await rm(profile, { recursive: true, force: true, maxRetries: 5, retryDelay: 100 });
}
