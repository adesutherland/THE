// Real native web driver + production UI. Uses only a disposable source/profile.
import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { mkdtemp, readFile, writeFile, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import WebSocket from "ws";

const [binaryArg, assetsArg] = process.argv.slice(2);
if (!binaryArg || !assetsArg || !process.env.CHROMIUM_BIN) throw Error("usage: CHROMIUM_BIN=... node file-field-browser-smoke.mjs BINARY ASSETS");
const binary = resolve(binaryArg), assets = resolve(assetsArg), directory = await mkdtemp(join(tmpdir(), "THE-file-fields-"));
const sample = join(directory, "fields.txt"), profile = join(directory, "profile.the"), chromeProfile = join(directory, "chrome");
const source = Array.from({ length: 12 }, (_, index) => `!pName ${index + 1}: !uGuest   !p END ${"protected ".repeat(15)}`).join("\n") + "\n";
await writeFile(sample, source);
const stock = new URL(`../../web-profile${process.env.THE_REXX_INTERPRETER === "REGINA" ? "-regina" : ""}.the`, import.meta.url);
await writeFile(profile, await readFile(stock, "utf8") + "\n'set ctlchar ! escape';\n'set ctlchar p protect white';\n'set ctlchar u noprotect green';\n'set filectlchar on';\n");
let errors = "", output = "";
const server = spawn(binary, ["--driver", "web", "-p", profile, sample], { env: { ...process.env,
  THE_HOME_DIR: process.env.THE_HOME_DIR || join(dirname(binary), "release"), THE_REXX_INTERPRETER: process.env.THE_REXX_INTERPRETER || "CREXX",
  THE_WEB_ROOT: assets, THE_WEB_WORKSPACE: directory, THE_WEB_PROFILE: profile }, stdio: ["ignore", "pipe", "pipe"] });
server.stdout.on("data", (data) => { output += data; }); server.stderr.on("data", (data) => { errors += data; });
const chrome = spawn(process.env.CHROMIUM_BIN, ["--headless=new", "--no-sandbox", "--disable-gpu", "--no-first-run", "--remote-debugging-port=0", `--user-data-dir=${chromeProfile}`, "about:blank"], { stdio: "ignore" });
const pause = (milliseconds) => new Promise((done) => setTimeout(done, milliseconds));
async function until(check, label, timeout = 15000) {
  const end = Date.now() + timeout;
  while (Date.now() < end) { if (await check()) return; await pause(30); }
  throw Error(`Timed out: ${label}; server=${errors.slice(-1600)}`);
}
let socket, request = 0; const pending = new Map(), exceptions = [];
function cdp(method, params = {}) {
  return new Promise((resolve, reject) => { const id = ++request; pending.set(id, { resolve, reject }); socket.send(JSON.stringify({ id, method, params })); });
}
async function evaluate(expression) {
  const result = await cdp("Runtime.evaluate", { expression, awaitPromise: true, returnByValue: true });
  if (result.exceptionDetails) throw Error(result.exceptionDetails.exception?.description || result.exceptionDetails.text);
  return result.result?.value;
}
async function command(text) {
  const before = await evaluate("window.__snapshots.length");
  await evaluate(`window.__socket.send(JSON.stringify({type:'command',command:${JSON.stringify(text)}}))`);
  await until(() => evaluate(`window.__snapshots.length>${before}`), text);
}
const input = ".file-field-editable input";
async function change(value) {
  await evaluate(`(() => {const input=document.querySelector(${JSON.stringify(input)});input.focus();input.select();})()`);
  await cdp("Input.insertText", { text: value });
  await until(() => evaluate(`document.querySelector(${JSON.stringify(input)})?.value.trimEnd()===${JSON.stringify(value)} && !document.querySelector(${JSON.stringify(input)}).disabled`), `field value ${value}`);
}
async function click(index) {
  // A field SET may have updated the input while the coordinator is still
  // loading the next page. Wait for the action itself to be admitted.
  await until(() => evaluate(`(() => {const button=document.querySelector('.file-field-controls button:nth-child(${index})');return !!button&&!button.disabled})()`), `file-field control ${index} ready`);
  await evaluate(`document.querySelector('.file-field-controls button:nth-child(${index})').click()`);
}
try {
  await until(() => /THE web UI: (\S+)/.test(output), "native web URL", 30000);
  const url = output.match(/THE web UI: (\S+)/)[1];
  let port;
  await until(async () => { try { port = Number((await readFile(join(chromeProfile, "DevToolsActivePort"), "utf8")).split("\n")[0]); return port; } catch { return false; } }, "Chrome", 30000);
  const target = await (await fetch(`http://127.0.0.1:${port}/json/new?about:blank`, { method: "PUT" })).json();
  socket = new WebSocket(target.webSocketDebuggerUrl);
  await new Promise((done, reject) => { socket.once("open", done); socket.once("error", reject); });
  socket.on("message", (data) => {
    const message = JSON.parse(data);
    if (message.id && pending.has(message.id)) {
      const handler = pending.get(message.id); pending.delete(message.id);
      if (message.error) handler.reject(Error(message.error.message)); else handler.resolve(message.result);
    } else if (message.method === "Runtime.exceptionThrown") exceptions.push(message.params.exceptionDetails.text);
  });
  await cdp("Page.enable"); await cdp("Runtime.enable"); await cdp("Page.bringToFront");
  await cdp("Page.addScriptToEvaluateOnNewDocument", { source: `
    window.__messages=[];window.__snapshots=[];window.__sent=[];const Socket=window.WebSocket;
    window.WebSocket=class extends Socket{constructor(...args){super(...args);window.__socket=this;
      this.addEventListener('message',event=>{const value=JSON.parse(event.data);window.__messages.push(value);if(value.screen_rows)window.__snapshots.push(value);});}
      send(data){window.__sent.push(JSON.parse(data));return super.send(data);}};
  ` });
  await cdp("Page.navigate", { url });
  await until(() => evaluate("document.querySelector('.connection')?.textContent.trim()==='online' && window.__snapshots.length>0"), "online editor");
  for (const text of ["workbench begin owner user.fields", "workbench panel put user.fields.pane kind fields buffer active markup ctlchar access staged", "workbench panel place user.fields.pane right fallback below,tab mincols 10", "workbench commit"]) await command(text);
  await until(() => evaluate("!!document.querySelector('.named-file-fields-toggle')"), "file-field declaration");
  await evaluate("document.querySelector('#the-command').focus();document.querySelector('.named-file-fields-toggle').click()");
  await until(() => evaluate("document.querySelectorAll('.file-field-row').length===12"), "all C-owned rows");
  assert.equal(await evaluate("document.activeElement?.id==='the-command'"), true, "SHOW preserves command focus");
  assert.equal(await evaluate("document.querySelectorAll('.file-field-protected input').length"), 0);
  await evaluate("document.querySelector('.named-file-fields-toggle').click()");
  await until(() => evaluate("document.activeElement?.dataset.fieldId && !document.activeElement.disabled"), "first field focus");
  // Ordinary middle typing must overwrite through EDIT, not insert locally.
  await evaluate(`document.querySelector('${input}').setSelectionRange(1,1)`);
  await cdp("Input.insertText", { text: "Z" });
  await until(() => evaluate(`document.querySelector('${input}').value.trimEnd()==='GZest'`), "core overwrite at exact UTF16 caret");
  await until(() => evaluate(`document.querySelector('${input}').selectionStart===2`), "core receipt restores admitted caret");
  assert.equal(await evaluate("window.__sent.filter(m=>m.operation==='filefield.edit').at(-1).offset"), 1);
  await cdp("Input.dispatchKeyEvent", { type: "keyDown", key: "Backspace", code: "Backspace", windowsVirtualKeyCode: 8 });
  await cdp("Input.dispatchKeyEvent", { type: "keyUp", key: "Backspace", code: "Backspace", windowsVirtualKeyCode: 8 });
  await until(() => evaluate(`document.querySelector('${input}').value.trimEnd()==='Gest' && document.querySelector('${input}').selectionStart===1`), "core Backspace shifts and pads");
  await change("界😀");
  assert.equal(await readFile(sample, "utf8"), source, "staging never writes source");
  await click(1);
  await until(() => evaluate("window.__messages.some(m=>m.operation==='result' && m.group && m.group!=='0' && m.code==='ok') && !document.querySelector('.file-field-editable input').disabled"), "atomic Apply receipt");
  assert.equal(await readFile(sample, "utf8"), source, "Apply leaves Save explicit");
  assert.equal(await evaluate("window.__snapshots.at(-1).buffer.dirty"), 1);
  await change("draft");
  await click(2);
  await until(() => evaluate("document.querySelector('.file-field-status').textContent.includes('Projection closed')"), "Cancel releases projection");
  assert.equal(await evaluate(`document.querySelector('${input}').value.trimEnd()`), "界😀");
  await click(4);
  await until(() => evaluate(`!document.querySelector('${input}').disabled && document.querySelector('${input}').value.trimEnd()==='界😀'`), "Reload recaptures applied buffer");
  await change("stale");
  await command("cursor goto 1 1");
  await click(1);
  await until(() => evaluate("document.querySelector('.file-field-status').textContent.includes('stale-revision')"), "conservative write token rejects stale Apply");
  assert.equal(await evaluate(`document.querySelector('${input}').value.trimEnd()`), "stale", "stale draft remains visible");
  assert.equal(await readFile(sample, "utf8"), source);
  await click(4);
  await until(() => evaluate(`!document.querySelector('${input}').disabled && document.querySelector('${input}').value.trimEnd()==='界😀'`), "explicit Reload discards stale draft");
  for (const [width, placement] of [[1280, "right"], [768, "below"], [390, "tab"]]) {
    await cdp("Emulation.setDeviceMetricsOverride", { width, height: 800, deviceScaleFactor: 1, mobile: false });
    await until(() => evaluate(`window.__sent.some(m=>m.operation==='panel.layout'&&m.placement==='${placement}')`), `${placement} layout`);
    assert.equal(await evaluate("(() => {const n=document.querySelector('.file-field-grid');return n.scrollWidth>n.clientWidth&&document.documentElement.scrollWidth<=innerWidth})()"), true);
  }
  await click(3);
  await until(() => evaluate("document.activeElement?.id==='the-command'"), "Return restores command origin");
  await evaluate("window.__socket.send(JSON.stringify({type:'action',action:'file.save'}))");
  const expected = source.replace("!uGuest   !p", "!u界😀    !p");
  await until(async () => (await readFile(sample, "utf8").catch((error) => { if (error.code === "ENOENT") return null; throw error; })) === expected, "explicit Save retains exact CTLCHAR bytes");
  assert.deepEqual(exceptions, []);
  console.log("PASS real native file-field browser: core rows/EDIT/caret/Unicode, SET/Apply/Cancel/Reload/Return, stale retained draft, explicit saved bytes and wide/narrow/phone layout");
} catch (error) {
  if (socket?.readyState === WebSocket.OPEN) console.error(JSON.stringify(await evaluate("({active:document.activeElement?.outerHTML?.slice(0,350),inputs:[...document.querySelectorAll('.file-field-editable input')].slice(0,2).map(n=>({value:n.value,disabled:n.disabled})),status:document.querySelector('.file-field-status')?.textContent,messages:window.__messages?.filter(m=>m.type==='workbench').slice(-5).map(m=>m.data?{operation:m.operation,draftRevision:m.data.draftRevision}:m),sent:window.__sent?.slice(-4)})"),null,2));
  throw error;
} finally {
  socket?.close(); chrome.kill(); server.kill();
  await Promise.all([chrome, server].filter((child) => child.exitCode === null).map((child) => new Promise((done) => child.once("exit", done))));
  await rm(directory, { recursive: true, force: true, maxRetries: 10, retryDelay: 100 });
}
