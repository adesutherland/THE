import { useEffect, useImperativeHandle, useLayoutEffect, useRef, useState } from "preact/hooks";
import type { Snapshot } from "./types";
import type { ToolSession } from "./tool-session";

// Presentation only: THE owns source capture, registry, job identity and streams.
// This component stays mounted when its pane is hidden or the layout changes.
export type ProgramReaderHandle = { captureSelection: () => void };

function readerRange(output: HTMLElement, start: number, end: number): Range | null {
  const range = document.createRange(), nodes = document.createTreeWalker(output, NodeFilter.SHOW_TEXT);
  let offset = 0, started = false;
  for (let node = nodes.nextNode(); node; node = nodes.nextNode()) {
    const next = offset + (node.textContent?.length || 0);
    if (!started && start <= next) { range.setStart(node, start - offset); started = true; }
    if (started && end <= next) { range.setEnd(node, end - offset); return range; }
    offset = next;
  }
  return null;
}

function selectedReaderRange(output: HTMLElement): Range | null {
  const selection = window.getSelection(), range = selection?.rangeCount ? selection.getRangeAt(0) : null;
  return range && !range.collapsed && output.contains(range.startContainer) && output.contains(range.endContainer) ? range : null;
}

export function RexxConsole({ send, session, snapshot, focusRequest, onClose, focusEditor, expanded, onExpand, visible, readerHandle }: {
  send: (message: Record<string, unknown>) => number | undefined;
  session: ToolSession;
  snapshot: Snapshot | null;
  focusRequest: number;
  onClose: () => void;
  focusEditor: () => void;
  expanded: boolean;
  onExpand: () => void;
  visible: boolean;
  readerHandle: { current: ProgramReaderHandle | null };
}) {
  const [input, setInput] = useState("");
  const [unread, setUnread] = useState(false);
  const root = useRef<HTMLDivElement>(null);
  const log = useRef<HTMLPreElement>(null);
  const entry = useRef<HTMLInputElement>(null);
  const lastFocusRequest = useRef(focusRequest);
  const readerViewport = useRef({ job: session.job.id, top: 0, left: 0, follow: true });
  const requestedInputFocus = useRef(false);
  const outputVersion = useRef("");
  const previousReader = useRef({ visible, job: session.job.id });
  const retainedSelection = useRef<{ job: number; content: string; text: string; start: number; end: number } | null>(null);
  const job = session.job;
  const act = (action: string, text = "") => {
    if (action === "clear") {
      retainedSelection.current = null;
      readerViewport.current = { job: job.id, top: 0, left: 0, follow: true };
    }
    return send({ type: "tool", action, tool: "", text, arguments: "" });
  };

  const captureReaderSelection = () => {
    const output = log.current, selection = window.getSelection();
    const range = selection?.rangeCount ? selection.getRangeAt(0) : null;
    if (!output) return;
    if (output.getClientRects().length) {
      readerViewport.current.top = output.scrollTop;
      readerViewport.current.left = output.scrollLeft;
    }
    if (!range || range.collapsed || !output.contains(range.startContainer) || !output.contains(range.endContainer)) {
      if (output.getClientRects().length) retainedSelection.current = null;
      return;
    }
    const before = document.createRange(); before.selectNodeContents(output); before.setEnd(range.startContainer, range.startOffset);
    const start = before.toString().length, text = range.toString();
    if (!text) return;
    readerViewport.current.follow = false;
    retainedSelection.current = { job: job.id, content: output.textContent || "", text, start, end: start + text.length };
  };
  useImperativeHandle(readerHandle, () => ({ captureSelection: captureReaderSelection }), [job.id]);

  useLayoutEffect(() => {
    const previous = previousReader.current;
    previousReader.current = { visible, job: job.id };
    if (previous.job !== job.id) {
      retainedSelection.current = null;
      readerViewport.current = { job: job.id, top: 0, left: 0, follow: true };
    }
    else if (previous.visible && !visible) captureReaderSelection();
    const saved = retainedSelection.current, output = log.current;
    const content = output?.textContent || "";
    const validSelection = saved && saved.job === job.id && content.startsWith(saved.content)
      && content.slice(saved.start, saved.end) === saved.text;
    if (saved && !validSelection) {
      retainedSelection.current = null;
    }
    if (!visible || !output?.getClientRects().length) return;
    if (validSelection && saved) {
      retainedSelection.current = null;
      const range = readerRange(output, saved.start, saved.end);
      if (range?.toString() === saved.text) {
        requestedInputFocus.current = false;
        readerViewport.current.follow = false;
        output.focus({ preventScroll: true });
        const selection = window.getSelection();
        selection?.removeAllRanges(); selection?.addRange(range);
      }
    }
    if (!previous.visible || validSelection) {
      const viewport = readerViewport.current;
      output.scrollTop = viewport.follow ? output.scrollHeight : viewport.top;
      output.scrollLeft = viewport.left;
      viewport.top = output.scrollTop;
      viewport.left = output.scrollLeft;
    }
  }, [visible, job.id, session.version]);

  useEffect(() => {
    if (lastFocusRequest.current === focusRequest) return;
    lastFocusRequest.current = focusRequest;
    requestedInputFocus.current = true;
    log.current?.focus();
  }, [focusRequest]);
  useEffect(() => {
    setUnread(false);
  }, [job.id]);
  useEffect(() => {
    const version = `${job.id}:${JSON.stringify(session.chunks)}`;
    if (version === outputVersion.current) return;
    outputVersion.current = version;
    const output = log.current, viewport = readerViewport.current;
    if (!session.chunks.some((chunk) => !!chunk.text)) {
      readerViewport.current = { job: job.id, top: 0, left: 0, follow: true };
      setUnread(false);
      return;
    }
    if (output && selectedReaderRange(output)) viewport.follow = false;
    if (viewport.follow && visible && output?.getClientRects().length) {
      output.scrollTop = output.scrollHeight;
      viewport.top = output.scrollTop;
      viewport.left = output.scrollLeft;
    } else if (!viewport.follow) setUnread(true);
  }, [session.version, job.id]);
  useEffect(() => {
    // A user who has gone back to editing keeps focus there while a job waits.
    if (job.waiting && requestedInputFocus.current && root.current?.contains(document.activeElement)) {
      entry.current?.focus();
      requestedInputFocus.current = false;
    }
    if (!job.running) requestedInputFocus.current = false;
  }, [job.waiting, job.running]);

  const otherSource = !!snapshot?.source && !!job.sourceId && snapshot.source.id !== job.sourceId;
  const changed = !!snapshot?.source && !!job.sourceId && !otherSource && snapshot.source.revision !== job.revision;
  const buildRunning = !!session.build?.job.running;

  return <div class="rexx-dock" ref={root} aria-label="Program console">
    <div class="console-heading">
      <strong>Program console</strong>
      <button type="button" class="pane-expand" onClick={onExpand}>{expanded ? "Return to editor" : "Expand pane"}</button>
      <button type="button" onClick={onClose} aria-label="Hide program console">Hide</button>
    </div>
    <div class="console-runtime-controls">
      <span class="run-state" role="status">{job.state}</span>
      <button type="button" onClick={() => act("stop")} disabled={!job.running}>Stop</button>
      <button type="button" onClick={() => act("clear")} disabled={buildRunning}>Clear</button>
    </div>
    <div class="console-context">
      <span title={job.filename}>{job.filename ? `${job.filename} · revision ${job.revision} · ${session.catalog.find((tool) => tool.id === job.tool)?.label ?? (job.tool === "built-macro" ? "THE macro" : job.tool)}` : "Run a file using its actions beside the filename."}</span>
      {otherSource ? <span class="console-notice">Output retained from another file</span>
        : changed ? <span class="console-notice">Run source changed · output retained from revision {job.revision}</span> : null}
      {session.omitted ? <span class="console-notice">Older output omitted</span> : null}
    </div>
    <pre class="console-output" aria-label="Program output" ref={log} tabIndex={0}
      onScroll={(event) => {
        const output = event.currentTarget;
        // Hidden geometry can report zeroes; it cannot change the reader's
        // last visible viewport or turn scrollback into following output.
        if (!visible || !output.getClientRects().length) return;
        const follow = !selectedReaderRange(output) && output.scrollHeight - output.scrollTop - output.clientHeight < 24;
        readerViewport.current = { job: job.id, top: output.scrollTop, left: output.scrollLeft, follow };
        if (follow) setUnread(false);
      }}>
      {session.chunks.map((chunk, index) => <span key={index}
        class={chunk.fd === 2 ? "console-stderr" : chunk.fd === 0 ? "console-echo" : ""}
        data-stream={chunk.fd === 2 ? "stderr" : chunk.fd === 0 ? "stdin" : "stdout"}>{chunk.text}</span>)}
    </pre>
    {unread ? <button type="button" class="new-output" onClick={() => {
      readerViewport.current.follow = true;
      if (log.current) {
        log.current.scrollTop = log.current.scrollHeight;
        readerViewport.current.top = log.current.scrollTop;
        readerViewport.current.left = log.current.scrollLeft;
      }
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
