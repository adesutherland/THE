import { useEffect, useRef, useState } from "preact/hooks";
import type { Snapshot } from "./types";
import type { ToolSession } from "./tool-session";

// Presentation only: THE owns source capture, registry, job identity and streams.
// This component stays mounted when its pane is hidden or the layout changes.
export function RexxConsole({ send, session, snapshot, focusRequest, onClose, focusEditor }: {
  send: (message: Record<string, unknown>) => number | undefined;
  session: ToolSession;
  snapshot: Snapshot | null;
  focusRequest: number;
  onClose: () => void;
  focusEditor: () => void;
}) {
  const [input, setInput] = useState("");
  const [unread, setUnread] = useState(false);
  const root = useRef<HTMLDivElement>(null);
  const log = useRef<HTMLPreElement>(null);
  const entry = useRef<HTMLInputElement>(null);
  const lastFocusRequest = useRef(focusRequest);
  const followingOutput = useRef(true);
  const requestedInputFocus = useRef(false);
  const outputVersion = useRef("");
  const job = session.job;
  const act = (action: string, text = "") =>
    send({ type: "tool", action, tool: "", text, arguments: "" });

  useEffect(() => {
    if (lastFocusRequest.current === focusRequest) return;
    lastFocusRequest.current = focusRequest;
    requestedInputFocus.current = true;
    log.current?.focus();
  }, [focusRequest]);
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
  const buildRunning = !!session.build?.job.running;

  return <div class="rexx-dock" ref={root} aria-label="Program console">
    <div class="console-heading">
      <strong>Program console</strong>
      <span class="run-state" role="status">{job.state}</span>
      <button type="button" onClick={onClose} aria-label="Hide program console">Hide</button>
    </div>
    <div class="console-runtime-controls">
      <button type="button" onClick={() => act("stop")} disabled={!job.running}>Stop</button>
      <button type="button" onClick={() => act("clear")} disabled={buildRunning}>Clear</button>
    </div>
    <div class="console-context">
      <span title={job.filename}>{job.filename ? `${job.filename} · revision ${job.revision} · ${session.catalog.find((tool) => tool.id === job.tool)?.label ?? (job.tool === "built-macro" ? "THE macro" : job.tool)}` : "Run a file using its actions beside the filename."}</span>
      {changed ? <span class="console-notice">Buffer changed since run</span> : null}
      {session.omitted ? <span class="console-notice">Older output omitted</span> : null}
    </div>
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
      if (act("input", input) !== undefined) {
        setInput("");
        entry.current?.focus();
      }
    }}>
      <label for="rexx-input">{job.waiting ? "Program input requested" : "Program input"}</label>
      <input id="rexx-input" ref={entry} value={input} onInput={(event) => setInput(event.currentTarget.value)}
        onKeyDown={(event) => { if (event.key === "Escape") { event.preventDefault(); focusEditor(); } }}
        disabled={!job.running || job.eof || !!snapshot?.panel} autoComplete="off" aria-label="Program input" />
      <button type="submit" disabled={!job.running || job.eof || !!snapshot?.panel}>Send</button>
      <button type="button" onClick={() => act("eof")} disabled={!job.running || job.eof}>End input</button>
    </form>
  </div>;
}
