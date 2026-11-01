import { useRef, useState } from "preact/hooks";
import { browserWasm } from "./transport";
import type { Snapshot } from "./types";
import type { FileToolAction, ToolSession } from "./tool-session";

type Send = (message: Record<string, unknown>) => number | undefined;

function readableReason(reason: string): string {
  return reason.replaceAll("-", " ");
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

export function CurrentFileActions({ session, snapshot, online, onAction, onSetup }: {
  session: ToolSession | null;
  snapshot: Snapshot | null;
  online: boolean;
  onAction: (action: FileToolAction) => void;
  onSetup: () => void;
}) {
  const file = session?.currentFile;
  const path = snapshot?.buffer?.path || file?.path || "No file selected";
  const filename = path.replaceAll("\\", "/").split("/").pop() || path;
  const actions = file?.actions ?? [];
  const relevant = actions.filter((action) => action.enabled || action.tool);
  // An unassociated source still explains how to make Run available. Other
  // unrelated actions remain in core state, without crowding the editor.
  const shown = relevant.length ? relevant : actions.filter((action) => action.id === "run");
  return <section class="current-file-tools" aria-label="Current file actions">
    <div class="current-file-identity">
      <strong title={path}>{filename}</strong>
      <span>Current buffer{snapshot?.buffer?.dirty ? " · unsaved edits" : ""}</span>
    </div>
    <div class="current-file-action-list">
      {shown.map((action) => {
        const reason = fileActionReason(session, snapshot, online, action);
        const tool = session?.catalog.find((candidate) => candidate.id === action.tool);
        const toolLabel = action.id === "activate" ? "Built profile"
          : tool?.label || (action.tool === "built-macro" ? "THE macro" : action.tool);
        const target = action.id === "activate" ? session?.profile?.build.image || `Compiled current revision of ${path}`
          : action.tool === "built-macro" ? `Compiled current revision of ${path}`
          : action.id === "run" || tool?.role === "compiler" ? `${path}, including unsaved edits` : path;
        return <div class="current-file-action" key={action.id}>
          <button type="button" class={`file-action-${action.id}`} disabled={!!reason}
            title={reason || `${action.label}: ${toolLabel} · ${target}`}
            aria-describedby={`file-${action.id}-context`} onClick={() => onAction(action)}>{action.label}</button>
          <span id={`file-${action.id}-context`} class="file-action-context">
            {action.tool ? <span class="file-action-tool">{toolLabel}</span> : null}
            <span>{reason || (action.tool === "built-macro" ? `Ready · compiled revision ${file?.revision}` : "Ready · current file")}</span>
          </span>
        </div>;
      })}
      {!shown.length ? <span class="file-action-context">{online ? "File actions unavailable" : "Editor is offline"}</span> : null}
    </div>
    <button type="button" class="file-setup-link" onClick={onSetup}>Tools & Profiles</button>
    {session?.fileResult?.serial && session.fileResult.rc && session.fileResult.sourceId === file?.sourceId ? <p class="current-file-result" role="status">
      {session.fileResult.message || readableReason(session.fileResult.code)} (RC {session.fileResult.rc})
    </p> : null}
  </section>;
}

export function ToolsProfiles({ send, session, snapshot, online, argumentsText, onArguments,
  onProgramRun, onClose, focusEditor }: {
  send: Send;
  session: ToolSession;
  snapshot: Snapshot | null;
  online: boolean;
  argumentsText: string;
  onArguments: (value: string) => void;
  onProgramRun: (tool: string, image: string) => void;
  onClose: () => void;
  focusEditor: () => void;
}) {
  const [toolId, setToolId] = useState("");
  const [image, setImage] = useState("");
  const [importState, setImportState] = useState("");
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
  const buildState = profile?.build.state === "ready" ? "Ready"
    : profile?.build.state === "stale" ? "Stale · different current file or revision" : "Not built";
  return <div class="tools-profiles" aria-label="Tools and profiles">
    <div class="tools-heading"><strong>Tools & Profiles</strong>
      <button type="button" onClick={onClose}>Hide</button></div>
    <div class="tools-body">
      <section class="profile-setup" aria-label="Profile setup">
        <h2>Profiles</h2>
        <p>Profiles define editor settings, file associations and workbench actions. Open a THE source, then use its actions beside the filename.</p>
        <dl class="profile-status-list">
          <dt>Source</dt><dd class="profile-source-path">{profile?.sourcePath || "User profile source"}</dd>
          <dt>Build</dt><dd class="profile-build-state">{buildState}{profile?.build.image ? <small>{profile.build.image}</small> : null}</dd>
          <dt>Active</dt><dd class="profile-active-image">{profile?.active.image || "No user profile activated"}</dd>
        </dl>
        <button type="button" disabled={!online || busy || !!snapshot?.panel} onClick={() => {
          if (send({ type: "tool", action: "profile", tool: "", text: "", arguments: "" }) !== undefined)
            window.setTimeout(focusEditor, 0);
        }}>Edit profile</button>
        <p class="tools-hint">THE sources use cREXX Level B. Build creates an artifact; Run built macro executes it once. Activate applies it as a profile.</p>
      </section>
      <section class="build-results" aria-label="Build results">
        <div class="tools-section-heading"><h2>Build results</h2>
          {build?.job.running ? <button type="button" disabled={!online} onClick={() => send({ type: "tool", action: "stop", tool: "", text: "", arguments: "" })}>Stop build</button> : null}
        </div>
        <p class="build-state" role="status">{build?.job.id ? `${build.job.state}${build.job.filename ? ` · ${build.job.filename} · revision ${build.job.revision}` : ""}` : "No compiler build has run in this session."}</p>
        {result?.serial ? <p class={`file-result ${result.rc ? "file-result-error" : ""}`} role="status">{result.action}: {result.message || readableReason(result.code)}{result.rc ? ` (RC ${result.rc})` : ""}</p> : null}
        {build?.omitted ? <p class="tools-hint">Older build output omitted</p> : null}
        <pre class="build-output" aria-label="Build output" tabIndex={0}>{build?.chunks.map((chunk, index) =>
          <span key={index} class={chunk.fd === 2 ? "console-stderr" : ""} data-stream={chunk.fd === 2 ? "stderr" : "stdout"}>{chunk.text}</span>)}</pre>
      </section>
      <section class="run-setup" aria-label="Explicit program tool">
        <h2>Run with a tool</h2>
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
            const reader = new FileReader();
            reader.onerror = () => setImportState("Could not read RXBIN file");
            reader.onload = () => {
              if (send({ type: "tool-import", name: file.name, bytes: String(reader.result).split(",")[1] }) !== undefined) {
                setImage("/workspace/" + file.name);
                setToolId("rxbin");
                setImportState(`RXBIN path: /workspace/${file.name}`);
              }
            };
            reader.readAsDataURL(file);
            event.currentTarget.value = "";
          }} />
          <button type="button" disabled={!online || busy} onClick={() => importer.current?.click()}>Import RXBIN</button>
          {importState ? <span class="tools-hint" role="status">{importState}</span> : null}
          <p class="tools-hint">Import prepared images for this runtime cohort. File associations are configured in a profile.</p>
        </div> : null}
      </section>
      <section class="tool-catalog" aria-label="Installed tools">
        <h2>Installed tools and associations</h2>
        <ul>{session.catalog.map((candidate) => <li key={candidate.id}>
          <strong>{candidate.label}</strong><span>{candidate.role} · {candidate.available ? "Available" : "Unavailable"}</span>
          <small>{candidate.extensions || "No file association"}</small>
        </li>)}</ul>
      </section>
    </div>
  </div>;
}
