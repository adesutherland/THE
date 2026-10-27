/** Generic process-style RXBIN host; usable from a page or the editor Worker. */
export class CrexxVMProcess {
  static async start({images, arguments: args = [], stdout = () => {}, stderr = () => {},
                      program, inputState = () => {}, signal, outputLimit,
                      workerUrl = new URL('./vm-worker.mjs', import.meta.url)}) {
    const process = new CrexxVMProcess(workerUrl, stdout, stderr, inputState);
    if (signal) {
      const abort = () => process.terminate(signal.reason ?? new Error('Stopped'));
      signal.addEventListener('abort', abort, {once: true});
      process.removeAbort = () => signal.removeEventListener('abort', abort);
      if (signal.aborted) abort();
    }
    try {
      await process.request({type: 'init', images, outputLimit});
      await process.request(program ? {type: 'interpret', ...program} : {type: 'run', arguments: args});
      return process;
    } catch (error) { process.terminate(error); throw error; }
  }
  constructor(url, stdout, stderr, inputState = () => {}) {
    this.worker = new Worker(url, {type: 'module'});
    this.pending = new Map(); this.nextId = 1;
    this.exit = new Promise((resolve, reject) => { this.resolveExit = resolve; this.rejectExit = reject; });
    // Initialization may fail before start() returns the exit promise to its caller.
    this.exit.catch(() => {});
    this.worker.onmessage = ({data}) => {
      if (this.closed) return;
      if (data.type === 'stream') {
        try { (data.fd === 1 ? stdout : stderr)(data.bytes); }
        catch (error) { this.terminate(error); }
      }
      else if (data.type === 'input-state') inputState(data.waiting);
      else if (data.type === 'exit') {
        this.resolveExit(data); this.finish(new Error('VM has exited'));
      } else if (data.type === 'reply' || data.type === 'error') {
        const request = this.pending.get(data.id); this.pending.delete(data.id);
        if (data.type === 'reply') request?.resolve();
        else {
          const error = new Error(data.message); request?.reject(error);
          if (data.fatal) this.terminate(error);
        }
      }
    };
    this.worker.onerror = event => this.terminate(new Error(event.message || 'VM Worker failed'));
    this.worker.onmessageerror = () => this.terminate(new Error('Invalid VM Worker response'));
  }
  request(message) {
    if (this.closed) return Promise.reject(new Error('VM is closed'));
    const id = this.nextId++;
    return new Promise((resolve, reject) => {
      this.pending.set(id, {resolve, reject});
      try { this.worker.postMessage({...message, id}); }
      catch (error) { this.pending.delete(id); reject(error); }
    });
  }
  write(bytes) { return this.request({type: 'stdin', bytes}); }
  endInput() { return this.request({type: 'eof'}); }
  failInput(message) { return this.request({type: 'stdin-error', message}); }
  finish(error) {
    this.removeAbort?.();
    this.closed = true; this.worker.terminate();
    for (const request of this.pending.values()) request.reject(error);
    this.pending.clear();
  }
  terminate(error = new Error('VM terminated')) { this.rejectExit(error); this.finish(error); }
}
