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

  const consoleAreas = ["body", "header", "input", "control", "selected", "divider", "error", "status"];
  for (const area of consoleAreas) {
    const coloured = await command(`set color console${area} #123456 on #234567`);
    assert.deepEqual(coloured.console_colours?.[area], { foreground: "#123456", background: "#234567", style: 0 },
                     `SET COLOR CONSOLE${area.toUpperCase()} must update the shared snapshot`);
    assert.equal(coloured.buffer.dirty, 0, "console colours must not change file bytes");
  }

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
    const reversed = await submit("set colour consolebody #345678 on #456789 reverse");
    assert.deepEqual(reversed.console_colours.body, { foreground: "#345678", background: "#456789", style: 2 },
                     "both command routes admit console COLOUR and preserve logical styles");
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
      "set synonym b search /safe 2/ bottom",
      "set synonym b search /safe 2/ osquiet echo POLICY_ESCAPE",
      "set prefix synonym pp policy", "set pre s pp policy",
      "set color filearea red on blue", "set colour popup white on blue",
      "set color * red on blue", "set color consolebody",
      "set color consolebody nonsense", "set color consolebody red on blue bottom",
      "set colour consolebody red on blue;top", "set color consolebody red on blue osquiet echo POLICY_ESCAPE",
      "macro policy", "sos firstcol ex", "sos ex", "sos doprefix", "finishx",
      "save policy-escape.txt", "save ../policy-escape.txt", "ssave policy-escape.txt",
      "file", "ffile", "set autosave 1", "set filename policy-escape.txt",
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

// Editor operations need their real effects checked, rather than merely an
// allow-list result. Both transports run this against a disposable new file.
export async function checkEditorCommands(request) {
  const command = text => request({type: "command", command: text});
  const row = (view, line = 1) => view.screen_rows.find(item => item.role === "file" && item.line === line);
  const initial = await command("cursor goto 1 1");
  const originalPath = initial.buffer.path;
  let view = await request({type: "action", action: "file.create", argument: "Command contract.txt"});
  const path = view.buffer.path;
  const openCount = view.buffers.length;
  const savedPaths = [originalPath];
  if (originalPath.includes("\\")) savedPaths.push(originalPath.replaceAll("\\", "/"));
  for (const argument of savedPaths) {
    view = await request({type: "action", action: "buffer.switch", argument});
    assert.equal(view.buffer.path, originalPath, "switching a tab or canonical path must select the existing buffer");
    assert.equal(view.buffer.lines, initial.buffer.lines);
    assert.equal(view.buffer.dirty, initial.buffer.dirty);
    assert.equal(row(view).t, row(initial).t, "switching must preserve the existing buffer contents");
    assert.equal(view.buffers.length, openCount, "switching must not create a duplicate buffer");
    assert.ok(!/buffer is not open/.test(view.status), "an existing contained buffer must be selectable");
    view = await request({type: "action", action: "buffer.switch", argument: path});
    assert.equal(view.buffer.path, path, "an unsaved new buffer must remain selectable by its tab");
    assert.equal(view.buffer.lines, 0);
    assert.equal(view.buffer.dirty, 0);
    assert.equal(view.buffers.length, openCount, "switching to an unsaved buffer must reuse it");
  }
  view = await request({type: "action", action: "buffer.switch", argument: "Never opened.txt"});
  assert.equal(view.buffer.path, path, "buffer.switch must not create an unopened file");
  assert.equal(view.buffers.length, openCount);
  assert.match(view.status, /buffer is not open/);
  view = await command("input alpha beta");
  const draft = { ...view.buffer };
  view = await request({type: "action", action: "buffer.switch", argument: originalPath.replaceAll("\\", "/")});
  assert.equal(view.buffer.path, originalPath, "a canonical saved-file path must switch away from the draft");
  assert.equal(view.buffer.dirty, initial.buffer.dirty);
  assert.equal(row(view).t, row(initial).t);
  assert.equal(view.buffers.length, openCount);
  view = await request({type: "action", action: "buffer.switch", argument: path.replaceAll("\\", "/")});
  assert.equal(view.buffer.path, path, "canonical separators must select the existing unsaved draft");
  assert.equal(view.buffer.lines, draft.lines);
  assert.equal(view.buffer.dirty, 1, "switching must retain the unsaved draft's dirty state");
  assert.equal(row(view).t, "alpha beta", "switching must retain unsaved bytes");
  assert.equal(view.buffers.length, openCount, "a dirty draft switch must not add another buffer");
  await command("cursor goto 1 1");
  view = await command("save");
  assert.equal(view.buffer.dirty, 0, "bare SAVE must save the current workspace file");
  assert.equal(row(view).t, "alpha beta");

  for (const submit of [command, async text => {
    await command("cursor cmdline 1");
    await request({type: "action", action: "edit.undo"}, s => s.command === "");
    await request({type: "text", text}, s => s.command === text);
    return request({type: "key", key: "enter"});
  }]) {
    view = await submit("right 5");
    assert.equal(row(view).sc, 5, "RIGHT must scroll the viewport by five columns");
    view = await submit("left 3");
    assert.equal(row(view).sc, 2, "LEFT must scroll the viewport back three columns");
    view = await submit("right 0");
    assert.equal(row(view).sc, 0, "RIGHT 0 must restore the first column");
    view = await submit("rgtleft 5");
    assert.equal(row(view).sc, 5, "RGTLEFT must scroll right from the first column");
    view = await submit("rgtleft 5");
    assert.equal(row(view).sc, 0, "RGTLEFT must scroll left when already shifted");
    assert.equal(view.buffer.dirty, 0, "viewport commands must preserve clean bytes");
  }
  await command("cursor goto 1 1");
  view = await command("cursor right");
  assert.equal(view.focus.cell, 1, "CURSOR RIGHT moves the caret independently of RIGHT");
  assert.equal(row(view).sc, 0);
  await command("cursor goto 1 1");
  view = await command("search /beta/");
  assert.equal(view.focus.line, 1);
  assert.equal(view.focus.cell, 6, "SEARCH must locate the matching string");
  view = await command("mark line 1 1");
  assert.equal(view.selection.active, 1, "MARK must create a selection");
  view = await command("reset block");
  assert.equal(view.selection.active, 0, "RESET BLOCK must clear a selection");
  await command("cursor goto 1 6");
  view = await command("split cursor");
  assert.equal(view.buffer.lines, 2, "SPLIT must create a second line");
  assert.equal(row(view, 1).t, "alpha");
  assert.equal(row(view, 2).t, " beta");
  await command("cursor goto 1 6");
  view = await command("join cursor");
  assert.equal(view.buffer.lines, 1, "JOIN must rejoin the split text");
  assert.equal(row(view).t, "alpha beta");
  view = await command("add 1");
  assert.equal(view.buffer.lines, 2, "ADD must insert a blank line");
  await command("cursor goto 2 1");
  await command("replace gamma");
  view = await command("all /alpha/");
  assert.ok(row(view, 1), "ALL target must retain matching lines");
  assert.ok(!row(view, 2), "ALL target must hide nonmatching lines");
  view = await command("all");
  assert.equal(row(view, 2).t, "gamma", "ALL without a target must restore hidden lines");
  await command("cursor goto 1 1");
  // Exercise SAVE through an actual command field and Enter as well.
  await command("cursor cmdline 1");
  await request({type: "action", action: "edit.undo"}, s => s.command === "");
  await request({type: "text", text: "save"}, s => s.command === "save");
  view = await request({type: "key", key: "enter"});
  assert.equal(view.buffer.dirty, 0, "typed SAVE must clear dirty state after saving");
  await command("cursor goto 1 1");
  await command("set insertmode on");
  view = await request({type: "text", text: "DISCARD "}, s => row(s)?.t === "DISCARD alpha beta");
  assert.equal(view.buffer.dirty, 1);
  view = await command("quit");
  assert.equal(view.buffer.path, path, "QUIT must retain a changed file");
  assert.equal(view.buffer.dirty, 1, "QUIT must preserve its unsaved edits");
  await command("cursor cmdline 1");
  await request({type: "action", action: "edit.undo"}, s => s.command === "");
  await request({type: "text", text: "qquit"}, s => s.command === "qquit");
  view = await request({type: "key", key: "enter"});
  assert.equal(view.buffer.path, originalPath, "QQUIT must return to the previous ring file");
  assert.ok(!view.buffers.some(buffer => buffer.path === path), "QQUIT must remove the discarded buffer");
  view = await request({type: "action", action: "file.open", argument: path});
  assert.equal(view.buffer.dirty, 0, "reopening a discarded file must be clean");
  assert.equal(row(view).t, "alpha beta", "QQUIT must not save discarded text");
  assert.equal(row(view, 2).t, "gamma");
  view = await command("quit");
  assert.equal(view.buffer.path, originalPath, "QUIT must close a clean file");
  assert.equal(view.buffer.dirty, 0);
}
