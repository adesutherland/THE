export interface EditorTransport {
  readonly readyState: number;
  onopen: (() => void) | null;
  onclose: (() => void) | null;
  onerror: (() => void) | null;
  onmessage: ((event: { data: string }) => void) | null;
  send(data: string): void;
  close(): void;
}

class WorkerTransport implements EditorTransport {
  readyState = 0;
  onopen: (() => void) | null = null;
  onclose: (() => void) | null = null;
  onerror: (() => void) | null = null;
  onmessage: ((event: { data: string }) => void) | null = null;
  private worker = new Worker(new URL("./wasm-worker.ts", import.meta.url), { type: "module" });

  constructor() {
    this.worker.onmessage = ({ data }) => {
      if (this.readyState === 3) return;
      if (data.type === "ready") {
        this.readyState = 1;
        this.onopen?.();
      } else if (data.type === "ended") {
        this.readyState = 3;
        this.worker.terminate();
        this.onclose?.();
      } else {
        this.onmessage?.({ data: JSON.stringify(data) });
        if (data.type === "fatal") {
          this.readyState = 3;
          this.worker.terminate();
        }
      }
    };
    this.worker.onerror = () => {
      this.readyState = 3;
      this.worker.terminate();
      this.onerror?.();
    };
  }

  send(data: string) { if (this.readyState === 1) this.worker.postMessage(JSON.parse(data)); }
  close() { this.readyState = 3; this.worker.terminate(); }
}

export const browserWasm = import.meta.env.VITE_THE_BACKEND === "wasm";

export function createEditorTransport(url: string): EditorTransport {
  return browserWasm ? new WorkerTransport() : new WebSocket(url) as unknown as EditorTransport;
}
