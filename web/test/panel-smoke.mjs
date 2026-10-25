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

  await evaluate("document.querySelector('.input-sink').focus()");
  await domKey("F7", "F7");
  await waitFor(() => evaluate("!!document.querySelector('.fullscreen-panel')"), "macro panel");
  const labels = await evaluate("[...document.querySelectorAll('.panel-protected')].map(n => n.textContent).join(' ')");
  if (!labels.includes("Name:") || !labels.includes("Topic:")) throw new Error("missing protected labels");
  await insertText("Alice");
  await domKey("Tab", "Tab");
  await insertText("Rexx");
  await waitFor(() => evaluate("[...document.querySelectorAll('.panel-editable')].some(n => n.textContent.startsWith('Alice')) && [...document.querySelectorAll('.panel-editable')].some(n => n.textContent.startsWith('Rexx'))"), "edited fields");
  await evaluate("document.querySelector('.panel-protected').click()");
  if (await evaluate("document.querySelector('.panel-protected').textContent") !== "Name: ") throw new Error("protected label changed");
  await domKey("F3", "F3");
  await waitFor(() => evaluate("!document.querySelector('.fullscreen-panel')"), "submitted panel");
  await domKey("F7", "F7");
  await waitFor(() => evaluate("!!document.querySelector('.fullscreen-panel')"), "second panel");
  await insertText("discard");
  await domKey("Escape", "Escape");
  await waitFor(() => evaluate("!document.querySelector('.fullscreen-panel')"), "cancelled panel");
  await domKey("F7", "F7");
  await waitFor(() => evaluate("!!document.querySelector('.fullscreen-panel')"), "disconnect panel");
  await command("Page.navigate", { url: "about:blank" });
  await sleep(150);
  await command("Page.navigate", { url: pageUrl });
  await waitFor(() => evaluate("document.querySelector('.connection')?.textContent.trim() === 'online'"), "reconnected editor");
  await waitFor(() => evaluate("!!document.querySelector('.app-shell') && !document.querySelector('.fullscreen-panel')"), "disconnect cancellation");
  // The WebSocket opens before its first buffer snapshot is rendered.
  await waitFor(() => evaluate("[...document.querySelectorAll('.line-text')].map(n => n.textContent).filter(t => t === 'seed' || t === 'tail').join('|') === 'seed|tail'"), "unchanged buffer snapshot after reconnect");
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
