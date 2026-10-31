export type StyleRun = [start: number, length: number, style: string];

export interface ScreenRow {
  r: number;
  role: string;
  footer?: string;
  footer_text?: string;
  footer_colour?: PanelColour;
  line: number;
  cur: number;
  sc: number;
  p?: string;
  pc?: string;
  t: string;
  s?: StyleRun[];
}

export interface Focus {
  zone: string;
  line: number;
  row: number;
  cell: number;
}

export interface BufferInfo {
  index: number;
  path: string;
  dirty: number;
  lines: number;
  current: number;
}

export interface Diagnostic {
  line: number;
  column: number;
  severity: string;
  code: string;
  message: string;
}

export interface PanelColour {
  foreground?: string;
  background?: string;
  style: number;
}

export type ConsoleColours = Partial<Record<"body" | "header" | "input" | "control" | "selected" | "divider" | "error" | "status", PanelColour>>;
export type DiagnosticColours = Partial<Record<"error" | "warning" | "information", PanelColour>>;

export interface Snapshot {
  console_colours?: ConsoleColours;
  diagnostic_colours?: DiagnosticColours;
  status_pmsg?: { text: string; severity: string; start: number; length: number; colour?: PanelColour } | null;
  source?: {id:number; revision:number};
  parser?: { provider: string; providerId?: string; state: string; id: number; revision: number; applied: number; stale: number };
  viewport?: { rows: number; cols: number; reserved_rows: number; prefix_cols: number; prefix_gap: number };
  panel?: {
    rows: number;
    cols: number;
    active: number;
    cursor: number;
    cursor_cell?: number;
    cursor_width?: number;
    colour?: PanelColour;
    fields: { row: number; cell: number; width: number; editable: number; changed: number; kind: string; text: string; colour?: PanelColour }[];
  };
  mode: string;
  rows: number;
  cols: number;
  focus: Focus;
  command: string;
  status: string;
  status_line?: string | null;
  last_message_serial?: number;
  last_message?: string;
  buffer?: {
    path: string;
    dirty: number;
    lines: number;
  };
  history: {
    undo: number;
    redo: number;
  };
  buffers: BufferInfo[];
  diagnostics?: Diagnostic[];
  screen_rows: ScreenRow[];
}

export interface DriverMessage {
  type: string;
  workbenchSession?: string;
  workbenchNextRequest?: number;
  id?: number;
  message?: string;
  actions?: FrontendActionDefinition[];
}

export interface FrontendActionDefinition {
  id: string;
  menu: string | null;
  label: string;
  requires_argument: boolean;
}

export interface WorkbenchActionDefinition {
  id: string;
  owner: string;
  label: string;
  generation: number;
  kind: "builtin" | "command" | "macro" | "panel";
  builtin: string;
  operand: string;
  panel?: string;
  operation?: "show" | "hide" | "focus";
  enabled: boolean;
  reason: string;
}

export interface WorkbenchMenuDefinition {
  id: string;
  owner: string;
  label: string;
  generation: number;
  order: number;
}

export interface WorkbenchItemDefinition {
  id: string;
  owner: string;
  label: string;
  generation: number;
  menu: string;
  action: string;
  order: number;
  separator: boolean;
}

export interface WorkbenchPanelDefinition {
  id: string;
  owner: string;
  title: string;
  generation: string;
  kind: "log" | "document";
  model: string;
  modelGeneration: string;
  placement: "right" | "below" | "tab";
  fallbacks: Array<"right" | "below" | "tab">;
  mincols: number;
  minrows: number;
  visible: boolean;
  focused: boolean;
  effectivePlacement: "none" | "right" | "below" | "tab";
  reason: string;
  contentRevision: string;
  modelError: string;
  readonly?: boolean;
  documentId?: string;
  documentRevision?: string;
  documentAvailable?: boolean;
  documentReason?: string;
  lineCount?: number;
}

export interface WorkbenchPanelState {
  id: string;
  generation: string;
  visible: boolean;
  focused: boolean;
  effectivePlacement: "none" | "right" | "below" | "tab";
  reason: string;
  model: string;
  modelGeneration: string;
  contentRevision: string;
  modelError: string;
  omittedBytes: string;
  omittedRecords: string;
  kind?: "log" | "document";
  readonly?: boolean;
  documentId?: string;
  documentRevision?: string;
  documentAvailable?: boolean;
  documentReason?: string;
  lineCount?: number;
}

export interface WorkbenchDocumentPage {
  schemaVersion: 1;
  revision: number;
  section: "document";
  panel: string;
  panelGeneration: string;
  documentId: string;
  documentRevision: string;
  lineCount: number;
  dirty: boolean;
  nextLine: number;
  nextOffset: number;
  more: boolean;
  records: Array<{ line: number; offset: number; length: number; originalLength: number; text: string; bytesHex: string }>;
}

export interface WorkbenchFocusRequest {
  serial: string;
  panel: string;
  generation: string;
  previousPanel: string;
  previousGeneration: string;
  restoring: boolean;
}

export interface WorkbenchPanelStateMessage {
  type: "workbench";
  operation: "panel-state";
  revision: number;
  stateRevision: string;
  focus: WorkbenchFocusRequest;
  panels: WorkbenchPanelState[];
}

export interface WorkbenchLogRecord {
  cursor: string;
  job: string;
  stream: number;
  sequence: string;
  offset: number;
  length: number;
  originalLength: number;
  text: string;
}

export interface WorkbenchLogPage {
  schemaVersion: 1;
  revision: number;
  section: "log";
  id: string;
  generation: string;
  contentRevision: string;
  firstCursor: string;
  nextCursor: string;
  nextOffset: number;
  more: boolean;
  omitted: boolean;
  omittedBytes: string;
  omittedRecords: string;
  records: WorkbenchLogRecord[];
}

export interface WorkbenchPage {
  schemaVersion: 1;
  revision: number;
  section: "actions" | "menus" | "items" | "panels";
  total: number;
  offset: number;
  nextOffset: number | null;
  records: WorkbenchActionDefinition[] | WorkbenchMenuDefinition[] | WorkbenchItemDefinition[] | WorkbenchPanelDefinition[];
}

export interface WorkspaceRoot {
  id: number;
  name: string;
  readonly: boolean;
}

export interface WorkspaceEntry {
  name: string;
  path: string;
  target: string;
  type: "directory" | "file";
  readonly: boolean;
}

export interface FileListMessage {
  type: "files";
  id: number;
  root: number;
  path: string;
  roots: WorkspaceRoot[];
  entries: WorkspaceEntry[];
}
