import assert from "node:assert/strict";

// Shared assertions run against the native WebSocket host and Wasm core.
// The disposable buffer contains 80 lines and must remain byte-for-byte clean.
export async function checkCommandPolicy(request, bufferCount = 1) {
  const command = text => request({type: "command", command: text});
  const typed = async text => {
    await command("cursor cmdline 1");
    await request({type: "action", action: "edit.undo"}, s => s.command === "");
    await request({type: "text", text}, s => s.command === text);
    return request({type: "key", key: "enter"});
  };

  for (const submit of [command, typed]) {
    let bottomFirst;
    for (const spelling of ["b", "bot", "bottom", "BoT"]) {
      await command("cursor goto 1 1");
      const bottom = await submit(spelling);
      assert.ok(bottom.screen_rows.some(row => row.line === 80),
                `${spelling} must display the bottom of the file`);
      assert.equal(bottom.buffer.dirty, 0);
      bottomFirst = bottom.screen_rows.find(row => row.role === "file").line;
    }
    const backward = await submit("ba");
    assert.ok(backward.screen_rows.find(row => row.role === "file").line < bottomFirst,
              `ba must move the viewport back a page: ${JSON.stringify(backward)}`);
    await submit("set v 1 80");
    await submit("set num off");
    const hiddenPrefix = await submit("set pre off");
    assert.equal(hiddenPrefix.viewport.prefix_cols, 0, "SET PRE must resolve to PREFIX");
    const shownPrefix = await submit("set pre on");
    assert.ok(shownPrefix.viewport.prefix_cols > 0, "SET PRE ON must restore the prefix");
    await submit("sos bottomedg topedge");
    await command("cursor goto 1 1");
    const nested = await submit("locate /safe 2/ osquiet echo POLICY_ESCAPE > policy-escape.txt");
    assert.ok(nested.status.includes("unavailable in restricted frontend"),
              `nested LOCATE command must be restricted: ${nested.status}`);

    await submit("set synonym finish 3 bottom");
    await command("cursor goto 1 1");
    const alias = await submit("FiN");
    assert.ok(alias.screen_rows.some(row => row.line === 80), "unique alias abbreviation must execute");
    const inspected = await submit("query synonym finish");
    assert.ok(inspected.status.toLowerCase().includes("finish 3 bottom"), inspected.status);
    await submit("set synonym off");
    assert.ok((await submit("finish")).status.includes("unavailable in restricted frontend"));
    await submit("set synonym on");
    await submit("set synonym columns 3 set v 1");
    await submit("col 80");
    await submit("set synonym end 1 bottom");
    await submit("set synonym ending 1 top");
    await command("cursor goto 1 1");
    const exact = await submit("end");
    assert.ok(exact.screen_rows.some(row => row.line === 80), "full alias name wins over an abbreviation");
    const ambiguous = await submit("en");
    assert.ok(ambiguous.status.includes("unavailable in restricted frontend"), ambiguous.status);
    await submit("set synonym inspect query synonym");
    assert.ok((await submit("inspect finish")).status.toLowerCase().includes("finish 3 bottom"));
    await submit("query synonym *");
    await submit("set synonym search locate");
    await command("cursor goto 1 1");
    const found = await submit("search /safe 2/");
    assert.equal(found.buffer.dirty, 0);
    await submit("set synonym b sos bottomedge");
    for (const text of ["b ex", "search /safe 3/ osquiet echo POLICY_ESCAPE",
                        "search /safe 3/ bottom"]) {
      const denied = await submit(text);
      assert.ok(denied.status.includes("unavailable in restricted frontend"),
                `expanded alias must be rechecked without builtin fallback: ${text}: ${denied.status}`);
    }
    await submit("set synonym b");
    await submit("set synonym commander 1 bottom");
    const bypass = await submit("command bottom");
    assert.ok(bypass.status.includes("unavailable in restricted frontend"), bypass.status);
    await submit("set synonym commander");
    await submit("set synonym finish");
    const removed = await submit("finish");
    assert.ok(removed.status.includes("unavailable in restricted frontend"), removed.status);
    for (const name of ["columns", "end", "ending", "inspect", "search"]) await submit(`set synonym ${name}`);

    for (const text of [
      "bottum", "bottomx", "baq", "set verx 1 80", "sos bottomedgx",
      "os echo POLICY_ESCAPE > policy-escape.txt",
      "!echo POLICY_ESCAPE > policy-escape.txt", "set impos on",
      "set impm on", "set synonym b osnowait echo POLICY_ESCAPE",
      "set synonym b macro policy", "set synonym b rexx return 0",
      "set synonym linend # b bottom#top", "set synonym b rx run policy.rxbin",
      "set synonym b set prefix synonym pp policy", "set synonym b set synonym pp top",
      "set synonym set bottom", "set synonym settings 3 bottom",
      "set synonym b locate /safe 2/ osquiet echo POLICY_ESCAPE",
      "set prefix synonym pp policy", "set pre s pp policy",
      "macro policy", "sos firstcol ex", "sos ex", "sos doprefix", "finishx",
    ]) {
      const denied = await submit(text);
      assert.ok(denied.status.includes("unavailable in restricted frontend"),
                `must deny ${text}: ${denied.status}`);
      assert.equal(denied.buffer.lines, 80, text);
      assert.equal(denied.buffer.dirty, 0, text);
      assert.equal(denied.buffers.length, bufferCount, text);
    }
  }
  await command("set synonym save input UNSAVED");
  const saved = await request({type: "action", action: "file.save"});
  assert.equal(saved.buffer.lines, 80, "Save action must bypass a SAVE alias");
  assert.equal(saved.buffer.dirty, 0, "Save action must preserve a clean buffer");
  await command("set synonym save");
  // Prefix Enter must not load a matching workspace macro. The cREXX harness
  // provides a real source/RXBIN probe; ordinary prefix built-ins remain usable.
  await command("cursor goto 1 1");
  let view = await command("cursor prefix");
  assert.equal(view.focus.zone, "prefix");
  // The native host publishes one frame per character in a text request.
  // Wait for the complete field before submitting the prefix command.
  view = await request({type: "text", text: "pfx"}, s =>
    s.focus.zone === "prefix" && s.screen_rows.some(row => row.p?.includes("pfx")));
  assert.ok(view.screen_rows.some(row => row.p?.includes("pfx")),
            `prefix probe must reach the prefix field: ${JSON.stringify(view)}`);
  view = await request({type: "key", key: "enter"});
  assert.equal(view.buffer.dirty, 0, "workspace prefix macro must not mutate the buffer");
  assert.equal(view.buffer.lines, 80);
  assert.ok(!view.panel, "workspace prefix RXBIN must not open a panel");
  assert.ok(view.status.includes("Invalid prefix"), "prefix probe must be processed without macro lookup");
  await request({type: "action", action: "edit.undo"});
  // A denied command stays visible for correction in the editor field.
  await command("cursor cmdline 1");
  await request({type: "action", action: "edit.undo"}, s => s.command === "");
  await command("cursor goto 1 1");
}
