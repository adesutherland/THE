import assert from 'node:assert/strict';
import { execFile } from 'node:child_process';
import { mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import { promisify } from 'node:util';
import { pathToFileURL } from 'node:url';

const runFile = promisify(execFile);
const directory = resolve(process.argv[2] ?? 'build-wasm-classic');
const compiler = resolve(process.env.THE_CREXX_HOST_BIN ?? 'build/crexx-classic-host/bin');
const temporary = await mkdtemp(join(tmpdir(), 'the-wasm-log-'));
let editor;
try {
  const source = join(temporary, 'startup.the');
  const output = join(temporary, 'startup');
  await writeFile(source, `options levelb
import rxfnsb
address the
'WORKBENCH BEGIN OWNER public.logs';
'WORKBENCH PANEL PUT public.output KIND LOG MODEL public.programlog';
'WORKBENCH PANEL PLACE public.output RIGHT FALLBACK BELOW,TAB MINCOLS 10';
'WORKBENCH ROUTE PUT public.stdout FROM tool:classic-rexx/stdout TO log:public.programlog';
'WORKBENCH COMMIT';
return 0
`);
  await runFile(join(compiler, 'rxc'), ['-i', compiler, '-o', output, source]);
  await runFile(join(compiler, 'rxas'), ['-o', output, output]);
  const { default: createEditor } = await import(pathToFileURL(join(directory, 'crxthe_wasm.mjs')));
  const data = await readFile(join(directory, 'crxthe_wasm.data'));
  const messages = [];
  const requests = [];
  let wake;
  let startedJob = 0;
  editor = await createEditor({
    noInitialRun: true,
    wasmBinary: await readFile(join(directory, 'crxthe_wasm.wasm')),
    getPreloadedPackage: () => data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength),
    sendEditorMessage(message) { messages.push(message); },
    nextEditorRequest() { return requests.length ? Promise.resolve(requests.shift())
      : new Promise((resolveRequest) => { wake = resolveRequest; }); },
    startTool(request) { startedJob = request.id; return true; },
    toolInput() { return true; }, toolEof() { return true; }, stopTool() {},
    async flushWorkspace() {},
  });
  editor.FS.mkdir('/workspace');
  editor.FS.writeFile('/workspace/Welcome.txt', "say 'startup routed stdout'\nexit 0\n");
  editor.FS.writeFile('/workspace/startup.rxbin', await readFile(`${output}.rxbin`));
  editor.FS.chdir('/workspace');
  assert.equal(editor.ccall('the_wasm_has_crexx', 'number', [], [], {}), 1,
    'trusted profile fixture requires the classic Wasm engine');
  for (const [name, value] of Object.entries({
    HOME: '/home/web_user', THE_HOME_DIR: '/app/', THE_WEB_WORKSPACE: '/workspace',
    THE_WEB_READONLY_ROOTS: '/app', THE_WEB_PROFILE: '/workspace/startup.rxbin',
    THE_WEB_MACRO_PATH: '/app:/workspace', THE_REXX_INTERPRETER: 'CREXX',
  })) editor.ccall('setenv', 'number', ['string', 'string', 'number'], [name, value, 1], {});
  const run = editor.ccall('the_wasm_main', 'number', [], [], { async: true });
  const send = (request) => {
    if (wake) { const resume = wake; wake = undefined; resume(request); }
    else requests.push(request);
  };
  const wait = async (condition, label) => {
    const end = Date.now() + 10000;
    while (Date.now() < end) {
      const value = condition();
      if (value) return value;
      await new Promise((accept) => setTimeout(accept, 10));
    }
    throw new Error(`Timeout: ${label} ${JSON.stringify(messages.slice(-3))}`);
  };
  const page = (section) => messages.filter((message) => message.type === 'workbench'
    && message.operation === 'list' && message.data?.section === section).at(-1)?.data;
  const panel = await wait(() => page('panels')?.records.find((record) => record.id === 'public.output'),
    'trusted startup LOG panel publication');
  assert.equal(panel.model, 'public.programlog');
  send({ type: 'workbench', version: 1, operation: 'list', section: 'routes', offset: 0, limit: 8 });
  const routes = await wait(() => page('routes'), 'trusted startup route publication');
  assert.equal(routes.records.find((record) => record.id === 'public.stdout')?.model,
    'public.programlog');
  send({ type: 'workbench', version: 1, operation: 'panel.show',
    requestId: `${messages.find((message) => message.type === 'hello').workbenchSession}.1`,
    panel: 'public.output', expectGeneration: panel.generation });
  await wait(() => messages.some((message) => message.operation === 'panel-state'
    && message.panels.some((record) => record.id === 'public.output' && record.visible)),
    'visible LOG panel');
  const classic = await wait(() => messages.findLast((message) => message.type === 'tools')
    ?.catalog.find((tool) => tool.id === 'classic-rexx'), 'classic-rexx catalog');
  if (classic.available) {
    send({ type: 'tool', action: 'run', tool: 'classic-rexx' });
    await wait(() => startedJob > 0, 'classic-rexx tool start');
    send({ type: 'tool-event', event: 'started', job: startedJob });
    send({ type: 'tool-event', event: 'output', job: startedJob, fd: 1,
      bytes: new TextEncoder().encode('startup routed stdout\n') });
    const state = await wait(() => messages.findLast((message) => message.operation === 'panel-state'
      && message.panels.some((record) => record.id === 'public.output'
        && BigInt(record.contentRevision) > 1n)), 'routed stdout content revision');
    const log = state.panels.find((record) => record.id === 'public.output');
    send({ type: 'workbench', version: 1, operation: 'log.list', model: log.model,
      generation: log.modelGeneration, cursor: '0', offset: 0, records: 8, bytes: 4096 });
    const entry = await wait(() => messages.findLast((message) => message.operation === 'log'
      && message.data?.records.some((record) => record.text.includes('startup routed stdout'))),
      'routed stdout LOG page');
    assert.equal(entry.data.records[0].stream, 1);
  }
  assert.equal(messages.findLast((message) => message.screen_rows)?.buffer?.dirty, 0);
  send({ type: 'action', action: 'file.close' });
  assert.equal(await Promise.race([run, new Promise((_, reject) =>
    setTimeout(() => reject(new Error('shutdown timeout')), 3000))]), 0);
  console.log(`PASS Wasm trusted startup LOG route and panel-state${classic.available ? ', stdout and log page' : ' (runner unavailable in this build)'}`);
} finally {
  await rm(temporary, { recursive: true, force: true });
}
