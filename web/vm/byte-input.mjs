/** A byte pipe: temporary starvation is a wait; close means EOF after drain. */
export class ByteInput {
  constructor(limit = 16 * 1024 * 1024) { this.limit = limit; this.chunks = []; this.bytes = 0; }
  write(bytes) {
    if (this.closed) throw new Error('stdin is closed');
    if (!(bytes instanceof Uint8Array)) throw new Error('stdin requires bytes');
    if (bytes.length > this.limit - this.bytes) throw new Error('stdin queue is full');
    if (bytes.length) { this.chunks.push(bytes.slice()); this.bytes += bytes.length; }
    this.wake?.(); this.wake = undefined;
  }
  close() { this.closed = true; this.wake?.(); this.wake = undefined; }
  fail(error) { this.failure = error; this.chunks = []; this.bytes = 0; this.close(); }
  async read(capacity) {
    if (!Number.isSafeInteger(capacity) || capacity < 1) throw new Error('Invalid read capacity');
    while (!this.chunks.length && !this.closed) await new Promise(resolve => { this.wake = resolve; });
    if (this.failure) throw this.failure;
    if (!this.chunks.length) return null;
    const first = this.chunks[0], result = first.subarray(0, capacity);
    if (result.length === first.length) this.chunks.shift();
    else this.chunks[0] = first.subarray(result.length);
    this.bytes -= result.length;
    return result;
  }
}
