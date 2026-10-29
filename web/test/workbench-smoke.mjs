// Production UI contract with a deterministic semantic-protocol fixture.
// Native/Wasm smoke tests separately qualify the real editor and tool runtime.
import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { createServer } from "node:http";
import { mkdir, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { extname, join, resolve } from "node:path";
import WebSocket, { WebSocketServer } from "ws";

const chromium = process.env.CHROMIUM_BIN;
if (!chromium) throw new Error("CHROMIUM_BIN is required");
const assets = resolve(process.env.THE_WEB_ASSETS || new URL("../dist", import.meta.url).pathname);
const profile = await mkdtemp(join(tmpdir(), "the-workbench-"));
const received = [];
const snapshot = {
  mode: "editor", rows: 24, cols: 80,
  focus: { zone: "filearea", line: 1, row: 0, cell: 2 }, command: "",
  status: "focus=filearea line=1 row=0 cell=2 verify=1 message=Initial response",
  status_line: "PROFILE  Line=1  Column=3  Insert mode",
  last_message_serial: 7, last_message: "Initial response",
  buffer: { path: "/workspace/example.rexx", dirty: 0, lines: 100 },
  buffers: [{ index: 0, path: "/workspace/example.rexx", dirty: 0, lines: 100, current: 1 }],
  source: { id: 1, revision: 3 }, history: { undo: 0, redo: 0 },
  viewport: { rows: 24, cols: 80, reserved_rows: 4, prefix_cols: 7, prefix_gap: 1 },
  diagnostics: [{ line: 2, column: 1, severity: "warning", code: "DEMO", message: "Example diagnostic" }],
  screen_rows: [],
};
const tools = {
  type: "tools", version: 1, visible: false, omitted: false, chunks: [],
  job: { id: 0, sourceId: 1, revision: 3, running: false, waiting: false, eof: false, state: "Ready", filename: "/workspace/example.rexx", tool: "classic-rexx" },
  catalog: [
    { id: "classic-rexx", label: "Classic Rexx", role: "runner", image: "runner.rxbin", extensions: ".rexx", available: true },
    { id: "rxbin", label: "RXBIN", role: "program", image: "", extensions: ".rxbin", available: true },
    { id: "profile-compiler", label: "Profile compiler", role: "compiler", image: "", extensions: ".crexx", available: true },
  ],
};
let client;
function publishSnapshot() {
  snapshot.screen_rows = Array.from({ length: snapshot.viewport.rows - 4 }, (_, row) => ({
    r: row, role: "file", line: row + 1, cur: row === 0 ? 1 : 0, sc: 0,
    p: String(row + 1).padStart(7, "0"), t: row === 0 ? "say 'Hello from THE'" : `/* record ${row + 1} */`,
  }));
  snapshot.screen_rows[1] = { r: 1, role: "reserved", line: 0, cur: 0, sc: 0, t: "Profile note: keep this reserved row in the editor" };
  const guide = snapshot.screen_rows.length - 1;
  snapshot.screen_rows[guide] = { r: guide, role: "reserved", line: 0, cur: 0, sc: 0,
    t: "F1 Profile help F6 Cmd/File F10 Menu Esc Cmd Ctrl-S Save" };
  client?.send(JSON.stringify(snapshot));
}
function publishTools() { tools.version++; client?.send(JSON.stringify(tools)); }
const server = createServer(async (request, response) => {
  try {
    const pathname = new URL(request.url, "http://localhost").pathname;
    const filename = resolve(assets, "." + (pathname === "/" ? "/index.html" : pathname));
    if (!filename.startsWith(assets + "/")) throw new Error("outside assets");
    const body = await readFile(filename);
    response.setHeader("Content-Type", ({ ".html": "text/html", ".js": "text/javascript", ".css": "text/css" })[extname(filename)] || "application/octet-stream");
    response.end(body);
  } catch { response.writeHead(404); response.end(); }
});
const ws = new WebSocketServer({ server });
ws.on("connection", (connection) => {
  client = connection;
  connection.send(JSON.stringify({ type: "actions", actions: [
    { id: "file.open", menu: "File", label: "Open", requires_argument: true },
    { id: "file.save", menu: "File", label: "Save", requires_argument: false },
  ] }));
  publishSnapshot(); publishTools();
  connection.on("message", (data) => {
    const message = JSON.parse(data);
    received.push(message);
    if (message.type === "resize") {
      Object.assign(snapshot.viewport, { rows: message.rows, cols: message.cols });
      publishSnapshot();
    } else if (message.type === "files") {
      connection.send(JSON.stringify({ type: "files", root: 0, path: "", roots: [{ id: 0, name: "Workspace", readonly: false }], entries: [{ name: "example.rexx", path: "example.rexx", target: "/workspace/example.rexx", type: "file", readonly: false }] }));
    } else if (message.type === "command" || message.type === "action") {
      snapshot.last_message_serial++;
      snapshot.last_message = "Repeated response";
      publishSnapshot();
    } else if (message.type === "tool" && message.action === "run") {
      Object.assign(tools.job, { id: tools.job.id + 1, running: true, waiting: false, state: "Running" });
      tools.visible = true;
      publishTools();
    }
  });
});
await new Promise((accept) => server.listen(0, "127.0.0.1", accept));
const chrome = spawn(chromium, ["--headless=new", "--no-sandbox", "--disable-gpu", "--no-first-run", "--remote-debugging-port=0", `--user-data-dir=${profile}`, "about:blank"], { stdio: "ignore" });
let socket;
let nextId = 0;
const pending = new Map();
const exceptions = [];
const sleep = (ms) => new Promise((accept) => setTimeout(accept, ms));
async function wait(check, label) {
  const deadline = Date.now() + 10000;
  while (Date.now() < deadline) { const result = await check(); if (result) return result; await sleep(30); }
  throw new Error("Timed out: " + label);
}
function cdp(method, params = {}) {
  return new Promise((accept, reject) => { const id = ++nextId; pending.set(id, { accept, reject }); socket.send(JSON.stringify({ id, method, params })); });
}
async function evaluate(expression) {
  const result = await cdp("Runtime.evaluate", { expression, awaitPromise: true, returnByValue: true });
  if (result.exceptionDetails) throw new Error(JSON.stringify(result.exceptionDetails));
  return result.result?.value;
}
async function viewport(width, height) {
  await cdp("Emulation.setDeviceMetricsOverride", { width, height, deviceScaleFactor: 1, mobile: false });
  await evaluate("new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)))");
}
async function geometry(selector) {
  return evaluate(`(() => { const n=document.querySelector(${JSON.stringify(selector)}),r=n.getBoundingClientRect(); return {left:r.left,right:r.right,top:r.top,bottom:r.bottom,width:r.width,height:r.height,visible:!!n.getClientRects().length}; })()`);
}
async function setInput(selector, value) {
  await evaluate(`(() => {const n=document.querySelector(${JSON.stringify(selector)});n.value=${JSON.stringify(value)};n.dispatchEvent(new Event('input',{bubbles:true}));})()`);
}
async function pane(label) {
  await evaluate(`[...document.querySelectorAll('.pane-switcher button')].find(n=>n.textContent.startsWith(${JSON.stringify(label)})).click()`);
}
try {
  const port = await wait(async () => { try { return +(await readFile(join(profile, "DevToolsActivePort"), "utf8")).split("\n")[0]; } catch { return false; } }, "Chrome port");
  const target = await (await fetch(`http://127.0.0.1:${port}/json/new?about:blank`, { method: "PUT" })).json();
  socket = new WebSocket(target.webSocketDebuggerUrl);
  await new Promise((accept) => socket.once("open", accept));
  socket.on("message", (data) => {
    const message = JSON.parse(data);
    if (message.id) {
      const request = pending.get(message.id); pending.delete(message.id);
      if (message.error) request.reject(new Error(JSON.stringify(message.error))); else request.accept(message.result);
    } else if (message.method === "Runtime.exceptionThrown") exceptions.push(message.params.exceptionDetails.text);
  });
  await cdp("Runtime.enable"); await cdp("Page.enable");
  await viewport(1280, 800);
  await cdp("Page.navigate", { url: `http://127.0.0.1:${server.address().port}/` });
  await wait(() => evaluate("!!document.querySelector('.status-line[data-focus-line=\"1\"]')"), "initial editor");
  await cdp("Page.bringToFront");
  assert.equal(await evaluate("document.querySelectorAll('.dock-entry').length"), 0, "initial snapshot must not fabricate a response event");
  assert.equal(await evaluate("document.querySelector('.editor-details')"), null);
  assert.equal(await evaluate("document.querySelector('.status-line').textContent.includes('focus=')"), false);
  assert.equal(await evaluate("document.body.textContent.includes('focus=filearea')"), false, "LLM debug state is not product status");
  assert.equal(await evaluate("document.querySelector('.profile-status').textContent"), snapshot.status_line, "profile status text is shown verbatim");
  assert.deepEqual(await evaluate("[...document.querySelectorAll('.key-hint')].map(n=>[n.querySelector('kbd').textContent,n.querySelector('span').textContent])"),
    [["F1", "Profile help"], ["F6", "Cmd/File"], ["F10", "Menu"], ["Esc", "Cmd"], ["Ctrl-S", "Save"]], "profile key names and labels are preserved");
  assert.equal(await evaluate("document.querySelector('.editor-grid .role-reserved:not(.key-hint-row)').textContent.trim()"), "Profile note: keep this reserved row in the editor");
  for (let index = 0; index < 2; index++) {
    await setInput("#the-command", "locate /alpha/");
    await evaluate("document.querySelector('.command-form').requestSubmit()");
    await wait(() => evaluate(`document.querySelectorAll('.dock-entry-response').length===${index + 1}`), "semantic response event");
  }
  assert.equal(await evaluate("document.querySelectorAll('.dock-entry-command').length"), 2);
  snapshot.focus.cell++; snapshot.status_line = "PROFILE  Line=1  Column=4  Insert mode"; publishSnapshot();
  await evaluate("new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)))");
  assert.equal(await evaluate("document.querySelectorAll('.dock-entry-response').length"), 2, "cursor updates do not duplicate responses");
  assert.equal(await evaluate("document.querySelector('.profile-status').textContent"), snapshot.status_line, "profile status updates with snapshots");
  await evaluate("document.querySelector('[aria-label=\"Save\"]').click()");
  await wait(() => evaluate("document.querySelectorAll('.dock-entry-response').length===3"), "Save response");
  assert.equal(await evaluate("document.querySelectorAll('.dock-entry-command').length"), 2, "Save is not an editor command submission");
  await evaluate("document.querySelector('.input-sink').dispatchEvent(new KeyboardEvent('keydown',{key:'s',code:'KeyS',ctrlKey:true,bubbles:true,cancelable:true}))");
  await wait(() => received.filter((message) => message.type === "action" && message.action === "file.save").length === 2, "Ctrl-S Save action");
  await evaluate("document.querySelector('.input-sink').dispatchEvent(new KeyboardEvent('keydown',{key:'F1',bubbles:true,cancelable:true}))");
  await wait(() => received.some((message) => message.type === "key" && message.key === "f1"), "profile function key forwarding");
  await evaluate("document.querySelector('.input-sink').focus();document.activeElement.dispatchEvent(new KeyboardEvent('keydown',{key:'F6',bubbles:true,cancelable:true}))");
  assert.equal(await evaluate("document.activeElement.id"), "the-command", "F6 focuses THE command");
  await evaluate("document.activeElement.dispatchEvent(new KeyboardEvent('keydown',{key:'Escape',bubbles:true,cancelable:true}))");
  assert.equal(await evaluate("document.activeElement.className"), "input-sink", "Escape restores editing");
  await pane("Files"); await pane("Program console");
  await evaluate("document.querySelector('.console-options').open=true");
  await setInput("[aria-label='Rexx arguments']", "first\n\nlast");
  await evaluate("document.querySelector('.run-program').click()");
  await wait(() => evaluate("document.querySelector('.run-state').textContent==='Running'"), "run fixture");
  for (const [width, height] of [[1280, 800], [1920, 1080]]) {
    await viewport(width, height);
    const files = await geometry(".file-explorer"), editor = await geometry(".editor-pane"), diagnostics = await geometry(".diagnostics"), programConsole = await geometry(".rexx-dock");
    assert(files.visible && editor.visible && diagnostics.visible && programConsole.visible, "all wide panes visible");
    assert(files.right <= editor.left && editor.right <= programConsole.left && diagnostics.bottom <= programConsole.top, "wide panes do not overlap");
    assert(programConsole.right <= width && programConsole.bottom <= height + 1, "console fits viewport");
    const surface = await geometry(".editor-scroll"), status = await geometry(".editor-status"), commands = await geometry(".command-area");
    assert(Math.abs(surface.bottom - status.top) < 1 && status.bottom <= commands.top, "status and guide immediately follow editor");
  }
  const initialFiles = (await geometry(".file-explorer")).width;
  await evaluate("document.querySelector('[aria-label=\"Resize files pane\"]').dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowRight',bubbles:true,cancelable:true}))");
  await wait(async () => (await geometry(".file-explorer")).width > initialFiles, "keyboard pane resize");
  const filesBeforeDrag = (await geometry(".file-explorer")).width;
  const divider = await geometry("[aria-label='Resize files pane']");
  const pointer = { x: divider.left + divider.width / 2, y: divider.top + 40 };
  await cdp("Input.dispatchMouseEvent", { type: "mouseMoved", ...pointer, buttons: 0 });
  await cdp("Input.dispatchMouseEvent", { type: "mousePressed", ...pointer, button: "left", buttons: 1, clickCount: 1 });
  // CDP must keep the button explicit during the move to retain mouse capture.
  await cdp("Input.dispatchMouseEvent", { type: "mouseMoved", x: pointer.x + 24, y: pointer.y, button: "left", buttons: 1, pointerType: "mouse" });
  await cdp("Input.dispatchMouseEvent", { type: "mouseReleased", x: pointer.x + 24, y: pointer.y, button: "left", buttons: 0, clickCount: 1 });
  await wait(async () => (await geometry(".file-explorer")).width > filesBeforeDrag, "pointer pane resize");
  const previousConsoleWidth = (await geometry(".rexx-dock")).width;
  await evaluate("document.querySelector('[aria-label=\"Resize auxiliary panes\"]').dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowLeft',bubbles:true,cancelable:true}))");
  await wait(async () => (await geometry(".rexx-dock")).width > previousConsoleWidth, "auxiliary resize");
  const previousDiagnosticsHeight = (await geometry(".diagnostics")).height;
  await evaluate("document.querySelector('[aria-label=\"Resize diagnostics and program console\"]').dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowDown',bubbles:true,cancelable:true}))");
  await wait(async () => (await geometry(".diagnostics")).height > previousDiagnosticsHeight, "diagnostics/console resize");
  await setInput("#rexx-input", "unsent input");
  await setInput("#the-command", "unsent command");
  await evaluate("window.originalConsole=document.querySelector('.rexx-dock');document.querySelector('.input-sink').focus()");
  tools.job.waiting = true; tools.job.state = "Waiting for input"; publishTools();
  await wait(() => evaluate("document.querySelector('.run-state').textContent==='Waiting for input'"), "waiting job");
  assert.equal(await evaluate("document.activeElement.className"), "input-sink", "waiting output must not steal editor focus");
  tools.chunks = [{ fd: 1, text: Array.from({length:200}, (_, index) => `output ${index}\n`).join("") }]; publishTools();
  await wait(() => evaluate("document.querySelector('.console-output').scrollHeight>1000"), "long output");
  await evaluate("const output=document.querySelector('.console-output');output.scrollTop=0;output.dispatchEvent(new Event('scroll'))");
  tools.chunks.push({ fd: 2, text: "latest output\n" }); publishTools();
  await wait(() => evaluate("!!document.querySelector('.new-output')"), "unread output control");
  assert.equal(await evaluate("document.querySelector('.console-output').scrollTop"), 0, "output preserves scrollback position");
  await evaluate("document.querySelector('[aria-label=\"Close program console\"]').click()"); await pane("Program console");
  await viewport(390, 844);
  assert.equal(await evaluate("document.documentElement.scrollWidth<=innerWidth"), true, "phone shell has no horizontal overflow");
  assert.equal((await geometry(".pane-switcher")).visible, false, "phone has one auxiliary selector");
  assert.equal((await geometry(".mobile-aux-tabs")).visible, true);
  await evaluate("[...document.querySelectorAll('.mobile-aux-tabs button')].find(n=>n.textContent==='Files').click()");
  await wait(() => evaluate("document.querySelector('.workspace').classList.contains('mobile-pane-files')"), "phone files selection");
  assert.equal((await geometry(".mobile-aux-tabs")).visible, true, "selector remains available with Files selected");
  await evaluate("[...document.querySelectorAll('.mobile-aux-tabs button')].find(n=>n.textContent.startsWith('Program console')).click()");
  await wait(() => evaluate("document.querySelector('.workspace').classList.contains('mobile-pane-console')"), "phone program console selection");
  const editor = await geometry(".editor-pane"), commands = await geometry(".command-area"), programConsole = await geometry(".rexx-dock");
  assert(editor.top < commands.top && commands.bottom <= programConsole.top && programConsole.right <= 390, "phone editor/command/console vertical flow");
  assert.equal((await geometry(".file-explorer")).visible, false, "phone has one selected auxiliary pane");
  for (const width of [320, 390, 768]) {
    await viewport(width, 844);
    assert.equal(await evaluate("document.documentElement.scrollWidth<=innerWidth"), true, `no shell overflow at ${width}`);
    assert.equal(await evaluate("[...document.querySelectorAll('.key-hint')].every(n=>{const r=n.getBoundingClientRect(),b=n.closest('.key-hints').getBoundingClientRect();return r.left>=b.left&&r.right<=b.right&&r.top>=b.top&&r.bottom<=b.bottom})"), true, `all profile hints wrap at ${width}`);
    const surface = await geometry(".editor-scroll"), status = await geometry(".editor-status"), commands = await geometry(".command-area");
    assert(Math.abs(surface.bottom - status.top) < 1 && status.bottom <= commands.top, `phone footer follows editor at ${width}`);
  }
  await viewport(390, 844);
  await evaluate("document.querySelector('.input-sink').focus();document.activeElement.dispatchEvent(new KeyboardEvent('keydown',{key:'F10',bubbles:true,cancelable:true}))");
  assert.equal(await evaluate("document.querySelectorAll('.menu-trigger').length"), 1);
  assert((await geometry(".menu-popover")).right <= 390, "phone menu fits");
  await evaluate("[...document.querySelectorAll('.menu-popover button')].find(n=>n.textContent==='About CRXTHE').click()");
  assert.equal(await evaluate("document.querySelector('.about-menu').getAttribute('aria-label')"), "About CRXTHE");
  assert.equal(await evaluate("document.querySelector('.about-menu').textContent.includes('Program console')"), true);
  assert.equal(await evaluate("document.querySelector('.about-menu').textContent.includes('focus=')"), false);
  assert((await geometry(".about-menu")).right <= 390, "phone About fits");
  await wait(() => evaluate("document.querySelector('.about-menu').contains(document.activeElement)"), "About keyboard focus");
  await evaluate("document.activeElement.click()");
  await wait(() => evaluate("!document.querySelector('.about-menu') && document.activeElement.classList.contains('menu-trigger')"), "About Back to Menu focus");
  await evaluate("[...document.querySelectorAll('.menu-popover button')].find(n=>n.textContent==='About CRXTHE').click()");
  await wait(() => evaluate("document.querySelector('.about-menu').contains(document.activeElement)"), "About focus after reopening");
  await evaluate("document.activeElement.dispatchEvent(new KeyboardEvent('keydown',{key:'Escape',bubbles:true,cancelable:true}))");
  await wait(() => evaluate("!document.querySelector('.menu-popover') && document.activeElement.classList.contains('input-sink')"), "F10 Menu Escape returns to editing");
  await viewport(390, 450);
  await evaluate("document.querySelector('#rexx-input').scrollIntoView({block:'nearest'})");
  assert((await geometry("#rexx-input")).bottom <= 450, "program input stays reachable in a short phone viewport");
  await viewport(1920, 1080);
  assert.equal(await evaluate("window.originalConsole===document.querySelector('.rexx-dock')"), true, "layout and hiding preserve component identity");
  assert.equal(await evaluate("document.querySelector('#rexx-input').value"), "unsent input");
  assert.equal(await evaluate("document.querySelector('#the-command').value"), "unsent command");
  assert.equal(await evaluate("document.querySelector('[aria-label=\"Rexx arguments\"]').value"), "first\n\nlast");
  assert.equal(tools.job.running, true);
  const profileStatus = snapshot.status_line;
  snapshot.status_line = null; publishSnapshot();
  await wait(() => evaluate("document.querySelector('.status-line').hidden"), "profile statusline off");
  assert.equal((await geometry(".key-hints")).visible, true, "statusline off does not hide the profile guide");
  delete snapshot.status_line; publishSnapshot();
  await wait(() => evaluate("document.querySelector('.status-line').textContent.includes('Ln 1, Col 4')"), "legacy semantic summary fallback");
  snapshot.status_line = profileStatus; publishSnapshot();
  await wait(() => evaluate("!!document.querySelector('.profile-status')"), "profile status restored");
  assert(received.some((message) => message.type === "resize"));
  assert.deepEqual(exceptions, []);
  if (process.env.THE_WORKBENCH_SCREENSHOTS) {
    const destination = resolve(process.env.THE_WORKBENCH_SCREENSHOTS);
    await mkdir(destination, { recursive: true });
    tools.chunks = [{ fd: 1, text: "Hello from THE\nWhat is your name?\n" }, { fd: 0, text: "Zoë\n" }, { fd: 1, text: "Hello, ZOË\nWaiting for another line…\n" }];
    publishTools();
    await evaluate("document.querySelector('.console-options').open=false;document.querySelector('.new-output')?.click()");
    for (const [width, height] of [[390, 844], [1280, 800], [1920, 1080]]) {
      await viewport(width, height);
      await evaluate("document.querySelector('.workspace').scrollTop=0");
      const screenshot = await cdp("Page.captureScreenshot", { format: "png" });
      await writeFile(join(destination, `workbench-${width}.png`), Buffer.from(screenshot.data, "base64"));
      if (width === 390) {
        await evaluate("document.querySelector('#rexx-input').scrollIntoView({block:'nearest'})");
        const consoleImage = await cdp("Page.captureScreenshot", { format: "png" });
        await writeFile(join(destination, "workbench-390-console.png"), Buffer.from(consoleImage.data, "base64"));
      }
    }
    console.log("Presentation-fixture screenshots: " + destination);
  }
  console.log("PASS Guided Workbench: 320/390/768/1280/1920 layouts, visible profile hints/status, About, key dispatch, semantic history, separate program input, resize, retained drafts/job, focus and scrollback");
} finally {
  socket?.close(); chrome.kill();
  for (const connection of ws.clients) connection.terminate();
  ws.close(); await new Promise((accept) => server.close(accept));
  await rm(profile, { recursive: true, force: true, maxRetries: 10, retryDelay: 100 });
}
