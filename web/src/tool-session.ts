export type ToolJob = {
  id: number; sourceId: number; revision: number;
  running: boolean; waiting: boolean; eof: boolean;
  state: string; status?: string; filename: string; tool: string;
};

export type ToolOutput = {
  job: ToolJob;
  omitted: boolean;
  chunks: { fd: number; text: string }[];
};

export type FileToolAction = {
  id: "build" | "run" | "activate";
  label: string;
  enabled: boolean;
  reason: string;
  tool: string;
  target: string;
  generation?: string;
};

export type ToolSession = ToolOutput & {
  type: "tools";
  version: number;
  visible: boolean;
  catalog: {
    id: string; label: string; role: string; image: string;
    extensions: string; available: boolean;
  }[];
  currentFile?: {
    sourceId: number; revision: number; path: string;
    actions: FileToolAction[];
  };
  profile?: {
    sourcePath: string;
    build: { sourceId: number; revision: number; image: string; state: "none" | "ready" | "stale" };
    active: { image: string };
  };
  fileResult?: {
    serial: number; action: string; sourceId: number; revision: number;
    code: string; rc: number; message: string;
  };
  build?: ToolOutput;
};
