import assert from "node:assert/strict";

// Qualify the bundled defaults before any user profile or language registration.
export async function defaultSyntaxSmoke({ pageUrl, manifest, send, snapshot, evaluate, waitFor, openWorkspaceFile }) {
  const durable = path => evaluate(`new Promise((resolve,reject)=>{const open=indexedDB.open('/workspace');
    open.onerror=()=>reject(open.error);open.onsuccess=()=>{const db=open.result;
    const get=db.transaction('FILE_DATA').objectStore('FILE_DATA').get(${JSON.stringify(path)});
    get.onsuccess=()=>{resolve(get.result?Array.from(get.result.contents):null);db.close();};
    get.onerror=()=>{reject(get.error);db.close();};};})`);
  assert.equal((await snapshot()).buffer.path, "/workspace/Welcome.txt", "fresh workspace opens Welcome");
  const welcome = Buffer.from(await durable("/workspace/Welcome.txt")).toString("utf8");
  const welcomeSteps = ["Files"];
  if (manifest.capabilities.source_compiler && manifest.files["examples/LevelBSquares.crexx"])
    welcomeSteps.push("LevelBSquares.crexx", "Build source", "Run with cREXX");
  if (manifest.capabilities.classic_rexx_interpreter && manifest.files["examples/RexxRunner.rexx"])
    welcomeSteps.push("RexxRunner.rexx", "Run with Classic Rexx");
  if (manifest.capabilities.minibasic_candidate)
    welcomeSteps.push("MiniBASIC.bas", "Run with MiniBASIC", "no build or profile setup");
  if (welcomeSteps.length > 1) welcomeSteps.push("Console");
  for (const text of welcomeSteps)
    assert.ok(welcome.includes(text), `Welcome explains ${text}`);

  async function check(name, styles, provider) {
    const path = `/workspace/${name}`;
    const original = Buffer.from(await (await fetch(new URL(`examples/${name}`, pageUrl))).arrayBuffer());
    await openWorkspaceFile(name);
    if (provider) await waitFor(async () => {
      const state = await snapshot();
      return state.parser?.providerId === provider && state.parser.state === "ready"
        && state.parser.applied === state.parser.revision;
    }, `${name} default parser`, 30000);
    await waitFor(async () => {
      const rows = (await snapshot()).screen_rows.filter(row => row.role === "file");
      return styles.every(style => rows.some(row => row.s?.some(([, , value]) => value === style)));
    }, `${name} default syntax spans`);
    assert.equal((await snapshot()).buffer.dirty, 0, `${name} highlighting preserves clean state`);
    assert.deepEqual(await durable(path), [...original], `${name} highlighting preserves exact saved bytes`);
    for (const style of styles) assert.equal(await evaluate(`(() => {
      const token=document.querySelector('.syntax-${style}'),row=document.querySelector('.screen-row.role-file');
      return !!token&&!!row&&getComputedStyle(token).color!==getComputedStyle(row).color;
    })()`), true, `${name} ${style} is visibly distinct from ordinary text`);
  }

  if (manifest.capabilities.classic_rexx_highlighter) {
    for (const name of ["ClassicDemo.rexx", "RexxRunner.rexx", "RexxWords.rexx", "RexxLedger.rexx"])
      if (manifest.files[`examples/${name}`]) await check(name, ["comment", "keyword", "string"], "classic-highlight");
  }
  for (const name of ["LevelBSquares.crexx", "LevelGAccumulator.crexx", "LevelGFibonacci.crexx"])
    if (manifest.files[`examples/${name}`]) await check(name, ["comment", "keyword", "number"]);
  if (manifest.capabilities.minibasic_candidate) {
    for (const name of ["MiniBASIC.bas", "MiniBASICPrimes.bas", "MiniBASICStrings.bas"]) {
      assert.ok(manifest.files[`examples/${name}`], `MiniBASIC candidate includes ${name}`);
      await check(name, ["comment", "keyword", "number"], "minibasic-syntax");
      await waitFor(() => evaluate(`window.__tools?.currentFile?.path===${JSON.stringify(`/workspace/${name}`)}`), `${name} default actions`);
      assert.deepEqual(await evaluate("(({enabled,tool})=>({enabled,tool}))(window.__tools.currentFile.actions.find(action=>action.id==='run'))"),
        { enabled: true, tool: "minibasic" }, `${name} stock Run selects the bundled interpreter`);
      assert.equal(await evaluate("document.querySelector('.file-action-run')?.disabled"), false,
        `${name} exposes enabled Run without profile setup`);
      assert.match(await evaluate("document.querySelector('.file-action-run')?.textContent"), /MiniBASIC/,
        `${name} Run names its interpreter`);
      assert.equal(await evaluate("window.__tools.currentFile.actions.find(action=>action.id==='build').enabled"), false,
        `${name} needs no stock Build action`);
      assert.equal(await evaluate("document.querySelector('.file-action-build,.file-action-configure')"), null,
        `${name} offers direct Run without Build or Configure runner`);
    }
  }
  await openWorkspaceFile("Welcome.txt");
  await send({ type: "command", command: "cursor goto 1 1" });
  console.log("PASS browser defaults: Welcome program steps, first-open Rexx/cREXX/BASIC visible syntax and exact clean source bytes");
}
