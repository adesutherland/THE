import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { resolve } from "node:path";
import { pathToFileURL } from "node:url";
import { ByteInput } from "../vm/byte-input.mjs";
import { parseRunResult } from "../vm/run-host.mjs";

const site = resolve(process.argv[2] ?? "build/wasm-review-candidate/site");
const { default: createVM } = await import(pathToFileURL(`${site}/vm/runner/crxthe_rxvm.mjs`));
const readExample = name => readFile(`${site}/examples/${name}`);

function check(vm, code) {
  assert.equal(code, 0, vm.ccall("the_vm_error", "string", [], []));
}
function copy(vm, bytes) {
  const pointer = vm._malloc(Math.max(1, bytes.length));
  assert.ok(pointer);
  vm.HEAPU8.set(bytes, pointer);
  return pointer;
}
function argumentsBlock(vm, values) {
  const bytes = values.map(value => new TextEncoder().encode(value + "\0"));
  const size = values.length * 4 + bytes.reduce((sum, part) => sum + part.length, 0);
  const pointer = vm._malloc(Math.max(1, size));
  assert.ok(pointer);
  let offset = values.length * 4;
  bytes.forEach((part, index) => {
    vm.HEAPU32[(pointer >>> 2) + index] = pointer + offset;
    vm.HEAPU8.set(part, pointer + offset);
    offset += part.length;
  });
  return pointer;
}
async function execute(image, { source, args = [], stdin = "", classic = false } = {}) {
  const input = new ByteInput();
  if (stdin) input.write(new TextEncoder().encode(stdin));
  input.close();
  const output = [], errors = [];
  const vm = await createVM({
    noInitialRun: true,
    vmStdinRead: capacity => input.read(capacity),
    vmStreamWrite: (fd, bytes) => (fd === 1 ? output : errors).push(Buffer.from(bytes)),
  });
  check(vm, vm._the_vm_create());
  if (!classic) {
    const library = await readFile(`${site}/compiler/library.rxbin`);
    const libraryPointer = copy(vm, library);
    check(vm, vm._the_vm_load(libraryPointer, library.length));
    vm._free(libraryPointer);
  }
  const imageBytes = image === "llcbrowser.rxbin"
    ? await readFile(`${site}/vm/runner/${image}`) : await readExample(image);
  const imagePointer = copy(vm, imageBytes);
  check(vm, vm._the_vm_load(imagePointer, imageBytes.length));
  vm._free(imagePointer);
  vm.FS.mkdir("/run"); vm.FS.chdir("/run");
  let sourcePointer, filenamePointer;
  let values = args;
  if (source && !classic) {
    vm.FS.writeFile(source, await readExample(source));
    values = [source, ...args];
  }
  const argPointer = argumentsBlock(vm, values);
  let code, result;
  if (classic) {
    const sourceBytes = await readExample(source);
    const filename = new TextEncoder().encode(source);
    sourcePointer = copy(vm, sourceBytes);
    filenamePointer = copy(vm, filename);
    code = await vm.ccall("the_vm_interpret", "number",
      ["number", "number", "number", "number", "number", "number"],
      [sourcePointer, sourceBytes.length, filenamePointer, filename.length, values.length, argPointer],
      { async: true });
    if (code === 0) result = parseRunResult(vm.ccall("the_vm_result", "string", [], []));
  } else {
    code = await vm.ccall("the_vm_run", "number", ["number", "number"],
      [values.length, argPointer], { async: true });
  }
  check(vm, code);
  const programCode = vm._the_vm_program_rc();
  check(vm, vm._the_vm_destroy());
  for (const pointer of [argPointer, sourcePointer, filenamePointer]) if (pointer) vm._free(pointer);
  return { stdout: Buffer.concat(output).toString(), stderr: Buffer.concat(errors).toString(),
    programCode, result };
}

let run = await execute("LevelGAccumulator.rxbin");
assert.equal(run.programCode, 0);
assert.match(run.stdout, /after 5 squares: 55\nsum of squares: 55\n$/);
run = await execute("LevelGFibonacci.rxbin");
assert.equal(run.programCode, 0);
assert.match(run.stdout, /10 : 55\n$/);

run = await execute("llcbrowser.rxbin", { source: "RexxWords.rexx", classic: true });
assert.equal(run.result?.status, "0");
assert.match(run.stdout, /Words: 6\n/);
run = await execute("llcbrowser.rxbin", { source: "RexxLedger.rexx", args: ["40"], classic: true });
assert.equal(run.result?.status, "0");
assert.match(run.stdout, /Entry 3 change 12 balance 60\n/);

run = await execute("MiniBASIC.rxbin", { source: "MiniBASICPrimes.bas" });
assert.equal(run.programCode, 0);
assert.equal(run.stdout, "2\n3\n5\n7\n11\n13\n17\n19\n23\n29\n");
run = await execute("MiniBASIC.rxbin", { source: "MiniBASICStrings.bas" });
assert.equal(run.programCode, 0);
assert.equal(run.stdout, 'direct "quote"\nassigned "quote"\nassigned "quote"\nassigned "quote"\nleft  right\nleft  right\n\n');
console.log("PASS browser Wasm example programs: Level G RXBIN, Classic Rexx source and MiniBASIC source");
