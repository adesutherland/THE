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
    : key === "Delete" ? 46
    : key === "Home" ? 36
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
  await command("Input.dispatchKeyEvent", {
    type: "char",
    key: text,
    text,
    unmodifiedText: text,
  });
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
  await command("Page.addScriptToEvaluateOnNewDocument", { source: `
    window.__theSockets = [];
    window.__workbenchResults = [];
    window.__workbenchMessages = [];
    window.__workbenchSession = '';
    window.__workbenchNext = 1;
    window.__nextWorkbenchId = () => window.__workbenchSession + '.' + window.__workbenchNext++;
    const NativeSocket = window.WebSocket;
    window.WebSocket = class extends NativeSocket {
      constructor(...args) {
        super(...args);
        window.__theSockets.push(this);
        this.addEventListener('message', (event) => {
          try {
            const value = JSON.parse(event.data);
            if (value.type === 'hello') {
              window.__workbenchSession = value.workbenchSession;
              window.__workbenchNext = value.workbenchNextRequest;
            }
            if (value.screen_rows) window.__snapshot = value;
            if (value.type === 'workbench' && value.operation === 'result') {
              window.__workbenchResults.push(value);
              const sequence = Number(value.requestId?.split('.').at(-1));
              if (Number.isSafeInteger(sequence)) window.__workbenchNext = Math.max(window.__workbenchNext, sequence + 1);
            }
            if (value.type === 'workbench') window.__workbenchMessages.push(value);
          } catch {}
        });
      }
    };
  ` });
  await command("Page.navigate", { url: pageUrl });
  await waitFor(
    () => evaluate("document.readyState === 'complete' && !!document.querySelector('.app-shell')"),
    "THE application shell",
  );
  await waitFor(
    () => evaluate("document.querySelector('.connection')?.textContent.trim() === 'online'"),
    "web driver connection",
  );
  await command("Page.bringToFront");

  async function sizeViewport(width, height) {
    await command("Emulation.setDeviceMetricsOverride", {width, height, deviceScaleFactor: 1, mobile: false});
    await evaluate("new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)))");
    return waitFor(() => evaluate(`(() => {
      const viewport = document.querySelector('.editor-scroll'), grid = document.querySelector('.editor-grid');
      const style = getComputedStyle(grid);
      const fit = Math.floor((viewport.clientHeight-parseFloat(style.paddingTop)-parseFloat(style.paddingBottom))/parseFloat(style.lineHeight));
      const rows = document.querySelectorAll('.screen-row:not(.profile-footer-source)').length;
      return rows === fit ? rows : false;
    })()`), "editor rows to fit browser viewport");
  }
  const largeRows = await sizeViewport(1200, 950);
  const smallRows = await sizeViewport(900, 550);
  if (smallRows >= largeRows) throw new Error("browser shrink did not reduce visible rows: " + JSON.stringify({largeRows, smallRows, geometry: await evaluate("[...document.querySelectorAll('.app-shell,.workbench,.workspace,.editor-pane,.editor-scroll,.command-area')].map(n=>({class:n.className,height:n.clientHeight,rows:getComputedStyle(n).gridTemplateRows}))")}));
  const grownRows = await sizeViewport(1200, 950);
  await sizeViewport(455, 700);
  const hints = await evaluate(`(() => {
    const bar=document.querySelector('.key-hints');
    if (!bar) return null;
    return {text:bar.textContent,footer:bar.dataset.footer,fits:bar.scrollWidth<=bar.clientWidth && bar.scrollHeight<=bar.clientHeight};
  })()`);
  if (!hints?.fits || hints.footer !== 'keys' || !/F10\s+Menu/.test(hints.text) || !/Ctrl-S\s+Save/.test(hints.text)) throw new Error('incomplete or clipped narrow-browser named footer: '+JSON.stringify(hints));
  await sizeViewport(1200,950);
  if (grownRows !== largeRows) throw new Error("browser grow did not restore visible rows");

  await evaluate("document.querySelector('.role-file .prefix-cell').click()");
  await waitFor(() => evaluate("!!document.querySelector('.prefix-cell-active .prefix-cursor')"), "prefix cursor");
  let prefixGeometry = await evaluate(`(() => {
    const field=document.querySelector('.prefix-cell-active'), cursor=field.querySelector('.prefix-cursor');
    const f=field.getBoundingClientRect(), c=cursor.getBoundingClientRect();
    return {left:c.left-f.left,right:c.right-f.left,width:f.width,margin:getComputedStyle(cursor).marginLeft};
  })()`);
  if (Math.abs(prefixGeometry.left)>1 || prefixGeometry.right>prefixGeometry.width || prefixGeometry.margin!=='0px') throw new Error('misaligned prefix cursor '+JSON.stringify(prefixGeometry));
  await insertText('d');
  await waitFor(() => evaluate("document.querySelector('.prefix-cursor')?.style.left === '1ch'"), "prefix cursor after typing");
  await domKey('Backspace','Backspace');
  await waitFor(() => evaluate("document.querySelector('.prefix-cursor')?.style.left === '0ch'"), "prefix cleared");
  for (const character of 'zzzzzzz') await insertText(character);
  await waitFor(() => evaluate("document.querySelector('.prefix-cursor')?.style.left === '6ch'"), "visible prefix cursor at field boundary");
  for (let i=0;i<7;i++) await domKey('Backspace','Backspace');
  await waitFor(() => evaluate("document.querySelector('.prefix-cursor')?.style.left === '0ch'"), "full prefix cleared");
  await domKey('Escape','Escape');
  await waitFor(() => evaluate("document.activeElement?.getAttribute('aria-label') === 'THE command' && !document.querySelector('.prefix-cursor')"), "Escape command focus");
  await domKey('Escape','Escape');
  await waitFor(() => evaluate("document.activeElement?.getAttribute('aria-label') === 'Editor input' && !!document.querySelector('.prefix-cursor')"), "Escape back to prefix editing");
  await domKey('F6','F6');
  await waitFor(() => evaluate("document.activeElement?.getAttribute('aria-label') === 'THE command'"), "F6 command focus");
  await domKey('F6','F6');
  await waitFor(() => evaluate("document.activeElement?.getAttribute('aria-label') === 'Editor input' && !!document.querySelector('.line-content .logical-cursor') && !document.querySelector('.prefix-cursor')"), "F6 back to file editing");
  await domKey('F6','F6');
  await waitFor(() => evaluate("document.activeElement?.getAttribute('aria-label') === 'THE command'"), "F6 back to command field");
  await evaluate("new Promise(resolve => {const input=document.querySelector('.command-form input');input.value='cursor goto 1 1';input.dispatchEvent(new Event('input',{bubbles:true}));setTimeout(()=>{input.form.requestSubmit();resolve(true)},0)})");
  await waitFor(() => evaluate("!!document.querySelector('.line-content .logical-cursor')"), "command execution returns to file editing");
  async function navigationCommand(text) {
    await evaluate(`new Promise(resolve => {const input=document.querySelector('.command-form input');input.value=${JSON.stringify(text)};input.dispatchEvent(new Event('input',{bubbles:true}));setTimeout(()=>{input.form.requestSubmit();resolve(true)},0)})`);
  }
  await navigationCommand('cursor goto 3 1');
  await waitFor(() => evaluate("document.querySelector('.status-line')?.dataset.focusZone==='filearea' && document.querySelector('.status-line')?.dataset.focusLine==='3'"), "third record for left navigation");
  await domKey('ArrowLeft','ArrowLeft');
  await waitFor(() => evaluate("document.querySelector('.status-line')?.dataset.focusZone==='prefix' && document.querySelector('.status-line')?.dataset.focusLine==='3' && document.querySelector('.prefix-cursor')?.style.left === '6ch'"), "left into current prefix");
  for (let i=0;i<7;i++) await domKey('ArrowLeft','ArrowLeft');
  await waitFor(() => evaluate("document.querySelector('.status-line')?.dataset.focusZone==='filearea' && document.querySelector('.status-line')?.dataset.focusLine==='2' && document.querySelector('.line-content .logical-cursor')?.style.left === '9ch'"), "left from prefix to previous record end");
  await navigationCommand('cursor goto 1 1');
  await waitFor(() => evaluate("document.querySelector('.status-line')?.dataset.focusZone==='filearea' && document.querySelector('.status-line')?.dataset.focusLine==='1'"), "file focus restored");
  if (!(await evaluate("document.querySelector('.key-hints')?.textContent.includes('F6 Cmd/File') && /F10\\s+Menu/.test(document.querySelector('.key-hints')?.textContent)"))) throw new Error('function-key guide missing');
  await domKey('F10','F10');
  await waitFor(() => evaluate("!!document.querySelector('.menu-popover')"), "F10 application Menu");
  await domKey('Escape','Escape');
  await waitFor(() => evaluate("!document.querySelector('.menu-popover') && document.activeElement.classList.contains('input-sink')"), "Menu Escape restores editor focus");
  await evaluate("document.querySelector('.menu-trigger').click()");
  await waitFor(() => evaluate("!!document.querySelector('.menu-popover')"), "application Menu reopened");
  await evaluate("[...document.querySelectorAll('.menu-popover button')].find(node=>node.textContent.trim()==='Key definitions').click()");
  await waitFor(() => evaluate("[...document.querySelectorAll('.line-text')].some(n=>n.textContent.toLowerCase().includes('define '))"), "complete key definitions");
  const keyBuffer = await evaluate("document.querySelector('.buffer-tab-current span').textContent.trim()");
  await closeCurrentBuffer(keyBuffer);
  await selectBuffer('sample.txt');

  await evaluate("document.querySelector('.input-sink').focus()");
  await domKey("F1", "F1");
  await waitFor(() => evaluate("document.querySelector('.buffer-tab-current span')?.textContent.trim() === 'THE_Help.txt'"), "help buffer");
  const helpPrefixes = await evaluate("[...document.querySelectorAll('.role-file .prefix-cell')].map(node=>node.textContent.trim())");
  if (helpPrefixes.length < 25 || helpPrefixes.some(text=>!/^\d{7}$/.test(text))) throw new Error("help prefix numbers incomplete before refresh");
  await closeCurrentBuffer("THE_Help.txt");
  await selectBuffer("sample.txt");

  await openWorkspaceFile("tabs.txt");
  if (!(await evaluate("[...document.querySelectorAll('.line-text')].some(node=>node.textContent==='a→b')"))) throw new Error("retained tab not visibly marked");
  await evaluate("document.querySelector('[aria-label=\"Save\"]').click()");
  await closeCurrentBuffer("tabs.txt");
  await openWorkspaceFile("tab-entry.txt");
  await evaluate("document.querySelector('.role-file .line-content').click(); document.querySelector('.input-sink').focus()");
  await domKey("Home", "Home");
  await domKey("Tab", "Tab");
  await waitFor(() => evaluate("[...document.querySelectorAll('.line-text')].some(node=>node.textContent==='        alpha')"), "Tab to insert eight spaces");
  await domKey("s", "KeyS", true);
  await waitFor(() => evaluate("document.querySelector('.status-line')?.dataset.bufferDirty === '0'"), "space-indented file saved");
  await closeCurrentBuffer("tab-entry.txt");
  await selectBuffer("sample.txt");

  await evaluate("document.querySelector('.menu-trigger').click()");
  const fileMenu = await waitFor(
    () => evaluate("[...document.querySelectorAll('.menu-popover button')].map((node) => node.textContent.trim())"),
    "File menu",
  );
  for (const label of ["New", "Open", "Save", "Close"]) {
    if (!fileMenu.some((entry) => entry.startsWith(label))) {
      throw new Error(`File menu is missing ${label}`);
    }
  }

  await evaluate("document.querySelector('[aria-label=\"Open\"]').click()");
  await waitFor(
    () => evaluate("[...document.querySelectorAll('.file-list button')].some((node) => node.textContent.includes('sample.txt'))"),
    "workspace file listing",
  );
  await evaluate("document.querySelector('.explorer-heading [aria-label=\"Close files\"]').click()");

  await evaluate("document.querySelector('.menu-trigger').click()");
  await evaluate("[...document.querySelectorAll('.menu-popover button')].find((node) => node.textContent.trim().startsWith('New')).click()");
  await waitFor(() => evaluate("!!document.querySelector('.path-dialog')"), "New file dialog");
  await evaluate("[...document.querySelectorAll('.dialog-actions button')].find((node) => node.textContent.trim() === 'Cancel').click()");

  await openWorkspaceFile("sample.crexx");
  try {
    await waitFor(
      () => evaluate("!!document.querySelector('.syntax-comment') && !!document.querySelector('.syntax-keyword') && !!document.querySelector('.syntax-string')"),
      "cRexx syntax styles",
    );
  } catch (error) {
    const state = await evaluate("({buffer: document.querySelector('.buffer-tab-current')?.textContent.trim(), status: document.querySelector('.status-line')?.textContent.trim(), lines: [...document.querySelectorAll('.line-text')].map((node) => ({text: node.textContent, classes: [...node.querySelectorAll('.syntax')].map((span) => span.className)})).filter((row) => row.text).slice(0, 6)})");
    throw new Error(`${error.message}; state=${JSON.stringify(state)}`);
  }

  await openWorkspaceFile("sample.md");
  if (process.env.THE_WEB_TEST_MDP !== "0") {
  try {
    await waitFor(
      () => evaluate("!!document.querySelector('.syntax-operator') && (!!document.querySelector('.syntax-preprocessor') || !!document.querySelector('.syntax-identifier')) && !document.querySelector('.syntax-comment') && !document.querySelector('.syntax-string')"),
      "Markdown syntax styles without stale cRexx spans",
    );
  } catch (error) {
    const state = await evaluate("({buffer: document.querySelector('.buffer-tab-current')?.textContent.trim(), status: document.querySelector('.status-line')?.textContent.trim(), lines: [...document.querySelectorAll('.line-text')].map((node) => ({text: node.textContent, classes: [...node.querySelectorAll('.syntax')].map((span) => span.className)})).filter((row) => row.text).slice(0, 6)})");
    throw new Error(`${error.message}; state=${JSON.stringify(state)}`);
  }

  } else console.log("Markdown parser unavailable: buffer editing checked; semantic styles skipped");

  await openWorkspaceFile("sample.py");
  if (process.env.THE_WEB_TEST_PYP !== "0") {
  try {
    await waitFor(
      () => evaluate("!!document.querySelector('.syntax-keyword') && !!document.querySelector('.syntax-identifier') && !!document.querySelector('.syntax-string') && !!document.querySelector('.syntax-operator') && !document.querySelector('.syntax-preprocessor') && !document.querySelector('.syntax-comment')"),
      "Python syntax styles without stale Markdown spans",
    );
  } catch (error) {
    const state = await evaluate("({buffer: document.querySelector('.buffer-tab-current')?.textContent.trim(), status: document.querySelector('.status-line')?.textContent.trim(), lines: [...document.querySelectorAll('.line-text')].map((node) => ({text: node.textContent, classes: [...node.querySelectorAll('.syntax')].map((span) => span.className)})).filter((row) => row.text).slice(0, 6)})");
    throw new Error(`${error.message}; state=${JSON.stringify(state)}`);
  }
  const pythonColors = await evaluate("({keyword: getComputedStyle(document.querySelector('.syntax-keyword')).color, identifier: getComputedStyle(document.querySelector('.syntax-identifier')).color, string: getComputedStyle(document.querySelector('.syntax-string')).color})");
  if (new Set(Object.values(pythonColors)).size !== 3) {
    throw new Error(`Python semantic styles are not visually distinct: ${JSON.stringify(pythonColors)}`);
  }

  } else console.log("Python parser unavailable: buffer editing checked; semantic styles skipped");

  await selectBuffer("sample.crexx");
  await waitFor(
    () => evaluate("!!document.querySelector('.syntax-comment') && !!document.querySelector('.syntax-string') && !document.querySelector('.syntax-identifier')"),
    "cRexx styles after buffer switch",
  );
  for (const name of ["sample.crexx", "sample.md", "sample.py"]) {
    await selectBuffer(name);
    await closeCurrentBuffer(name);
  }
  await selectBuffer("sample.txt");

  await evaluate("new Promise((resolve) => { const input = document.querySelector('.command-form input'); input.value = 'set insertmode on'; input.dispatchEvent(new Event('input', {bubbles: true})); setTimeout(() => { input.form.requestSubmit(); resolve(true); }, 0); })");
  await sleep(100);
  await evaluate("(() => { const line = [...document.querySelectorAll('.line-content')].find((node) => node.textContent.includes('alpha beta gamma')); const rect = line.getBoundingClientRect(); line.dispatchEvent(new MouseEvent('click', {bubbles: true, clientX: rect.left + 12, clientY: rect.top + 8})); return true; })()");
  await sleep(100);
  await evaluate("document.querySelector('.input-sink').focus()");
  await insertText("X");
  try {
    await waitFor(
      () => evaluate("[...document.querySelectorAll('.line-text')].some((node) => node.textContent === 'Xalpha beta gamma') && document.querySelector('.line-content .logical-cursor')?.style.left === '1ch' && document.querySelector('.status-line').dataset.bufferDirty === '1'"),
      "printable keyboard input",
    );
  } catch (error) {
    const state = await evaluate("({active: document.activeElement?.className, sink: document.querySelector('.input-sink')?.value, lines: [...document.querySelectorAll('.line-text')].map((node) => node.textContent).filter(Boolean).slice(0, 5)})");
    throw new Error(`${error.message}; state=${JSON.stringify(state)}`);
  }
  await key("Backspace", "Backspace");
  try {
    await waitFor(
      () => evaluate("[...document.querySelectorAll('.line-text')].some((node) => node.textContent === 'alpha beta gamma') && document.querySelector('.line-content .logical-cursor')?.style.left === '0ch' && document.querySelector('.status-line').dataset.bufferDirty === '0'"),
      "physical Backspace deletes left and moves the cursor",
    );
  } catch (error) {
    const lines = await evaluate("[...document.querySelectorAll('.line-text')].map((node) => node.textContent).filter(Boolean).slice(0, 5)");
    throw new Error(`${error.message}; lines=${JSON.stringify(lines)}`);
  }
  await key("Delete", "Delete");
  await waitFor(
    () => evaluate("[...document.querySelectorAll('.line-text')].some((node) => node.textContent === 'lpha beta gamma') && document.querySelector('.line-content .logical-cursor')?.style.left === '0ch' && document.querySelector('.status-line').dataset.bufferDirty === '1'"),
    "physical forward Delete removes the current character without moving the cursor",
  );
  await domKey("s", "KeyS", true);
  await waitFor(() => evaluate("document.querySelector('.status-line').dataset.bufferDirty === '0'"), "forward Delete saved");
  // Keep a second file open while the edited file is closed and read afresh.
  await openWorkspaceFile("second.txt");
  await selectBuffer("sample.txt");
  await closeCurrentBuffer("sample.txt");
  await openWorkspaceFile("sample.txt");
  await navigationCommand("cursor goto 1 1");
  await waitFor(
    () => evaluate("[...document.querySelectorAll('.line-text')].some((node) => node.textContent === 'lpha beta gamma') && document.querySelector('.line-content .logical-cursor')?.style.left === '0ch' && document.querySelector('.status-line').dataset.bufferDirty === '0'"),
    "saved Delete bytes survive closing and reopening",
  );
  await selectBuffer("second.txt");
  await closeCurrentBuffer("second.txt");
  await selectBuffer("sample.txt");
  await evaluate("document.querySelector('.input-sink').focus()");
  await insertText("a");
  await key("Home", "Home");
  await insertText("Z");
  await evaluate("(() => { const row = [...document.querySelectorAll('.screen-row')].find((node) => node.querySelector('.line-text')?.textContent === 'delete me'); const prefix = row?.querySelector('.prefix-cell'); if (!prefix) return false; const rect = prefix.getBoundingClientRect(); prefix.dispatchEvent(new MouseEvent('click', {bubbles: true, clientX: rect.left + 4, clientY: rect.top + 8})); return true; })()");
  await waitFor(
    () => evaluate("document.querySelector('.status-line').dataset.focusZone === 'prefix'"),
    "prefix focus",
  );
  await evaluate("document.querySelector('.input-sink').focus()");
  await insertText("d");
  await waitFor(
    () => evaluate("[...document.querySelectorAll('.screen-row')].some((node) => node.querySelector('.line-text')?.textContent === 'delete me' && node.querySelector('.prefix-cell')?.textContent.trim() === 'd')"),
    "visible prefix command entry",
  );
  await domKey("Enter", "Enter");
  await waitFor(
    () => evaluate("![...document.querySelectorAll('.line-text')].some((node) => node.textContent === 'delete me') && [...document.querySelectorAll('.line-text')].some((node) => node.textContent === 'gamma')"),
    "prefix delete execution",
  );
  await domKey("s", "KeyS", true);
  await waitFor(
    () => evaluate("document.querySelector('.status-line').dataset.bufferDirty === '0'"),
    "Ctrl+S action",
  );


  if(await evaluate("!!document.querySelector('.pane-switcher .run-rexx:not(:disabled)')")){
    await openWorkspaceFile('console.rexx');
    await evaluate("document.querySelector('.pane-switcher .run-rexx').click()");
    await waitFor(()=>evaluate("document.querySelector('.console-output').textContent.includes('native graphical proof')"),'native program console',30000);
    await navigationCommand('set color consolebody #123456 on #234567');
    await waitFor(()=>evaluate("getComputedStyle(document.querySelector('.console-output')).color==='rgb(18, 52, 86)' && getComputedStyle(document.querySelector('.console-output')).backgroundColor==='rgb(35, 69, 103)'"),'SET COLOR recolours running native browser console');
    await selectBuffer('sample.txt');
    await waitFor(()=>evaluate("getComputedStyle(document.querySelector('.console-output')).color!=='rgb(18, 52, 86)'"),'file switch changes running console colours');
    await selectBuffer('console.rexx');
    await waitFor(()=>evaluate("getComputedStyle(document.querySelector('.console-output')).color==='rgb(18, 52, 86)'"),'return restores console file colours');
    if(!await evaluate("document.querySelector('.run-state').textContent==='Running'"))throw Error('Colour commands or file switching interrupted the program');
    await evaluate("document.querySelector('#rexx-input').focus()");await command('Input.insertText',{text:'Zoë😀'});await command('Input.dispatchKeyEvent',{type:'keyDown',key:'Enter',code:'Enter',windowsVirtualKeyCode:13,text:'\r'});await command('Input.dispatchKeyEvent',{type:'keyUp',key:'Enter',code:'Enter',windowsVirtualKeyCode:13});
    await waitFor(()=>evaluate("document.querySelector('.run-state').textContent==='Completed · status 7'"),'native completion',30000);
    if(!await evaluate("document.querySelector('.console-output').textContent.includes('Hello ZOË😀')"))throw Error('Native Unicode console output');
    await closeCurrentBuffer('console.rexx');
    await selectBuffer('sample.txt');
    console.log('PASS native graphical RXBIN host: actual Classic execution, live SET COLOR/file-switch console palette, Unicode stdin/output and status');
  }
  // The restricted browser admits typed builtins through the shared registry.
  // Its command and macro action kinds remain subject to core policy.
  const beforeWorkbench = await evaluate("({draft:document.querySelector('#the-command').value,dirty:document.querySelector('.status-line').dataset.bufferDirty,zone:document.querySelector('.status-line').dataset.focusZone,line:document.querySelector('.status-line').dataset.focusLine,cell:document.querySelector('.status-line').dataset.focusCell})");
  const poisonedLabel = 'Check / <img src=x onerror="window.poisoned=true">';
  await navigationCommand('workbench begin owner user.web');
  await navigationCommand('workbench action put user.save label "Safe Save" builtin file.save');
  await navigationCommand('workbench menu put user.tools label "Tools" order 50');
  await navigationCommand(`workbench item put user.tools user.saveitem label "${poisonedLabel.replaceAll('"', '""')}" action user.save`);
  await navigationCommand('workbench commit');
  await evaluate("document.querySelector('.menu-trigger').click()");
  await waitFor(() => evaluate("!!document.querySelector('[data-menu-id=\"user.tools\"] button')"), "published profile menu in native Chrome");
  if (!await evaluate(`document.querySelector('[data-menu-id="user.tools"] button span').textContent === ${JSON.stringify(poisonedLabel)} && !document.querySelector('.menu-popover img') && !window.poisoned`))
    throw new Error("workbench label changed markup or text");
  const oldGeneration = +(await evaluate("document.querySelector('[data-menu-id=\"user.tools\"] button').dataset.generation"));
  await evaluate("document.querySelector('[data-menu-id=\"user.tools\"] button').click()");
  await waitFor(() => evaluate("window.__workbenchResults.some(result => result.code === 'ok' && result.state === 'complete')"), "shared menu action result");
  await navigationCommand('workbench begin owner user.web');
  await navigationCommand('workbench action put user.save label "Replaced Save" builtin file.save');
  await navigationCommand('workbench item put user.tools user.saveitem label "Replaced Save" action user.save');
  await navigationCommand('workbench commit');
  await evaluate("document.querySelector('.menu-trigger').click()");
  await waitFor(() => evaluate("document.querySelector('[data-menu-id=\"user.tools\"] button span')?.textContent === 'Replaced Save'"), "replaced shared menu");
  const newGeneration = +(await evaluate("document.querySelector('[data-menu-id=\"user.tools\"] button').dataset.generation"));
  if (newGeneration <= oldGeneration) throw new Error("replaced action did not advance generation");
  const staleRequest = await evaluate("window.__nextWorkbenchId()");
  await evaluate(`window.__theSockets[0].send(JSON.stringify({type:'workbench',version:1,operation:'invoke',requestId:${JSON.stringify(staleRequest)},action:'user.save',expectGeneration:${oldGeneration}}))`);
  await waitFor(() => evaluate(`window.__workbenchResults.some(result => result.requestId === ${JSON.stringify(staleRequest)} && result.code === 'stale-generation')`), "stale generation refusal");
  const nulRequest = await evaluate("window.__nextWorkbenchId()");
  await evaluate(`window.__theSockets[0].send(JSON.stringify({type:'workbench',version:1,operation:'invoke',requestId:${JSON.stringify(nulRequest)},action:'user.save\\u0000hidden',expectGeneration:${newGeneration}}))`);
  await waitFor(() => evaluate(`window.__workbenchResults.some(result => result.requestId === ${JSON.stringify(nulRequest)} && result.code === 'invalid-operand')`), "native JSON NUL refusal");
  await navigationCommand('workbench begin owner user.web');
  await navigationCommand('workbench item remove user.tools user.saveitem');
  await navigationCommand('workbench action remove user.save');
  await navigationCommand('workbench menu remove user.tools');
  await navigationCommand('workbench commit');
  await waitFor(() => evaluate("!document.querySelector('[data-menu-id=\"user.tools\"]')"), "removed shared menu");
  await evaluate("document.querySelector('.menu-trigger').click()");
  if (JSON.stringify(await evaluate("({draft:document.querySelector('#the-command').value,dirty:document.querySelector('.status-line').dataset.bufferDirty,zone:document.querySelector('.status-line').dataset.focusZone,line:document.querySelector('.status-line').dataset.focusLine,cell:document.querySelector('.status-line').dataset.focusCell})")) !== JSON.stringify(beforeWorkbench))
    throw new Error("workbench menu cycle changed editor state");
  // A transport replay with the same request ID returns the cached completion.
  await evaluate("document.querySelector('.menu-trigger').click()");
  const saveGeneration = +(await evaluate("document.querySelector('[data-action-id=\"the.file.save\"]').dataset.generation"));
  await evaluate("document.querySelector('.menu-trigger').click();document.querySelector('.input-sink').focus()");
  const replayId = await evaluate("window.__nextWorkbenchId()");
  await evaluate(`(() => { const request={type:'workbench',version:1,operation:'invoke',requestId:${JSON.stringify(replayId)},action:'the.file.save',expectGeneration:${saveGeneration}};window.__theSockets[0].send(JSON.stringify(request));window.__theSockets[0].send(JSON.stringify(request)); })()`);
  await waitFor(() => evaluate(`window.__workbenchResults.some(result => result.requestId === ${JSON.stringify(replayId)} && result.state === 'complete' && result.code === 'ok')`), "deduplicated Save receipt");
  await sleep(200);
  if (await evaluate(`new Set(window.__workbenchResults.filter(result => result.requestId === ${JSON.stringify(replayId)} && result.state === 'complete').map(result => JSON.stringify(result))).size !== 1`))
    throw new Error("request replay changed its completed receipt");
  await evaluate(`window.__theSockets[0].send(JSON.stringify({type:'workbench',version:1,operation:'invoke',requestId:${JSON.stringify(replayId)},action:'the.edit.undo',expectGeneration:${saveGeneration}}))`);
  await waitFor(() => evaluate(`window.__workbenchResults.some(result => result.requestId === ${JSON.stringify(replayId)} && result.code === 'request-id-conflict')`), "request ID payload conflict");
  console.log("PASS native browser shared workbench: add, invoke, replace, stale generation, poisoned label and remove");
  await navigationCommand('workbench begin owner user.logs');
  await navigationCommand('workbench panel put user.output kind log model user.outputlog');
  await navigationCommand('workbench panel place user.output right fallback below,tab mincols 10');
  await navigationCommand('workbench route put user.stdout from tool:classic-rexx/stdout to log:user.outputlog');
  await navigationCommand('workbench action put user.show label "Show output" panel user.output op show');
  await navigationCommand('workbench menu put user.logs label "Logs" order 50');
  await navigationCommand('workbench item put user.logs user.showitem label "Show output" action user.show');
  await navigationCommand('workbench commit');
  await waitFor(() => evaluate("!!document.querySelector('.named-log-toggle[data-panel-id=\"user.output\"]')"), 'native LOG catalog publication');
  const logRunnerAvailable = await evaluate("!!document.querySelector('.pane-switcher .run-rexx:not(:disabled)')");
  if (logRunnerAvailable) {
    await openWorkspaceFile('console.rexx');
    await evaluate("document.querySelector('.pane-switcher .run-rexx').click()");
    await waitFor(() => evaluate("document.querySelector('.console-output')?.textContent.includes('native graphical proof')"), 'native routed program stdout', 30000);
    await waitFor(() => evaluate("!!document.querySelector('#rexx-input:not(:disabled)')?.getClientRects().length"), 'active Program input before native PANEL SHOW');
    await evaluate("document.querySelector('#rexx-input').focus()");
    await command('Input.insertText', {text: 'pending program input'});
    await evaluate("document.querySelector('#rexx-input').setSelectionRange(2,8)");
    await domKey('F10', 'F10');
    await waitFor(() => evaluate("!!document.querySelector('[data-item-id=\"user.showitem\"]')"), 'native PANEL SHOW menu item');
    await evaluate("document.querySelector('[data-item-id=\"user.showitem\"]').click()");
    await waitFor(() => evaluate("!!document.querySelector('.named-log-pane[data-panel-id=\"user.output\"]')?.getClientRects().length && document.activeElement?.id==='rexx-input'"), 'native SHOW displays LOG and retains Program focus');
    if (!await evaluate("document.querySelector('#rexx-input').value==='pending program input' && document.querySelector('#rexx-input').selectionStart===2 && document.querySelector('#rexx-input').selectionEnd===8 && window.__workbenchMessages.some(m=>m.operation==='panel-state'&&m.panels?.some(p=>p.id==='user.output'&&p.visible&&!p.focused))"))
      throw Error('native PANEL SHOW lost Program draft/selection or focused core pane');
    await navigationCommand('workbench panel hide user.output');
    await waitFor(() => evaluate("window.__workbenchMessages.at(-1)?.operation==='panel-state' && window.__workbenchMessages.at(-1).panels.some(p=>p.id==='user.output'&&!p.visible)"), 'native LOG hidden before command SHOW');
  }
  await evaluate("document.querySelector('#the-command').focus()");
  await command('Input.insertText', {text: 'pending command'});
  const pendingCommandDraft = await evaluate("document.querySelector('#the-command').value");
  if (!pendingCommandDraft.includes('pending command')) throw Error('native command draft was not entered');
  await domKey('F10', 'F10');
  await waitFor(() => evaluate("!!document.querySelector('[data-item-id=\"user.showitem\"]')"), 'command PANEL SHOW menu item');
  await evaluate("document.querySelector('[data-item-id=\"user.showitem\"]').click()");
  await waitFor(() => evaluate("!!document.querySelector('.named-log-pane[data-panel-id=\"user.output\"]')?.getClientRects().length && document.activeElement?.id==='the-command'"), 'native SHOW displays LOG and retains command focus');
  const afterCommandDraft = await evaluate("document.querySelector('#the-command').value");
  if (afterCommandDraft !== pendingCommandDraft)
    throw Error(`native PANEL SHOW changed command draft: ${JSON.stringify({before:pendingCommandDraft,after:afterCommandDraft})}`);
  if (logRunnerAvailable) {
    await waitFor(() => evaluate("document.querySelector('.named-log-output')?.textContent.includes('native graphical proof')"), 'native LOG pane renders actual stdout', 30000).catch(async (error) => {
      throw new Error(`${error.message}: ${JSON.stringify(await evaluate("({pane:document.querySelector('.named-log-pane')?.outerHTML.slice(0,500),messages:window.__workbenchMessages.slice(-12),status:document.querySelector('.status-line')?.textContent})"))}`);
    });
    const logBeforeInput = await evaluate("window.__snapshot");
    if (logBeforeInput.buffer.dirty || !logBeforeInput.focus) throw Error('LOG output changed editor state');
    await evaluate("[...document.querySelectorAll('.auxiliary-tabs [role=tab]')].find(node=>node.textContent==='Program console').click()");
    await evaluate("document.querySelector('#rexx-input').focus()");
    await command('Input.dispatchKeyEvent', {type:'keyDown',key:'Enter',code:'Enter',windowsVirtualKeyCode:13,text:'\r'});
    await command('Input.dispatchKeyEvent', {type:'keyUp',key:'Enter',code:'Enter',windowsVirtualKeyCode:13});
    await waitFor(() => evaluate("document.querySelector('.run-state')?.textContent.includes('Completed')"), 'native routed program completion', 30000);
    await closeCurrentBuffer('console.rexx');
    await selectBuffer('sample.txt');
    console.log('PASS native browser LOG pane: actual routed Classic stdout and dynamic content');
  } else {
    await waitFor(() => evaluate("!!document.querySelector('.named-log-output')"), 'native LOG pane without runner');
    console.log('PASS native browser LOG pane: route catalog and panel UI (Classic runner unavailable)');
  }
  await navigationCommand('workbench begin owner user.documents');
  await navigationCommand('workbench panel put user.reference kind document buffer active access readonly');
  await navigationCommand('workbench panel place user.reference right fallback below,tab mincols 10');
  await navigationCommand('workbench commit');
  await waitFor(() => evaluate("!!document.querySelector('.named-document-toggle[data-panel-id=\"user.reference\"]')"), 'native DOCUMENT catalog publication');
  await evaluate("document.querySelector('#the-command').focus();document.querySelector('.named-document-toggle').click()");
  await waitFor(() => evaluate("document.querySelector('.named-document-output')?.textContent.includes('Zalpha beta gamma')"), 'native DOCUMENT copied bytes');
  if (!await evaluate("document.activeElement?.id==='the-command' && window.__workbenchMessages.findLast(message=>message.operation==='panel-state').panels.some(panel=>panel.id==='user.reference'&&panel.visible&&!panel.focused)"))
    throw new Error('native DOCUMENT SHOW took focus from command input');
  await evaluate("document.querySelector('.named-document-toggle').click()");
  await waitFor(() => evaluate("document.activeElement?.classList.contains('named-document-output')"), 'native DOCUMENT explicit FOCUS');
  const beforeDocument = await evaluate("JSON.stringify({buffer:window.__snapshot.buffer,focus:window.__snapshot.focus,rows:window.__snapshot.screen_rows.map(row=>[row.line,row.t])})");
  await evaluate("(() => {const node=document.querySelector('.named-document-output');for(const key of ['a','Delete','Backspace'])node.dispatchEvent(new KeyboardEvent('keydown',{key,bubbles:true,cancelable:true}));for(const key of ['s','z','v'])node.dispatchEvent(new KeyboardEvent('keydown',{key,ctrlKey:true,bubbles:true,cancelable:true}));node.dispatchEvent(new Event('paste',{bubbles:true,cancelable:true}));})()");
  await sleep(100);
  if (await evaluate("JSON.stringify({buffer:window.__snapshot.buffer,focus:window.__snapshot.focus,rows:window.__snapshot.screen_rows.map(row=>[row.line,row.t])})") !== beforeDocument)
    throw new Error('read-only DOCUMENT input changed editor text, cursor or dirty state');
  const copiedDocument = await evaluate("window.__workbenchMessages.findLast(message=>message.operation==='document').data");
  if (copiedDocument.records.length > 8 || copiedDocument.records[0].bytesHex !== Buffer.from('Zalpha beta gamma').toString('hex'))
    throw new Error(`native DOCUMENT copied range mismatch: ${JSON.stringify(copiedDocument)}`);
  await domKey('Escape','Escape');
  await waitFor(() => evaluate("document.activeElement?.id==='the-command'"), 'native DOCUMENT RETURN');
  await openWorkspaceFile('second.txt');
  await evaluate("document.querySelector('.named-document-toggle').click()");
  await waitFor(() => evaluate("document.querySelector('.named-document-output')?.textContent.includes('Zalpha beta gamma')"), 'DOCUMENT retains original binding after buffer switch');
  await domKey('Escape','Escape');
  await closeCurrentBuffer('second.txt');
  await selectBuffer('sample.txt');
  await navigationCommand('cursor goto 1 1');
  const staleDocumentResults = +(await evaluate("window.__workbenchResults.length"));
  await evaluate(`window.__theSockets[0].send(JSON.stringify({type:'workbench',version:1,operation:'document.list',panel:'user.reference',expectGeneration:${JSON.stringify(copiedDocument.panelGeneration)},expectRevision:${JSON.stringify(copiedDocument.documentRevision)},line:1,offset:0,records:8,bytes:8192}))`);
  await waitFor(() => evaluate(`window.__workbenchResults.slice(${staleDocumentResults}).some(result=>result.code==='stale-revision')`), 'native stale DOCUMENT revision refusal');
  console.log('PASS native browser DOCUMENT: bounded raw/display copy, read-only input, RETURN, immutable binding and stale revision');
  if (exceptions.length) throw new Error(`browser exceptions: ${exceptions.join("; ")}`);
  await evaluate("document.querySelector('.menu-trigger').click()");
  const beforeCloseResults = +(await evaluate("window.__workbenchResults.length"));
  await evaluate("[...document.querySelectorAll('.menu-popover button')].find((node) => node.textContent.trim().startsWith('Close')).click()");
  await waitFor(() => evaluate(`window.__workbenchResults.length > ${beforeCloseResults} || window.__theSockets[0].readyState !== WebSocket.OPEN`), "shared Close action response");
  if (await evaluate("window.__theSockets[0].readyState === WebSocket.OPEN")) {
    const result = await evaluate("window.__workbenchResults.at(-1)");
    if (result.code !== "ok") throw new Error(`shared Close refused: ${JSON.stringify(result)}`);
  }
} finally {
  if (socket?.readyState === WebSocket.OPEN) socket.close();
  if (chrome.exitCode === null) {
    chrome.kill("SIGTERM");
    await new Promise((resolve) => chrome.once("exit", resolve));
  }
  await rm(profile, { recursive: true, force: true, maxRetries: 5, retryDelay: 100 });
}
