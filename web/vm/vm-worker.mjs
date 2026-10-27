import { ByteInput } from './byte-input.mjs';

let vm, input = new ByteInput(), phase = 'new';
const send = message => self.postMessage(message);
const stream = (fd, bytes) => self.postMessage({type: 'stream', fd, bytes}, [bytes.buffer]);
const check = result => {
  if (result !== 0) throw new Error(vm.ccall('the_vm_error', 'string', [], []));
};
function copy(bytes) {
  const pointer = vm._malloc(Math.max(1, bytes.length));
  if (!pointer) throw new Error('Unable to allocate VM transfer');
  vm.HEAPU8.set(bytes, pointer);
  return pointer;
}
function argumentsBlock(values) {
  if (!Array.isArray(values) || values.length > 128 || values.some(v => typeof v !== 'string' || v.includes('\0')))
    throw new Error('Invalid VM arguments');
  const encoded = values.map(value => new TextEncoder().encode(value));
  const length = encoded.reduce((n, value) => n + value.length + 1, values.length * 4);
  if (length > 1048576) throw new Error('VM arguments exceed the host budget');
  const pointer = vm._malloc(Math.max(1, length));
  if (!pointer) throw new Error('Unable to allocate VM arguments');
  let offset = values.length * 4;
  encoded.forEach((bytes, index) => {
    vm.HEAPU32[(pointer >>> 2) + index] = pointer + offset;
    vm.HEAPU8.set(bytes, pointer + offset);
    vm.HEAPU8[pointer + offset + bytes.length] = 0;
    offset += bytes.length + 1;
  });
  return pointer;
}

self.onmessage = async ({data}) => {
  const id = data?.id;
  let fatal = false;
  try {
    if (!data || typeof data.type !== 'string') throw new Error('Invalid VM message');
    if (data.type === 'init') {
      if (phase !== 'new') throw new Error('VM already initialized');
      phase = 'initializing';
      fatal = true;
      if (!Array.isArray(data.images) || !data.images.length) throw new Error('No RXBIN images');
      const moduleUrl = new URL('./crxthe_rxvm.mjs', import.meta.url);
      const {default: createVM} = await import(moduleUrl.href);
      const encode = text => new TextEncoder().encode(text + '\n');
      vm = await createVM({
        noInitialRun: true,
        locateFile: name => new URL(name, moduleUrl).href,
        vmStdinRead: capacity => input.read(capacity), vmStreamWrite: stream,
        print: text => stream(1, encode(text)), printErr: text => stream(2, encode(text)),
      });
      check(vm._the_vm_create());
      for (const image of data.images) {
        if (!(image instanceof Uint8Array)) throw new Error('RXBIN images require byte arrays');
        const pointer = copy(image);
        try { check(vm._the_vm_load(pointer, image.length)); }
        finally { vm._free(pointer); }
      }
      phase = 'ready'; fatal = false; send({type: 'reply', id});
    } else if (data.type === 'run') {
      if (phase !== 'ready') throw new Error('VM is not ready to run');
      const pointer = argumentsBlock(data.arguments ?? []);
      fatal = true;
      phase = 'running'; send({type: 'reply', id});
      try {
        const runtimeCode = await vm.ccall('the_vm_run', 'number', ['number', 'number'],
          [(data.arguments ?? []).length, pointer], {async: true});
        const programCode = vm._the_vm_program_rc();
        const error = runtimeCode ? vm.ccall('the_vm_error', 'string', [], []) : vm.vmStreamError;
        check(vm._the_vm_destroy()); phase = 'ended';
        send({type: 'exit', runtimeCode, programCode, error});
      } finally { vm._free(pointer); }
    } else if (['stdin', 'eof', 'stdin-error'].includes(data.type)) {
      if (!['ready', 'running'].includes(phase)) throw new Error('VM input is unavailable');
      // No C calls here: the only VM owner may be suspended inside fread.
      if (data.type === 'stdin') input.write(data.bytes);
      else if (data.type === 'eof') input.close();
      else input.fail(new Error(String(data.message ?? 'Input failed')));
      send({type: 'reply', id});
    } else throw new Error('Unknown VM message');
  } catch (error) {
    send({type: 'error', id, message: error?.message ?? String(error), fatal});
    if (fatal) { phase = 'failed'; self.close(); }
  }
};
