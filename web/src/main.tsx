import { render } from "preact";
import { useCallback, useEffect, useMemo, useRef, useState } from "preact/hooks";
import {
  AlertTriangle,
  ArrowUp,
  Check,
  FileText,
  Folder,
  FolderOpen,
  RefreshCw,
  Save,
  Undo2,
  X,
} from "lucide-preact";
import type { DriverMessage, FileListMessage, FrontendActionDefinition, ScreenRow, Snapshot, StyleRun } from "./types";
import "./styles.css";

const keyNames: Record<string, string> = {
  ArrowLeft: "left",
  ArrowRight: "right",
  ArrowUp: "up",
  ArrowDown: "down",
  Home: "home",
  End: "end",
  PageUp: "pageup",
  PageDown: "pagedown",
  Enter: "enter",
  Escape: "esc",
  Tab: "tab",
  Backspace: "backspace",
  Delete: "delete",
  Insert: "insert",
};

function basename(path: string): string {
  const normalized = path.replaceAll("\\", "/");
  return normalized.slice(normalized.lastIndexOf("/") + 1) || path;
}

function styledText(text: string, styles: StyleRun[] | undefined) {
  if (!styles?.length) return text;

  const characters = Array.from(text);
  const segments: preact.ComponentChildren[] = [];
  let offset = 0;

  for (const [start, length, style] of [...styles].sort((a, b) => a[0] - b[0])) {
    if (start > offset) segments.push(characters.slice(offset, start).join(""));
    const end = Math.min(characters.length, start + length);
    if (end > start) {
      segments.push(
        <span class={`syntax syntax-${style}`} key={`${start}-${style}`}>
          {characters.slice(start, end).join("")}
        </span>,
      );
    }
    offset = Math.max(offset, end);
  }
  if (offset < characters.length) segments.push(characters.slice(offset).join(""));
  return segments;
}

function pointerCell(event: MouseEvent, startCell: number): number {
  const target = event.currentTarget as HTMLElement;
  const rect = target.getBoundingClientRect();
  const style = getComputedStyle(target);
  const padding = Number.parseFloat(style.paddingLeft) || 0;
  const probe = document.createElement("span");
  probe.className = "cell-probe";
  probe.textContent = "0000000000";
  target.appendChild(probe);
  const width = probe.getBoundingClientRect().width / 10;
  probe.remove();
  return startCell + Math.max(0, Math.floor((event.clientX - rect.left - padding) / width));
}

function App() {
  const [snapshot, setSnapshot] = useState<Snapshot | null>(null);
  const [connection, setConnection] = useState<"connecting" | "online" | "reconnecting" | "offline">("connecting");
  const [lastError, setLastError] = useState("");
  const [command, setCommand] = useState("");
  const [actions, setActions] = useState<FrontendActionDefinition[]>([]);
  const [openMenu, setOpenMenu] = useState<string | null>(null);
  const [pathAction, setPathAction] = useState<FrontendActionDefinition | null>(null);
  const [pathValue, setPathValue] = useState("");
  const [explorerOpen, setExplorerOpen] = useState(false);
  const [files, setFiles] = useState<FileListMessage | null>(null);
  const socketRef = useRef<WebSocket | null>(null);
  const inputRef = useRef<HTMLTextAreaElement | null>(null);
  const nextId = useRef(1);

  const send = useCallback((message: Record<string, unknown>) => {
    const socket = socketRef.current;
    if (!socket || socket.readyState !== WebSocket.OPEN) return;
    socket.send(JSON.stringify({ v: 1, id: nextId.current++, ...message }));
  }, []);

  const focusEditor = useCallback(() => inputRef.current?.focus(), []);

  useEffect(() => {
    const token = new URLSearchParams(window.location.search).get("token") ?? "";
    const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
    const url = `${protocol}//${window.location.host}/ws?token=${encodeURIComponent(token)}`;
    let disposed = false;
    let retryTimer: number | undefined;
    let retryAttempt = 0;

    const connect = () => {
      if (disposed) return;
      setConnection(retryAttempt === 0 ? "connecting" : "reconnecting");
      const socket = new WebSocket(url);
      socketRef.current = socket;

      socket.onopen = () => {
        if (disposed || socketRef.current !== socket) return;
        retryAttempt = 0;
        setConnection("online");
        setLastError("");
        window.setTimeout(focusEditor, 0);
      };
      socket.onclose = () => {
        if (disposed || socketRef.current !== socket) return;
        socketRef.current = null;
        retryAttempt = Math.min(retryAttempt + 1, 5);
        setConnection("reconnecting");
        setLastError("Web driver connection interrupted");
        const delay = Math.min(500 * (2 ** (retryAttempt - 1)), 5000);
        retryTimer = window.setTimeout(connect, delay);
      };
      socket.onerror = () => {
        if (!disposed && socketRef.current === socket) {
          setLastError("Web driver connection interrupted");
        }
      };
      socket.onmessage = (event) => {
        try {
          const message = JSON.parse(String(event.data)) as Snapshot | DriverMessage;
          if ("mode" in message && "screen_rows" in message) {
            const nextSnapshot = message as Snapshot;
            setSnapshot(nextSnapshot);
            if (nextSnapshot.focus.zone === "command") setCommand(nextSnapshot.command);
            setLastError("");
          } else if ("type" in message && message.type === "actions" && message.actions) {
            setActions(message.actions);
          } else if ("type" in message && message.type === "files") {
            setFiles(message as FileListMessage);
          } else if ("type" in message && message.type === "error") {
            setLastError(message.message ?? "Driver rejected the input");
          }
        } catch {
          setLastError("The web driver returned invalid JSON");
        }
      };
    };

    connect();
    return () => {
      disposed = true;
      if (retryTimer !== undefined) window.clearTimeout(retryTimer);
      const socket = socketRef.current;
      socketRef.current = null;
      if (socket) {
        socket.onclose = null;
        socket.close();
      }
    };
  }, [focusEditor]);

  const runCommand = useCallback(
    (value: string) => {
      const trimmed = value.trim();
      if (!trimmed) return;
      send({ type: "command", command: trimmed });
      setCommand("");
      window.setTimeout(focusEditor, 0);
    },
    [focusEditor, send],
  );

  const sendAction = useCallback(
    (action: string, argument = "") => {
      send({ type: "action", action, argument });
      setOpenMenu(null);
      window.setTimeout(focusEditor, 0);
    },
    [focusEditor, send],
  );

  const actionById = useMemo(
    () => new Map(actions.map((action) => [action.id, action])),
    [actions],
  );

  const requestFiles = useCallback(
    (root = files?.root ?? 0, path = files?.path ?? "") => {
      send({ type: "files", root, path });
    },
    [files?.path, files?.root, send],
  );

  const requestAction = useCallback(
    (id: string, argument?: string) => {
      const action = actionById.get(id);
      if (!action) return;
      setOpenMenu(null);
      if (id === "file.open" && argument === undefined) {
        setExplorerOpen(true);
        requestFiles();
        return;
      }
      if (action.requires_argument && argument === undefined) {
        setPathValue("");
        setPathAction(action);
        return;
      }
      sendAction(id, argument ?? "");
    },
    [actionById, requestFiles, sendAction],
  );

  const onEditorKeyDown = useCallback(
    (event: KeyboardEvent) => {
      const commandModifier = event.ctrlKey || event.metaKey;
      if (commandModifier && event.key.toLowerCase() === "s") {
        event.preventDefault();
        requestAction("file.save");
        return;
      }
      if (commandModifier && event.key.toLowerCase() === "o") {
        event.preventDefault();
        requestAction("file.open");
        return;
      }
      if (commandModifier && event.key.toLowerCase() === "n") {
        event.preventDefault();
        requestAction("file.create");
        return;
      }
      if (commandModifier && event.key.toLowerCase() === "w") {
        event.preventDefault();
        requestAction("file.close");
        return;
      }
      if (commandModifier && event.key.toLowerCase() === "z") {
        event.preventDefault();
        if (!event.shiftKey) requestAction("edit.undo");
        return;
      }
      const key = event.shiftKey && event.key === "Tab" ? "backtab" : keyNames[event.key];
      if (key) {
        event.preventDefault();
        send({ type: "key", key });
      }
    },
    [requestAction, send],
  );

  const rows = snapshot?.screen_rows ?? [];
  const currentBuffer = snapshot?.buffers.find((buffer) => buffer.current);
  const title = currentBuffer ? basename(currentBuffer.path) : "THE";
  const cursorRow = useMemo(
    () => rows.find((row) => row.r === snapshot?.focus.row && row.line === snapshot?.focus.line),
    [rows, snapshot?.focus.line, snapshot?.focus.row],
  );
  const menus = useMemo(() => {
    const result = new Map<string, FrontendActionDefinition[]>();
    for (const action of actions) {
      if (!action.menu) continue;
      const entries = result.get(action.menu) ?? [];
      entries.push(action);
      result.set(action.menu, entries);
    }
    return [...result.entries()];
  }, [actions]);

  const hitRow = useCallback(
    (event: MouseEvent, row: ScreenRow, target: "filearea" | "prefix") => {
      const cell = pointerCell(event, target === "filearea" ? row.sc : 0);
      send({ type: "hit", target, line: row.line, row: row.r, cell });
      window.setTimeout(focusEditor, 0);
    },
    [focusEditor, send],
  );

  return (
    <main class="app-shell">
      <header class="toolbar">
        <div class="product-mark">THE</div>
        <nav class="menu-bar" aria-label="Application menu">
          {menus.map(([menu, entries]) => (
            <div class="menu-root" key={menu}>
              <button
                type="button"
                class={`menu-trigger ${openMenu === menu ? "menu-trigger-open" : ""}`}
                aria-expanded={openMenu === menu}
                onClick={() => setOpenMenu(openMenu === menu ? null : menu)}
              >
                {menu}
              </button>
              {openMenu === menu ? (
                <div class="menu-popover" role="menu">
                  {entries.map((action) => (
                    <button type="button" role="menuitem" onClick={() => requestAction(action.id)}>
                      <span>{action.label}</span>
                      <kbd>{action.id === "file.create" ? "Ctrl+N" : action.id === "file.open" ? "Ctrl+O" : action.id === "file.save" ? "Ctrl+S" : action.id === "file.close" ? "Ctrl+W" : action.id === "edit.undo" ? "Ctrl+Z" : ""}</kbd>
                    </button>
                  ))}
                </div>
              ) : null}
            </div>
          ))}
        </nav>
        <div class="document-title" title={currentBuffer?.path}>{title}</div>
        <div class="toolbar-actions">
          <button class="icon-button" type="button" title="Open" aria-label="Open" onClick={() => requestAction("file.open")}>
            <FolderOpen size={17} />
          </button>
          <button class="icon-button" type="button" title="Save" aria-label="Save" onClick={() => requestAction("file.save")}>
            <Save size={17} />
          </button>
          <button class="icon-button" type="button" title="Undo" aria-label="Undo" onClick={() => requestAction("edit.undo")}>
            <Undo2 size={17} />
          </button>
          <button class="icon-button" type="button" title="Refresh" aria-label="Refresh" onClick={() => send({ type: "snapshot" })}>
            <RefreshCw size={17} />
          </button>
        </div>
        <div class={`connection connection-${connection}`} title={`Web driver: ${connection}`}>
          {connection === "online" ? <Check size={14} /> : connection === "reconnecting" ? <RefreshCw size={14} /> : <AlertTriangle size={14} />}
          <span>{connection}</span>
        </div>
      </header>

      <nav class="buffer-tabs" aria-label="Open buffers">
        {snapshot?.buffers.map((buffer) => (
          <button
            type="button"
            class={`buffer-tab ${buffer.current ? "buffer-tab-current" : ""}`}
            title={buffer.path}
            onClick={() => !buffer.current && requestAction("buffer.switch", buffer.path)}
          >
            <span>{basename(buffer.path)}</span>
            {buffer.dirty ? <span class="dirty-mark" aria-label="Modified" /> : null}
          </button>
        ))}
      </nav>

      <section class="workspace">
        {explorerOpen ? (
          <aside class="file-explorer" aria-label="Workspace files">
            <div class="explorer-heading">
              <span>Files</span>
              <button class="icon-button" type="button" title="Close files" aria-label="Close files" onClick={() => setExplorerOpen(false)}><X size={16} /></button>
            </div>
            <div class="root-switcher" aria-label="Workspace roots">
              {files?.roots.map((root) => (
                <button type="button" class={root.id === files.root ? "root-current" : ""} onClick={() => requestFiles(root.id, "")}>
                  <Folder size={14} />
                  <span>{root.name}</span>
                  {root.readonly ? <span class="readonly-label">RO</span> : null}
                </button>
              ))}
            </div>
            <div class="explorer-path" title={files?.path || "/"}>
              <button
                class="icon-button"
                type="button"
                title="Parent directory"
                aria-label="Parent directory"
                disabled={!files?.path}
                onClick={() => requestFiles(files?.root ?? 0, (files?.path ?? "").split("/").slice(0, -1).join("/"))}
              >
                <ArrowUp size={15} />
              </button>
              <span>{files?.path || "/"}</span>
              <button class="icon-button" type="button" title="Refresh files" aria-label="Refresh files" onClick={() => requestFiles()}><RefreshCw size={14} /></button>
            </div>
            <div class="file-list">
              {[...(files?.entries ?? [])].sort((left, right) => {
                if (left.type !== right.type) return left.type === "directory" ? -1 : 1;
                return left.name.localeCompare(right.name);
              }).map((entry) => (
                <button
                  type="button"
                  title={entry.path}
                  onClick={() => {
                    if (entry.type === "directory") requestFiles(files?.root ?? 0, entry.path);
                    else {
                      requestAction("file.open", entry.target);
                      setExplorerOpen(false);
                    }
                  }}
                >
                  {entry.type === "directory" ? <Folder size={15} /> : <FileText size={15} />}
                  <span>{entry.name}</span>
                  {entry.readonly ? <span class="readonly-label">RO</span> : null}
                </button>
              ))}
              {files && files.entries.length === 0 ? <div class="empty-directory">No files</div> : null}
            </div>
          </aside>
        ) : null}
        <div class="editor-scroll" onClick={focusEditor}>
          <div class="editor-grid" style={{ minWidth: `${Math.max(snapshot?.cols ?? 80, 80)}ch` }}>
            {rows.map((row) => {
              const cursorVisible = cursorRow === row && snapshot?.focus.zone === "filearea";
              const cursorCell = Math.max(0, (snapshot?.focus.cell ?? 0) - row.sc);
              return (
                <div class={`screen-row role-${row.role} ${row.cur ? "screen-row-current" : ""}`} key={`${currentBuffer?.path ?? ""}-${row.r}-${row.line}-${row.role}`}>
                  <button
                    type="button"
                    class="prefix-cell"
                    tabIndex={-1}
                    onClick={(event) => hitRow(event, row, "prefix")}
                  >
                    {row.pc || row.p || ""}
                  </button>
                  <div class="line-content" onClick={(event) => hitRow(event, row, "filearea")}>
                    <span class="line-text">{styledText(row.t, row.s)}</span>
                    {cursorVisible ? <span class="logical-cursor" style={{ left: `${cursorCell}ch` }} /> : null}
                  </div>
                </div>
              );
            })}
          </div>
        </div>

        {snapshot?.diagnostics?.length ? (
          <aside class="diagnostics" aria-label="Diagnostics">
            <div class="panel-heading">Diagnostics <span>{snapshot.diagnostics.length}</span></div>
            {snapshot.diagnostics.map((diagnostic) => (
              <button
                type="button"
                class="diagnostic-row"
                onClick={() => {
                  send({ type: "hit", target: "filearea", line: diagnostic.line, row: 0, cell: Math.max(0, diagnostic.column - 1) });
                  focusEditor();
                }}
              >
                <span class={`severity severity-${diagnostic.severity}`}>{diagnostic.severity || "info"}</span>
                <span class="diagnostic-location">{diagnostic.line}:{diagnostic.column}</span>
                <span class="diagnostic-message">{diagnostic.message}</span>
              </button>
            ))}
          </aside>
        ) : null}
      </section>

      <footer class="command-area">
        <form class="command-form" onSubmit={(event) => { event.preventDefault(); runCommand(command); }}>
          <span class="command-prompt">Command</span>
          <input
            aria-label="THE command"
            value={command}
            onInput={(event) => setCommand(event.currentTarget.value)}
            spellcheck={false}
            autoComplete="off"
          />
        </form>
        <div class="status-line">
          <span class="status-text">{lastError || snapshot?.status || "Waiting for THE"}</span>
          <span>{snapshot?.focus.zone ?? "-"}</span>
          <span>{snapshot?.focus.line ?? 0}:{(snapshot?.focus.cell ?? 0) + 1}</span>
          <span>{snapshot?.buffer?.dirty ? "Modified" : "Saved"}</span>
        </div>
      </footer>

      <textarea
        ref={inputRef}
        class="input-sink"
        aria-label="Editor input"
        onKeyDown={onEditorKeyDown}
        onInput={(event) => {
          const value = event.currentTarget.value;
          if (value) send({ type: "text", text: value });
          event.currentTarget.value = "";
        }}
        autoCapitalize="off"
        autoCorrect="off"
        spellcheck={false}
      />

      {pathAction ? (
        <div class="dialog-backdrop" role="presentation" onMouseDown={(event) => {
          if (event.currentTarget === event.target) setPathAction(null);
        }}>
          <form class="path-dialog" role="dialog" aria-modal="true" aria-labelledby="path-dialog-title" onSubmit={(event) => {
            event.preventDefault();
            const value = pathValue.trim();
            if (!value) return;
            sendAction(pathAction.id, value);
            setPathAction(null);
          }}>
            <div class="dialog-heading">
              <h2 id="path-dialog-title">{pathAction.label} file</h2>
              <button class="icon-button" type="button" aria-label="Close" title="Close" onClick={() => setPathAction(null)}><X size={17} /></button>
            </div>
            <label for="file-path">Path</label>
            <input id="file-path" autoFocus value={pathValue} onInput={(event) => setPathValue(event.currentTarget.value)} spellcheck={false} autoComplete="off" />
            <div class="dialog-actions">
              <button type="button" onClick={() => setPathAction(null)}>Cancel</button>
              <button type="submit" class="primary-command" disabled={!pathValue.trim()}>{pathAction.label}</button>
            </div>
          </form>
        </div>
      ) : null}
    </main>
  );
}

render(<App />, document.getElementById("app")!);
