/** Real editor/VM contract through the shared RX commands. */
export async function sourceFileRunnerSmoke({pageUrl, send, snapshot, evaluate, waitFor}) {
  const image = Buffer.from(await (await fetch(new URL('examples/SourceFileProbe.rxbin', pageUrl))).arrayBuffer());
  await send({type:'tool-import', name:'SourceFileProbe.rxbin', bytes:image.toString('base64')});
  await waitFor(() => evaluate("window.__messages.some(m=>m.type==='tool-imported'&&m.path==='/workspace/SourceFileProbe.rxbin')"), 'source runner image import');
  await send({type:'command', command:'RX REGISTER source-probe runner /workspace/SourceFileProbe.rxbin .bas "Source probe"'});
  await waitFor(() => evaluate("window.__tools.catalog.some(t=>t.id==='source-probe'&&t.role==='runner')"), 'source runner registration');
  const path = '/workspace/Lesson Zoë.BAS';
  await send({type:'action', action:'file.create', argument:'Lesson Zoë.BAS'});
  await waitFor(async () => (await snapshot()).buffer.path === path, 'DSL source buffer');
  await send({type:'command', command:'input DISK FIRST'});
  await send({type:'action', action:'file.save'});
  await waitFor(async () => !(await snapshot()).buffer.dirty, 'saved DSL source');
  await send({type:'command', command:'cursor goto 1 1'});
  await send({type:'command', command:'set insertmode on'});
  await send({type:'text', text:'PENDING '});
  await waitFor(async () => (await snapshot()).buffer.dirty && (await snapshot()).screen_rows.some(r => r.t === 'PENDING DISK FIRST'), 'pending DSL record');
  const before = await snapshot();
  await send({type:'command', command:'RX RUN'});
  await waitFor(() => evaluate("window.__tools.job.tool==='source-probe'&&window.__tools.job.waiting"), 'mapped RXBIN source runner stdin', 30000);
  const active = await evaluate('window.__tools');
  if (active.job.filename !== path || active.job.sourceId !== before.source.id || active.job.revision !== before.source.revision)
    throw Error('Source runner lost logical identity/revision');
  const launched = await snapshot();
  if (JSON.stringify(launched.buffer) !== JSON.stringify(before.buffer) || launched.focus.line !== before.focus.line || launched.focus.cell !== before.focus.cell)
    throw Error('Source runner changed editor state');
  await send({type:'text', text:'LATER '});
  await waitFor(async () => (await snapshot()).screen_rows.some(r => r.t.includes('LATER')), 'edit while source runner waits');
  const edited = await snapshot();
  await send({type:'command', command:'RX INPUT Zoë😀'});
  await send({type:'command', command:'RX EOF'});
  await waitFor(() => evaluate("window.__tools.job.state==='Completed · status 0'"), 'source runner completion', 30000);
  const finished = await evaluate('window.__tools');
  const output = finished.chunks.map(c => c.text).join('');
  const captured = 'SOURCE-BEGIN\nPENDING DISK FIRST\nSOURCE-END\n';
  if (output.split(captured).length !== 3 || !output.includes('SOURCE-NAME:Lesson Zoë.BAS\n') ||
      !output.includes('STDIN-BEGIN\nZoë😀\nSTDIN-END') || output.includes('LATER') || output.includes('/run/'))
    throw Error('Source snapshot/argv/stdin changed: ' + output);
  if (finished.job.sourceId !== active.job.sourceId || finished.job.revision !== active.job.revision)
    throw Error('Running source identity followed editor changes');
  const completed = await snapshot();
  if (JSON.stringify(completed.buffer) !== JSON.stringify(edited.buffer) || completed.focus.line !== edited.focus.line || completed.focus.cell !== edited.focus.cell)
    throw Error('Source runner completion changed editor state');
  // Reopen the saved file: execution and later edits never saved the snapshot.
  await send({type:'command', command:'qquit'});
  await waitFor(async () => (await snapshot()).buffer.path !== path, 'close unsaved DSL source');
  await send({type:'command', command:'edit "' + path + '"'});
  await waitFor(async () => (await snapshot()).buffer.path === path && !(await snapshot()).buffer.dirty, 'reopen saved DSL source');
  if (!(await snapshot()).screen_rows.some(r => r.t === 'DISK FIRST')) throw Error('Source run overwrote saved bytes');
  await send({type:'command', command:'RX RUN source-probe "Mixed Case" "" "Zoë😀"'});
  await waitFor(() => evaluate(`window.__tools.job.id>${finished.job.id}&&window.__tools.job.waiting`), 'explicit source runner ID', 30000);
  await send({type:'command', command:'RX EOF'});
  await waitFor(() => evaluate("window.__tools.job.state==='Completed · status 0'"), 'explicit runner completion', 30000);
  if (!await evaluate("window.__tools.chunks.map(c=>c.text).join('').includes('ARGC:3\\nARG:Mixed Case\\nARG:\\nARG:Zoë😀\\n')"))
    throw Error('Ordinary source runner arguments changed');
  console.log('PASS browser source-file RXBIN: extension/explicit dispatch, pending snapshot, stable identity, stdin, argv and saved bytes');
}
