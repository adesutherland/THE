import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {pathToFileURL} from 'node:url';
import {resolve} from 'node:path';
import {ByteInput} from '../vm/byte-input.mjs';
const directory = resolve(process.argv[2] ?? 'build-wasm-vm');
const {default: createVM} = await import(pathToFileURL(`${directory}/site/crxthe_rxvm.mjs`));
let input = new ByteInput();
const output = [];
const vm = await createVM({noInitialRun: true,
  vmStdinRead: capacity => input.read(capacity),
  vmStreamWrite: (fd, bytes) => output.push({fd, bytes: [...bytes]}),
});
function load(bytes) {
  const pointer = vm._malloc(bytes.length);
  assert.ok(pointer); vm.HEAPU8.set(bytes, pointer);
  try { return vm._the_vm_load(pointer, bytes.length); }
  finally { vm._free(pointer); }
}
const check = code => assert.equal(code, 0, vm.ccall('the_vm_error','string',[],[]));
const tick = () => new Promise(resolve => setTimeout(resolve, 10));
const images = await Promise.all(['library', 'stdio-probe'].map(name => readFile(`${directory}/site/modules/${name}.rxbin`)));
async function prepare() {
  check(vm._the_vm_create()); images.forEach(image => check(load(image)));
  input = new ByteInput(); output.length = 0;
}
await prepare();
let complete = false;
const running = vm.ccall('the_vm_run','number',['number','number'],[0,0],{async:true}).then(code => {complete=true;return code;});
await tick(); assert.equal(complete,false,'starvation must wait');
input.write(Uint8Array.of(0,0xce));
await tick(); assert.equal(complete,false,'partial input must wait');
input.write(Uint8Array.of(0xb1,10,255,65));
await tick(); assert.equal(complete,false,'open stream must not signal EOF');
input.close(); check(await running);
assert.equal(vm._the_vm_program_rc(),0);
assert.deepEqual(output.filter(p=>p.fd===1).flatMap(p=>p.bytes),[0,0xce,0xb1,10,255,65]);
assert.equal(new TextDecoder().decode(Uint8Array.from(output.filter(p=>p.fd===2).flatMap(p=>p.bytes))),'VM_STDIO_DONE');
check(vm._the_vm_destroy());
await prepare();
const failure = vm.ccall('the_vm_run','number',['number','number'],[0,0],{async:true});
input.fail(new Error('input transport refused'));
check(await failure);
assert.equal(vm._the_vm_program_rc(),2,'input failure is ferror, not clean EOF');
assert.match(vm.vmStreamError,/input transport refused/);
check(vm._the_vm_destroy());
console.log('PASS Wasm VM: RXBIN buffers, delayed binary stdin/stdout, stderr, explicit EOF and input failure');
