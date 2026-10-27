import { browserWasm, createEditorTransport, type EditorTransport } from "./transport";
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
import type { DriverMessage, FileListMessage, FrontendActionDefinition, PanelColour, ScreenRow, Snapshot, StyleRun } from "./types";
import "./styles.css";
import {RexxConsole, type RunSource} from './rexx-console';

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

// Keep a profile's key-guide text intact even when the editor has few columns.
// Other reserved rows retain their normal editor positions.
function keyHintsForRow(row: ScreenRow): { key: string; label: string }[] {
  if (row.role !== "reserved") return [];
  const text = row.t.trim();
  const matches = [...text.matchAll(/\b(F(?:[1-9]\d?)|Esc)\s+(.+?)(?=\s+(?:F(?:[1-9]\d?)|Esc)\s+|$)/g)];
  if (matches.length < 2 || matches[0].index !== 0) return [];
  return matches.map((match) => ({ key: match[1], label: match[2] }));
}

function panelColour(colour: PanelColour | undefined): preact.JSX.CSSProperties {
  if (!colour) return {};
  const reverse = Boolean(colour.style & 2);
  return {
    color: reverse ? colour.background : colour.foreground,
    backgroundColor: reverse ? colour.foreground : colour.background,
    fontWeight: colour.style & 1 ? 700 : undefined,
    fontStyle: colour.style & 32 ? "italic" : undefined,
    textDecoration: [colour.style & 4 ? "underline" : "", colour.style & 512 ? "overline" : "", colour.style & 1024 ? "line-through" : ""].filter(Boolean).join(" ") || undefined,
    opacity: colour.style & 16 ? 0.65 : undefined,
  };
}

function styledText(text: string, styles: StyleRun[] | undefined) {
  if (!styles?.length) return text.replaceAll("\t", "→");

  const characters = Array.from(text, (character) => character === "\t" ? "→" : character);
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
  const [storageState, setStorageState] = useState("");
  const [interpreterAvailable,setInterpreterAvailable] = useState(false);
  const [runSource,setRunSource] = useState<RunSource|null>(null);
  const [command, setCommand] = useState("");
  const [commandFocused, setCommandFocused] = useState(false);
  const [actions, setActions] = useState<FrontendActionDefinition[]>([]);
  const [openMenu, setOpenMenu] = useState<string | null>(null);
  const [pathAction, setPathAction] = useState<FrontendActionDefinition | null>(null);
  const [pathValue, setPathValue] = useState("");
  const [explorerOpen, setExplorerOpen] = useState(false);
  const [files, setFiles] = useState<FileListMessage | null>(null);
  const socketRef = useRef<EditorTransport | null>(null);
  const inputRef = useRef<HTMLTextAreaElement | null>(null);
  const commandInputRef = useRef<HTMLInputElement | null>(null);
  const nextId = useRef(1);
  const viewportRef = useRef<HTMLDivElement | null>(null);
  const gridRef = useRef<HTMLDivElement | null>(null);
  const cellProbeRef = useRef<HTMLSpanElement | null>(null);
  const snapshotRef = useRef<Snapshot | null>(null);
  const lastFileFocusRef = useRef<Snapshot["focus"] | null>(null);
  const viewportSizeRef = useRef("");

  const send = useCallback((message: Record<string, unknown>) => {
    const socket = socketRef.current;
    if (!socket || socket.readyState !== WebSocket.OPEN) return;
    const id = nextId.current++;
    socket.send(JSON.stringify({ v: 1, id, ...message }));
    return id;
  }, []);

  const focusEditor = useCallback(() => inputRef.current?.focus(), []);

  useEffect(() => {
    const token = new URLSearchParams(window.location.search).get("token") ?? "";
    const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
    const url = `${protocol}//${window.location.host}/ws?token=${encodeURIComponent(token)}`;
    let disposed = false;
    let pageHidden = false;
    let retryTimer: number | undefined;
    let retryAttempt = 0;

    const connect = () => {
      if (disposed || pageHidden) return;
      setConnection(retryAttempt === 0 ? "connecting" : "reconnecting");
      const socket = createEditorTransport(url);
      socketRef.current = socket;

      socket.onopen = () => {
        if (disposed || socketRef.current !== socket) return;
        retryAttempt = 0;
        viewportSizeRef.current = "";
        setConnection("online");
        setLastError("");
        window.setTimeout(focusEditor, 0);
      };
      socket.onclose = () => {
        if (disposed || socketRef.current !== socket) return;
        socketRef.current = null;
        if (browserWasm) {
          setConnection("offline");
          setLastError("Browser editor closed. Reload to open your workspace.");
          return;
        }
        retryAttempt = Math.min(retryAttempt + 1, 5);
        setConnection("reconnecting");
        setLastError("Web driver connection interrupted");
        const delay = Math.min(500 * (2 ** (retryAttempt - 1)), 5000);
        retryTimer = window.setTimeout(connect, delay);
      };
      socket.onerror = () => {
        if (!disposed && socketRef.current === socket) {
          if (browserWasm) setConnection("offline");
          setLastError(browserWasm ? "Browser editor stopped. Reload to reopen saved files." : "Web driver connection interrupted");
        }
      };
      socket.onmessage = (event) => {
        try {
          const message = (typeof event.data === 'string' ? JSON.parse(event.data) : event.data) as Snapshot | DriverMessage;
          if ("type" in message && message.type === "run-source") {
            setRunSource(message as unknown as RunSource);
          } else if ("type" in message && message.type === "interpreter") {
            setInterpreterAvailable(true);
          } else if ("type" in message && message.type === "storage") {
            const storage = message as DriverMessage & { state: string };
            setStorageState(storage.state === "error" ? storage.message ?? "Storage error" : storage.state === "saving" ? "Saving browser workspace…" : "Saved to browser workspace");
          } else if ("type" in message && message.type === "fatal") {
            setConnection("offline");
            setLastError(message.message ?? "Browser editor could not start");
          } else if ("mode" in message && "screen_rows" in message) {
            const nextSnapshot = message as Snapshot;
            if (snapshotRef.current?.buffer?.path !== nextSnapshot.buffer?.path) lastFileFocusRef.current = null;
            if (nextSnapshot.focus.zone === "filearea") lastFileFocusRef.current = nextSnapshot.focus;
            snapshotRef.current = nextSnapshot;
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

    const close = () => {
      if (retryTimer !== undefined) window.clearTimeout(retryTimer);
      retryTimer = undefined;
      const socket = socketRef.current;
      socketRef.current = null;
      if (socket) {
        socket.onclose = null;
        socket.close();
      }
    };
    // Navigating away can cache the page without unmounting the application.
    // Release control so READ cancels and a returning page can reconnect.
    const hide = () => {
      pageHidden = true;
      close();
    };
    const show = () => {
      if (!pageHidden) return;
      pageHidden = false;
      connect();
    };
    const beforeUnload = (event: BeforeUnloadEvent) => {
      if (browserWasm && snapshotRef.current?.buffers.some((buffer) => buffer.dirty)) {
        event.preventDefault();
        event.returnValue = "";
      }
    };
    window.addEventListener("pagehide", hide);
    window.addEventListener("pageshow", show);
    window.addEventListener("beforeunload", beforeUnload);
    connect();
    return () => {
      disposed = true;
      window.removeEventListener("pagehide", hide);
      window.removeEventListener("pageshow", show);
      window.removeEventListener("beforeunload", beforeUnload);
      close();
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

  const keyHintRows = useMemo(() => snapshot?.panel ? [] :
    (snapshot?.screen_rows ?? []).map((row) => ({ row, hints: keyHintsForRow(row) }))
      .filter((entry) => entry.hints.length > 0), [snapshot?.screen_rows, !!snapshot?.panel]);
  const keyHintRowIds = new Set(keyHintRows.map(({ row }) => row.r));
  const keyHints = keyHintRows.flatMap(({ hints }) => hints);

  useEffect(() => {
    const viewport = viewportRef.current;
    if (connection !== "online" || !viewport) return;
    let animation = 0;
    const measure = () => {
      cancelAnimationFrame(animation);
      animation = requestAnimationFrame(() => {
        const current = snapshotRef.current;
        const grid = gridRef.current;
        const probe = cellProbeRef.current;
        if (!current || current.panel || !grid || !probe) return;
        const style = getComputedStyle(grid);
        const cellWidth = probe.getBoundingClientRect().width / 10;
        const lineHeight = Number.parseFloat(style.lineHeight);
        if (!cellWidth || !lineHeight) return;
        const padding = Number.parseFloat(style.paddingTop) + Number.parseFloat(style.paddingBottom);
        // The browser draws only the file area; the model also reserves its
        // command/status/heading rows. Preserve that overhead on resize.
        const overhead = Math.max(0, current.viewport?.reserved_rows ?? 4);
        // Key-guide rows are shown in a wrapping footer, so their model rows
        // take no space in this grid. Fill the recovered rows with file text.
        const hintRows = current.screen_rows.filter((row) => keyHintsForRow(row).length > 0).length;
        const rows = Math.max(12, Math.min(256, Math.floor((viewport.clientHeight - padding) / lineHeight) + overhead + hintRows));
        const cols = Math.max(40, Math.min(1000, Math.floor((viewport.clientWidth - 20) / cellWidth)));
        const size = `${rows}:${cols}`;
        if (size !== viewportSizeRef.current || current.viewport?.rows !== rows || current.viewport?.cols !== cols) {
          viewportSizeRef.current = size;
          send({ type: "resize", rows, cols });
        }
      });
    };
    const observer = new ResizeObserver(measure);
    observer.observe(viewport);
    measure();
    return () => { observer.disconnect(); cancelAnimationFrame(animation); };
  }, [connection, send, snapshot?.buffer?.path, !!snapshot?.panel, keyHintRows.length]);

  const returnToFile = useCallback(() => {
    const current = snapshotRef.current?.focus;
    const saved = lastFileFocusRef.current;
    const target = current && (current.zone === "filearea" || current.zone === "prefix") ? current : saved;
    if (target) {
      const cell = target.zone === "prefix" ? (saved?.line === target.line ? saved.cell : 0) : target.cell;
      send({ type: "hit", target: "filearea", line: target.line, row: target.row, cell });
    }
    focusEditor();
  }, [focusEditor, send]);

  const onEditorKeyDown = useCallback(
    (event: KeyboardEvent) => {
      if (snapshot?.panel && (event.ctrlKey || event.metaKey)) {
        event.preventDefault();
        return;
      }
      const commandModifier = event.ctrlKey || event.metaKey;
      if (!snapshot?.panel && (event.key === "Escape" || event.key === "F6"
          || (commandModifier && event.key.toLowerCase() === "l"))) {
        event.preventDefault();
        commandInputRef.current?.focus();
        return;
      }
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
      if (commandModifier && ["f", "g"].includes(event.key.toLowerCase())) {
        event.preventDefault();
        send({ type: "key", key: `C-${event.key.toLowerCase()}` });
        return;
      }
      let key = /^F([1-9]|1[0-9]|2[0-4])$/.test(event.key) ? event.key.toLowerCase() : event.shiftKey && event.key === "Tab" ? "backtab" : keyNames[event.key];
      if (event.shiftKey && key && ["left", "right", "up", "down", "home", "end"].includes(key)) key = `shift-${key}`;
      if (key) {
        event.preventDefault();
        send({ type: "key", key });
      }
    },
    [requestAction, send, snapshot?.panel],
  );

  const rows = snapshot?.screen_rows ?? [];
  const currentBuffer = snapshot?.buffers.find((buffer) => buffer.current);
  const title = currentBuffer ? basename(currentBuffer.path) : "CRXTHE";
  const cursorRow = useMemo(
    () => rows.find((row) => row.r === snapshot?.focus.row && row.line === snapshot?.focus.line),
    [rows, snapshot?.focus.line, snapshot?.focus.row],
  );
  const prefixCols = snapshot?.viewport?.prefix_cols ?? 7;
  const prefixGap = snapshot?.viewport?.prefix_gap ?? 1;
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
        <div class="product-mark">CRXTHE</div>
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

      <div class="workbench">
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
        <div class="editor-scroll" ref={viewportRef} onClick={focusEditor}>
          {snapshot?.panel ? (
            <div class="fullscreen-panel" role="dialog" aria-modal="true" aria-label="Macro panel" style={panelColour(snapshot.panel.colour)}>
              <div class="panel-viewport">
              <div class="panel-surface" style={{ width: `${snapshot.panel.cols}ch`, height: `${snapshot.panel.rows * 21}px` }}>
                {snapshot.panel.fields.map((field, index) => (
                  <span
                    class={`panel-field ${field.editable ? "panel-editable" : "panel-protected"} ${snapshot.panel?.active === index ? "panel-focused" : ""}`}
                    style={{ ...panelColour(field.colour), top: `${field.row * 21}px`, left: `${field.cell}ch`, width: `${field.width}ch` }}
                    onClick={(event) => {
                      if (!field.editable) return;
                      const bounds = event.currentTarget.getBoundingClientRect();
                      const offset = Math.max(0, Math.min(field.width - 1,
                        Math.floor((event.clientX - bounds.left) * field.width / bounds.width)));
                      send({ type: "hit", target: "window", row: field.row, cell: field.cell + offset });
                    }}
                  >
                    {field.text}
                    {snapshot.panel?.active === index ? <span class="logical-cursor" style={{
                      left: `${snapshot.panel.cursor_cell ?? Math.min(snapshot.panel.cursor, field.width - 1)}ch`,
                      width: `${snapshot.panel.cursor_width ?? 1}ch`,
                    }} /> : null}
                  </span>
                ))}
              </div>
              </div>
              <div class="panel-controls">
                <span>Macro input · Tab changes field</span>
                <button type="button" onClick={() => send({ type: "key", key: "enter" })}>Enter</button>
                <select aria-label="Panel function key" onChange={(event) => { if (event.currentTarget.value) send({ type: "key", key: event.currentTarget.value }); event.currentTarget.value = ""; }}>
                  <option value="">Function key…</option>
                  {Array.from({ length: 24 }, (_, i) => <option value={`f${i + 1}`}>PF{i + 1}</option>)}
                  {Array.from({ length: 3 }, (_, i) => <option value={`pa${i + 1}`}>PA{i + 1}</option>)}
                </select>
                <button type="button" onClick={() => send({ type: "key", key: "esc" })}>Cancel</button>
              </div>
            </div>
          ) : null}
          <div class="editor-grid" ref={gridRef} style={{ width: `calc(${snapshot?.viewport?.cols ?? 80}ch + 20px)`, "--prefix-width": `${prefixCols + prefixGap}ch` }}>
            <span class="cell-probe" ref={cellProbeRef} aria-hidden="true">0000000000</span>
            {rows.map((row) => {
              const cursorVisible = cursorRow === row && snapshot?.focus.zone === "filearea" && !commandFocused;
              const prefixCursorVisible = cursorRow === row && snapshot?.focus.zone === "prefix" && !commandFocused;
              const prefixCursorCell = Math.max(0, Math.min((snapshot?.focus.cell ?? 0), prefixCols - 1));
              const cursorCell = Math.max(0, (snapshot?.focus.cell ?? 0) - row.sc);
              return (
                <div class={`screen-row role-${row.role} ${row.cur ? "screen-row-current" : ""} ${keyHintRowIds.has(row.r) ? "key-hint-row" : ""}`} key={`${currentBuffer?.path ?? ""}-${row.r}-${row.line}-${row.role}`}>
                  <button
                    type="button"
                    class={`prefix-cell ${prefixCursorVisible ? "prefix-cell-active" : ""}`}
                    aria-label={`Prefix line ${row.line}`}
                    style={{ paddingRight: `${prefixGap}ch` }}
                    tabIndex={-1}
                    onClick={(event) => hitRow(event, row, "prefix")}
                  >
                    {row.pc || row.p || ""}
                    {prefixCursorVisible ? <span class="logical-cursor prefix-cursor" style={{ left: `${prefixCursorCell}ch` }} /> : null}
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
      {browserWasm && interpreterAvailable ? <RexxConsole send={send} source={runSource} snapshot={snapshot} /> : null}
      </div>

      <footer class="command-area">
        {keyHints.length > 0 ? (
          <div class="key-hints" role="group" aria-label="Editor key hints">
            {keyHints.map((hint, index) => <span class="key-hint" key={`${hint.key}-${index}`}>
              <kbd>{hint.key}</kbd><span>{hint.label}</span>
            </span>)}
          </div>
        ) : null}
        {!snapshot?.panel ? <form class="command-form" onSubmit={(event) => { event.preventDefault(); runCommand(command); }}>
          <span class="command-prompt">Command</span>
          <input
            aria-label="THE command"
            ref={commandInputRef}
            onFocus={() => setCommandFocused(true)}
            onBlur={() => setCommandFocused(false)}
            onKeyDown={(event) => {
              if (event.key === "Escape") { event.preventDefault(); focusEditor(); }
              if (event.key === "F6") { event.preventDefault(); returnToFile(); }
            }}
            value={command}
            onInput={(event) => setCommand(event.currentTarget.value)}
            spellcheck={false}
            autoComplete="off"
          />
        </form> : null}
        <div class="status-line">
          <span class="status-text">{lastError || snapshot?.status || "Waiting for CRXTHE"}</span>
          {browserWasm ? <span class="workspace-storage" role="status">{storageState}</span> : null}
          {snapshot?.parser && snapshot.parser.state !== "inactive" ? <button class="parser-status" title="Restart the highlighter" onClick={() => send({type: "parser-restart"})}>
            {snapshot.parser.provider}: {snapshot.parser.state}
          </button> : null}
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
