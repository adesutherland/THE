// Production UI contract with a deterministic semantic-protocol fixture.
// Native/Wasm smoke tests separately qualify the real editor and tool runtime.
import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { createServer } from "node:http";
import { mkdir, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { extname, join, resolve } from "node:path";
import WebSocket, { WebSocketServer } from "ws";
import "./form-coordinator-smoke.mjs";

const chromium = process.env.CHROMIUM_BIN;
if (!chromium) throw new Error("CHROMIUM_BIN is required");
const assets = resolve(process.env.THE_WEB_ASSETS || new URL("../dist", import.meta.url).pathname);
const profile = await mkdtemp(join(tmpdir(), "the-workbench-"));
const received = [];
let workbenchGeneration = 0;
let workbenchPublication = 0;
let workbenchRuns = 0;
let customFooter = true;
let presentDiagnostic = false;
let fixturePanels = [];
let fixturePanelState = null;
const fixtureLogExtras = new Map();
let holdDocumentPage = false;
const heldDocumentPages = [];
const fixtureForms = new Map();
function publishFormState() {
  fixturePanelState = { ...fixturePanelState, stateRevision: String(Number(fixturePanelState.stateRevision) + 1),
    panels: fixturePanelState.panels.map((panel) => {
      const form = fixtureForms.get(panel.form);
      return form ? { ...panel, draftRevision: form.draftRevision, resultRevision: form.resultRevision,
        submitted: form.submitted, formDirty: form.dirty } : panel;
    }) };
  client?.send(JSON.stringify(fixturePanelState));
}
const snapshot = {
  mode: "editor", rows: 24, cols: 80,
  focus: { zone: "filearea", line: 1, row: 0, cell: 2 }, command: "",
  status: "focus=filearea line=1 row=0 cell=2 verify=1 message=",
  status_line: "PROFILE  Line=1  Column=3  Insert mode",
  last_message_serial: 7, last_message: "",
  buffer: { path: "/workspace/example.rexx", dirty: 0, lines: 100 },
  buffers: [{ index: 0, path: "/workspace/example.rexx", dirty: 0, lines: 100, current: 1 }],
  parser: { provider: "fixture", state: "ready", id: 1, revision: 3, applied: 3, stale: 0 },
  source: { id: 1, revision: 3 }, history: { undo: 0, redo: 0 },
  viewport: { rows: 24, cols: 80, reserved_rows: 4, prefix_cols: 7, prefix_gap: 1 },
  diagnostics: [{ line: 2, column: 1, severity: "ERROR", code: "DEMO", message: "Example diagnostic" }],
  console_colours: {
    body: { foreground: "#D4D4D4", background: "#1E1E1E", style: 0 },
    header: { foreground: "#F1F5FA", background: "#25415F", style: 0 },
    input: { foreground: "#D4D4D4", background: "#2D2D30", style: 0 },
    control: { foreground: "#D4D4D4", background: "#1E1E1E", style: 0 },
    selected: { foreground: "#E6F7FF", background: "#264F78", style: 0 },
    divider: { foreground: "#6C7B90", background: "#1E1E1E", style: 0 },
    error: { foreground: "#FF6347", background: "#1E1E1E", style: 0 },
    status: { foreground: "#D4D4D4", background: "#1E1E1E", style: 0 },
  },
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
  snapshot.screen_rows[2] = { r: 2, role: "reserved", line: 0, cur: 0, sc: 0, t: "F1 Reserved note F6 Another note F10 Keep in grid" };
  if (presentDiagnostic) snapshot.screen_rows[3].s = [[3, 6, "diagnostic-error"]];
  if (customFooter) snapshot.screen_rows[snapshot.screen_rows.length - 3] = {
    r: snapshot.screen_rows.length - 3, role: "reserved", footer: "build", line: 0, cur: 0, sc: 0,
    t: "profile-defined footer text", footer_text: "Build: profile-defined footer text", s: [[0, 7, "keyword"]],
    footer_colour: { style: 0, foreground: "#AA2200", background: "#FFF4CC" },
  };
  snapshot.screen_rows[snapshot.screen_rows.length - 2] = {
    r: snapshot.screen_rows.length - 2, role: "reserved", footer: "response", line: 0, cur: 0, sc: 0,
    t: "Response placement placeholder must not become static chrome",
  };
  const guide = snapshot.screen_rows.length - 1;
  snapshot.screen_rows[guide] = { r: guide, role: "reserved", footer: "keys", line: 0, cur: 0, sc: 0,
    t: "F1 Profile help F6 Cmd/File F10 Menu Esc Cmd Ctrl-S Save" };
  client?.send(JSON.stringify(snapshot));
}
function publishTools() { tools.version++; client?.send(JSON.stringify(tools)); }
function publishWorkbench(revision, generation, label, saveReason = "ok") {
  workbenchGeneration = generation;
  const publication = ++workbenchPublication;
  const actions = [
    { id: "the.file.open", owner: "the.builtin", label: "Open", generation: 1, kind: "builtin", builtin: "file.open", operand: "", enabled: true, reason: "ok" },
    { id: "the.file.save", owner: "the.builtin", label: "Save", generation: 2, kind: "builtin", builtin: "file.save", operand: "", enabled: saveReason === "ok", reason: saveReason },
    ...(fixturePanels.length ? [{ id: "user.show", owner: "user.logs", label: "Show output", generation: 5,
      kind: "panel", panel: "user.output", operation: "show", enabled: true, reason: "ok" }] : []),
    ...(label ? [{ id: "user.run", owner: "user.tools", label, generation, kind: "builtin", builtin: "edit.undo", operand: "", enabled: true, reason: "ok" }] : []),
    ...(label ? [{ id: "user.boundopen", owner: "user.tools", label: "Bound Open", generation, kind: "builtin", builtin: "file.open", operand: "/workspace/example.rexx", enabled: true, reason: "ok" }] : []),
    ...Array.from({ length: 9 }, (_, index) => ({ id: `user.unlisted${index}`, owner: "user.tools", label: `Unlisted ${index}`, generation: index + 30, kind: "builtin", builtin: "edit.undo", operand: "", enabled: true, reason: "ok" })),
  ];
  const menus = [
    { id: "the.file", owner: "the.builtin", label: "File", generation: 1, order: 0 },
    ...(label ? [{ id: "user.tools", owner: "user.tools", label: "Tools", generation, order: 50 }] : []),
    ...(label ? [{ id: "user.zmenu", owner: "user.tools", label: "Later", generation, order: 50 },
      { id: "user.amenu", owner: "user.tools", label: "Last", generation, order: 50 }] : []),
  ];
  const items = [
    { id: "the.item.open", owner: "the.builtin", label: "Open", generation: 1, menu: "the.file", action: "the.file.open", order: 0, separator: false },
    { id: "the.item.save", owner: "the.builtin", label: "Save", generation: 2, menu: "the.file", action: "the.file.save", order: 1, separator: false },
    ...(fixturePanels.length ? [{ id: "user.showitem", owner: "user.logs", label: "Show output", generation: 5,
      menu: "the.file", action: "user.show", order: 2, separator: false }] : []),
    ...(label ? [{ id: "user.item", owner: "user.tools", label, generation, menu: "user.tools", action: "user.run", order: 0, separator: false }] : []),
    ...(label ? [{ id: "user.zitem", owner: "user.tools", label: "Second", generation, menu: "user.tools", action: "user.run", order: 0, separator: false },
      { id: "user.separator", owner: "user.tools", label: "", generation, menu: "user.tools", action: "", order: 0, separator: true },
      { id: "user.aitem", owner: "user.tools", label: "Third", generation, menu: "user.tools", action: "user.run", order: 0, separator: false },
      { id: "user.bounditem", owner: "user.tools", label: "Bound Open", generation, menu: "user.tools", action: "user.boundopen", order: 1, separator: false }] : []),
  ];
  for (const [section, records] of Object.entries({ actions, menus, items, panels: fixturePanels }))
    for (let offset = 0; offset < records.length || offset === 0; offset += 8)
      client?.send(JSON.stringify({ type: "workbench", operation: "list", publication, data: { schemaVersion: 1, revision, section, total: records.length, offset, nextOffset: offset + 8 < records.length ? offset + 8 : null, records: records.slice(offset, offset + 8) } }));
}
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
  connection.send(JSON.stringify({ type: "hello", workbenchSession: "fixture123", workbenchNextRequest: 1 }));
  connection.send(JSON.stringify({ type: "actions", actions: [
    { id: "file.open", menu: "File", label: "Open", requires_argument: true },
    { id: "file.save", menu: "File", label: "Save", requires_argument: false },
    { id: "file.create", menu: "File", label: "Create", requires_argument: true },
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
    } else if (message.type === "workbench" && message.operation === "invoke") {
      const stale = message.expectGeneration !== (message.action === "user.show" ? 5 : workbenchGeneration);
      if (!stale) workbenchRuns++;
      if (!stale && message.action === "user.show" && fixturePanelState) {
        fixturePanelState = { ...fixturePanelState, stateRevision: String(Number(fixturePanelState.stateRevision) + 1),
          panels: fixturePanelState.panels.map((panel) => panel.id === "user.output"
            ? { ...panel, visible: true, reason: "ok" } : panel) };
        connection.send(JSON.stringify(fixturePanelState));
      }
      connection.send(JSON.stringify({ type: "workbench", operation: "result", requestId: message.requestId,
        state: "complete", code: stale ? "stale-generation" : "ok", rc: stale ? -3 : 0,
        revision: snapshot.source.revision, message: stale ? "Action generation is stale" : "" }));
    } else if (message.type === "workbench" && message.operation === "log.list") {
      const second = message.model === "user.errorlog";
      const continuing = message.cursor !== "0" && fixtureLogExtras.has(message.model);
      const text = continuing ? fixtureLogExtras.get(message.model)
        : (second ? "second output\n" : "safe <img>\u0007 output\n")
        + Array.from({ length: 100 }, (_, index) => `${second ? "second" : "first"} line ${index}\n`).join("");
      connection.send(JSON.stringify({ type: "workbench", operation: "log", data: {
        schemaVersion: 1, revision: 5, section: "log", id: message.model,
        generation: "1", contentRevision: continuing ? "3" : "2", firstCursor: "1", nextCursor: continuing ? "3" : "2",
        nextOffset: 0, more: false, omitted: !continuing && !second, omittedBytes: second ? "0" : "4", omittedRecords: second ? "0" : "1",
        records: [{ cursor: continuing ? "2" : "1", job: "9", stream: 1, sequence: continuing ? "2" : "1", offset: 0,
          length: text.length, originalLength: text.length, text }],
      } }));
    } else if (message.type === "workbench" && message.operation === "document.list") {
      const panel = fixturePanelState?.panels.find((item) => item.id === message.panel);
      const records = Array.from({ length: Math.min(8, 25 - message.line) }, (_, index) => {
        const line = message.line + index;
        const text = `${message.panel} revision ${panel.documentRevision} line ${line} <img>\u0007 ${"x".repeat(240)}`;
        return { line, offset: 0, length: text.length, originalLength: text.length,
          text, bytesHex: Buffer.from(text).toString("hex") };
      });
      const page = { type: "workbench", operation: "document", data: {
        schemaVersion: 1, revision: 10, section: "document", panel: panel.id,
        panelGeneration: panel.generation, documentId: panel.documentId,
        documentRevision: panel.documentRevision, lineCount: 24, dirty: false,
        nextLine: message.line + records.length, nextOffset: 0,
        more: message.line + records.length <= 24, records,
      } };
      if (holdDocumentPage) heldDocumentPages.push(page);
      else connection.send(JSON.stringify(page));
    } else if (message.type === "workbench" && message.operation === "form.list") {
      const form = fixtureForms.get(message.form);
      connection.send(JSON.stringify({ type: "workbench", operation: "form", data: {
        schemaVersion: 1, revision: 12, section: "fields", form: message.form, generation: form.generation,
        draftRevision: form.draftRevision, resultRevision: form.resultRevision, submitted: form.submitted,
        dirty: form.dirty, total: form.fields.length, offset: message.offset,
        nextOffset: message.offset + 8 < form.fields.length ? message.offset + 8 : null,
        records: form.fields.slice(message.offset, message.offset + 8),
      } }));
    } else if (message.type === "workbench" && ["form.set", "form.submit", "form.cancel"].includes(message.operation)) {
      connection.send(JSON.stringify({ type: "workbench", operation: "result", requestId: message.requestId, state: "queued", code: "queued" }));
      setTimeout(() => {
        const form = fixtureForms.get(message.form);
        const field = form.fields.find((field) => field.id === message.field);
        const code = message.expectGeneration !== form.generation ? "stale-generation"
          : message.expectRevision !== form.draftRevision ? "stale-revision"
          : message.operation === "form.set" && field?.access !== "edit" ? "protected-field"
          : message.operation === "form.set" && (Buffer.byteLength(message.value) > 1024 || Array.from(message.value).length > field.maxcells) ? "limit-exceeded" : "ok";
        if (code === "ok") {
          if (message.operation === "form.set") field.value = message.value;
          if (message.operation === "form.submit") {
            form.baseline = form.fields.map((field) => field.value); form.submitted = true;
            form.resultRevision = String(BigInt(form.resultRevision) + 1n);
          }
          if (message.operation === "form.cancel") form.fields.forEach((field, index) => { field.value = form.baseline[index]; });
          form.draftRevision = String(BigInt(form.draftRevision) + 1n);
          form.dirty = form.fields.some((field, index) => field.value !== form.baseline[index]);
          form.fields.forEach((field, index) => { field.length = Buffer.byteLength(field.value); field.dirty = field.value !== form.baseline[index]; });
          publishFormState();
        }
        connection.send(JSON.stringify({ type: "workbench", operation: "result", requestId: message.requestId,
          state: "complete", code, rc: code === "ok" ? 0 : -3, form: message.form,
          formGeneration: form.generation, draftRevision: form.draftRevision, resultRevision: form.resultRevision,
          message: code === "ok" ? "" : code }));
      }, 25);
    } else if (message.type === "workbench" && message.operation?.startsWith("panel.")) {
      if (fixturePanelState && message.operation !== "panel.layout") {
        const focus = message.operation === "panel.focus";
        const restoring = message.operation === "panel.return";
        fixturePanelState = { ...fixturePanelState,
          focus: focus || restoring ? { ...fixturePanelState.focus, serial: String(Number(fixturePanelState.focus.serial) + 1),
            panel: restoring ? "" : message.panel, generation: "1", restoring } : fixturePanelState.focus,
          panels: fixturePanelState.panels.map((panel) => ({ ...panel,
            visible: panel.id === message.panel && message.operation === "panel.show" ? true
              : panel.id === message.panel && message.operation === "panel.hide" ? false : panel.visible,
            focused: focus ? panel.id === message.panel : restoring ? false : panel.focused })) };
        connection.send(JSON.stringify(fixturePanelState));
        connection.send(JSON.stringify({ type: "workbench", operation: "result", requestId: message.requestId,
          state: "complete", code: "ok", rc: 0, revision: 5, message: "" }));
      }
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
async function key(key, modifiers = 0) {
  const keyCodes = { F10: 121, F6: 117, Escape: 27, ArrowDown: 40, ArrowUp: 38, Home: 36, End: 35, Enter: 13, Tab: 9 };
  const parameters = { key, code: key, windowsVirtualKeyCode: keyCodes[key], modifiers };
  await cdp("Input.dispatchKeyEvent", { type: "keyDown", ...parameters });
  await cdp("Input.dispatchKeyEvent", { type: "keyUp", ...parameters });
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
  assert.equal(await evaluate("document.querySelector('.latest-response').textContent"), "", "idle response strip stays quiet");
  assert.equal(await evaluate("document.querySelector('.editor-details')"), null);
  assert.equal(await evaluate("document.querySelector('.parser-status').closest('header').className"), "toolbar", "workspace/syntax status uses the existing top bar");
  assert.equal(await evaluate("document.querySelector('.editor-indicators')"), null, "workspace/syntax does not add a footer row");
  assert.equal(await evaluate("document.querySelector('.status-line').textContent.includes('focus=')"), false);
  assert.equal(await evaluate("document.body.textContent.includes('focus=filearea')"), false, "LLM debug state is not product status");
  assert.equal(await evaluate("document.querySelector('.profile-status').textContent"), snapshot.status_line, "profile status text is shown verbatim");
  assert.equal(await evaluate("document.querySelector('.key-hints').textContent"), "F1 Profile help F6 Cmd/File F10 Menu Esc Cmd Ctrl-S Save", "named profile footer text is preserved verbatim");
  assert.equal(await evaluate("document.querySelector('[data-footer=build]').textContent"), "Build: profile-defined footer text", "arbitrary named footer is rendered");
  assert.equal(await evaluate("getComputedStyle(document.querySelector('[data-footer=build]')).color"), "rgb(170, 34, 0)", "profile footer foreground survives relocation");
  assert.equal(await evaluate("getComputedStyle(document.querySelector('[data-footer=build]')).backgroundColor"), "rgb(255, 244, 204)", "profile footer background survives relocation");
  assert.equal(await evaluate("document.querySelector('[data-footer=build] .syntax-keyword').textContent"), "profile", "named footer preserves semantic styles after prefix split");
  assert.equal(await evaluate("document.querySelector('[data-footer=response]')"), null, "response placement does not add a static line");
  assert.equal(await evaluate("document.querySelector('.dock-heading')"), null, "command heading does not consume a row");
  assert.equal(await evaluate("document.querySelector('.latest-response').hidden"), true, "empty response consumes no footer height");
  assert.deepEqual(await evaluate("[...document.querySelectorAll('.editor-grid .role-reserved:not(.profile-footer-source)')].map(n=>n.textContent.trim())"),
    ["Profile note: keep this reserved row in the editor", "F1 Reserved note F6 Another note F10 Keep in grid"], "ordinary reserved rows stay in the grid even when they look like shortcuts");
  customFooter = false; publishSnapshot();
  await wait(() => evaluate("!document.querySelector('[data-footer=build]')"), "profile removes custom footer");
  assert.equal(await evaluate("!!document.querySelector('.diagnostics, .diagnostic-row, [aria-label=\"Close diagnostics\"]')"), false,
    "parser diagnostics use editor/status presentation, not a separate pane");
  assert.equal(await evaluate("[...document.querySelectorAll('button')].some(button => button.textContent.startsWith('Diagnostics'))"), false,
    "no browser-only Diagnostics control is exposed");
  const originalStatus = snapshot.status_line;
  presentDiagnostic = true;
  snapshot.status_line = "PROFILE  DEMO error  Line=4";
  snapshot.status_pmsg = { text: "DEMO error in this record", severity: "ERROR", start: 9, length: 10,
    colour: { foreground: "#FFFFFF", background: "#B22222", style: 1 } };
  snapshot.diagnostic_colours = { error: { foreground: "#FFFFFF", background: "#B22222", style: 1 } };
  publishSnapshot();
  await wait(() => evaluate("!!document.querySelector('.syntax-diagnostic-error, .status-pmsg')"), "inline parser error");
  assert.equal(await evaluate("document.querySelector('.status-pmsg').textContent"), "DEMO error", "cursor-local error uses profile status line");
  assert.equal(await evaluate("getComputedStyle(document.querySelector('.status-pmsg')).backgroundColor"), "rgb(178, 34, 34)", "PMSGERROR profile colour reaches status");
  assert.equal(await evaluate("getComputedStyle(document.querySelector('.syntax-diagnostic-error')).backgroundColor"), "rgb(178, 34, 34)", "CBERROR profile colour reaches editor text");
  assert.equal(await evaluate("document.querySelectorAll('.dock-entry').length"), 0, "parser cursor message does not enter command history");
  presentDiagnostic = false;
  snapshot.status_line = originalStatus;
  snapshot.status_pmsg = null;
  publishSnapshot();
  await wait(() => evaluate("!document.querySelector('.syntax-diagnostic-error, .status-pmsg')"), "parser error clears");
  const closedPaneGeometry = [];
  for (const [width, height] of [[390, 844], [800, 700]]) {
    await viewport(width, height);
    assert.equal(await evaluate("!document.querySelector('.workspace').matches('.files-open,.aux-open')"), true, "auxiliary panes are hidden");
    const workspace = await geometry(".workspace"), editor = await geometry(".editor-pane"), controls = await geometry(".mobile-aux-tabs"), surface = await geometry(".editor-scroll");
    assert(Math.abs(editor.bottom - controls.top) < 1, `closed-pane editor meets controls at ${width}x${height}`);
    assert(Math.abs(controls.bottom - workspace.bottom) < 1, `closed-pane editor fills remaining workspace at ${width}x${height}`);
    assert.equal(await evaluate("document.querySelector('.workspace').scrollHeight<=document.querySelector('.workspace').clientHeight+1"), true, "closed panes do not require workspace scrolling");
    closedPaneGeometry.push({ width, height, workspace: workspace.height, editor: editor.height, surface: surface.height, controls: controls.height });
    if (process.env.THE_WORKBENCH_SCREENSHOTS) {
      const destination = resolve(process.env.THE_WORKBENCH_SCREENSHOTS);
      await mkdir(destination, { recursive: true });
      const screenshot = await cdp("Page.captureScreenshot", { format: "png" });
      await writeFile(join(destination, `workbench-${width}-editor-only.png`), Buffer.from(screenshot.data, "base64"));
    }
  }
  console.log("Closed-pane responsive geometry: " + JSON.stringify(closedPaneGeometry));
  await viewport(1280, 800);
  for (let index = 0; index < 2; index++) {
    await setInput("#the-command", "locate /alpha/");
    await evaluate("document.querySelector('.command-form').requestSubmit()");
    await wait(() => evaluate(`document.querySelectorAll('.dock-entry-response').length===${index + 1}`), "semantic response event");
  }
  assert.equal(await evaluate("document.querySelectorAll('.dock-entry-command').length"), 2);
  assert.equal(await evaluate("document.querySelector('.latest-response').textContent"), "Repeated response", "actual editor responses remain visible");
  snapshot.last_message_serial++; snapshot.last_message = "  "; publishSnapshot();
  await evaluate("new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)))");
  assert.equal(await evaluate("document.querySelector('.latest-response').textContent"), "Repeated response", "empty responses retain the latest nonempty message");
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
  tools.catalog.forEach(candidate => candidate.available = false); publishTools();
  await wait(() => evaluate("document.querySelector('#run-unavailable')?.textContent==='Classic Rexx is unavailable'"), "unavailable runtime reason");
  for (const [width, selector, reason] of [[1280, '.pane-switcher .run-rexx', '#run-unavailable'], [390, '.mobile-aux-tabs .run-rexx', '#mobile-run-unavailable']]) {
    await viewport(width, 844);
    await evaluate(`document.querySelector(${JSON.stringify(selector)}).scrollIntoView({block:'nearest'})`);
    assert.equal(await evaluate(`document.querySelector(${JSON.stringify(selector)}).disabled`), true, "unavailable Run remains disabled");
    assert.equal(await evaluate(`document.querySelector(${JSON.stringify(reason)}).textContent`), "Classic Rexx is unavailable", "unavailable Run reason is visible text");
    const control = await geometry(selector), explanation = await geometry(reason);
    assert(control.visible && explanation.visible && explanation.right<=width, "unavailable Run and reason fit the viewport");
  }
  await viewport(1280, 800);
  tools.catalog.forEach(candidate => candidate.available = true); publishTools();
  await wait(() => evaluate("!document.querySelector('.pane-switcher .run-rexx').disabled"), "runtime available again");
  await pane("Files"); await pane("Console");
  await evaluate("document.querySelector('.console-options').open=true");
  await setInput("[aria-label='Rexx arguments']", "first\n\nlast");
  const runCount = received.filter(message => message.type === "tool" && message.action === "run").length;
  snapshot.buffer.dirty = snapshot.buffers[0].dirty = 1; snapshot.source.revision++;
  publishSnapshot();
  await evaluate("document.querySelector('[aria-label=\"Hide program console\"]').click()");
  await evaluate("document.querySelector('.pane-switcher .run-rexx').click()");
  await wait(() => evaluate("document.querySelector('.run-state').textContent==='Running'"), "visible Run Rexx opens the console and runs");
  const runMessage = received.filter(message => message.type === "tool" && message.action === "run");
  assert.equal(runMessage.length, runCount + 1, "visible Run Rexx sends one run request");
  assert.deepEqual([runMessage.at(-1).tool, runMessage.at(-1).text, runMessage.at(-1).arguments], ["classic-rexx", "", "first\n\nlast"], "Run Rexx uses the current buffer capture route and retains arguments");
  assert.equal(await evaluate("document.querySelector('.pane-switcher .run-rexx').disabled && document.querySelector('#run-unavailable').textContent"), "A program is already running", "disabled Run reason is visible");

  // Use real browser key dispatch: default Tab traversal and Enter activation
  // cannot be qualified by dispatching synthetic KeyboardEvents alone.
  await setInput("#the-command", "draft editor command");
  await setInput("#rexx-input", "draft program input");
  await evaluate("document.querySelector('#the-command').setSelectionRange(2,7);document.querySelector('#rexx-input').setSelectionRange(3,8)");
  const profileMenusBefore = received.filter((message) => message.type === "key" && message.key === "f10").length;
  for (const selector of [".input-sink", "#the-command", "#rexx-input", ".console-output", ".console-run-controls button:nth-of-type(2)", ".console-heading button"]) {
    await evaluate(`window.menuInvoker=document.querySelector(${JSON.stringify(selector)});window.menuInvoker.focus()`);
    assert.equal(await evaluate("document.activeElement===window.menuInvoker"), true, `focus ${selector}`);
    await key("F10");
    await wait(() => evaluate("document.activeElement===document.querySelector('.menu-popover button')"), `F10 focuses first action from ${selector}`);
    await key("Escape");
    await wait(() => evaluate("!document.querySelector('.menu-popover') && document.activeElement===window.menuInvoker"), `Escape restores ${selector}`);
  }
  assert.equal(received.filter((message) => message.type === "key" && message.key === "f10").length, profileMenusBefore, "workbench F10 stays out of the editor key route");
  assert.deepEqual(await evaluate("[document.querySelector('#the-command').selectionStart,document.querySelector('#the-command').selectionEnd]"), [2, 7], "menu roundtrip retains command selection");
  assert.deepEqual(await evaluate("[document.querySelector('#rexx-input').selectionStart,document.querySelector('#rexx-input').selectionEnd]"), [3, 8], "menu roundtrip retains program input selection");

  await evaluate("document.querySelector('.input-sink').focus()");
  await key("F10");
  await wait(() => evaluate("document.activeElement===document.querySelector('.menu-popover button')"), "menu keyboard starting point");
  assert.equal(await evaluate("getComputedStyle(document.activeElement).backgroundColor"), "rgb(38, 79, 120)", "keyboard menu selection uses the shared blue highlight");
  await key("ArrowDown");
  assert.equal(await evaluate("document.activeElement.querySelector('span')?.textContent"), "Save", "Down selects next menu action");
  await key("ArrowUp");
  assert.equal(await evaluate("document.activeElement.querySelector('span')?.textContent"), "Open", "Up selects previous menu action");
  await key("ArrowUp");
  assert.equal(await evaluate("document.activeElement.textContent"), "About CRXTHE", "Up wraps to the last menu action");
  await key("ArrowDown");
  assert.equal(await evaluate("document.activeElement.querySelector('span')?.textContent"), "Open", "Down wraps to the first menu action");
  await key("End");
  assert.equal(await evaluate("document.activeElement.textContent"), "About CRXTHE", "End selects last menu action");
  await key("Home");
  assert.equal(await evaluate("document.activeElement.querySelector('span')?.textContent"), "Open", "Home selects first menu action");
  await key("Tab");
  assert.equal(await evaluate("document.activeElement.querySelector('span')?.textContent"), "Save", "Tab traverses ordinary menu buttons");
  await key("Tab", 8);
  await key("Tab", 8);
  assert.equal(await evaluate("document.activeElement.classList.contains('menu-trigger')"), true, "Shift-Tab can leave menu items for the trigger");
  await key("Tab");
  await key("ArrowDown");
  const savesBeforeMenu = received.filter((message) => message.type === "action" && message.action === "file.save").length;
  await key("Enter");
  await wait(() => received.filter((message) => message.type === "action" && message.action === "file.save").length === savesBeforeMenu + 1, "Enter activates selected menu action once");
  await wait(() => evaluate("!document.querySelector('.menu-popover') && document.activeElement.classList.contains('input-sink')"), "menu action restores editor work");
  assert.equal(received.filter((message) => message.type === "action" && message.action === "file.save").length, savesBeforeMenu + 1, "Enter does not duplicate the native button click");

  const editorTabsBefore = received.filter((message) => message.type === "key" && message.key === "tab").length;
  await key("Tab");
  await wait(() => received.filter((message) => message.type === "key" && message.key === "tab").length === editorTabsBefore + 1, "editor Tab remains a profile key");
  assert.equal(await evaluate("document.activeElement.classList.contains('input-sink')"), true, "editor Tab retains editor focus");
  const backtabsBefore = received.filter(message => message.type === "key" && message.key === "backtab").length;
  await key("Tab", 8);
  assert.equal(await evaluate("document.activeElement===document.querySelector('.pane-switcher .console-toggle')"), true, "editor Shift-Tab reaches visible Console");
  await key("Tab", 8);
  assert.equal(await evaluate("document.activeElement===document.querySelector('.menu-trigger')"), true, "next Shift-Tab reaches Menu");
  assert.equal(received.filter(message => message.type === "key" && message.key === "backtab").length, backtabsBefore, "workbench traversal never sends backtab to editor");
  for (const selector of ["#rexx-input", ".console-input button[type=submit]", ".console-run-controls button:last-child", ".console-output", ".console-heading button"]) {
    await evaluate(`document.querySelector(${JSON.stringify(selector)}).focus()`);
    await key("F6");
    assert.equal(await evaluate("document.activeElement.id"), "the-command", `F6 from ${selector} reaches THE command`);
    await evaluate(`document.querySelector(${JSON.stringify(selector)}).focus()`);
    await key("Escape");
    assert.equal(await evaluate("document.activeElement.className"), "input-sink", `Escape from ${selector} reaches editor`);
  }
  assert.equal(await evaluate("document.querySelector('#the-command').value"), "draft editor command", "console exit retains command draft");
  assert.equal(await evaluate("document.querySelector('#rexx-input').value"), "draft program input", "console exit retains program input draft");
  await evaluate("document.querySelector('#the-command').focus()");
  await key("Tab");
  assert.equal(await evaluate("document.activeElement===document.querySelector('.command-form button')"), true, "command Tab uses native control traversal");
  await evaluate("document.querySelector('#rexx-input').focus()");
  await key("Tab");
  assert.equal(await evaluate("document.activeElement===document.querySelector('.console-input button[type=submit]')"), true, "program input Tab uses native control traversal");
  assert.equal(received.filter((message) => message.type === "key" && message.key === "tab").length, editorTabsBefore + 1, "control Tab never reaches the editor");

  // Path dialogs and macro panels retain function-key ownership.
  await evaluate("document.querySelector('.input-sink').focus()");
  await key("F10");
  await wait(() => evaluate("document.activeElement===document.querySelector('.menu-popover button')"), "menu before path dialog");
  await key("ArrowDown");
  await key("ArrowDown");
  await key("Enter");
  await wait(() => evaluate("document.activeElement.id==='file-path'"), "path dialog owns focus");
  await key("F10");
  assert.equal(await evaluate("!!document.querySelector('.menu-popover')"), false, "F10 does not open the workbench menu over a path dialog");
  assert.equal(await evaluate("document.activeElement.id"), "file-path", "dialog retains its invoking focus");
  await evaluate("(() => { const input=document.querySelector('#file-path'); const setter=Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set; setter.call(input,'  exact path  '); input.dispatchEvent(new Event('input',{bubbles:true})); })()");
  await evaluate("document.querySelector('.dialog-actions button[type=submit]').click()");
  await wait(() => received.some((message) => message.type === "action" && message.action === "file.create" && message.argument === "  exact path  "), "path prompt preserves exact whitespace");
  await wait(() => evaluate("!document.querySelector('.path-dialog')"), "path prompt closes before next menu");
  await evaluate("document.querySelector('#the-command').focus()");
  await key("F10");
  await wait(() => evaluate("document.activeElement===document.querySelector('.menu-popover button')"), "command opened menu");
  await evaluate("[...document.querySelectorAll('.menu-popover button')].find(node=>node.textContent.trim().startsWith('Create')).click()");
  await wait(() => evaluate("document.activeElement.id==='file-path'"), "command invoked path prompt");
  await key("Escape");
  await wait(() => evaluate("document.activeElement.id==='the-command'"), "Escape returns to command input");
  await evaluate("document.querySelector('#rexx-input').focus()");
  await key("F10");
  await wait(() => evaluate("document.activeElement===document.querySelector('.menu-popover button')"), "program input opened menu");
  await evaluate("[...document.querySelectorAll('.menu-popover button')].find(node=>node.textContent.trim().startsWith('Create')).click()");
  await wait(() => evaluate("document.activeElement.id==='file-path'"), "program input invoked path prompt");
  await evaluate("document.querySelector('.dialog-actions button[type=button]').click()");
  await wait(() => evaluate("document.activeElement.id==='rexx-input'"), "Cancel returns to program input");
  snapshot.panel = { rows: 6, cols: 40, active: 0, cursor: 0,
    fields: [{ row: 1, cell: 1, width: 16, editable: 1, changed: 0, kind: "input", text: "Panel field" }] };
  publishSnapshot();
  await wait(() => evaluate("!!document.querySelector('.fullscreen-panel')"), "macro panel fixture");
  await evaluate("document.querySelector('.input-sink').focus()");
  const panelF10Before = received.filter((message) => message.type === "key" && message.key === "f10").length;
  await key("F10");
  await wait(() => received.filter((message) => message.type === "key" && message.key === "f10").length === panelF10Before + 1, "panel F10 follows the existing editor key route");
  assert.equal(await evaluate("!!document.querySelector('.menu-popover')"), false, "F10 does not open the workbench menu over a panel");
  delete snapshot.panel;
  publishSnapshot();
  await wait(() => evaluate("!document.querySelector('.fullscreen-panel')"), "macro panel closes");

  for (const [width, height] of [[1280, 800], [1920, 1080]]) {
    await viewport(width, height);
    const files = await geometry(".file-explorer"), editor = await geometry(".editor-pane"), programConsole = await geometry(".rexx-dock");
    assert(files.visible && editor.visible && programConsole.visible, "wide workbench panes visible");
    assert(files.right <= editor.left && editor.right <= programConsole.left, "wide panes do not overlap");
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
  const originalColours = snapshot.console_colours;
  const originalPath = snapshot.buffer.path;
  const toolVersionBeforePalette = tools.version;
  await evaluate("document.querySelector('#rexx-input').focus();document.querySelector('#rexx-input').setSelectionRange(2,5)");
  snapshot.console_colours = {
    body: { foreground: "#123456", background: "#102030", style: 0 },
    header: { foreground: "#ABCDEF", background: "#112233", style: 0 },
    input: { foreground: "#FEDCBA", background: "#223344", style: 0 },
    control: { foreground: "#A1B2C3", background: "#334455", style: 0 },
    selected: { foreground: "#556677", background: "#CCDDEE", style: 2 },
    divider: { foreground: "#6789AB", background: "#445566", style: 0 },
    error: { foreground: "#E14567", background: "#556677", style: 5 },
    status: { foreground: "#89ABCD", background: "#667788", style: 0 },
  };
  publishSnapshot();
  await wait(() => evaluate("getComputedStyle(document.querySelector('.console-output')).color==='rgb(18, 52, 86)'"), "snapshot recolours live console without a tool update");
  async function colours(selector) {
    return evaluate(`(() => {const style=getComputedStyle(document.querySelector(${JSON.stringify(selector)}));return [style.color,style.backgroundColor];})()`);
  }
  for (const [selector, expected] of [
    [".console-output", ["rgb(18, 52, 86)", "rgb(16, 32, 48)"]],
    [".console-heading", ["rgb(171, 205, 239)", "rgb(17, 34, 51)"]],
    ["#rexx-input", ["rgb(254, 220, 186)", "rgb(34, 51, 68)"]],
    [".console-run-controls button:nth-of-type(3)", ["rgb(161, 178, 195)", "rgb(51, 68, 85)"]],
    [".console-divider", ["rgb(103, 137, 171)", "rgb(68, 85, 102)"]],
    [".console-stderr", ["rgb(225, 69, 103)", "rgb(85, 102, 119)"]],
    [".run-state", ["rgb(137, 171, 205)", "rgb(102, 119, 136)"]],
    [".run-program", ["rgb(137, 171, 205)", "rgb(102, 119, 136)"]],
  ]) assert.deepEqual(await colours(selector), expected, `console role on ${selector}`);
  assert.equal(await evaluate("getComputedStyle(document.querySelector('.console-stderr')).fontWeight"), "700");
  assert.equal(await evaluate("getComputedStyle(document.querySelector('.console-stderr')).textDecorationLine"), "underline");
  assert.equal(await evaluate("document.activeElement.id"), "rexx-input", "palette update preserves input focus");
  assert.deepEqual(await evaluate("[document.activeElement.selectionStart,document.activeElement.selectionEnd]"), [2, 5]);
  assert.equal(await evaluate("document.querySelector('.console-output').scrollTop"), 0, "palette update preserves scrollback");
  assert.equal(tools.version, toolVersionBeforePalette, "colour updates do not require a tool event");
  await key("Tab");
  assert.deepEqual(await colours(".console-input button[type=submit]"), ["rgb(204, 221, 238)", "rgb(85, 102, 119)"], "focused control uses CONSOLESELECTED with REVERSE");
  snapshot.buffer.path = snapshot.buffers[0].path = "/workspace/other-colours.rexx";
  snapshot.console_colours = originalColours;
  publishSnapshot();
  await wait(() => evaluate("getComputedStyle(document.querySelector('.console-output')).color==='rgb(212, 212, 212)'"), "file switch changes console palette immediately");
  assert.equal(await evaluate("window.originalConsole===document.querySelector('.rexx-dock')"), true, "palette and file switches preserve console identity");
  assert.equal(await evaluate("document.querySelector('#rexx-input').value"), "unsent input");
  assert.equal(await evaluate("document.querySelector('#the-command').value"), "unsent command");
  snapshot.buffer.path = snapshot.buffers[0].path = originalPath;
  publishSnapshot();
  await evaluate("document.querySelector('[aria-label=\"Hide program console\"]').click()"); await pane("Console");
  await viewport(390, 844);
  assert.equal(await evaluate("document.documentElement.scrollWidth<=innerWidth"), true, "phone shell has no horizontal overflow");
  assert.equal((await geometry(".pane-switcher")).visible, false, "phone has one auxiliary selector");
  assert.equal((await geometry(".mobile-aux-tabs")).visible, true);
  assert.equal(await evaluate("!!document.querySelector('.diagnostics, .diagnostic-row')"), false,
    "phone has no Diagnostics pane");
  assert.equal(await evaluate("[...document.querySelectorAll('.mobile-aux-tabs button')].some(button => button.textContent.startsWith('Diagnostics'))"), false,
    "phone controls use the same editor/status diagnostic presentation");
  await viewport(390, 844);
  await evaluate("[...document.querySelectorAll('.mobile-aux-tabs button')].find(n=>n.textContent==='Files').click()");
  await wait(() => evaluate("document.querySelector('.workspace').classList.contains('mobile-pane-files')"), "phone files selection");
  assert.equal((await geometry(".mobile-aux-tabs")).visible, true, "selector remains available with Files selected");
  await evaluate("[...document.querySelectorAll('.mobile-aux-tabs button')].find(n=>n.textContent.startsWith('Console')).click()");
  await wait(() => evaluate("document.querySelector('.workspace').classList.contains('mobile-pane-console')"), "phone program console selection");
  const editor = await geometry(".editor-pane"), commands = await geometry(".command-area"), programConsole = await geometry(".rexx-dock");
  assert(editor.top < commands.top && commands.bottom <= programConsole.top && programConsole.right <= 390, "phone editor/command/console vertical flow");
  assert.equal((await geometry(".file-explorer")).visible, false, "phone has one selected auxiliary pane");
  for (const width of [320, 390, 768]) {
    await viewport(width, 844);
    assert.equal(await evaluate("document.documentElement.scrollWidth<=innerWidth"), true, `no shell overflow at ${width}`);
    assert.equal(await evaluate("(() => {const n=document.querySelector('.key-hints');return n.scrollWidth<=n.clientWidth && n.scrollHeight<=n.clientHeight && n.textContent==='F1 Profile help F6 Cmd/File F10 Menu Esc Cmd Ctrl-S Save';})()"), true, `entire named profile footer wraps at ${width}`);
    const surface = await geometry(".editor-scroll"), status = await geometry(".editor-status"), commands = await geometry(".command-area");
    assert(Math.abs(surface.bottom - status.top) < 1 && status.bottom <= commands.top, `phone footer follows editor at ${width}`);
  }
  await viewport(390, 844);
  assert.equal((await geometry(".mobile-aux-tabs .run-rexx")).visible, true, "phone has visible Run Rexx");
  assert.equal(await evaluate("document.querySelector('.mobile-aux-tabs .run-rexx').disabled && document.querySelector('#mobile-run-unavailable').textContent"), "A program is already running", "phone shows disabled Run reason");
  await evaluate("document.querySelector('.input-sink').focus()");
  await key("Tab", 8);
  assert.equal(await evaluate("document.activeElement===document.querySelector('.mobile-aux-tabs .console-toggle')"), true, "phone Shift-Tab reaches visible Console");
  await key("Tab", 8);
  assert.equal(await evaluate("document.activeElement===document.querySelector('.menu-trigger')"), true, "phone next Shift-Tab reaches Menu");
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
  await wait(() => evaluate("!document.querySelector('.about-menu') && document.activeElement===document.querySelector('.menu-popover button')"), "About Back to Menu focus");
  await evaluate("[...document.querySelectorAll('.menu-popover button')].find(n=>n.textContent==='About CRXTHE').click()");
  await wait(() => evaluate("document.querySelector('.about-menu').contains(document.activeElement)"), "About focus after reopening");
  await evaluate("document.activeElement.dispatchEvent(new KeyboardEvent('keydown',{key:'Escape',bubbles:true,cancelable:true}))");
  await wait(() => evaluate("!document.querySelector('.menu-popover') && document.activeElement.classList.contains('input-sink')"), "F10 Menu Escape returns to editing");
  tools.job.running = false; tools.job.waiting = false; tools.job.state = "Completed"; publishTools();
  await wait(() => evaluate("!document.querySelector('.mobile-aux-tabs .run-rexx').disabled"), "phone runtime ready");
  const phoneRunsBefore = received.filter(message => message.type === "tool" && message.action === "run").length;
  await evaluate("document.querySelector('.mobile-aux-tabs .run-rexx').click()");
  await wait(() => received.filter(message => message.type === "tool" && message.action === "run").length === phoneRunsBefore + 1, "phone visible Run starts current buffer");
  await wait(() => evaluate("document.querySelector('.run-state').textContent==='Running' && document.querySelector('.workspace').classList.contains('mobile-pane-console')"), "phone Run opens the console");
  assert.equal(await evaluate("document.querySelector('#rexx-input').value"), "unsent input", "phone run retains unsent stdin draft");
  assert.equal(await evaluate("document.querySelector('#the-command').value"), "unsent command", "phone run retains editor command draft");
  await viewport(390, 450);
  await evaluate("document.querySelector('#rexx-input').scrollIntoView({block:'nearest'})");
  assert((await geometry("#rexx-input")).bottom <= 450, "program input stays reachable in a short phone viewport");
  for (const [width, height] of [[390, 844], [800, 700]]) {
    await viewport(width, height);
    assert.equal(await evaluate("document.querySelector('.workspace').scrollHeight>document.querySelector('.workspace').clientHeight"), true, `open console keeps scrollable workspace at ${width}x${height}`);
    await evaluate("document.querySelector('#rexx-input').scrollIntoView({block:'nearest'})");
    const input = await geometry("#rexx-input");
    assert(input.top >= 0 && input.bottom <= height, `open console input remains reachable at ${width}x${height}`);
  }
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
  const preservedEditor = await evaluate("({draft:document.querySelector('#the-command').value,dirty:document.querySelector('.status-line').dataset.bufferDirty,zone:document.querySelector('.status-line').dataset.focusZone,line:document.querySelector('.status-line').dataset.focusLine,cell:document.querySelector('.status-line').dataset.focusCell})");
  const poisoned = 'Check / <img src=x onerror="window.poisoned=true">';
  publishWorkbench(2, 24, poisoned);
  await evaluate("document.querySelector('.input-sink').focus();document.querySelector('.menu-trigger').click()");
  await wait(() => evaluate("!!document.querySelector('[data-menu-id=\"user.tools\"]')"), "profile menu published");
  client.send(JSON.stringify({ type: "workbench", operation: "list", data: {
    schemaVersion: 1, revision: 2, section: "menus", total: 4, offset: 1, nextOffset: 2,
    records: [{ id: "user.tools", owner: "user.tools", label: "Stale Tools", generation: 24, order: 50 }],
  } }));
  const statusBeforeStalePage = snapshot.status_line;
  snapshot.status_line = "STALE_PAGE_BARRIER"; publishSnapshot();
  await wait(() => evaluate("document.querySelector('.profile-status')?.textContent === 'STALE_PAGE_BARRIER'"), "stale page delivery barrier");
  assert.equal(await evaluate("document.querySelector('[data-menu-id=\"user.tools\"] h2')?.textContent"),
    "Tools", "unversioned page cannot replace a numbered publication");
  snapshot.status_line = statusBeforeStalePage; publishSnapshot();
  assert.deepEqual(await evaluate("[...document.querySelectorAll('.menu-popover section[data-menu-id]')].map(node=>node.dataset.menuId)"),
    ["the.file", "user.tools", "user.zmenu", "user.amenu"], "equal menu order retains definition sequence");
  assert.deepEqual(await evaluate("[...document.querySelector('[data-menu-id=\"user.tools\"]').children].slice(1).map(node=>node.dataset.itemId||node.tagName.toLowerCase())"),
    ["user.item", "user.zitem", "hr", "user.aitem", "user.bounditem"], "equal item order retains definition sequence including separator");
  assert.equal(await evaluate("document.querySelector('[data-menu-id=\"user.tools\"] button span').textContent"), poisoned, "profile label is plain text");
  assert.equal(await evaluate("!!document.querySelector('.menu-popover img') || !!window.poisoned"), false, "poisoned label cannot create markup");
  await evaluate("document.querySelector('[data-menu-id=\"user.tools\"] button').click()");
  await wait(() => received.some((message) => message.type === "workbench" && message.operation === "invoke" && message.action === "user.run"), "semantic action invoke");
  assert.equal(received.filter((message) => message.type === "workbench" && message.operation === "invoke" && message.action === "user.run").length, 1, "menu activation runs once");
  assert.equal(workbenchRuns, 1);
  publishWorkbench(3, 25, "Replaced action");
  await evaluate("document.querySelector('.menu-trigger').click()");
  await wait(() => evaluate("document.querySelector('[data-menu-id=\"user.tools\"] button span')?.textContent === 'Replaced action'"), "replaced menu item");
  assert.equal(await evaluate("document.querySelector('[data-menu-id=\"user.tools\"] button').dataset.generation"), "25");
  await evaluate("document.querySelector('[data-menu-id=\"user.tools\"] button').click()");
  await wait(() => workbenchRuns === 2, "replaced generation invoke");
  publishWorkbench(3, 25, "Replaced action", "policy-denied");
  await evaluate("document.querySelector('.menu-trigger').click()");
  await wait(() => evaluate("document.querySelector('[data-action-id=\"the.file.save\"]')?.disabled === true"), "same-revision policy refusal refresh");
  assert.equal(await evaluate("document.querySelector('[data-action-id=\"the.file.save\"]').title"), "policy-denied");
  publishWorkbench(3, 25, "Replaced action", "ok");
  await wait(() => evaluate("document.querySelector('[data-action-id=\"the.file.save\"]')?.disabled === false"), "same-revision policy recovery refresh");
  const filesBeforeBoundOpen = received.filter((message) => message.type === "files").length;
  const explorerBeforeBoundOpen = await evaluate("!!document.querySelector('.file-explorer')");
  await evaluate("document.querySelector('[data-item-id=\"user.bounditem\"]').click()");
  await wait(() => received.some((message) => message.type === "workbench" && message.operation === "invoke" && message.action === "user.boundopen"), "bound Open invokes semantic action");
  assert.equal(received.filter((message) => message.type === "files").length, filesBeforeBoundOpen, "bound Open skips chooser request");
  assert.equal(await evaluate("!!document.querySelector('.file-explorer')"), explorerBeforeBoundOpen, "bound Open preserves explorer state");
  assert.equal(received.filter((message) => message.type === "workbench" && message.operation === "invoke" && message.action === "user.boundopen").at(-1).argument,
    undefined, "bound Open keeps registered operand");
  publishWorkbench(4, 0, "");
  await evaluate("document.querySelector('.menu-trigger').click()");
  await wait(() => evaluate("!document.querySelector('[data-menu-id=\"user.tools\"]')"), "removed menu item");
  await evaluate("document.querySelector('.menu-trigger').click()");
  assert.deepEqual(await evaluate("({draft:document.querySelector('#the-command').value,dirty:document.querySelector('.status-line').dataset.bufferDirty,zone:document.querySelector('.status-line').dataset.focusZone,line:document.querySelector('.status-line').dataset.focusLine,cell:document.querySelector('.status-line').dataset.focusCell})"), preservedEditor, "menu publication and activation preserve editor state");
  fixturePanels = [{ id: "user.output", owner: "user.logs", title: "Named output", generation: "1",
    kind: "log", model: "user.outputlog", modelGeneration: "1", placement: "right",
    fallbacks: ["below", "tab"], mincols: 10, minrows: 4, visible: true,
    focused: false, effectivePlacement: "none", reason: "ok", contentRevision: "2", modelError: "ok" },
  { id: "user.errors", owner: "user.logs", title: "Error output", generation: "1",
    kind: "log", model: "user.errorlog", modelGeneration: "1", placement: "below",
    fallbacks: ["tab"], mincols: 10, minrows: 4, visible: true,
    focused: false, effectivePlacement: "none", reason: "ok", contentRevision: "2", modelError: "ok" }];
  fixturePanelState = { type: "workbench", operation: "panel-state", revision: 5,
    stateRevision: "1", focus: { serial: "0", panel: "", generation: "0",
      previousPanel: "", previousGeneration: "0", restoring: false },
    panels: [{ id: "user.output", generation: "1", visible: true, focused: false,
      effectivePlacement: "none", reason: "ok", model: "user.outputlog",
      modelGeneration: "1", contentRevision: "2", omittedBytes: "4", omittedRecords: "1", modelError: "ok" },
    { id: "user.errors", generation: "1", visible: true, focused: false,
      effectivePlacement: "none", reason: "ok", model: "user.errorlog",
      modelGeneration: "1", contentRevision: "2", omittedBytes: "0", omittedRecords: "0", modelError: "ok" }] };
  publishWorkbench(5, 0, "");
  client.send(JSON.stringify(fixturePanelState));
  await wait(() => evaluate("document.querySelector('.named-log-toggle')?.textContent.includes('Named output')"), "named log tab");
  await evaluate("document.querySelector('.named-log-toggle').click()");
  await wait(() => evaluate("document.querySelector('.named-log-output')?.textContent.includes('safe <img>')"), "named log output");
  assert.equal(await evaluate("!!document.querySelector('.named-log-output img')"), false, "log text cannot create markup");
  assert.equal(await evaluate("document.querySelector('.named-log-output')?.textContent.includes('\\\\x07')"), true, "log control byte is visible escaped text");
  assert.equal(await evaluate("!!document.querySelector('.named-log-omitted')"), true, "retention omission is visible");
  await wait(() => received.some((message) => message.type === "workbench" && message.operation === "panel.layout" && message.placement === "right"), "wide measured placement");
  await evaluate("(() => { const output=document.querySelector('.named-log-output');output.scrollTop=100;output.dispatchEvent(new Event('scroll')); })()");
  await evaluate("document.querySelector('.named-log-toggle[data-panel-id=\"user.errors\"]').click()");
  await wait(() => evaluate("document.querySelector('.named-log-output')?.textContent.includes('second output')"), "second named log content");
  await wait(() => evaluate("document.querySelector('.workspace').classList.contains('aux-below')"), "preferred BELOW uses actual wide layout");
  await wait(() => received.some((message) => message.type === "workbench" && message.operation === "panel.layout" && message.panel === "user.errors" && message.placement === "below"), "wide below placement");
  await wait(() => received.some((message) => message.type === "workbench" && message.operation === "panel.layout" && message.panel === "user.output" && message.placement === "inactive-tab"), "inactive named pane reports tab");
  await evaluate("(() => { const output=document.querySelector('.named-log-output');output.scrollTop=40;output.dispatchEvent(new Event('scroll')); })()");
  await evaluate("document.querySelector('.named-log-toggle[data-panel-id=\"user.output\"]').click()");
  await wait(() => evaluate("document.querySelector('.named-log-output')?.textContent.includes('safe <img>')"), "first named log restored");
  assert.equal(await evaluate("document.querySelector('.named-log-output').scrollTop"), 100, "first log retains its scroll position");
  await evaluate("document.querySelector('.named-log-toggle[data-panel-id=\"user.errors\"]').click()");
  await wait(() => evaluate("document.querySelector('.named-log-output')?.textContent.includes('second output')"), "second named log restored");
  assert.equal(await evaluate("document.querySelector('.named-log-output').scrollTop"), 40, "second log retains independent scroll position");
  await evaluate("(() => { const output=document.querySelector('.named-log-output');output.scrollTop=output.scrollHeight;output.dispatchEvent(new Event('scroll')); })()");
  fixtureLogExtras.set("user.outputlog", "live first output\n");
  fixtureLogExtras.set("user.errorlog", "live second output\n");
  fixturePanelState = { ...fixturePanelState, stateRevision: "2",
    panels: fixturePanelState.panels.map((panel) => ({ ...panel, contentRevision: "3" })) };
  client.send(JSON.stringify(fixturePanelState));
  await wait(() => evaluate("document.querySelector('.named-log-output')?.textContent.includes('live second output')"), "live append to following second log");
  assert.equal(await evaluate("(() => { const node=document.querySelector('.named-log-output');return node.scrollHeight-node.scrollTop-node.clientHeight<24; })()"), true,
    "second log follows live output");
  await evaluate("document.querySelector('.named-log-toggle[data-panel-id=\"user.output\"]').click()");
  await wait(() => evaluate("document.querySelector('.named-log-output')?.textContent.includes('live first output')"), "inactive first log received live append");
  assert.equal(await evaluate("document.querySelector('.named-log-output').scrollTop"), 100,
    "first log keeps scrolled-back position after live append");
  fixturePanels[0] = { ...fixturePanels[0], mincols: 80 };
  publishWorkbench(6, 0, "");
  await wait(() => evaluate("document.querySelector('.workspace').classList.contains('aux-below')"), "right mincols fallback renders below at wide width");
  await wait(() => received.some((message) => message.type === "workbench" && message.operation === "panel.layout" && message.panel === "user.output" && message.placement === "below"), "right mincols fallback reports below");
  fixturePanels[0] = { ...fixturePanels[0], mincols: 10 };
  publishWorkbench(7, 0, "");
  await wait(() => evaluate("!document.querySelector('.workspace').classList.contains('aux-below')"), "restored right pane uses rail");
  await viewport(768, 800);
  await wait(() => received.some((message) => message.type === "workbench" && message.operation === "panel.layout" && message.panel === "user.errors" && message.placement === "below"), "narrow below placement");
  await evaluate("document.querySelector('.named-log-toggle[data-panel-id=\"user.output\"]').click()");
  await viewport(390, 844);
  await wait(() => received.some((message) => message.type === "workbench" && message.operation === "panel.layout" && message.placement === "tab"), "phone measured placement");
  await viewport(1920, 1080);
  await evaluate("document.querySelector('.named-log-output').dispatchEvent(new KeyboardEvent('keydown',{key:'Escape',bubbles:true,cancelable:true}))");
  await wait(() => fixturePanelState.panels[0].focused === false, "initial named panel focus returned");
  await evaluate("document.querySelector('#the-command').focus();document.querySelector('.named-log-toggle').click()");
  await wait(() => evaluate("document.activeElement?.classList.contains('named-log-output')"), "named panel keyboard focus");
  await evaluate("document.activeElement.dispatchEvent(new KeyboardEvent('keydown',{key:'Escape',bubbles:true,cancelable:true}))");
  await wait(() => evaluate("document.activeElement?.id==='the-command'"), "named panel returns previous command focus");
  await evaluate("[...document.querySelectorAll('.auxiliary-tabs [role=tab]')].find(node=>node.textContent==='Program console').click()");
  await evaluate("(() => { const input=document.querySelector('#rexx-input');input.focus();input.setSelectionRange(2,6);document.querySelector('.named-log-toggle').click(); })()");
  await wait(() => evaluate("document.activeElement?.classList.contains('named-log-output')"), "named panel focus from Program input");
  await evaluate("document.activeElement.dispatchEvent(new KeyboardEvent('keydown',{key:'Escape',bubbles:true,cancelable:true}))");
  await wait(() => evaluate("document.activeElement?.id==='rexx-input'"), "Escape returns Program input focus");
  assert.deepEqual(await evaluate("({value:document.querySelector('#rexx-input').value,start:document.querySelector('#rexx-input').selectionStart,end:document.querySelector('#rexx-input').selectionEnd,visible:!!document.querySelector('.program-console-pane').getClientRects().length})"),
    { value: "unsent input", start: 2, end: 6, visible: true }, "Escape restores Program draft and selection");
  await evaluate("document.querySelector('.named-log-toggle').click()");
  await wait(() => evaluate("document.activeElement?.classList.contains('named-log-output')"), "named panel focus before Program tab");
  await evaluate("[...document.querySelectorAll('.auxiliary-tabs [role=tab]')].find(node=>node.textContent==='Program console').click()");
  await wait(() => evaluate("document.activeElement?.id==='rexx-input'"), "Program tab returns input focus");
  assert.equal(await evaluate("document.querySelector('.program-console-pane').getClientRects().length>0 && document.querySelector('#rexx-input').selectionStart===2 && document.querySelector('#rexx-input').selectionEnd===6"), true,
    "Program tab restores pane and exact selection");
  assert.equal(fixturePanelState.panels[0].focused, false, "Program tab queues semantic panel RETURN");
  fixturePanels[0] = { ...fixturePanels[0], visible: false };
  fixturePanelState = { ...fixturePanelState, stateRevision: "3",
    panels: fixturePanelState.panels.map((panel) => panel.id === "user.output"
      ? { ...panel, visible: false, focused: false, reason: "hidden" } : panel) };
  publishWorkbench(8, 0, "");
  client.send(JSON.stringify(fixturePanelState));
  await wait(() => evaluate("!document.querySelector('.named-log-pane[data-panel-id=\"user.output\"]')"), "hidden LOG starts PANEL SHOW case");
  await evaluate("document.querySelector('#rexx-input').focus();document.querySelector('#rexx-input').setSelectionRange(2,6)");
  await key("F10");
  await wait(() => evaluate("!!document.querySelector('[data-item-id=\"user.showitem\"]')"), "Program input opens PANEL SHOW menu item");
  await evaluate("document.querySelector('[data-item-id=\"user.showitem\"]').click()");
  await wait(() => evaluate("document.activeElement?.id==='rexx-input'"), "PANEL SHOW retains Program input focus");
  await wait(() => evaluate("!!document.querySelector('.named-log-pane[data-panel-id=\"user.output\"]')?.getClientRects().length && !!document.querySelector('.program-console-pane')?.getClientRects().length"),
    "PANEL SHOW displays LOG body alongside Program input");
  assert.equal(await evaluate("document.querySelector('#rexx-input').selectionStart===2 && document.querySelector('#rexx-input').selectionEnd===6"), true,
    "PANEL SHOW retains Program input selection");
  assert.equal(fixturePanelState.panels[0].visible, true, "PANEL SHOW receives visible runtime state");
  await viewport(390, 844);
  assert.equal(await evaluate("!!document.querySelector('.named-log-pane[data-panel-id=\"user.output\"]')?.getClientRects().length && !!document.querySelector('.program-console-pane')?.getClientRects().length && document.activeElement?.id==='rexx-input'"), true,
    "phone SHOW keeps LOG and Program accessible without taking typing focus");
  await viewport(1920, 1080);
  await evaluate("document.querySelector('[aria-label=\"Hide program console\"]').click()");
  await wait(() => evaluate("document.querySelector('.program-console-pane')?.hidden && !document.querySelector('.auxiliary-rail')?.classList.contains('program-and-log') && !!document.querySelector('.named-log-pane[data-panel-id=\"user.output\"]')?.getClientRects().length"),
    "hiding Program lets LOG fill the rail");
  await evaluate("document.querySelector('.pane-switcher .console-toggle').click()");
  await wait(() => evaluate("!document.querySelector('.program-console-pane')?.hidden"), "reopened Program pane");
  assert.equal(await evaluate("document.querySelector('#rexx-input').value==='unsent input' && document.querySelector('#rexx-input').selectionStart===2 && document.querySelector('#rexx-input').selectionEnd===6"), true,
    "reopening Program retains draft and selection");
  fixturePanels[0] = { ...fixturePanels[0], visible: false };
  fixturePanelState = { ...fixturePanelState, stateRevision: "5",
    panels: fixturePanelState.panels.map((panel) => panel.id === "user.output"
      ? { ...panel, visible: false, focused: false, reason: "hidden" } : panel) };
  publishWorkbench(9, 0, "");
  client.send(JSON.stringify(fixturePanelState));
  await wait(() => evaluate("!document.querySelector('.named-log-pane[data-panel-id=\"user.output\"]')"), "hidden LOG starts command SHOW case");
  await evaluate("document.querySelector('#the-command').focus()");
  await key("F10");
  await wait(() => evaluate("!!document.querySelector('[data-item-id=\"user.showitem\"]')"), "command input opens PANEL SHOW menu item");
  await evaluate("document.querySelector('[data-item-id=\"user.showitem\"]').click()");
  await wait(() => evaluate("document.activeElement?.id==='the-command'"), "PANEL SHOW retains command input focus");
  await wait(() => evaluate("!!document.querySelector('.named-log-pane[data-panel-id=\"user.output\"]')?.getClientRects().length"),
    "PANEL SHOW displays LOG body from command input");
  assert.equal(fixturePanelState.panels[0].focused, false, "PANEL SHOW does not take core focus");
  assert.deepEqual(await evaluate("({draft:document.querySelector('#the-command').value,dirty:document.querySelector('.status-line').dataset.bufferDirty,zone:document.querySelector('.status-line').dataset.focusZone,line:document.querySelector('.status-line').dataset.focusLine,cell:document.querySelector('.status-line').dataset.focusCell})"), preservedEditor, "named output does not mutate editor state");
  const logFixturePanels = fixturePanels;
  fixturePanels = ["user.reference", "user.reference2"].map((id, index) => ({
    ...logFixturePanels[0], id, title: `Reference ${index + 1}`, kind: "document", generation: "9",
    model: "", modelGeneration: "0", readonly: true, documentId: String(9007199254740993n + BigInt(index)),
    documentRevision: "9007199254740997", documentAvailable: true, documentReason: "ok", lineCount: 24,
    visible: true, focused: false, placement: "right", mincols: 10,
  }));
  fixturePanelState = { ...fixturePanelState, revision: 10, stateRevision: "10",
    panels: fixturePanels.map((panel) => ({ ...panel })),
    focus: { serial: "20", panel: "", generation: "0", previousPanel: "", previousGeneration: "0", restoring: false } };
  publishWorkbench(10, 0, "");
  client.send(JSON.stringify(fixturePanelState));
  await viewport(1280, 800);
  await evaluate("document.querySelector('#the-command').focus()");
  await wait(() => evaluate("!!document.querySelector('.named-document-toggle')"), "DOCUMENT selector");
  await evaluate("document.querySelector('.named-document-toggle[data-panel-id=\"user.reference\"]').click()");
  await wait(() => evaluate("document.activeElement?.classList.contains('named-document-output') && document.querySelector('.named-document-output')?.textContent.includes('line 1')"), "bounded DOCUMENT text and focus");
  const documentReads = received.filter((message) => message.operation === "document.list");
  assert(documentReads.every((message) => message.records === 8 && message.bytes === 8192
    && message.expectGeneration === "9" && message.expectRevision === "9007199254740997"), "copied document bounds and exact string tokens");
  assert.equal(await evaluate("document.querySelectorAll('.named-document-line').length"), 8, "document retains one bounded page");
  assert.equal(await evaluate("!!document.querySelector('.named-document-output img')"), false, "DOCUMENT text cannot create markup");
  assert.equal(await evaluate("document.querySelector('.named-document-output').textContent.includes('\\\\x07')"), true, "DOCUMENT control bytes are visible escaped text");
  assert.equal(await evaluate("/Read only/.test(document.querySelector('.named-document-status').textContent) && !document.querySelector('.named-log-omitted,.named-log-error,.named-log-output')"), true, "DOCUMENT read-only status has no log semantics");
  const beforeDocumentInput = received.length;
  const deniedDocumentEvents = await evaluate(`(() => { const node=document.querySelector('.named-document-output');return [
    ...['a','Delete','Backspace','Enter'].map(key=>new KeyboardEvent('keydown',{key,bubbles:true,cancelable:true})),
    ...['s','z','y','v'].map(key=>new KeyboardEvent('keydown',{key,ctrlKey:true,bubbles:true,cancelable:true})),
    new Event('beforeinput',{bubbles:true,cancelable:true}),new Event('paste',{bubbles:true,cancelable:true}),new Event('drop',{bubbles:true,cancelable:true})
  ].map(event=>!node.dispatchEvent(event));})()`);
  assert(deniedDocumentEvents.every(Boolean), "DOCUMENT consumes typing, paste, drop, save, delete and undo");
  assert.equal(received.slice(beforeDocumentInput).some((message) => ["key", "text", "command", "action"].includes(message.type)), false, "DOCUMENT mutations cannot reach editor");
  await evaluate("document.querySelector('.named-document-navigation button:nth-child(2)').click()");
  await wait(() => evaluate("document.querySelector('.named-document-line')?.textContent.includes('line 9')"), "DOCUMENT next bounded page");
  await evaluate("(() => {const node=document.querySelector('.named-document-output');node.scrollLeft=130;node.dispatchEvent(new Event('scroll'));})()");
  await evaluate("document.querySelector('.named-document-toggle[data-panel-id=\"user.reference2\"]').click()");
  await wait(() => evaluate("document.querySelector('.named-document-output')?.textContent.includes('user.reference2')"), "second DOCUMENT binding");
  assert.equal(await evaluate("document.querySelector('.named-document-output').scrollLeft"), 0, "second DOCUMENT starts at its own viewport");
  await evaluate("document.querySelector('.named-document-toggle[data-panel-id=\"user.reference\"]').click()");
  await wait(() => evaluate("document.querySelector('.named-document-line')?.textContent.includes('line 9')"), "first DOCUMENT page restored");
  assert.equal(await evaluate("document.querySelector('.named-document-output').scrollLeft"), 130, "DOCUMENT viewport is independent per pane generation");
  await evaluate("document.querySelector('.named-document-navigation button:first-child').click()");
  await wait(() => evaluate("document.querySelector('.named-document-line')?.textContent.includes('line 1')"), "DOCUMENT previous bounded page");
  for (const [width, placement] of [[1280, "right"], [768, "below"], [390, "tab"]]) {
    await viewport(width, 800);
    await wait(() => received.some((message) => message.operation === "panel.layout"
      && message.panel === "user.reference" && message.placement === placement), `DOCUMENT measured ${placement} placement`);
    assert((await geometry('.named-document-output')).width <= width, "DOCUMENT fits measured viewport");
  }
  await key("Escape");
  await wait(() => evaluate("document.activeElement?.id==='the-command'"), "DOCUMENT RETURN restores invoking control");
  holdDocumentPage = true;
  fixturePanelState = { ...fixturePanelState, stateRevision: "11", panels: fixturePanelState.panels.map((panel) => ({ ...panel, documentRevision: "9007199254740998" })) };
  client.send(JSON.stringify(fixturePanelState));
  await wait(() => heldDocumentPages.length === 2, "old revision DOCUMENT reads held");
  fixturePanelState = { ...fixturePanelState, stateRevision: "12", panels: fixturePanelState.panels.map((panel) => ({ ...panel, documentRevision: "9007199254740999" })) };
  client.send(JSON.stringify(fixturePanelState));
  holdDocumentPage = false;
  for (const page of heldDocumentPages.splice(0)) client.send(JSON.stringify(page));
  await wait(() => evaluate("document.querySelector('.named-document-output')?.textContent.includes('revision 9007199254740999')"), "DOCUMENT rejects stale copied revision and refreshes");
  fixturePanelState = { ...fixturePanelState, stateRevision: "13", panels: fixturePanelState.panels.map((panel) => ({ ...panel, documentAvailable: false, documentReason: "document-closed" })) };
  client.send(JSON.stringify(fixturePanelState));
  await wait(() => evaluate("document.querySelector('.named-document-status')?.textContent.includes('document closed') && !document.querySelector('.named-document-line')"), "closed DOCUMENT invalidates copied display");
  fixturePanels = fixturePanels.map((panel) => ({ ...panel, generation: "10", documentId: String(BigInt(panel.documentId) + 100n) }));
  fixturePanelState = { ...fixturePanelState, stateRevision: "14", panels: fixturePanels.map((panel) => ({ ...panel, documentRevision: "9007199254741000" })) };
  publishWorkbench(11, 0, "");
  client.send(JSON.stringify(fixturePanelState));
  await wait(() => evaluate("document.querySelector('.named-document-output')?.textContent.includes('revision 9007199254741000')"), "replacement DOCUMENT generation copies fresh page");
  assert.equal(await evaluate("document.querySelector('.named-document-output').scrollLeft"), 0, "replacement DOCUMENT generation does not inherit retired viewport");
  for (const [id, label] of [["user.person", "Name"], ["user.options", "Option"]]) {
    const fields = Array.from({ length: 12 }, (_, index) => ({ id: `${id}.field${index}`, generation: String(index + 1),
      label: index === 0 ? label : index === 1 ? "Protected value" : `Field ${index}`,
      access: index === 1 ? "readonly" : "edit", maxcells: 1024,
      value: index === 1 ? "safe <img> protected" : index === 0 ? "Guest" : `value ${index}`, length: 5, dirty: false }));
    fixtureForms.set(id, { generation: "9007199254741001", draftRevision: "9007199254741002", resultRevision: "0",
      submitted: false, dirty: false, fields, baseline: fields.map((field) => field.value) });
  }
  fixturePanels = [...fixtureForms].map(([form, state], index) => ({ ...logFixturePanels[0],
    id: `${form}.pane`, title: index ? "Options" : "Person", kind: "fields", generation: "20", visible: false,
    model: "", modelGeneration: "0", form, formGeneration: state.generation,
    draftRevision: state.draftRevision, resultRevision: "0", formDirty: false, submitted: false, fieldCount: 12,
  }));
  fixturePanelState = { ...fixturePanelState, revision: 12, stateRevision: "20", panels: fixturePanels.map((panel) => ({ ...panel })),
    focus: { ...fixturePanelState.focus, serial: "40", restoring: false, panel: "" } };
  publishWorkbench(12, 0, ""); client.send(JSON.stringify(fixturePanelState));
  await viewport(1280, 800);
  await evaluate("document.querySelector('#the-command').focus();document.querySelector('.named-form-toggle[data-panel-id=\"user.person.pane\"]').click()");
  await wait(() => evaluate("document.querySelector('.named-form-field input')?.value==='Guest'"), "FORM bounded field pages");
  assert.equal(await evaluate("document.activeElement?.id==='the-command'"), true, "FORM SHOW preserves command focus");
  assert.equal(await evaluate("document.querySelectorAll('.named-form-field').length"), 12, "FORM collects bounded C field pages");
  assert.equal(await evaluate("!!document.querySelector('.named-form-field output') && !document.querySelector('.named-form-field output img')"), true, "protected field has no editable control and escapes markup");
  await evaluate("document.querySelector('.named-form-toggle[data-panel-id=\"user.person.pane\"]').click()");
  await wait(() => evaluate("document.activeElement?.getAttribute('aria-label')==='Name'"), "FORM explicit FOCUS selects first editable field");
  const formEditsBefore = received.filter((message) => message.operation === "form.set").length;
  await evaluate("(() => {const input=document.querySelector('.named-form-field input');for(const value of ['A','AB','Alice 😀']){input.value=value;input.dispatchEvent(new Event('input',{bubbles:true}));}document.querySelector('.named-form-controls button[type=submit]').click();})()");
  await wait(() => fixtureForms.get("user.person").submitted && fixtureForms.get("user.person").fields[0].value === "Alice 😀", "FORM serial coalesced edits precede Submit");
  await wait(() => evaluate("document.querySelector('.named-form-status')?.textContent==='Submitted'"), "FORM submitted result state");
  await wait(() => evaluate("!document.querySelector('.named-form-field input')?.disabled"), "FORM submit receipt admits further typing");
  const admittedFormEdits = received.filter((message) => message.operation === "form.set").slice(formEditsBefore);
  assert(admittedFormEdits.length <= 2 && admittedFormEdits.every((message) => typeof message.expectRevision === "string" && message.expectGeneration === "9007199254741001"), "FORM serial edits preserve exact C revision tokens");
  const beforeComposition = received.filter((message) => message.operation === "form.set").length;
  await evaluate("(() => {const input=document.querySelector('.named-form-field input');input.dispatchEvent(new CompositionEvent('compositionstart',{bubbles:true}));input.value='未完';input.dispatchEvent(new Event('input',{bubbles:true}));})()");
  await sleep(50);
  assert.equal(received.filter((message) => message.operation === "form.set").length, beforeComposition, "FORM holds IME composition until commit");
  await evaluate("(() => {const input=document.querySelector('.named-form-field input');input.value='完成 😀';input.dispatchEvent(new CompositionEvent('compositionend',{bubbles:true}));document.querySelector('.named-form-controls button:nth-child(2)').click();})()");
  await wait(() => received.some((message) => message.operation === "form.cancel") && fixtureForms.get("user.person").fields[0].value === "Alice 😀" && !fixtureForms.get("user.person").dirty, "FORM Cancel restores submitted baseline");
  await wait(() => evaluate("!document.querySelector('.named-form-field input')?.disabled"), "FORM Cancel receipt admits further typing");
  const submitsBeforeComposition = received.filter((message) => message.operation === "form.submit").length;
  await evaluate("(() => {const input=document.querySelector('.named-form-field input');input.dispatchEvent(new CompositionEvent('compositionstart',{bubbles:true}));input.value='submit composition';input.dispatchEvent(new Event('input',{bubbles:true}));document.querySelector('.named-form-controls button[type=submit]').click();})()");
  await sleep(50);
  assert.equal(received.filter((message) => message.operation === "form.submit").length, submitsBeforeComposition, "FORM Submit waits for active composition");
  assert.equal(await evaluate("document.querySelector('.named-form-field input').disabled"), false, "FORM terminal intent keeps IME control enabled");
  await evaluate("document.querySelector('.named-form-field input').dispatchEvent(new CompositionEvent('compositionend',{bubbles:true}))");
  await wait(() => received.filter((message) => message.operation === "form.submit").length > submitsBeforeComposition
    && fixtureForms.get("user.person").fields[0].value === "submit composition" && !fixtureForms.get("user.person").dirty, "FORM compositionend drains edit then Submit");
  await wait(() => evaluate("!document.querySelector('.named-form-field input')?.disabled"), "FORM composition Submit completes");
  const returnsBeforeComposition = received.filter((message) => message.operation === "panel.return").length;
  await evaluate("(() => {const input=document.querySelector('.named-form-field input');input.dispatchEvent(new CompositionEvent('compositionstart',{bubbles:true}));input.value='retained draft';input.dispatchEvent(new Event('input',{bubbles:true}));document.querySelector('.named-form-controls button:nth-child(3)').click();})()");
  await sleep(50);
  assert.equal(received.filter((message) => message.operation === "panel.return").length, returnsBeforeComposition, "FORM RETURN waits for active composition");
  await evaluate("document.querySelector('.named-form-field input').dispatchEvent(new CompositionEvent('compositionend',{bubbles:true}))");
  await wait(async () => await evaluate("document.activeElement?.id==='the-command'") && fixtureForms.get("user.person").fields[0].value === "retained draft", "FORM RETURN flushes edit and restores origin");
  await evaluate("document.querySelector('.named-form-toggle[data-panel-id=\"user.person.pane\"]').click()");
  await wait(() => evaluate("document.activeElement?.getAttribute('aria-label')==='Name'"), "FORM focus restored after RETURN");
  const editsBeforeExternal = received.filter((message) => message.operation === "form.set").length;
  await evaluate("(() => {const input=document.querySelector('.named-form-field input');input.dispatchEvent(new CompositionEvent('compositionstart',{bubbles:true}));input.value='local conflict';input.dispatchEvent(new Event('input',{bubbles:true}));})()");
  const changedForm = fixtureForms.get("user.person"); changedForm.draftRevision = String(BigInt(changedForm.draftRevision) + 1n); changedForm.fields[0].value = "external value"; publishFormState();
  await wait(() => evaluate("document.querySelector('.named-form-status')?.textContent.includes('stale-revision') && !!document.querySelector('.named-form-controls button:last-child')"), "FORM stale edit is explicit");
  await evaluate("document.querySelector('.named-form-field input').dispatchEvent(new CompositionEvent('compositionend',{bubbles:true}))");
  await sleep(50);
  assert.equal(received.filter((message) => message.operation === "form.set").length, editsBeforeExternal, "FORM external change cannot rebase queued composition onto newer revision");
  assert.equal(changedForm.fields[0].value, "external value", "stale FORM edit preserves C-owned value");
  await evaluate("document.querySelector('.named-form-controls button:last-child').click()");
  await wait(() => evaluate("document.querySelector('.named-form-field input')?.value==='external value' && !document.querySelector('.named-form-field input').disabled"), "FORM explicit reload reconciles conflict");
  for (const [width, placement] of [[1280, "right"], [768, "below"], [390, "tab"]]) {
    await viewport(width, 800);
    await wait(() => received.some((message) => message.operation === "panel.layout" && message.panel === "user.person.pane" && message.placement === placement), `FORM measured ${placement} placement`);
    const formBox = await geometry('.named-form-body'); assert(formBox.visible && formBox.width > 0 && formBox.width <= width, "FORM fields fit viewport and remain scrollable");
  }
  await evaluate("document.querySelector('.named-form-toggle[data-panel-id=\"user.options.pane\"]').click()");
  await wait(() => evaluate("document.querySelector('.named-form-field input')?.getAttribute('aria-label')==='Option' && document.querySelector('.named-form-field input').value==='Guest'"), "second independent FORM retains own values");
  assert.deepEqual(await evaluate("({draft:document.querySelector('#the-command').value,dirty:document.querySelector('.status-line').dataset.bufferDirty,zone:document.querySelector('.status-line').dataset.focusZone,line:document.querySelector('.status-line').dataset.focusLine,cell:document.querySelector('.status-line').dataset.focusCell})"), preservedEditor, "DOCUMENT inspection preserves editor text cursor dirty state and command draft");
  fixturePanels = logFixturePanels;
  fixturePanelState = { ...fixturePanelState, revision: 13, stateRevision: "30", panels: fixturePanels.map((panel) => ({ ...panel })) };
  publishWorkbench(13, 0, "");
  client.send(JSON.stringify(fixturePanelState));
  await viewport(1280, 800);
  const localCommandDraft = await evaluate("document.querySelector('#the-command').value");
  snapshot.focus = { ...snapshot.focus, zone: "command" };
  publishSnapshot();
  await wait(() => evaluate("document.querySelector('.status-line')?.dataset.focusZone==='command'"), "unchanged core command snapshot delivered");
  assert.equal(await evaluate("document.querySelector('#the-command').value"), localCommandDraft,
    "unrelated command-focus snapshot preserves local draft");
  snapshot.command = "core command update";
  publishSnapshot();
  await wait(() => evaluate("document.querySelector('#the-command').value==='core command update'"), "genuine core command update reaches input");
  snapshot.command = "";
  publishSnapshot();
  await wait(() => evaluate("document.querySelector('#the-command').value===''"), "genuine core command clear reaches input");
  await evaluate(`(() => { const input=document.querySelector('#the-command');const setter=Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set;setter.call(input,${JSON.stringify(localCommandDraft)});input.dispatchEvent(new Event('input',{bubbles:true})); })()`);
  snapshot.focus = { ...snapshot.focus, zone: "filearea" };
  snapshot.command = "deferred core command";
  publishSnapshot();
  await wait(() => evaluate("document.querySelector('.status-line')?.dataset.focusZone==='filearea'"), "deferred core command arrives in file area");
  assert.equal(await evaluate("document.querySelector('#the-command').value"), localCommandDraft,
    "file-area core update waits while local draft remains visible");
  snapshot.focus = { ...snapshot.focus, zone: "command" };
  publishSnapshot();
  await wait(() => evaluate("document.querySelector('#the-command').value==='deferred core command'"),
    "command focus applies deferred core update");
  await evaluate("(() => { const input=document.querySelector('#the-command');const setter=Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set;setter.call(input,'fresh local draft');input.dispatchEvent(new Event('input',{bubbles:true})); })()");
  const savedCommandStatusLine = snapshot.status_line;
  snapshot.status_line = "UNCHANGED_CORE_COMMAND_BARRIER";
  publishSnapshot();
  await wait(() => evaluate("document.querySelector('.profile-status')?.textContent==='UNCHANGED_CORE_COMMAND_BARRIER'"),
    "unchanged core command snapshot delivered after local edit");
  assert.equal(await evaluate("document.querySelector('#the-command').value"), "fresh local draft",
    "unchanged later snapshot preserves newly typed draft");
  snapshot.status_line = savedCommandStatusLine;
  snapshot.command = "";
  publishSnapshot();
  await wait(() => evaluate("document.querySelector('#the-command').value===''"), "later core clear still applies");
  snapshot.focus = { ...snapshot.focus, zone: "filearea" };
  publishSnapshot();
  await evaluate(`(() => { const input=document.querySelector('#the-command');const setter=Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set;setter.call(input,${JSON.stringify(localCommandDraft)});input.dispatchEvent(new Event('input',{bubbles:true})); })()`);
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
  console.log("PASS Guided Workbench: 320/390/768/1280/1920 layouts, explicit profile footers/status, visible Run availability, shared F10 menu/navigation/focus return, contextual Tab, panel/dialog ownership, quiet idle responses, semantic history, separate program input, retained drafts/job and scrollback");
} finally {
  socket?.close(); chrome.kill();
  for (const connection of ws.clients) connection.terminate();
  ws.close(); await new Promise((accept) => server.close(accept));
  await rm(profile, { recursive: true, force: true, maxRetries: 10, retryDelay: 100 });
}
