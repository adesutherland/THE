import {ByteInput} from './byte-input.mjs';
import {CrexxVMProcess} from './vm-client.mjs';

const encoder = new TextEncoder();
const decoder = new TextDecoder('utf-8', {fatal: true});
const FRAME_LIMIT = 8 * 1024 * 1024;

export class FramedInput {
  constructor() { this.input = new ByteInput(2 * FRAME_LIMIT); }
  write(bytes) { this.input.write(bytes); }
  fail(error) { this.input.fail(error); }
  async exact(length) {
    const result = new Uint8Array(length);
    let offset = 0;
    while (offset < length) {
      const bytes = await this.input.read(length - offset);
      if (bytes === null) throw new Error('Incomplete parser frame');
      result.set(bytes, offset); offset += bytes.length;
    }
    return result;
  }
  async reply() {
    const header = decoder.decode(await this.exact(8));
    if (!/^[0-9a-fA-F]{8}$/.test(header)) throw new Error('Invalid parser frame header');
    const length = Number.parseInt(header, 16);
    if (!length || length > FRAME_LIMIT) throw new Error('Parser frame exceeds limit');
    return decoder.decode(await this.exact(length));
  }
}

function hex(bytes) {
  const result = new Array(bytes.length);
  for (let i = 0; i < bytes.length; i++) result[i] = bytes[i].toString(16).padStart(2, '0');
  return result.join('');
}

function frame(job) {
  // One service mirror is replaced for each ordered full snapshot. The host
  // retains the real editor document/revision envelope until its reply arrives.
  const payload = encoder.encode(`I|${job.revision}|${job.lines}|656469746f72|${hex(job.source)}|`);
  if (payload.length > FRAME_LIMIT) throw new Error('Source exceeds parser frame limit');
  const result = new Uint8Array(payload.length + 8);
  result.set(encoder.encode(payload.length.toString(16).padStart(8, '0')));
  result.set(payload, 8);
  return result;
}

/** Transport queue only: source, identity and revision always come from C. */
export class ClassicHighlighter {
  constructor(deliver, {base = new URL('./', import.meta.url), timeout = 10000} = {}) {
    this.deliver = deliver; this.base = base; this.timeout = timeout;
    this.pending = new Map(); this.open = new Set(); this.generation = 0;
  }
  submit(job) {
    this.open.add(job.id);
    this.pending.set(job.id, job); // Coalesce unsent revisions of a document.
    if (!this.running) void this.pump();
  }
  forget(id) { this.open.delete(id); this.pending.delete(id); }
  reset() {
    this.generation++;
    this.process?.terminate(); this.process = undefined;
    this.pending.clear(); this.running = false;
  }
  async image() {
    if (!this.imagePromise) this.imagePromise = (async () => {
      const metadata = await (await fetch(new URL('classic.json', this.base))).json();
      const response = await fetch(new URL('modules/dslsh-classic.rxbin', this.base));
      if (!response.ok) throw new Error('Classic parser image unavailable');
      const bytes = new Uint8Array(await response.arrayBuffer());
      const digest = hex(new Uint8Array(await crypto.subtle.digest('SHA-256', bytes)));
      if (digest !== metadata.sha256) throw new Error('Classic parser image checksum mismatch');
      return bytes;
    })().catch(error => { this.imagePromise = undefined; throw error; });
    return this.imagePromise;
  }
  async start(generation) {
    if (generation !== this.generation) throw new Error('Parser reset');
    if (this.process) return {process: this.process, reader: this.reader};
    const reader = new FramedInput();
    const image = await this.image();
    if (generation !== this.generation) throw new Error('Parser reset');
    const errors = new TextDecoder();
    const process = await CrexxVMProcess.start({images: [image], arguments: ['--stdio'],
      workerUrl: new URL('vm-worker.mjs', this.base),
      stdout: bytes => reader.write(bytes),
      stderr: bytes => console.warn(errors.decode(bytes, {stream: true})),
    });
    if (generation !== this.generation) { process.terminate(); throw new Error('Parser reset'); }
    process.exit.then(result => reader.fail(new Error(`Parser exited: ${result.programCode}/${result.runtimeCode}`)), error => reader.fail(error));
    this.reader = reader; this.process = process;
    return {process, reader};
  }
  async pump() {
    const generation = this.generation;
    this.running = true;
    while (this.pending.size && generation === this.generation) {
      const [id, job] = this.pending.entries().next().value;
      this.pending.delete(id);
      let timer;
      try {
        const operation = (async () => {
          const session = await this.start(generation);
          if (generation !== this.generation) throw new Error('Parser reset');
          await session.process.write(frame(job));
          return session.reader.reply();
        })();
        const payload = await Promise.race([operation, new Promise((_, reject) => {
          timer = setTimeout(() => reject(new Error('Parser request timed out')), this.timeout);
        })]);
        if (generation === this.generation && this.open.has(id))
          this.deliver({type: 'parser-result', id, revision: job.revision, payload, failed: 0});
      } catch (error) {
        if (generation !== this.generation) break;
        this.process?.terminate(error); this.process = undefined;
        if (this.open.has(id)) this.deliver({type: 'parser-result', id, revision: job.revision, payload: '', failed: 1});
        // Invalidate an operation still awaiting fetch/start after a timeout.
        this.generation++; this.running = false;
        if (this.pending.size) void this.pump();
        return;
      } finally { clearTimeout(timer); }
    }
    if (generation === this.generation) this.running = false;
  }
}
