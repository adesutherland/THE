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
    return waitFor(() => evaluate(`(() => {
      const viewport = document.querySelector('.editor-scroll'), grid = document.querySelector('.editor-grid');
      const style = getComputedStyle(grid);
      const fit = Math.floor((viewport.clientHeight-parseFloat(style.paddingTop)-parseFloat(style.paddingBottom))/parseFloat(style.lineHeight));
      const rows = document.querySelectorAll('.screen-row:not(.key-hint-row)').length;
      return rows === fit ? rows : false;
    })()`), "editor rows to fit browser viewport");
  }
  const largeRows = await sizeViewport(1200, 950);
  const smallRows = await sizeViewport(900, 550);
  if (smallRows >= largeRows) throw new Error("browser shrink did not reduce visible rows");
  const grownRows = await sizeViewport(1200, 950);
  await sizeViewport(455, 700);
  const hints = await evaluate(`(() => {
    const bar=document.querySelector('.key-hints');
    if (!bar) return null;
    const outer=bar.getBoundingClientRect();
    return [...bar.querySelectorAll('.key-hint')].map(n=>{
      const r=n.getBoundingClientRect();
      return {key:n.querySelector('kbd').textContent,visible:r.left>=outer.left && r.right<=outer.right && r.top>=outer.top && r.bottom<=outer.bottom};
    });
  })()`);
  if (!hints || hints.length !== 8 || hints.some(h=>!h.visible) || !hints.some(h=>h.key==='F10')) throw new Error('clipped narrow-browser key guide: '+JSON.stringify(hints));
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
  await waitFor(() => evaluate("document.querySelector('.status-text')?.textContent.includes('focus=filearea line=3')"), "third record for left navigation");
  await domKey('ArrowLeft','ArrowLeft');
  await waitFor(() => evaluate("document.querySelector('.status-text')?.textContent.includes('focus=prefix line=3') && document.querySelector('.prefix-cursor')?.style.left === '6ch'"), "left into current prefix");
  for (let i=0;i<7;i++) await domKey('ArrowLeft','ArrowLeft');
  await waitFor(() => evaluate("document.querySelector('.status-text')?.textContent.includes('focus=filearea line=2') && document.querySelector('.line-content .logical-cursor')?.style.left === '9ch'"), "left from prefix to previous record end");
  await navigationCommand('cursor goto 1 1');
  await waitFor(() => evaluate("document.querySelector('.status-text')?.textContent.includes('focus=filearea line=1')"), "file focus restored");
  if (!(await evaluate("[...document.querySelectorAll('.role-reserved .line-text')].some(n=>n.textContent.includes('F6 Cmd')&&n.textContent.includes('F10 Keys'))"))) throw new Error('function-key guide missing');
  await domKey('F10','F10');
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
  await waitFor(() => evaluate("document.querySelector('.status-line')?.textContent.includes('Saved')"), "space-indented file saved");
  await closeCurrentBuffer("tab-entry.txt");
  await selectBuffer("sample.txt");

  await evaluate("[...document.querySelectorAll('.menu-trigger')].find((node) => node.textContent.trim() === 'File').click()");
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

  await evaluate("[...document.querySelectorAll('.menu-trigger')].find((node) => node.textContent.trim() === 'File').click()");
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
    const state = await evaluate("({buffer: document.querySelector('.buffer-tab-current')?.textContent.trim(), status: document.querySelector('.status-text')?.textContent.trim(), lines: [...document.querySelectorAll('.line-text')].map((node) => ({text: node.textContent, classes: [...node.querySelectorAll('.syntax')].map((span) => span.className)})).filter((row) => row.text).slice(0, 6)})");
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
    const state = await evaluate("({buffer: document.querySelector('.buffer-tab-current')?.textContent.trim(), status: document.querySelector('.status-text')?.textContent.trim(), lines: [...document.querySelectorAll('.line-text')].map((node) => ({text: node.textContent, classes: [...node.querySelectorAll('.syntax')].map((span) => span.className)})).filter((row) => row.text).slice(0, 6)})");
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
    const state = await evaluate("({buffer: document.querySelector('.buffer-tab-current')?.textContent.trim(), status: document.querySelector('.status-text')?.textContent.trim(), lines: [...document.querySelectorAll('.line-text')].map((node) => ({text: node.textContent, classes: [...node.querySelectorAll('.syntax')].map((span) => span.className)})).filter((row) => row.text).slice(0, 6)})");
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
      () => evaluate("[...document.querySelectorAll('.line-text')].some((node) => node.textContent === 'Xalpha beta gamma')"),
      "printable keyboard input",
    );
  } catch (error) {
    const state = await evaluate("({active: document.activeElement?.className, sink: document.querySelector('.input-sink')?.value, lines: [...document.querySelectorAll('.line-text')].map((node) => node.textContent).filter(Boolean).slice(0, 5)})");
    throw new Error(`${error.message}; state=${JSON.stringify(state)}`);
  }
  await domKey("Backspace", "Backspace");
  try {
    await waitFor(
      () => evaluate("[...document.querySelectorAll('.line-text')].some((node) => node.textContent === 'alpha beta gamma')"),
      "Backspace mapping",
    );
  } catch (error) {
    const lines = await evaluate("[...document.querySelectorAll('.line-text')].map((node) => node.textContent).filter(Boolean).slice(0, 5)");
    throw new Error(`${error.message}; lines=${JSON.stringify(lines)}`);
  }
  await insertText("Z");
  await evaluate("(() => { const row = [...document.querySelectorAll('.screen-row')].find((node) => node.querySelector('.line-text')?.textContent === 'delete me'); const prefix = row?.querySelector('.prefix-cell'); if (!prefix) return false; const rect = prefix.getBoundingClientRect(); prefix.dispatchEvent(new MouseEvent('click', {bubbles: true, clientX: rect.left + 4, clientY: rect.top + 8})); return true; })()");
  await waitFor(
    () => evaluate("[...document.querySelectorAll('.status-line span')].some((node) => node.textContent.trim() === 'prefix')"),
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
    () => evaluate("[...document.querySelectorAll('.status-line span')].some((node) => node.textContent.trim() === 'Saved')"),
    "Ctrl+S action",
  );


  if(await evaluate("!!document.querySelector('.rexx-dock')")){
    await openWorkspaceFile('console.rexx');
    await evaluate("[...document.querySelectorAll('.rexx-dock button')].find(b=>b.textContent==='Run Rexx').click()");
    await waitFor(()=>evaluate("document.querySelector('.console-output').textContent.includes('native graphical proof')"),'native program console',30000);
    await evaluate("document.querySelector('#rexx-input').focus()");await command('Input.insertText',{text:'Zoë😀'});await command('Input.dispatchKeyEvent',{type:'keyDown',key:'Enter',code:'Enter',windowsVirtualKeyCode:13,text:'\r'});await command('Input.dispatchKeyEvent',{type:'keyUp',key:'Enter',code:'Enter',windowsVirtualKeyCode:13});
    await waitFor(()=>evaluate("document.querySelector('.run-state').textContent==='Completed · status 7'"),'native completion',30000);
    if(!await evaluate("document.querySelector('.console-output').textContent.includes('Hello ZOË😀')"))throw Error('Native Unicode console output');
    await closeCurrentBuffer('console.rexx');
    await selectBuffer('sample.txt');
    console.log('PASS native graphical RXBIN host: actual Classic execution, Unicode stdin/output and status');
  }
  if (exceptions.length) throw new Error(`browser exceptions: ${exceptions.join("; ")}`);
  await evaluate("[...document.querySelectorAll('.menu-trigger')].find((node) => node.textContent.trim() === 'File').click()");
  await evaluate("[...document.querySelectorAll('.menu-popover button')].find((node) => node.textContent.trim().startsWith('Close')).click()");
  await sleep(150);
} finally {
  if (socket?.readyState === WebSocket.OPEN) socket.close();
  if (chrome.exitCode === null) {
    chrome.kill("SIGTERM");
    await new Promise((resolve) => chrome.once("exit", resolve));
  }
  await rm(profile, { recursive: true, force: true, maxRetries: 5, retryDelay: 100 });
}
