import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import WebSocket from "ws";

const url = process.env.THE_WEB_URL;
const chromium = process.env.CHROMIUM_BIN;
if (!url || !chromium) throw new Error("THE_WEB_URL and CHROMIUM_BIN are required");

const profile = await mkdtemp(join(tmpdir(), "the-prefix-"));
const chrome = spawn(chromium, ["--headless=new", "--no-sandbox", "--disable-gpu",
  "--remote-debugging-port=0", `--user-data-dir=${profile}`, "about:blank"], { stdio: "ignore" });
let socket;
let nextId = 0;
const pending = new Map();
const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
async function until(check, label) {
  const deadline = Date.now() + 15000;
  while (Date.now() < deadline) {
    try { const value = await check(); if (value) return value; } catch { /* wait for startup */ }
    await sleep(50);
  }
  throw new Error(`Timed out waiting for ${label}`);
}
function cdp(method, params = {}) {
  const id = ++nextId;
  return new Promise((resolve, reject) => {
    pending.set(id, { resolve, reject });
    socket.send(JSON.stringify({ id, method, params }));
  });
}
async function evaluate(expression) {
  const result = await cdp("Runtime.evaluate", { expression, awaitPromise: true, returnByValue: true });
  if (result.exceptionDetails) throw new Error(result.exceptionDetails.text);
  return result.result?.value;
}
async function state() {
  return evaluate(`(() => {
    const cell = document.querySelector('.prefix-cell-active');
    if (!cell) return null;
    const cursor = cell.querySelector('.prefix-cursor');
    const style = getComputedStyle(cell);
    const caret = getComputedStyle(cursor);
    const bounds = cell.getBoundingClientRect(), cursorBounds = cursor.getBoundingClientRect();
    const s=window.__snapshot;
    const row=s?.screen_rows.find(r=>r.line===s.focus.line && r.r===s.focus.row);
    return {text: cell.firstChild?.textContent, focus: document.querySelector('.status-line')?.dataset.focusZone,
      color: style.color, background: style.backgroundColor, opacity: style.opacity,
      caretColor: caret.borderLeftColor, caretBackground: caret.backgroundColor,
      cursorLeft: cursorBounds.left - bounds.left, cursorWidth: cursorBounds.width,
      cellWidth: bounds.width, dirty: document.querySelector('.status-line')?.dataset.bufferDirty,
      modelPrefix: row?.p, lineText: row?.t};
  })()`);
}
function luminance(rgb) {
  const channels = rgb.match(/\d+/g).slice(0, 3).map(Number).map((value) => {
    const component = value / 255;
    return component <= 0.04045 ? component / 12.92 : ((component + 0.055) / 1.055) ** 2.4;
  });
  return channels[0] * 0.2126 + channels[1] * 0.7152 + channels[2] * 0.0722;
}
function contrast(a, b) {
  const [bright, dark] = [luminance(a), luminance(b)].sort((x, y) => y - x);
  return (bright + 0.05) / (dark + 0.05);
}
async function key(key, code) {
  await evaluate(`document.activeElement.dispatchEvent(new KeyboardEvent('keydown', {
    key: ${JSON.stringify(key)}, code: ${JSON.stringify(code)}, bubbles: true, cancelable: true
  }))`);
}
async function type(character) {
  await cdp("Input.dispatchKeyEvent", { type: "char", key: character,
    text: character, unmodifiedText: character });
}

try {
  const port = await until(async () => Number((await readFile(join(profile, "DevToolsActivePort"), "utf8")).split("\n")[0]), "Chrome port");
  const target = await (await fetch(`http://127.0.0.1:${port}/json/new?about:blank`, { method: "PUT" })).json();
  socket = new WebSocket(target.webSocketDebuggerUrl);
  await new Promise((resolve) => socket.once("open", resolve));
  socket.on("message", (raw) => {
    const message = JSON.parse(raw.toString());
    if (!message.id) return;
    const request = pending.get(message.id);
    pending.delete(message.id);
    if (message.error) request.reject(new Error(message.error.message));
    else request.resolve(message.result);
  });
  await until(() => cdp("Page.getFrameTree"), "Chrome page");
  await cdp("Runtime.enable");
  await cdp("Page.enable");
  await cdp("Page.addScriptToEvaluateOnNewDocument", { source: `
    const NativeWorker = Worker;
    window.Worker = class extends NativeWorker {
      constructor(...args) {
        super(...args);
        this.addEventListener('message', ({data}) => {
          if (data.screen_rows) window.__snapshot = data;
        });
      }
    };
  ` });
  await cdp("Page.navigate", { url });
  await until(() => evaluate("document.querySelector('.connection')?.textContent.trim() === 'online' && !!document.querySelector('.role-file .prefix-cell')"), "editor");
  const before = await evaluate("document.querySelector('.role-file .prefix-cell').firstChild?.textContent");
  const initialText = await evaluate("window.__snapshot?.screen_rows.find(r=>r.line===1)?.t");
  await evaluate("document.querySelector('.role-file .prefix-cell').click()");
  const focused = await until(async () => { const value=await state(); return value?.focus === "prefix" ? value : false; }, "prefix focus");
  const screenshot = process.env.THE_PREFIX_SCREENSHOT;
  if (screenshot) {
    const image = await cdp("Page.captureScreenshot", { format: "png" });
    await writeFile(screenshot, Buffer.from(image.data, "base64"));
  }
  console.log(JSON.stringify({ before, focused }));
  assert.match(before, /\d/);
  assert.equal(focused.text, before, "line number remains visible in prefix focus");
  assert.equal(focused.modelPrefix, before, "semantic frame keeps the configured number format");
  assert.equal(focused.focus, "prefix");
  assert.equal(focused.dirty, "0");
  assert(contrast(focused.color, focused.background) >= 4.5, "active number has readable contrast");
  assert(focused.cursorLeft >= 0 && focused.cursorLeft + focused.cursorWidth <= focused.cellWidth,
    "logical cursor remains within the prefix cell");
  assert.notEqual(focused.caretColor, focused.background, "logical cursor remains visible");

  await type("d");
  const typed = await until(async () => { const value=await state(); return value?.modelPrefix === "d" ? value : false; }, "typed prefix");
  assert.equal(typed.text, "d", "typed command replaces the number during editing");
  await key("Backspace", "Backspace");
  const cleared = await until(async () => { const value=await state(); return value?.modelPrefix === before ? value : false; }, "cleared prefix");
  assert.equal(cleared.text, before, "clearing the command restores the number");

  await evaluate("document.querySelector('.role-file .line-content').click()");
  await until(() => evaluate("document.querySelector('.status-line')?.dataset.focusZone === 'filearea'"), "file focus");
  await key("Home", "Home");
  await key("ArrowLeft", "ArrowLeft");
  const keyboard = await until(async () => { const value=await state(); return value?.focus === "prefix" ? value : false; }, "keyboard prefix focus");
  assert.equal(keyboard.text, before, "keyboard navigation preserves the line number");
  assert(keyboard.cursorLeft >= 0 && keyboard.cursorLeft + keyboard.cursorWidth <= keyboard.cellWidth);
  assert(contrast(keyboard.color, keyboard.background) >= 4.5, "keyboard focused number has readable contrast");
  assert.notEqual(keyboard.caretColor, keyboard.background, "keyboard focused cursor remains visible");

  await evaluate(`new Promise(resolve => { const input=document.querySelector('.command-form input');
    input.value='set number off'; input.dispatchEvent(new Event('input',{bubbles:true}));
    setTimeout(() => { input.form.requestSubmit(); resolve(true); }, 0); })`);
  await until(() => evaluate("window.__snapshot?.screen_rows.find(r=>r.line===1)?.p === '======='"), "NUMBER OFF");
  await evaluate("document.querySelector('.role-file .prefix-cell').click()");
  const unnumbered = await until(async () => { const value=await state(); return value?.focus === "prefix" ? value : false; }, "unnumbered prefix focus");
  assert.equal(unnumbered.text, "=======", "NUMBER OFF remains unnumbered in prefix focus");
  assert.equal(unnumbered.modelPrefix, "=======", "NUMBER OFF semantic prefix is preserved");
  assert.equal(unnumbered.lineText, initialText, "prefix focus does not change file text");
  assert.equal(unnumbered.dirty, "0", "prefix focus does not dirty the buffer");
} finally {
  socket?.close();
  chrome.kill();
  await until(() => chrome.exitCode !== null || chrome.signalCode !== null, "Chrome exit");
  await rm(profile, { recursive: true, force: true });
}
