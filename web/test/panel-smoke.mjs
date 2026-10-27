import { spawn } from "node:child_process";
import { mkdtemp, readFile, rm } from "node:fs/promises";
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

function command(method, params = {}) {
  const id = nextId++;
  return new Promise((resolve, reject) => {
    pending.set(id, { resolve, reject, method });
    socket.send(JSON.stringify({ id, method, params }));
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

async function key(key, code, modifiers = 0, text = undefined) {
  const virtualKey = key === "Backspace" ? 8
    : key === "End" ? 35
      : key.length === 1 ? key.toUpperCase().charCodeAt(0) : 0;
  await command("Input.dispatchKeyEvent", {
    type: "rawKeyDown",
    key,
    code,
    modifiers,
    windowsVirtualKeyCode: virtualKey,
    nativeVirtualKeyCode: virtualKey,
    ...(text === undefined ? {} : { text, unmodifiedText: text }),
  });
  await command("Input.dispatchKeyEvent", {
    type: "keyUp", key, code, modifiers,
    windowsVirtualKeyCode: virtualKey,
    nativeVirtualKeyCode: virtualKey,
  });
}

async function insertText(text) {
  await command("Input.insertText", { text });
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

async function closeCurrentBuffer(name) {
  await evaluate("document.querySelector('.input-sink').focus()");
  await domKey("w", "KeyW", true);
  await waitFor(
    () => evaluate(`![...document.querySelectorAll('.buffer-tab')].some((node) => node.textContent.trim() === ${JSON.stringify(name)})`),
    `${name} close`,
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
    } else if (message.method === "Runtime.exceptionThrown") {
      exceptions.push(message.params?.exceptionDetails?.text || "browser exception");
    }
  });

  // Creating a target may precede its active renderer on a loaded runner.
  // Attach to a ready blank page before starting the application navigation.
  await waitFor(() => command("Page.getFrameTree"), "active Chrome page", 30000);
  await command("Runtime.enable");
  await command("Page.enable");
  await command("Page.navigate", { url: pageUrl });
  await waitFor(
    () => evaluate("document.readyState === 'complete' && !!document.querySelector('.app-shell')"),
    "THE application shell",
    30000,
  );
  await waitFor(
    () => evaluate("document.querySelector('.connection')?.textContent.trim() === 'online'"),
    "web driver connection",
  );
  await command("Page.bringToFront");

  async function restoredEditor() {
    try {
      await waitFor(() => evaluate("document.querySelector('.status-line span:nth-child(2)')?.textContent === 'filearea' && document.querySelector('.status-line span:nth-child(3)')?.textContent === '2:3' && document.querySelector('.key-hints')?.textContent.includes('F10')"), "original file caret and key footer after panel cleanup");
    } catch (error) {
      throw new Error(error.message+'; '+JSON.stringify(await evaluate("({status:document.querySelector('.status-line')?.textContent,hints:document.querySelector('.key-hints')?.textContent,rows:[...document.querySelectorAll('.role-reserved .line-text')].map(n=>n.textContent)})")));
    }
  }

  await evaluate("document.querySelector('.input-sink').focus()");
  await domKey("F7", "F7");
  await waitFor(() => evaluate("!!document.querySelector('.fullscreen-panel')"), "macro panel");
  const labels = await evaluate("[...document.querySelectorAll('.panel-protected')].map(n => n.textContent).join(' ')");
  if (!labels.includes("Name:") || !labels.includes("Topic:")) throw new Error("missing protected labels");
  const colours = await evaluate(`(() => {
    const form=document.querySelector('.fullscreen-panel');
    const name=[...document.querySelectorAll('.panel-protected')].find(n=>n.textContent==='Name: ');
    const field=document.querySelector('.panel-editable');
    return {body:getComputedStyle(form).backgroundColor,label:getComputedStyle(name).color,
      field:getComputedStyle(field).color,background:getComputedStyle(field).backgroundColor,
      text:form.textContent};
  })()`);
  if (colours.body!=='rgb(30, 30, 30)' || colours.label!=='rgb(220, 229, 242)' ||
      colours.field!=='rgb(189, 233, 217)' || colours.background!=='rgb(36, 59, 54)')
    throw new Error('declared panel colours were not rendered: '+JSON.stringify(colours));
  if (/seed|tail|000000/.test(colours.text)) throw new Error('caller rows leaked into macro form');
  const controls = await evaluate(`(() => {
    const form=document.querySelector('.fullscreen-panel').getBoundingClientRect();
    const controls=document.querySelector('.panel-controls').getBoundingClientRect();
    return {top:controls.top,bottom:controls.bottom,formTop:form.top,formBottom:form.bottom};
  })()`);
  if (controls.top<controls.formTop || controls.bottom>controls.formBottom+1)
    throw new Error('panel action buttons clipped: '+JSON.stringify(controls));
  async function cursorGeometry() {
    return evaluate(`(() => {
      const field = document.querySelector('.panel-focused');
      const cursor = field?.querySelector('.logical-cursor');
      if (!field || !cursor) return null;
      const f = field.getBoundingClientRect(), c = cursor.getBoundingClientRect();
      return {offset: c.left-f.left, width: c.width, fieldWidth: f.width,
              left: cursor.style.left, margin: getComputedStyle(cursor).marginLeft};
    })()`);
  }
  let geometry = await cursorGeometry();
  if (!geometry || Math.abs(geometry.offset) > 1) throw new Error('panel cursor is offset from the first character: '+JSON.stringify(geometry));
  // Fill a field: the input cursor is at its end, but the painted cursor must
  // remain visible inside the field. The extra character must be rejected.
  await insertText('1234567890123456X');
  try {
    await waitFor(async () => (await cursorGeometry())?.left === '15ch', 'visible cursor at full field boundary');
  } catch (error) {
    const state = await evaluate("({active:document.activeElement?.outerHTML, fields:[...document.querySelectorAll('.panel-field')].map(n=>({text:n.textContent,cls:n.className,cursor:n.querySelector('.logical-cursor')?.style.left})),status:document.querySelector('.status-text')?.textContent})");
    throw new Error(error.message + "; state=" + JSON.stringify(state));
  }
  geometry = await cursorGeometry();
  if (geometry.offset+geometry.width > geometry.fieldWidth+1) throw new Error('panel cursor clipped at field boundary');
  await domKey('Home','Home');
  // A click halfway across the field must select that cell, not its start.
  const fieldTarget = await evaluate(`(() => {const r=document.querySelector('.panel-focused').getBoundingClientRect();return {x:r.left+r.width*0.4,y:r.top+r.height/2};})()`);
  await command('Input.dispatchMouseEvent',{type:'mousePressed',x:fieldTarget.x,y:fieldTarget.y,button:'left',clickCount:1});
  await command('Input.dispatchMouseEvent',{type:'mouseReleased',x:fieldTarget.x,y:fieldTarget.y,button:'left',clickCount:1});
  await waitFor(async () => (await cursorGeometry())?.left === '6ch', 'mouse-selected panel cell');
  await domKey('Home','Home');
  await insertText("Alice");
  await domKey("Tab", "Tab");
  await insertText("Rexx");
  await waitFor(() => evaluate("[...document.querySelectorAll('.panel-editable')].some(n => n.textContent.startsWith('Alice')) && [...document.querySelectorAll('.panel-editable')].some(n => n.textContent.startsWith('Rexx'))"), "edited fields");
  await domKey("Tab", "Tab");
  await insertText("B");
  await waitFor(() => evaluate("[...document.querySelectorAll('.panel-editable')].some(n => n.textContent.startsWith('Blice'))"), "two-field forward Tab cycle");
  await domKey("Tab", "Tab", false, true);
  await insertText("Q");
  await waitFor(() => evaluate("[...document.querySelectorAll('.panel-editable')].some(n => n.textContent.startsWith('Qexx'))"), "reverse Tab cycle");
  await evaluate("document.querySelector('.panel-protected').click()");
  if (await evaluate("document.querySelector('.panel-protected').textContent") !== "Name: ") throw new Error("protected label changed");
  await command("Emulation.setDeviceMetricsOverride", {width: 1100, height: 650, deviceScaleFactor: 1, mobile: false});
  await domKey("F3", "F3");
  await waitFor(() => evaluate("!document.querySelector('.fullscreen-panel')"), "submitted panel");
  await restoredEditor();
  await waitFor(() => evaluate(`(() => {
    const viewport=document.querySelector('.editor-scroll'), style=getComputedStyle(document.querySelector('.editor-grid'));
    return document.querySelectorAll('.screen-row:not(.key-hint-row)').length === Math.floor((viewport.clientHeight-parseFloat(style.paddingTop)-parseFloat(style.paddingBottom))/parseFloat(style.lineHeight));
  })()`), "editor resized after panel submission");
  await domKey("F7", "F7");
  await waitFor(() => evaluate("!!document.querySelector('.fullscreen-panel')"), "second panel");
  await insertText("discard");
  await domKey("Escape", "Escape");
  await waitFor(() => evaluate("!document.querySelector('.fullscreen-panel')"), "cancelled panel");
  await restoredEditor();
  await domKey("F7", "F7");
  await waitFor(() => evaluate("!!document.querySelector('.fullscreen-panel')"), "disconnect panel");
  await command("Page.navigate", { url: "about:blank" });
  await sleep(150);
  const history = await command("Page.getNavigationHistory");
  await command("Page.navigateToHistoryEntry", { entryId: history.entries[history.currentIndex - 1].id });
  try {
    await waitFor(() => evaluate("document.querySelector('.connection')?.textContent.trim() === 'online'"), "reconnected editor");
  } catch (error) {
    throw new Error(error.message + ': ' + await evaluate("document.body.innerText.slice(-1200)"));
  }
  await waitFor(() => evaluate("!!document.querySelector('.app-shell') && !document.querySelector('.fullscreen-panel')"), "disconnect cancellation");
  // The WebSocket opens before its first buffer snapshot is rendered.
  try {
    await waitFor(() => evaluate("[...document.querySelectorAll('.line-text')].map(n => n.textContent).filter(t => t === 'seed' || t === 'tail').join('|') === 'seed|tail'"), "unchanged buffer snapshot after reconnect");
  } catch (error) {
    throw new Error(error.message+'; '+JSON.stringify(await evaluate("({status:document.querySelector('.status-line')?.textContent,lines:[...document.querySelectorAll('.line-text')].map(n=>n.textContent),buffers:[...document.querySelectorAll('.buffer-tab')].map(n=>n.textContent),panel:!!document.querySelector('.fullscreen-panel')})")));
  }
  await restoredEditor();
  await evaluate("document.querySelector('.input-sink').focus()");
  await insertText('Z');
  await domKey('s','KeyS',true);
  await waitFor(() => evaluate("[...document.querySelectorAll('.line-text')].some(n => n.textContent === 'taZil') && document.querySelector('.status-line span:last-child')?.textContent === 'Saved'"), 'typing and saving at the restored panel caret');
  if (exceptions.length) throw new Error(exceptions.join("\n"));
  console.log("Browser panel: rendered fields, text, Tab, protected label, PF3, cancel and unchanged file passed");
} finally {
  if (socket?.readyState === WebSocket.OPEN) socket.close();
  if (chrome.exitCode === null) {
    chrome.kill("SIGTERM");
    await new Promise((resolve) => chrome.once("exit", resolve));
  }
  await rm(profile, { recursive: true, force: true, maxRetries: 5, retryDelay: 100 });
}
