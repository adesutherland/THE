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
'WORKBENCH PANEL PUT public.reference KIND DOCUMENT BUFFER ACTIVE ACCESS READONLY';
'WORKBENCH FORM PUT public.person';
'WORKBENCH FIELD PUT public.person name LABEL "Name" VALUE "Guest" ACCESS EDIT MAXCELLS 40';
'WORKBENCH FIELD PUT public.person locked LABEL "Protected" VALUE "Keep" ACCESS READONLY MAXCELLS 40';
'WORKBENCH PANEL PUT public.personpane KIND FIELDS FORM public.person';
'WORKBENCH ROUTE PUT public.stdout FROM tool:classic-rexx/stdout TO log:public.programlog';
'WORKBENCH COMMIT';
'DEFINE F7 MACRO /workspace/modal.rxbin';
return 0
`);
  await runFile(join(compiler, 'rxc'), ['-i', compiler, '-o', output, source]);
  await runFile(join(compiler, 'rxas'), ['-o', output, output]);
  const modalSource=join(temporary,'modal.the'),modalOutput=join(temporary,'modal');
  await writeFile(modalSource,`options levelb
import rxfnsb
address the
'SET CTLCHAR ! ESCAPE';
'SET CTLCHAR p PROTECT WHITE';
'SET CTLCHAR u NOPROTECT GREEN';
'SET RESERVED 2 !pName: !uGuest          !p';
'READ NOCHANGE TAG';
'SET RESERVED 2 OFF';
return 0
`);
  await runFile(join(compiler,'rxc'),['-i',compiler,'-o',modalOutput,modalSource]);
  await runFile(join(compiler,'rxas'),['-o',modalOutput,modalOutput]);
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
  editor.FS.writeFile('/workspace/Welcome.txt', "say 'startup routed stdout'\nexit 0\n"
    + Array.from({ length: 18 }, (_, index) => `/* document ${index} 😀 */\n`).join(''));
  editor.FS.writeFile('/workspace/startup.rxbin', await readFile(`${output}.rxbin`));
  editor.FS.writeFile('/workspace/modal.rxbin',await readFile(`${modalOutput}.rxbin`));
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
  let requestSequence=0;
  const requestId=()=>`${messages.find((message)=>message.type==='hello').workbenchSession}.${++requestSequence}`;
  const reference = await wait(() => page('panels')?.records.find((record) => record.id === 'public.reference'),
    'trusted startup DOCUMENT panel publication');
  assert.equal(reference.kind, 'document');
  assert.equal(reference.readonly, true);
  const referenceState = await wait(() => messages.findLast((message) => message.operation === 'panel-state')
    ?.panels.find((record) => record.id === 'public.reference' && record.documentAvailable), 'DOCUMENT available metadata');
  send({ type: 'workbench', version: 1, operation: 'document.list', panel: reference.id,
    expectGeneration: reference.generation, expectRevision: referenceState.documentRevision,
    line: 1, offset: 0, records: 8, bytes: 8192 });
  const documentPage = await wait(() => messages.findLast((message) => message.operation === 'document')?.data,
    'Wasm bounded copied DOCUMENT page');
  assert.equal(documentPage.documentId, reference.documentId);
  assert.equal(documentPage.records.length, 8);
  assert.equal(documentPage.records[0].bytesHex, Buffer.from("say 'startup routed stdout'").toString('hex'));
  assert.equal(documentPage.nextLine, 9);
  assert.equal(documentPage.more, true);
  send({ type: 'workbench', version: 1, operation: 'document.list', panel: reference.id,
    expectGeneration: reference.generation, expectRevision: referenceState.documentRevision,
    line: documentPage.nextLine, offset: documentPage.nextOffset, records: 8, bytes: 8192 });
  await wait(() => messages.findLast((message) => message.operation === 'document')?.data.records[0]?.line === 9,
    'Wasm DOCUMENT continuation');
  const beforeStale = messages.length;
  send({ type: 'workbench', version: 1, operation: 'document.list', panel: reference.id,
    expectGeneration: reference.generation, expectRevision: String(BigInt(referenceState.documentRevision) - 1n),
    line: 1, offset: 0, records: 8, bytes: 8192 });
  await wait(() => messages.slice(beforeStale).some((message) => message.code === 'stale-revision'),
    'Wasm DOCUMENT stale revision refusal');
  const beforeGeneration = messages.length;
  send({ type: 'workbench', version: 1, operation: 'document.list', panel: reference.id,
    expectGeneration: String(BigInt(reference.generation) + 1n), expectRevision: referenceState.documentRevision,
    line: 1, offset: 0, records: 8, bytes: 8192 });
  await wait(() => messages.slice(beforeGeneration).some((message) => message.code === 'stale-generation'),
    'Wasm DOCUMENT stale pane refusal');
  const person = page('panels').records.find((record) => record.id === 'public.personpane');
  assert.equal(person.kind,'fields');
  send({type:'workbench',version:1,operation:'panel.show',requestId:requestId(),panel:person.id,expectGeneration:person.generation});
  await wait(()=>messages.findLast((message)=>message.operation==='panel-state')?.panels.some((record)=>record.id===person.id&&record.visible),'Wasm visible form pane');
  const readForm = async () => {
    const before=messages.length;
    send({type:'workbench',version:1,operation:'form.list',form:'public.person',offset:0,records:8,bytes:8192});
    return (await wait(()=>messages.slice(before).find((message)=>message.operation==='form'),'Wasm form copied fields')).data;
  };
  const changeForm = async (operation,state,extra={}) => {
    const request={type:'workbench',version:1,operation,requestId:requestId(),form:'public.person',
      expectGeneration:state.generation,expectRevision:state.draftRevision,
      panel:'public.personpane',expectPanelGeneration:person.generation,...extra};
    send(request);
    const receipt=await wait(()=>messages.find((message)=>message.requestId===request.requestId&&message.state==='complete'),'Wasm typed form receipt');
    return {request,receipt};
  };
  let form=await readForm();
  assert.equal(form.records.find((field)=>field.id==='name').value,'Guest');
  const firstSet=await changeForm('form.set',form,{field:'name',value:'Wasm 😀'});
  assert.equal(firstSet.receipt.code,'ok');
  form=await readForm();
  assert.equal(form.records.find((field)=>field.id==='name').value,'Wasm 😀');
  assert.equal((await changeForm('form.set',form,{field:'locked',value:'forged'})).receipt.code,'field-protected');
  assert.equal((await changeForm('form.submit',form)).receipt.code,'ok');
  form=await readForm();
  assert.equal(form.submitted,true);
  assert.equal((await changeForm('form.set',form,{field:'name',value:'cancel this'})).receipt.code,'ok');
  form=await readForm();
  assert.equal((await changeForm('form.cancel',form)).receipt.code,'ok');
  form=await readForm();
  assert.equal(form.records.find((field)=>field.id==='name').value,'Wasm 😀');
  const stableRevision=form.draftRevision,beforeReplay=messages.length;
  send(firstSet.request);
  await wait(()=>messages.slice(beforeReplay).some((message)=>message.requestId===firstSet.request.requestId&&message.code==='ok'),'Wasm form receipt replay');
  assert.equal((await readForm()).draftRevision,stableRevision);
  const beforeConflict=messages.length;
  send({...firstSet.request,value:'conflict'});
  await wait(()=>messages.slice(beforeConflict).some((message)=>message.code==='request-id-conflict'),'Wasm form receipt payload conflict');
  assert.equal((await changeForm('form.set',form,{field:'name',value:'stale',expectGeneration:String(BigInt(form.generation)+1n)})).receipt.code,'stale-generation');
  assert.equal((await changeForm('form.set',form,{field:'name',value:'\ud800'})).receipt.code,'invalid-operand');
  send({type:'key',key:'f7'});
  const modal=await wait(()=>messages.findLast((message)=>message.screen_rows)?.panel,'Wasm real modal READ');
  assert.equal((await changeForm('form.set',form,{field:'name',value:'must refuse'})).receipt.code,'busy-modal');
  assert.deepEqual(messages.findLast((message)=>message.screen_rows).panel,modal,'Wasm typed form refusal preserves modal fields');
  send({type:'key',key:'esc'});
  await wait(()=>!messages.findLast((message)=>message.screen_rows)?.panel,'Wasm modal ownership released');
  assert.equal((await changeForm('form.set',form,{field:'name',value:'after modal'})).receipt.code,'ok');
  form=await readForm();
  assert.equal(form.records.find((field)=>field.id==='name').value,'after modal');
  assert.equal((await changeForm('form.cancel',form)).receipt.code,'ok');
  send({ type: 'workbench', version: 1, operation: 'list', section: 'routes', offset: 0, limit: 8 });
  const routes = await wait(() => page('routes'), 'trusted startup route publication');
  assert.equal(routes.records.find((record) => record.id === 'public.stdout')?.model,
    'public.programlog');
  send({ type: 'workbench', version: 1, operation: 'panel.show',
    requestId: requestId(),
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
  console.log(`PASS Wasm trusted startup LOG route and panel-state${classic.available ? ', stdout and log page' : ' (runner unavailable in this build)'}; DOCUMENT copied ranges; FORM Unicode/protection/Submit/Cancel, receipt replay/conflict and stale tokens`);
} finally {
  await rm(temporary, { recursive: true, force: true });
}
