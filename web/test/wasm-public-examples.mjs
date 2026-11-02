import assert from "node:assert/strict";

// Start in a fresh browser workspace: these are the actual public examples,
// selected through Files and executed through the visible per-file actions.
export async function publicExamplesSmoke({pageUrl, manifest, send, snapshot, evaluate, waitFor, openWorkspaceFile, consoleEnter}) {
  const names = Object.keys(manifest.files).filter(path => path.startsWith('examples/'))
    .map(path => path.slice('examples/'.length)).sort();
  const expected = ['Examples.txt'];
  if (manifest.capabilities.classic_rexx_highlighter) expected.push('ClassicDemo.rexx');
  if (manifest.capabilities.classic_rexx_interpreter) {
    expected.push('RexxRunner.rexx', 'RexxWords.rexx', 'RexxLedger.rexx');
    if (manifest.capabilities.editor_address) expected.push('LevelBSquares.crexx',
      'LevelGAccumulator.crexx', 'LevelGAccumulator.rxbin', 'LevelGFibonacci.crexx', 'LevelGFibonacci.rxbin');
  }
  if (manifest.capabilities.minibasic_candidate) expected.push('MiniBASIC.bas', 'MiniBASICPrimes.bas',
    'MiniBASICStrings.bas', 'MiniBASIC.rxbin', 'MiniBASICHighlight.rxbin');
  assert.deepEqual(names, expected.sort(), 'package contains only the curated public examples and language assets');
  const durable = path => evaluate(`new Promise((resolve,reject)=>{const open=indexedDB.open('/workspace');
    open.onerror=()=>reject(open.error);open.onsuccess=()=>{const db=open.result;
    const get=db.transaction('FILE_DATA').objectStore('FILE_DATA').get(${JSON.stringify(path)});
    get.onsuccess=()=>{resolve(get.result?Array.from(get.result.contents):null);db.close();};
    get.onerror=()=>{reject(get.error);db.close();};};})`);
  const cases = [];
  if (manifest.capabilities.classic_rexx_interpreter) {
    if (manifest.capabilities.classic_rexx_highlighter)
      cases.push(['ClassicDemo.rexx', /Hello, λ\nTotal: 6\nRoutine received: 6\n/]);
    cases.push(['RexxRunner.rexx', /Hello, QA\n[\s\S]*Total: 55/, 'QA'],
      ['RexxWords.rexx', /Words: 6\n/], ['RexxLedger.rexx', /Entry 3 change 12 balance 40\n/]);
  }
  if (manifest.capabilities.source_compiler && manifest.capabilities.classic_rexx_interpreter && manifest.capabilities.editor_address)
    cases.push(['LevelBSquares.crexx', /^Level B sum of squares: 55\n$/],
      ['LevelGAccumulator.crexx', /after 5 squares: 55\nsum of squares: 55\n$/],
      ['LevelGFibonacci.crexx', /10 : 55\n$/]);
  if (manifest.capabilities.minibasic_candidate)
    cases.push(['MiniBASIC.bas', /SUM = 55\n[\s\S]*The number is non-negative: 4\n/, '4'],
      ['MiniBASICPrimes.bas', /^2\n3\n5\n7\n11\n13\n17\n19\n23\n29\n$/],
      ['MiniBASICStrings.bas', /^direct "quote"\nassigned "quote"\nassigned "quote"\nassigned "quote"\nleft  right\nleft  right\n\n$/]);
  for (const [name, output, input] of cases) {
    const path = `/workspace/${name}`;
    const response = await fetch(new URL(`examples/${name}`, pageUrl));
    assert.equal(response.ok, true, `${name} public source is served`);
    const original = [...new Uint8Array(await response.arrayBuffer())];
    await openWorkspaceFile(name);
    await waitFor(() => evaluate(`window.__tools?.currentFile?.path===${JSON.stringify(path)}&&
      window.__tools.currentFile.sourceId===window.__snapshot.source.id&&
      window.__tools.currentFile.revision===window.__snapshot.source.revision`), `${name} visible actions`);
    const before = await snapshot();
    if (name.endsWith('.crexx')) {
      assert.equal(await evaluate("!!document.querySelector('.file-action-build')&&!document.querySelector('.file-action-build').disabled"), true, `${name} Build source enabled`);
      const job = await evaluate('window.__tools.build.job.id');
      await evaluate("document.querySelector('.file-action-build').click()");
      await waitFor(() => evaluate(`window.__tools.build.job.id>${job}&&!window.__tools.build.job.running`), `${name} Build source`, 60000);
      assert.equal(await evaluate('window.__tools.build.job.state'), 'Completed · status 0', `${name} compiles successfully`);
    } else {
      assert.equal(await evaluate("!!document.querySelector('.file-action-build')"), false, `${name} is interpreted, no Build required`);
    }
    await waitFor(() => evaluate("!!document.querySelector('.file-action-run')&&!document.querySelector('.file-action-run').disabled"), `${name} Run enabled`);
    const [tool, label] = name.endsWith('.crexx') ? ['crexx', 'cREXX']
      : name.endsWith('.bas') ? ['minibasic', 'MiniBASIC'] : ['classic-rexx', 'Classic Rexx'];
    assert.equal(await evaluate("window.__tools.currentFile.actions.find(action=>action.id==='run').tool"), tool, `${name} correct runner`);
    assert.equal(await evaluate("document.querySelector('.file-action-run').textContent.trim()"), `Run with ${label}`, `${name} visible action names its interpreter`);
    const job = await evaluate('window.__tools.job.id');
    await evaluate("document.querySelector('.file-action-run').click()");
    if (input !== undefined) {
      await waitFor(() => evaluate(`window.__tools.job.id>${job}&&window.__tools.job.waiting`), `${name} visible input request`, 30000);
      await consoleEnter(input);
    }
    await waitFor(() => evaluate(`window.__tools.job.id>${job}&&!window.__tools.job.running`), `${name} completion`, 30000);
    assert.equal(await evaluate('window.__tools.job.state'), 'Completed · status 0', `${name} succeeds`);
    const identity = await evaluate('(({tool,sourceId,revision})=>({tool,sourceId,revision}))(window.__tools.job)');
    assert.deepEqual(identity, {tool, sourceId: before.source.id, revision: before.source.revision}, `${name} job uses the selected source and revision`);
    assert.match(await evaluate("window.__tools.chunks.filter(c=>c.fd!==0).map(c=>c.text).join('')"), output, `${name} expected output`);
    const after = await snapshot();
    assert.equal(after.buffer.path, path);
    assert.equal(after.buffer.dirty, 0, `${name} Run preserves clean state`);
    assert.deepEqual(after.focus, before.focus, `${name} Run preserves logical cursor`);
    assert.deepEqual(await durable(path), original, `${name} Run preserves saved source bytes`);
    console.log(`PASS public example ${name}: selected-file actions, execution, output and source preservation`);
  }
  // Failure and cancellation probes live only in this isolated QA workspace.
  // Exercise each runtime route, then run its real example again to establish
  // that a failed/stopped Worker cannot poison the next execution.
  const probes = [];
  if (manifest.capabilities.classic_rexx_interpreter)
    probes.push(['Classic', '.rexx', ['say 1/0'], ['do forever', 'nop', 'end'], 'RexxWords.rexx']);
  if (manifest.capabilities.source_compiler && manifest.capabilities.editor_address && manifest.capabilities.classic_rexx_interpreter)
    for (const level of ['b', 'g']) probes.push([`Level${level.toUpperCase()}`, '.crexx',
      [`options level${level}`, 'import rxfnsb', 'zero = random(0,0)', 'say 1 / zero', 'return 0'],
      [`options level${level}`, 'counter = 0', 'do forever', 'counter = counter + 1', 'end', 'return 0'],
      level === 'b' ? 'LevelBSquares.crexx' : 'LevelGAccumulator.crexx']);
  if (manifest.capabilities.minibasic_candidate)
    probes.push(['BASIC', '.bas', ['10 GOTO 999'], ['10 GOTO 10'], 'MiniBASICPrimes.bas']);
  async function buildIfNeeded(name) {
    if (!name.endsWith('.crexx')) return;
    const id = await evaluate('window.__tools.build.job.id');
    await waitFor(() => evaluate("!!document.querySelector('.file-action-build')&&!document.querySelector('.file-action-build').disabled"), `${name} probe Build enabled`);
    await evaluate("document.querySelector('.file-action-build').click()");
    await waitFor(() => evaluate(`window.__tools.build.job.id>${id}&&!window.__tools.build.job.running`), `${name} probe build`, 60000);
    assert.equal(await evaluate('window.__tools.build.job.state'), 'Completed · status 0', `${name} probe compiles`);
  }
  async function launch(name) {
    await buildIfNeeded(name);
    await waitFor(() => evaluate("!!document.querySelector('.file-action-run')&&!document.querySelector('.file-action-run').disabled"), `${name} probe Run enabled`);
    const id = await evaluate('window.__tools.job.id');
    await evaluate("document.querySelector('.file-action-run').click()");
    await waitFor(() => evaluate(`window.__tools.job.id>${id}`), `${name} probe launched`);
    return id;
  }
  for (const [label, suffix, failure, infinite, example] of probes) {
    for (const [kind, lines] of [['error', failure], ['stop', infinite]]) {
      const name = `QA-${label}-${kind}${suffix}`;
      await send({type: 'action', action: 'file.create', argument: name});
      await waitFor(async () => (await snapshot()).buffer.path === `/workspace/${name}`, `${name} isolated source`);
      for (const line of lines) await send({type: 'command', command: `input ${line}`});
      await waitFor(async () => (await snapshot()).buffer.dirty, `${name} unsaved probe`);
      await launch(name);
      if (kind === 'error') {
        await waitFor(() => evaluate('!window.__tools.job.running'), `${label} runtime error completes`, 30000);
        assert.notEqual(await evaluate('window.__tools.job.state'), 'Completed · status 0', `${label} runtime failure is visible`);
        assert.ok(await evaluate("window.__tools.chunks.some(c=>c.fd!==0&&c.text.trim())"), `${label} runtime failure has details`);
      } else {
        await waitFor(() => evaluate('window.__tools.job.running'), `${label} loop running`);
        await new Promise(resolve => setTimeout(resolve, 150));
        await evaluate("[...document.querySelectorAll('.rexx-dock button')].find(button=>button.textContent==='Stop').click()");
        await waitFor(() => evaluate("window.__tools.job.state==='Stopped'&&!window.__tools.job.running"), `${label} Stop responsive`);
      }
      await openWorkspaceFile(example);
      await launch(example);
      await waitFor(() => evaluate('!window.__tools.job.running'), `${label} restart completes`, 30000);
      assert.equal(await evaluate('window.__tools.job.state'), 'Completed · status 0', `${label} restarts after ${kind}`);
    }
    console.log(`PASS ${label}: visible runtime failure, Stop infinite program, and successful fresh execution after both`);
  }
  console.log('PASS curated package and every bundled language example through browser Files/Build/Run');
}
