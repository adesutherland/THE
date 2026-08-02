export type StyleRun = [start: number, length: number, style: string];

export interface ScreenRow {
  r: number;
  role: string;
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

export interface Snapshot {
  mode: string;
  rows: number;
  cols: number;
  focus: Focus;
  command: string;
  status: string;
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
