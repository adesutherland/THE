// Real Wasm editor/DSLSH painting contract. Hold completed snapshots briefly so
// Chromium must paint the pending state; the parser and editor run unchanged.
import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import {mkdtemp, readFile, rm} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import WebSocket from 'ws';

const pageUrl = process.env.THE_WEB_URL;
const chromium = process.env.CHROMIUM_BIN;
if (!pageUrl || !chromium) throw Error('THE_WEB_URL and CHROMIUM_BIN are required');
const profile = await mkdtemp(join(tmpdir(), 'the-paint-'));
const chrome = spawn(chromium, ['--headless=new', '--no-sandbox', '--disable-gpu', '--no-first-run', '--remote-debugging-port=0', `--user-data-dir=${profile}`, 'about:blank'], {stdio: 'ignore'});
let socket, nextId = 0;
const pending = new Map(), exceptions = [];
const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));
async function wait(check, label) {
  const deadline = Date.now() + 30000;
  while (Date.now() < deadline) { const result = await check(); if (result) return result; await sleep(20); }
  throw Error('Timed out: ' + label);
}
function cdp(method, params = {}) {
  return new Promise((resolve, reject) => {
    const id = ++nextId;
    pending.set(id, {resolve, reject});
    socket.send(JSON.stringify({id, method, params}));
  });
}
async function evaluate(expression) {
  const result = await cdp('Runtime.evaluate', {expression, awaitPromise: true, returnByValue: true});
  if (result.exceptionDetails) throw Error(JSON.stringify(result.exceptionDetails));
  return result.result?.value;
}
const send = request => evaluate(`window.__editor.postMessage(${JSON.stringify(request)})`);
const frame = () => evaluate('new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)))');
async function lineImage() {
  const clip = await evaluate(`(() => {const r=window.__pixelSteady.getBoundingClientRect();return {x:r.x,y:r.y,width:r.width,height:r.height,scale:1};})()`);
  return (await cdp('Page.captureScreenshot', {format: 'png', clip})).data;
}
try {
  const port = await wait(async () => {
    try { return +(await readFile(join(profile, 'DevToolsActivePort'), 'utf8')).split('\n')[0]; }
    catch { return false; }
  }, 'Chromium');
  const target = await (await fetch(`http://127.0.0.1:${port}/json/new?about:blank`, {method: 'PUT'})).json();
  socket = new WebSocket(target.webSocketDebuggerUrl);
  await new Promise(resolve => socket.once('open', resolve));
  socket.on('message', bytes => {
    const message = JSON.parse(bytes);
    if (message.id) {
      const request = pending.get(message.id); pending.delete(message.id);
      if (message.error) request.reject(Error(JSON.stringify(message.error))); else request.resolve(message.result);
    } else if (message.method === 'Runtime.exceptionThrown') exceptions.push(message.params.exceptionDetails.exception?.description ?? message.params.exceptionDetails.text);
  });
  await cdp('Runtime.enable'); await cdp('Page.enable');
  await cdp('Emulation.setDeviceMetricsOverride', {width: 1280, height: 850, deviceScaleFactor: 1, mobile: false});
  await cdp('Page.addScriptToEvaluateOnNewDocument', {source: `
    const NativeWorker = window.Worker;
    const onmessage = Object.getOwnPropertyDescriptor(NativeWorker.prototype, 'onmessage');
    window.Worker = class extends NativeWorker {
      constructor(...args) {
        super(...args); window.__editor = this;
        Object.defineProperty(this, 'onmessage', {
          set(callback) { onmessage.set.call(this, event => {
            const deliver = () => {if (event.data.screen_rows) window.__snapshot = event.data; callback.call(this, event);};
            if (window.__delayReady && event.data.screen_rows && event.data.parser?.state === 'ready') setTimeout(deliver, 250);
            else deliver();
          }); }
        });
      }
    };
  `});
  await cdp('Page.navigate', {url: pageUrl});
  await wait(() => evaluate('!!window.__snapshot'), 'Wasm startup');
  await send({type: 'command', command: 'cursor goto 1 1'});
  await wait(() => evaluate("window.__snapshot?.status_line?.includes('[W]')"), 'current character in profile status');
  await frame();
  assert.equal(await evaluate("document.querySelector('.profile-status')?.textContent.includes('[W]')"), true,
    'browser paints the editor-owned current character');
  const fileTextLeft = () => evaluate(`(() => document.querySelector('[aria-label="Prefix line 1"]')?.closest('.screen-row')?.querySelector('.line-text')?.getBoundingClientRect().left)()`);
  await frame();
  const unscrolledLeft = await fileTextLeft();
  await send({type: 'command', command: 'right 5'});
  await wait(() => evaluate('window.__snapshot?.screen_rows.find(row => row.role === "file" && row.line === 1)?.sc === 5'), 'RIGHT command updates horizontal view');
  await frame();
  const scrolledLeft = await fileTextLeft();
  assert.ok(unscrolledLeft - scrolledLeft > 25, 'RIGHT visibly shifts the file text');
  await send({type: 'command', command: 'left 5'});
  await wait(() => evaluate('window.__snapshot?.screen_rows.find(row => row.role === "file" && row.line === 1)?.sc === 0'), 'LEFT command restores horizontal view');
  await frame();
  assert.ok(Math.abs((await fileTextLeft()) - unscrolledLeft) < 1, 'LEFT restores the file text position');
  await send({type: 'action', action: 'file.open', argument: '/workspace/ClassicDemo.rexx'});
  await wait(() => evaluate("window.__snapshot?.buffer.path.endsWith('ClassicDemo.rexx') && window.__snapshot.parser?.state==='ready'"), 'actual DSLSH styles');
  await send({type: 'command', command: 'cursor goto 1 4'});
  await wait(() => evaluate('window.__snapshot.focus.line===1 && window.__snapshot.focus.cell===3'), 'typing cursor');
  await frame();
  await evaluate(`
    window.__line = n => document.querySelector('[aria-label="Prefix line '+n+'"]').closest('.screen-row').querySelector('.line-text');
    window.__steady = window.__line(3);
    window.__pixelSteady = window.__line(4);
    window.__string = window.__steady.querySelector('.syntax-string');
    window.__steadyText = window.__steady.textContent;
    window.__editorGrid = document.querySelector('.editor-grid');
    window.__initialSource = window.__snapshot.source;
    window.__paintDrops = [];
    window.__observePaint = true;
    function observePaint() {
      if (!window.__observePaint) return;
      const style = getComputedStyle(window.__steady);
      if (!window.__editorGrid.isConnected || !window.__steady.isConnected || !window.__string.isConnected || window.__steady.textContent!==window.__steadyText || style.visibility==='hidden' || style.display==='none' || style.opacity==='0')
        window.__paintDrops.push({state:window.__snapshot.parser.state,text:window.__steady.textContent,styled:window.__string.isConnected});
      requestAnimationFrame(observePaint);
    }
    requestAnimationFrame(observePaint);
    document.querySelector('.input-sink').focus();
    window.__selectSteady = () => {
      const selection=getSelection(), range=document.createRange();
      range.setStart(window.__string.firstChild,1);range.setEnd(window.__string.firstChild,6);
      selection.removeAllRanges();selection.addRange(range);
      window.__selectionText=selection.toString();window.__selectionAnchor=selection.anchorNode;
    };
    window.__delayReady=true;
  `);
  assert.equal(await evaluate('!!window.__string'), true, 'baseline has real string highlighting');
  const baselineImage = await lineImage();
  const pendingStates = [];
  for (const [index, character] of [...'abc'].entries()) {
    const previousRevision = await evaluate('window.__snapshot.parser.revision');
    await cdp('Input.insertText', {text: character});
    await wait(() => evaluate(`window.__snapshot.parser.state==='pending' && window.__snapshot.parser.revision>${previousRevision} && window.__line(1).textContent.startsWith('/* ${'abc'.slice(0,index+1)}')`), 'pending typing frame');
    await frame();
    const state = await evaluate(`({parser:window.__snapshot.parser.state,authoritativeStyles:window.__snapshot.screen_rows.find(row=>row.line===3).s?.length??0,paintedStyles:window.__steady.querySelectorAll('.syntax').length,editedStyles:window.__line(1).querySelectorAll('.syntax').length,retained:window.__string.isConnected})`);
    pendingStates.push(state);
    assert.equal(state.authoritativeStyles, 0, 'test reaches the actual unstyled pending snapshot');
    assert.equal(state.retained, true, 'unchanged highlighted text must stay painted while typing');
    assert.equal(state.editedStyles, 0, 'changed text never receives cached styles');
    assert.equal(await lineImage() === baselineImage, true, 'unchanged row pixels remain identical during pending parse');
    // Native typing selects the textarea. A text selection made during the
    // pending paint must survive the unrelated asynchronous syntax update.
    await evaluate('window.__selectSteady()');
    await wait(() => evaluate("window.__snapshot.parser.state==='ready' && window.__snapshot.parser.applied===window.__snapshot.parser.revision"), 'async highlight completion');
    await frame();
    assert.equal(await lineImage() === baselineImage, true, 'unchanged row pixels remain identical after async highlighting');
    assert.equal(await evaluate('window.__string.isConnected'), true, 'async style updates preserve unchanged syntax nodes');
    assert.equal(await evaluate("document.activeElement.className"), 'input-sink', 'typing retains editor focus');
    assert.equal(await evaluate('window.__snapshot.focus.cell'), 4 + index, 'logical caret follows typed text');
    assert.equal(await evaluate('getSelection().anchorNode===window.__selectionAnchor && getSelection().toString()===window.__selectionText'), true, 'untouched text selection survives async highlighting');
  }
  await evaluate('window.__observePaint=false;window.__delayReady=false');
  assert.deepEqual(await evaluate('window.__paintDrops'), [], 'no animation frame removes unchanged text or highlighting');
  assert.equal(await evaluate('window.__snapshot.buffer.dirty'), 1, 'typing marks unsaved content');
  await send({type: 'action', action: 'file.save'});
  await wait(() => evaluate('window.__snapshot.buffer.dirty===0'), 'durable save');
  const saved = await evaluate(`new Promise((resolve,reject)=>{
    const open=indexedDB.open('/workspace');open.onerror=()=>reject(open.error);open.onsuccess=()=>{
      const db=open.result,get=db.transaction('FILE_DATA').objectStore('FILE_DATA').get('/workspace/ClassicDemo.rexx');
      get.onsuccess=()=>{resolve(Array.from(get.result.contents));db.close();};get.onerror=()=>reject(get.error);
    };
  })`);
  const original = await (await fetch(new URL('examples/ClassicDemo.rexx', pageUrl))).text();
  const expected = original.slice(0,3) + 'abc' + original.slice(3);
  assert.equal(new TextDecoder().decode(Uint8Array.from(saved)), expected, 'saved bytes contain exactly the typed edit');
  await cdp('Page.reload', {ignoreCache:true});
  await wait(() => evaluate("window.__snapshot?.buffer.path.endsWith('Welcome.txt')"), 'new editor after reload');
  await send({type: 'action', action: 'file.open', argument: '/workspace/ClassicDemo.rexx'});
  await wait(() => evaluate("window.__snapshot?.buffer.path.endsWith('ClassicDemo.rexx') && window.__snapshot.parser?.state==='ready'"), 'reopen edited source');
  assert.equal(await evaluate("window.__snapshot.screen_rows.find(row=>row.line===1).t.startsWith('/* abcClassic') && !window.__snapshot.buffer.dirty"), true, 'reopen restores saved bytes cleanly');
  assert.deepEqual(exceptions, []);
  console.log('PASS Wasm paint continuity: three real key inputs, pending and async-ready pixel equality for untouched highlighted text, retained DOM selection/focus, exact caret and saved/reopened bytes; pending frames '+JSON.stringify(pendingStates));
} catch (error) {
  if (socket?.readyState === WebSocket.OPEN)
    console.error('Paint failure state:', await evaluate("({body:document.body.innerText.slice(-1200),parser:window.__snapshot?.parser})"), exceptions);
  throw error;
} finally {
  socket?.close(); chrome.kill();
  await rm(profile, {recursive:true,force:true,maxRetries:10,retryDelay:100});
}
