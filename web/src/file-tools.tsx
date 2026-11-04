import { useEffect, useRef, useState } from "preact/hooks";
import { browserWasm } from "./transport";
import type { Snapshot } from "./types";
import type { FileToolAction, ToolSession } from "./tool-session";

type Send = (message: Record<string, unknown>) => number | undefined;

function readableReason(reason: string): string {
  const reasons: Record<string, string> = {
    "already-running": "Wait for the current task to finish",
    "runner-unavailable": "The associated runner is unavailable",
    "no-run-tool": "Choose a runner for this file in Tools & Profiles",
    "no-build-tool": "No build tool is associated with this file",
  };
  return reasons[reason] || reason.replaceAll("-", " ");
}

export function fileActionReason(session: ToolSession | null, snapshot: Snapshot | null,
  online: boolean, action: FileToolAction): string {
  if (!online) return "Editor is offline";
  if (snapshot?.panel) return "Close the macro panel first";
  const file = session?.currentFile;
  if (!file || !snapshot?.source || file.sourceId !== snapshot.source.id || file.revision !== snapshot.source.revision)
    return "Updating current file actions";
  return action.enabled ? "" : readableReason(action.reason || "Unavailable for this file");
}

function currentFileRole(file: ToolSession["currentFile"], path: string): string {
  if (file?.role) return file.role;
  // Older hosts do not publish roles; use the public naming convention.
  if (/\.profile\.the$/i.test(path)) return "profile-source";
  if (/\.the$/i.test(path)) return "macro-source";
  return /\.crexx$/i.test(path) ? "crexx-source" : "other";
}

function shortPath(path: string): string {
  return path.replaceAll("\\", "/").split("/").pop() || path;
}

export function CurrentFileActions({ session, snapshot, online, onAction, onSetup }: {
  session: ToolSession | null;
  snapshot: Snapshot | null;
  online: boolean;
  onAction: (action: FileToolAction) => void;
  onSetup: () => void;
}) {
  const file = session?.currentFile;
  const path = snapshot?.buffer?.path || file?.path || "No file selected";
  const sourceMatches = !!file && !!snapshot?.source && file.sourceId === snapshot.source.id
    && file.revision === snapshot.source.revision;
  const role = currentFileRole(sourceMatches ? file : undefined, path);
  const profile = role === "profile-source";
  const macro = role === "macro-source";
  const crexx = role === "crexx-source";
  const runner = session?.catalog.find((candidate) => candidate.id === file?.actions.find((action) => action.id === "run")?.tool);
  const purpose = profile ? "Profile source" : macro ? "THE macro" : crexx ? "cREXX source" : runner?.label || "Text";
  const actions = file?.actions ?? [];
  const build = actions.find((action) => action.id === "build");
  const next = actions.find((action) => action.id === (profile ? "activate" : "run"));
  const shown = profile || macro ? [build, next].filter((action): action is FileToolAction => !!action)
    : actions.filter((action) => action.id !== "activate" && (action.enabled || !!action.tool));
  const configure = crexx && !shown.length;
  const primary = next?.enabled ? next : build?.enabled ? build : shown.find((action) => action.enabled);
  const buildArtifact = session?.profile?.build;
  const buildMatches = !!file && !!buildArtifact?.image && buildArtifact.sourceId === file.sourceId
    && buildArtifact.revision === file.revision;
  const editedBuild = !!file && !!buildArtifact?.image && buildArtifact.sourceId === file.sourceId
    && buildArtifact.revision !== file.revision;
  const result = sourceMatches && session?.fileResult?.serial && session.fileResult.action !== "apply-profile"
    && session.fileResult.sourceId === file?.sourceId && session.fileResult.revision === file?.revision
    ? session.fileResult : undefined;
  const building = sourceMatches && session?.build?.job.running
    && session.build.job.sourceId === file?.sourceId;
  const running = sourceMatches && session?.job.running && session.job.sourceId === file?.sourceId;
  const blocked = !online ? "Editor is offline" : snapshot?.panel ? "Close the macro panel first"
    : !sourceMatches ? "Updating current file actions" : "";
  const nextReason = next ? fileActionReason(session, snapshot, online, next) : "";
  const buildReason = build ? fileActionReason(session, snapshot, online, build) : "";
  let status = blocked;
  let resultState = "";
  if (!status && building) status = `Building ${profile ? "profile" : macro ? "macro" : "source"}…`;
  else if (!status && running) status = `${macro ? "Macro" : "Program"} running. Output is in Program console.`;
  else if (!status && result?.rc) {
    status = `${result.message || readableReason(result.code)} (RC ${result.rc}). See ${result.action === "run" ? "Program console" : "Tools & Profiles"}.`;
    resultState = "error";
  } else if (!status && result && result.code !== "started") {
    if (result.action === "activate") status = session?.profile?.active.startupSaved === false
      ? "Profile applied for this session. Startup settings were not saved." : "Profile applied. Settings are saved for future sessions.";
    else if (result.action === "build") status = profile
      ? `Built ${buildArtifact?.name || "profile"}. Apply profile saves settings for future sessions.`
      : macro ? "Macro built. Run macro executes it once." : result.message || "Build completed.";
    else status = macro ? "Macro completed. Output is in Program console." : result.message || "Program completed. Output is in Program console.";
    resultState = "success";
  }
  if (!status) {
    if (configure) status = "Choose a runner for this file in Tools & Profiles.";
    else if (profile || macro || crexx) {
      if (next?.enabled) status = profile ? "Apply profile saves these settings for future sessions."
        : macro ? "Run macro executes the built source once."
        : "Run executes the build of this source revision. Output is in Program console.";
      else if (!build?.enabled) status = buildReason || nextReason;
      else if (editedBuild) status = `Source changed. Build ${profile ? "profile" : macro ? "macro" : "source"} again before ${profile ? "applying" : "running"}.`;
      else if (!buildMatches) status = `Build this ${profile ? "profile" : macro ? "macro" : "source"} before ${profile ? "applying" : "running"} it.`;
      else status = nextReason;
    } else status = primary?.id === "build" ? "Build uses the current buffer, including unsaved edits."
      : primary ? "Runs the current buffer, including unsaved edits. Output is in Program console."
      : next?.tool ? nextReason : "Text file · edit with THE commands or your profile’s keys.";
  }
  return <section class="current-file-tools" aria-label="Current file actions" data-file-role={role}>
    <div class="current-file-identity">
      <span class="file-purpose" title={path}>{purpose}</span>
      {snapshot?.buffer?.dirty ? <span class="file-unsaved">Unsaved edits</span> : null}
    </div>
    <div class="current-file-controls">
      <div class="current-file-action-list">
        {shown.map((action) => {
          const reason = fileActionReason(session, snapshot, online, action);
          const tool = session?.catalog.find((candidate) => candidate.id === action.tool);
          const label = profile ? action.id === "build" ? "Build profile" : "Apply profile"
            : macro ? action.id === "build" ? "Build macro" : "Run macro"
            : action.id === "run" && tool ? `Run with ${tool.label}` : action.label;
          return <button type="button" key={action.id}
            class={`file-action-${action.id}${action === primary ? " file-action-primary" : ""}`}
            disabled={!!reason} title={reason || label} aria-describedby="current-file-status"
            onClick={() => onAction(action)}>{label}</button>;
        })}
        {configure ? <button type="button" class="file-action-configure file-action-primary"
          aria-describedby="current-file-status" onClick={onSetup}>Configure runner</button> : null}
      </div>
    </div>
    <p id="current-file-status" class={`current-file-status${resultState ? ` current-file-result file-result-${resultState}` : ""}`}
      role="status" aria-live="polite" aria-atomic="true">{status}</p>
  </section>;
}

export function ToolsProfiles({ send, session, snapshot, online, argumentsText, onArguments,
  onProgramRun, onClose, focusEditor, expanded, onExpand }: {
  send: Send;
  session: ToolSession;
  snapshot: Snapshot | null;
  online: boolean;
  argumentsText: string;
  onArguments: (value: string) => void;
  onProgramRun: (tool: string, image: string) => void;
  onClose: () => void;
  focusEditor: () => void;
  expanded: boolean;
  onExpand: () => void;
}) {
  const [section, setSection] = useState<"profiles" | "tools" | "build">("profiles");
  const lastBuild = useRef(0);
  useEffect(() => {
    const id = session.build?.job.id || 0;
    if (id && id !== lastBuild.current) { lastBuild.current = id; setSection("build"); }
  }, [session.build?.job.id]);
  const [toolId, setToolId] = useState("");
  const [image, setImage] = useState("");
  const [importState, setImportState] = useState("");
  const [profileImage, setProfileImage] = useState("");
  const [profileImportState, setProfileImportState] = useState("");
  const [profileRequest, setProfileRequest] = useState<{ path: string; serial: number } | null>(null);
  const importer = useRef<HTMLInputElement>(null);
  const runners = session.catalog.filter((tool) => tool.role === "runner" || tool.role === "program");
  const tool = runners.find((candidate) => candidate.id === toolId);
  const busy = session.job.running || !!session.build?.job.running;
  const sourceMatches = !!session.currentFile && !!snapshot?.source
    && session.currentFile.sourceId === snapshot.source.id && session.currentFile.revision === snapshot.source.revision;
  const explicitReason = !online ? "Editor is offline" : snapshot?.panel ? "Close the macro panel first"
    : busy ? "Wait for the current task to finish" : !tool ? "Choose a tool explicitly"
    : !tool.available ? "This tool is unavailable" : tool.id === "rxbin" && !image.trim() ? "Choose an RXBIN workspace path"
    : tool.role === "runner" && !sourceMatches ? "Updating current file actions" : "";
  const profile = session.profile;
  const build = session.build;
  const result = session.fileResult?.action === "build" || session.fileResult?.action === "activate"
    ? session.fileResult : undefined;
  const latestArtifact = profile?.build;
  const artifact = latestArtifact?.role === "profile-source" ? latestArtifact : undefined;
  const profileResult = profileRequest && session.fileResult?.action === "apply-profile"
    && session.fileResult.serial > profileRequest.serial ? session.fileResult : undefined;
  const applyingProfile = online && !!profileRequest && (session.fileResult?.serial || 0) <= profileRequest.serial;
  const profileApplyReason = !online ? "Editor is offline" : snapshot?.panel ? "Close the macro panel first"
    : applyingProfile ? "Applying compiled profile…" : busy ? "Wait for the current task to finish"
    : !profile?.applyImage?.enabled ? readableReason(profile?.applyImage?.reason || "Applying compiled profiles is unavailable on this host")
    : !profileImage.trim() ? "Choose a workspace .profile.rxbin file"
    : !/\.profile\.rxbin$/i.test(profileImage.trim()) ? "Choose a file ending in .profile.rxbin" : "";
  const profileReceipt = profileResult && profileRequest?.path === profileImage.trim() ? profileResult : undefined;
  useEffect(() => { if (!online) setProfileRequest(null); }, [online]);
  const sameBuildSource = !!artifact?.image && artifact.sourceId === session.currentFile?.sourceId;
  const buildState = !artifact?.image ? "No profile artifact built"
    : !sameBuildSource ? `Built for ${shortPath(artifact.sourcePath || "another source")}`
    : artifact.revision !== session.currentFile?.revision ? "Source edited · rebuild before use"
    : "Built from the current source";
  const editProfile = () => {
    if (send({ type: "tool", action: "profile", tool: "", text: "", arguments: "" }) !== undefined)
      window.setTimeout(focusEditor, 0);
  };
  const activeName = profile?.active.image
    ? shortPath(profile.active.sourcePath || profile.active.image) : "Default editor settings";
  const startupSummary = !profile?.active.image ? "No user profile applied"
    : profile.active.startupSaved === true ? "Applied · used at startup"
    : profile.active.startupSaved === false ? "Applied for this session · startup settings were not saved"
    : "Applied · startup selection is profile-defined";
  // This is a declared-registration overview. The core's currentFile actions
  // remain the authority for what the selected file can build, run or apply.
  const toolsByExtension = new Map<string, ToolSession["catalog"]>();
  for (const candidate of session.catalog) {
    for (const extension of candidate.extensions.split(",")) {
      if (!/^\.[^\s,]+$/.test(extension)) continue;
      const key = extension.toLowerCase();
      const registered = toolsByExtension.get(key) || [];
      registered.push(candidate);
      toolsByExtension.set(key, registered);
    }
  }
  const associationGroups: { extensions: string[]; tools: ToolSession["catalog"] }[] = [];
  for (const [extension, registered] of toolsByExtension) {
    const same = associationGroups.find((group) => group.tools.length === registered.length
      && group.tools.every((candidate, index) => candidate.id === registered[index].id));
    if (same) same.extensions.push(extension);
    else associationGroups.push({ extensions: [extension], tools: registered });
  }
  const currentExtension = session.currentFile?.path.match(/\.[^./\\]+$/)?.[0].toLowerCase();
  const currentActions = sourceMatches ? session.currentFile?.actions.filter((action) => action.tool || action.enabled) || [] : [];
  const registrationRole = (role: string) => ({ runner: "Run", compiler: "Build", language: "Syntax / highlighting",
    program: "Run prepared image", profile: "Profile" } as Record<string, string>)[role] || role;
  const registrationOrder = (role: string) => ({ compiler: 0, runner: 1, program: 2, profile: 3, language: 4 } as Record<string, number>)[role] ?? 5;
  return <div class="tools-profiles" aria-label="Tools and profiles">
    <div class="tools-heading"><strong>Tools & Profiles</strong>
      <button type="button" class="pane-expand" onClick={onExpand}>{expanded ? "Return to editor" : "Expand pane"}</button>
      <button type="button" onClick={onClose}>Hide</button></div>
    <nav class="tools-sections" aria-label="Tools and profile sections">
      {([['profiles', 'Profiles'], ['tools', 'File types & tools'], ['build', 'Build results']] as const).map(([id, label]) =>
        <button type="button" key={id} aria-pressed={section === id} onClick={() => setSection(id)}>{label}</button>)}
    </nav>
    <div class="tools-body">
      <section class="profile-setup" aria-label="Profile setup" hidden={section !== "profiles"}>
        <div class="profile-summary">
          <span class="tools-hint">Active profile</span>
          <strong class="profile-name">{activeName}</strong>
          <span class="profile-summary-state">{startupSummary}</span>
        </div>
        <button type="button" class="edit-profile" disabled={!online || busy || !!snapshot?.panel} onClick={editProfile}>Edit profile</button>
        <div class="profile-workflow"><span class="tools-hint">Source workflow</span>
          <p>Open profile → Build profile → Apply profile</p>
        </div>
        <details class="profile-details"><summary>Profile details</summary>
        <p>Profiles define editor settings, file associations and workbench actions. Other .the files build and run as macros.</p>
        <dl class="profile-status-list">
          <dt>Editable</dt><dd class="profile-source-path">{profile?.sourcePath || "User profile source"}</dd>
          <dt>Build</dt><dd class="profile-build-state">{buildState}{artifact?.image ? <>
            <small>{artifact.name || shortPath(artifact.image)} · {artifact.role === "profile-source" ? "THE profile" : artifact.role === "macro-source" ? "THE macro" : "Program"}</small>
            <small>{artifact.sourcePath || "Source"} · revision {artifact.revision}</small>
            <small>{artifact.image}</small>
          </> : null}</dd>
          <dt>Active</dt><dd class="profile-active-image">{profile?.active.kind === "compiled-image" && profile.active.image
            ? "Compiled profile · saved copy" : profile?.active.image || "No user profile applied"}
            {profile?.active.kind === "compiled-image" && profile.active.image ? <small>{profile.active.image}</small> : null}
            {profile?.active.sourcePath ? <small>{profile.active.kind === "compiled-image" ? "Compiled profile" : "Source"}: {profile.active.sourcePath}</small> : null}</dd>
          <dt>Startup</dt><dd class="profile-startup-state">{!profile?.active.image ? "No user profile selected"
            : profile.active.startupSaved === true ? "Saved for future sessions"
            : profile?.active.startupSaved === false ? "Session only · startup settings were not saved"
            : "Profile-defined or unknown"}</dd>
        </dl>
        {profile?.active.kind === "compiled-image" ? <p class="tools-hint">The active imported image has no editable source attached. Edit profile opens your separate user profile source.</p> : null}
        <p class="tools-hint">THE sources use cREXX Level B. Run macro executes a build once. Apply profile applies its settings now and saves the profile for future sessions.</p>
        </details>
        <details class="compiled-profile-setup"><summary>Apply a compiled profile…</summary>
          <p>Choose a workspace .profile.rxbin file. Apply runs this profile now and saves it for future sessions.</p>
          <label>Compiled profile path<input aria-label="Compiled profile path" value={profileImage}
            placeholder={browserWasm ? "/workspace/My.profile.rxbin" : "/path/to/My.profile.rxbin"}
            disabled={applyingProfile} onInput={(event) => { setProfileImage(event.currentTarget.value); setProfileImportState(""); }} /></label>
          <div class="compiled-profile-choices">
            {browserWasm ? <button type="button" class="import-profile-image" disabled={!online || busy || applyingProfile}
              onClick={() => { if (importer.current) { importer.current.accept = ".profile.rxbin"; importer.current.click(); } }}>Import profile RXBIN</button> : null}
            <button type="button" class="use-built-profile" disabled={!artifact?.image || artifact.role !== "profile-source" || applyingProfile}
              onClick={() => { setProfileImage(artifact?.image || ""); setProfileImportState(""); }}>Use latest profile build</button>
          </div>
          {profileImportState ? <p class="tools-hint">{profileImportState}</p> : null}
          <button type="button" class="apply-compiled-profile" disabled={!!profileApplyReason}
            aria-describedby="compiled-profile-status" title={profileApplyReason || "Apply and save this compiled profile"}
            onClick={() => {
              if (profileApplyReason) return;
              const path = profileImage.trim();
              if (send({ type: "tool", action: "apply-profile", tool: "", text: path, arguments: "" }) !== undefined)
                setProfileRequest({ path, serial: session.fileResult?.serial || 0 });
            }}>Apply compiled profile</button>
          <p id="compiled-profile-status" class={`compiled-profile-status${profileReceipt ? ` file-result-${profileReceipt.rc ? "error" : "success"}` : ""}`}
            role="status" aria-live="polite">{profileReceipt ? profileReceipt.rc
              ? `${profileReceipt.message || readableReason(profileReceipt.code)} (RC ${profileReceipt.rc})`
              : profile?.active.startupSaved === false ? "Compiled profile applied for this session. Startup settings were not saved."
              : "Compiled profile applied. Settings are saved for future sessions."
              : profileApplyReason || "Ready to apply and save this compiled profile."}</p>
        </details>
      </section>
      <section class="build-results" aria-label="Build results" hidden={section !== "build"}>
        <div class="tools-section-heading"><h2>Build results</h2>
          {build?.job.running ? <button type="button" disabled={!online} onClick={() => send({ type: "tool", action: "stop", tool: "", text: "", arguments: "" })}>Stop build</button> : null}
        </div>
        <p class="build-state" role="status">{build?.job.id ? `${build.job.state}${build.job.filename ? ` · ${build.job.filename} · revision ${build.job.revision}` : ""}` : "No compiler build has run in this session."}</p>
        {latestArtifact?.image ? <p class="build-artifact">Artifact: {latestArtifact.name || shortPath(latestArtifact.image)}
          <small>{latestArtifact.sourcePath || "Source"} · revision {latestArtifact.revision}</small>
          <small>{latestArtifact.image}</small></p> : null}
        {result?.serial ? <p class={`file-result ${result.rc ? "file-result-error" : ""}`} role="status">{result.action}: {result.message || readableReason(result.code)}{result.rc ? ` (RC ${result.rc})` : ""}</p> : null}
        {build?.omitted ? <p class="tools-hint">Older build output omitted</p> : null}
        <pre class="build-output" aria-label="Build output" tabIndex={0}>{build?.chunks.map((chunk, index) =>
          <span key={index} class={chunk.fd === 2 ? "console-stderr" : ""} data-stream={chunk.fd === 2 ? "stderr" : "stdout"}>{chunk.text}</span>)}</pre>
      </section>
      <section class="run-setup" aria-label="Explicit program tool" hidden={section !== "tools"}>
        <div class="tool-associations" aria-label="File associations">
          {associationGroups.length ? <ul>{associationGroups.map((group) => <li key={group.extensions.join(",")} data-extensions={group.extensions.join(",")}>
            <span class="association-extensions">{group.extensions.join(", ")}</span>
            <span>{[...group.tools].sort((a, b) => registrationOrder(a.role) - registrationOrder(b.role)).map((candidate) => <small key={candidate.id} data-tool-id={candidate.id} data-tool-role={candidate.role}>
              {registrationRole(candidate.role)} · <strong>{candidate.label}</strong>{candidate.available ? "" : " · unavailable"}
            </small>)}
            {currentExtension && group.extensions.includes(currentExtension) && currentActions.length
              ? <small class="association-current-actions">Current file: {currentActions.map((action) => action.label).join(" → ")}</small> : null}</span>
          </li>)}</ul> : <p>No file associations are defined by the current profiles.</p>}
        </div>
        <p class="tools-hint association-owner">Registered by your profiles</p>
        <button type="button" class="edit-associations" disabled={!online || busy || !!snapshot?.panel} onClick={editProfile}>Edit tool associations in profile</button>
        <details class="advanced-run-setup"><summary>Run with an explicit tool…</summary>
        <p>Run uses the selected file’s association. An explicit tool here runs the current buffer with your chosen runner, or runs a prepared RXBIN program.</p>
        <label>Tool<select aria-label="Run tool" value={toolId} disabled={busy} onChange={(event) => setToolId(event.currentTarget.value)}>
          <option value="">Choose a tool…</option>
          {runners.map((candidate) => <option key={candidate.id} value={candidate.id} disabled={!candidate.available}>{candidate.label}{candidate.available ? "" : " · unavailable"}</option>)}
        </select></label>
        {tool?.id === "rxbin" ? <label>RXBIN workspace path<input value={image} aria-label="RXBIN path" disabled={busy}
          onInput={(event) => setImage(event.currentTarget.value)} /></label> : null}
        <label>Arguments<textarea rows={2} aria-label="Program arguments" placeholder="One argument per line" value={argumentsText}
          disabled={busy} onInput={(event) => onArguments(event.currentTarget.value)} /></label>
        {tool ? <p class="explicit-run-target">Target: {tool.role === "runner" ? session.currentFile?.path || "current buffer" : image || tool.image || "RXBIN workspace path"}</p> : null}
        <button type="button" class="run-with-tool" disabled={!!explicitReason} title={explicitReason || "Run with the explicitly selected tool"}
          aria-describedby="explicit-run-reason" onClick={() => onProgramRun(toolId, image)}>Run with tool</button>
        <span class="tools-hint" id="explicit-run-reason">{explicitReason || "Ready"}</span>
        {browserWasm ? <div class="tool-import">
          <input ref={importer} type="file" accept=".rxbin" hidden onChange={(event) => {
            const file = event.currentTarget.files?.[0];
            if (!file) return;
            const profileImport = /\.profile\.rxbin$/i.test(file.name);
            if (event.currentTarget.accept === ".profile.rxbin" && !profileImport) {
              setProfileImportState("Choose a file ending in .profile.rxbin");
              event.currentTarget.value = "";
              return;
            }
            const reader = new FileReader();
            reader.onerror = () => profileImport ? setProfileImportState("Could not read profile RXBIN file") : setImportState("Could not read RXBIN file");
            reader.onload = () => {
              if (send({ type: "tool-import", name: file.name, bytes: String(reader.result).split(",")[1] }) !== undefined) {
                const path = "/workspace/" + file.name;
                if (profileImport) {
                  setProfileImage(path);
                  setProfileImportState(`Profile path: ${path}`);
                  setImportState("");
                } else {
                  setImage(path);
                  setToolId("rxbin");
                  setImportState(`RXBIN path: ${path}`);
                }
              }
            };
            reader.readAsDataURL(file);
            event.currentTarget.value = "";
          }} />
          <button type="button" disabled={!online || busy || applyingProfile}
            onClick={() => { if (importer.current) { importer.current.accept = ".rxbin"; importer.current.click(); } }}>Import RXBIN</button>
          {importState ? <span class="tools-hint" role="status">{importState}</span> : null}
          <p class="tools-hint">Import prepared images for this runtime cohort. File associations are configured in a profile.</p>
        </div> : null}
        </details>
        <details class="tool-catalog"><summary>Installed tool details</summary>
          <ul>{session.catalog.map((candidate) => <li key={candidate.id}>
            <strong>{candidate.label}</strong><span>{candidate.role} · {candidate.available ? "Available" : "Unavailable"}</span>
            <small>{candidate.id} · {candidate.extensions || "No file association"}</small>
          </li>)}</ul>
        </details>
      </section>
    </div>
  </div>;
}
