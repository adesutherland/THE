import { useEffect, useRef, useState } from "preact/hooks";
import { browserWasm } from "./transport";
import type { Snapshot } from "./types";

export type ToolSession = {
  type: "tools";
  version: number;
  job: {
    id: number; sourceId: number; revision: number;
    running: boolean; waiting: boolean; eof: boolean;
    state: string; filename: string; tool: string;
  };
  visible: boolean;
  omitted: boolean;
  chunks: { fd: number; text: string }[];
  catalog: {
    id: string; label: string; role: string; image: string;
    extensions: string; available: boolean;
  }[];
};

// Presentation only: THE owns source capture, registry, job identity and streams.
// This component stays mounted when its pane is hidden or the layout changes.
export function RexxConsole({ send, session, snapshot, runRequest, onClose, focusEditor }: {
  send: (message: Record<string, unknown>) => number | undefined;
  session: ToolSession;
  snapshot: Snapshot | null;
  runRequest: number;
  onClose: () => void;
  focusEditor: () => void;
}) {
  const [args, setArgs] = useState("");
  const [input, setInput] = useState("");
  const [tool, setTool] = useState("classic-rexx");
  const [image, setImage] = useState("");
  const [unread, setUnread] = useState(false);
  const importer = useRef<HTMLInputElement>(null);
  const root = useRef<HTMLDivElement>(null);
  const log = useRef<HTMLPreElement>(null);
  const entry = useRef<HTMLInputElement>(null);
  const lastRunRequest = useRef(runRequest);
  const followingOutput = useRef(true);
  const requestedInputFocus = useRef(false);
  const outputVersion = useRef("");
  const job = session.job;
  const act = (action: string, text = "", selected = tool) =>
    send({ type: "tool", action, tool: selected, text, arguments: args });

  useEffect(() => {
    if (lastRunRequest.current === runRequest) return;
    lastRunRequest.current = runRequest;
    requestedInputFocus.current = true;
    log.current?.focus();
    act("run", "", "");
  }, [runRequest]);

  useEffect(() => {
    if (job.tool && session.catalog.some((candidate) => candidate.id === job.tool))
      setTool(job.tool);
  }, [job.id, job.tool, session.catalog]);

  useEffect(() => {
    if (!session.catalog.some((candidate) => candidate.id === tool)) {
      const available = session.catalog.find((candidate) => candidate.available && ["runner", "program"].includes(candidate.role));
      if (available) setTool(available.id);
    }
  }, [session.catalog, tool]);
  useEffect(() => {
    followingOutput.current = true;
    setUnread(false);
  }, [job.id]);
  useEffect(() => {
    const version = `${job.id}:${JSON.stringify(session.chunks)}`;
    if (version === outputVersion.current) return;
    outputVersion.current = version;
    if (followingOutput.current && log.current) log.current.scrollTop = log.current.scrollHeight;
    else setUnread(true);
  }, [session.version, job.id]);
  useEffect(() => {
    // A user who has gone back to editing keeps focus there while a job waits.
    if (job.waiting && requestedInputFocus.current && root.current?.contains(document.activeElement)) {
      entry.current?.focus();
      requestedInputFocus.current = false;
    }
    if (!job.running) requestedInputFocus.current = false;
  }, [job.waiting, job.running]);

  const changed = snapshot?.source && job.sourceId &&
    (snapshot.source.id !== job.sourceId || snapshot.source.revision !== job.revision);
  const runAvailable = session.catalog.some((candidate) => candidate.id === tool && candidate.available);

  return <div class="rexx-dock" ref={root} aria-label="Program console">
    <div class="console-heading">
      <strong>Program console</strong>
      <span class="run-state" role="status">{job.state}</span>
      <button type="button" onClick={onClose} aria-label="Hide program console">Hide</button>
    </div>
    <div class="console-run-controls">
      <label>Tool <select aria-label="Run tool" value={tool} disabled={job.running}
        onChange={(event) => setTool(event.currentTarget.value)}>
        {session.catalog.filter((candidate) => ["runner", "program"].includes(candidate.role)).map((candidate) =>
          <option key={candidate.id} value={candidate.id} disabled={!candidate.available}>{candidate.label}</option>)}
      </select></label>
      <button type="button" class="run-program" disabled={job.running || !!snapshot?.panel || !runAvailable}
        onClick={(event) => {
          event.currentTarget.focus();
          requestedInputFocus.current = true;
          act(tool === "rxbin" ? "rxbin" : "run", image);
        }}>Run {session.catalog.find((candidate) => candidate.id === tool)?.label ?? "program"}</button>
      <button type="button" onClick={() => act("stop")} disabled={!job.running}>Stop</button>
      <button type="button" onClick={() => act("clear")}>Clear</button>
    </div>
    <div class="console-context">
      <span title={job.filename}>{job.filename ? `${job.filename} · revision ${job.revision}` : "Runs the current buffer, including unsaved edits."}</span>
      {changed ? <span class="console-notice">Buffer changed since run</span> : null}
      {session.omitted ? <span class="console-notice">Older output omitted</span> : null}
    </div>
    <details class="console-options"><summary>Arguments and run options</summary>
      <label>Arguments<textarea rows={2} aria-label="Rexx arguments" placeholder="One argument per line"
        value={args} disabled={job.running} onInput={(event) => setArgs(event.currentTarget.value)} /></label>
      {tool === "rxbin" ? <label>RXBIN workspace path<input value={image} aria-label="RXBIN path" disabled={job.running}
        onInput={(event) => setImage(event.currentTarget.value)} /></label> : null}
      {browserWasm ? <>
        <input ref={importer} type="file" accept=".rxbin" hidden onChange={(event) => {
          const file = event.currentTarget.files?.[0];
          if (!file) return;
          const reader = new FileReader();
          reader.onload = () => {
            send({ type: "tool-import", name: file.name, bytes: String(reader.result).split(",")[1] });
            setImage("/workspace/" + file.name);
            setTool("rxbin");
          };
          reader.readAsDataURL(file);
          event.currentTarget.value = "";
        }} />
        <button type="button" disabled={job.running} onClick={() => importer.current?.click()}>Import RXBIN</button>
      </> : null}
    </details>
    <details class="console-options profile-options"><summary>Profile: Edit → Build → Activate</summary>
      <div class="profile-actions">
        <button type="button" onClick={() => act("profile")} disabled={job.running}>Edit profile</button>
        <button type="button" onClick={() => act("build")}
          disabled={job.running || !session.catalog.some((candidate) => candidate.id === "profile-compiler" && candidate.available)}>Build profile</button>
        <button type="button" onClick={() => act("activate")} disabled={job.running}>Activate profile</button>
      </div>
      <p>Source profiles use cREXX Level B.</p>
    </details>
    <pre class="console-output" aria-label="Program output" ref={log} tabIndex={0}
      onScroll={(event) => {
        const output = event.currentTarget;
        followingOutput.current = output.scrollHeight - output.scrollTop - output.clientHeight < 24;
        if (followingOutput.current) setUnread(false);
      }}>
      {session.chunks.map((chunk, index) => <span key={index}
        class={chunk.fd === 2 ? "console-stderr" : chunk.fd === 0 ? "console-echo" : ""}
        data-stream={chunk.fd === 2 ? "stderr" : chunk.fd === 0 ? "stdin" : "stdout"}>{chunk.text}</span>)}
    </pre>
    {unread ? <button type="button" class="new-output" onClick={() => {
      followingOutput.current = true;
      if (log.current) log.current.scrollTop = log.current.scrollHeight;
      setUnread(false);
    }}>New output · Jump to latest</button> : null}
    <form class="console-input" onSubmit={(event) => {
      event.preventDefault();
      if (!job.running || job.eof) return;
      if (act("input", input) !== undefined) setInput("");
    }}>
      <label for="rexx-input">{job.waiting ? "Program input requested" : "Program input"}</label>
      <input id="rexx-input" ref={entry} value={input} onInput={(event) => setInput(event.currentTarget.value)}
        onKeyDown={(event) => { if (event.key === "Escape") { event.preventDefault(); focusEditor(); } }}
        disabled={!job.running || job.eof || !!snapshot?.panel} autoComplete="off" aria-label="Rexx input" />
      <button type="submit" disabled={!job.running || job.eof || !!snapshot?.panel}>Send</button>
      <button type="button" onClick={() => act("eof")} disabled={!job.running || job.eof}>End input</button>
    </form>
  </div>;
}
