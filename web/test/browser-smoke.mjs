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
    pending.set(id, { resolve, reject });
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

try {
  const portFile = join(profile, "DevToolsActivePort");
  const port = await waitFor(async () => {
    const contents = await readFile(portFile, "utf8");
    return Number(contents.split("\n", 1)[0]) || 0;
  }, "Chrome DevTools port");
  const targetResponse = await fetch(
    `http://127.0.0.1:${port}/json/new?${encodeURIComponent(pageUrl)}`,
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
      if (message.error) request.reject(new Error(message.error.message));
      else request.resolve(message.result);
    } else if (message.method === "Runtime.exceptionThrown") {
      exceptions.push(message.params?.exceptionDetails?.text || "browser exception");
    }
  });

  await command("Runtime.enable");
  await command("Page.enable");
  await command("Page.bringToFront");
  await waitFor(
    () => evaluate("document.readyState === 'complete' && !!document.querySelector('.app-shell')"),
    "THE application shell",
  );
  await waitFor(
    () => evaluate("document.querySelector('.connection')?.textContent.trim() === 'online'"),
    "web driver connection",
  );

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
  await domKey("s", "KeyS", true);
  await waitFor(
    () => evaluate("[...document.querySelectorAll('.status-line span')].some((node) => node.textContent.trim() === 'Saved')"),
    "Ctrl+S action",
  );

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
