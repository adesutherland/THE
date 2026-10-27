import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { resolve } from "node:path";
import { pathToFileURL } from "node:url";
import { ByteInput } from "../vm/byte-input.mjs";
import { parseRunResult } from "../vm/run-host.mjs";

const directory = resolve(process.argv[2] ?? "build-wasm-rexx/runner");
const { default: createVM } = await import(
  pathToFileURL(`${directory}/crxthe_rxvm.mjs`)
);
const image = await readFile(`${directory}/llcbrowser.rxbin`);
const metadata = JSON.parse(
  await readFile(`${directory}/interpreter.json`, "utf8"),
);
const { createHash } = await import("node:crypto");
assert.equal(createHash("sha256").update(image).digest("hex"), metadata.sha256);

async function execute(source, args = [], feed) {
  const input = new ByteInput(),
    output = [],
    errors = [];
  let reading;
  const waiting = new Promise((resolve) => {
    reading = resolve;
  });
  const vm = await createVM({
    noInitialRun: true,
    vmStdinRead: (n) => {
      reading();
      return input.read(n);
    },
    vmStreamWrite: (fd, bytes) =>
      (fd === 1 ? output : errors).push(Buffer.from(bytes)),
  });
  const check = (n) =>
    assert.equal(n, 0, vm.ccall("the_vm_error", "string", [], []));
  check(vm._the_vm_create());
  const copy = (bytes) => {
    const pointer = vm._malloc(Math.max(1, bytes.length));
    vm.HEAPU8.set(bytes, pointer);
    return pointer;
  };
  const imagePointer = copy(image);
  check(vm._the_vm_load(imagePointer, image.length));
  vm._free(imagePointer);
  vm.FS.mkdir("/run");
  vm.FS.chdir("/run");
  const name = new TextEncoder().encode("memory/α.rexx"),
    bytes =
      typeof source === "string" ? new TextEncoder().encode(source) : source;
  const src = copy(bytes),
    filename = copy(name),
    block = vm._malloc(Math.max(1, args.length * 4)),
    argumentsAllocated = [];
  args.forEach((value, i) => {
    const p = copy(new TextEncoder().encode(value + "\0"));
    argumentsAllocated.push(p);
    vm.HEAPU32[(block >>> 2) + i] = p;
  });
  let finished = false;
  const promise = vm
    .ccall(
      "the_vm_interpret",
      "number",
      ["number", "number", "number", "number", "number", "number"],
      [src, bytes.length, filename, name.length, args.length, block],
      { async: true },
    )
    .then((code) => {
      finished = true;
      return code;
    });
  if (feed) await feed({ input, waiting, output, finished: () => finished });
  else input.close();
  const code = await promise;
  const error = vm.ccall("the_vm_error", "string", [], []),
    wire = vm.ccall("the_vm_result", "string", [], []);
  check(vm._the_vm_destroy());
  for (const p of [src, filename, block, ...argumentsAllocated]) vm._free(p);
  return {
    code,
    error,
    wire,
    result: code ? undefined : parseRunResult(wire),
    stdout: Buffer.concat(output).toString(),
    stderr: Buffer.concat(errors).toString(),
  };
}

let run = await execute(
  "numeric digits 20\ns.1=2\ns.2=3\nt=0\ndo i=1 to 2\nt=t+s.i\nend\ninterpret \"say 'dynamic'\"\nsay reverse('αβ') t\nsay greet('Ada')\nexit 7\ngreet: procedure\nparse arg name\nreturn 'Hello '||name\n",
);
assert.equal(run.code, 0, run.error);
assert.equal(run.result.status, "7");
assert.equal(run.result.primary, undefined);
assert.match(run.stdout, /dynamic\nβα 5\nHello Ada\n/);
run = await execute("numeric digits 20\nexit 9223372036854775807\n");
assert.equal(run.result.status, "9223372036854775807");
run = await execute(
  "say arg()\nsay '['||arg(1)||']['||arg(2)||']['||arg(3)||']'\nreturn 0\n",
  ["first value", "", "-last"],
);
assert.equal(run.result.status, "0");
assert.equal(run.stdout, "3\n[first value][][-last]\n");
run = await execute(
  "say 'Your name?'\nparse pull name\nsay 'Hello' name\nreturn 0\n",
  [],
  async ({ input, waiting, output, finished }) => {
    await waiting;
    assert.equal(finished(), false);
    assert.match(
      Buffer.concat(output).toString(),
      /Your name\?/,
      "prompt must arrive before stdin completes",
    );
    input.write(new TextEncoder().encode("Zoë\n"));
    const deadline = Date.now() + 10000;
    while (!finished() && Date.now() < deadline)
      await new Promise((resolve) => setTimeout(resolve, 10));
    assert.ok(finished(), "one input line must complete without closing stdin");
  },
);
assert.equal(run.code, 0, run.error);
assert.equal(run.stdout, "Your name?\nHello Zoë\n");
run = await execute("parse pull value\nsay 'EOF:' value\nexit 0\n");
assert.equal(run.result.status, "0");
assert.equal(run.stdout, "EOF: \n");
run = await execute("say [\n");
assert.ok(run.result.primary, JSON.stringify(run));
assert.equal(run.result.status, undefined);
run = await execute("say 1/0\n");
assert.ok(run.result.primary, JSON.stringify(run));
assert.equal(run.result.status, undefined);
run = await execute("call not_available_external_program\n");
assert.ok(run.result.primary, "external source search must be refused");
run = await execute(
  "call lineout 'temporary.txt','private'\ncall lineout 'temporary.txt'\nsay linein('temporary.txt')\nexit 0\n",
);
assert.equal(run.code, 0, run.error);
assert.equal(run.stdout, "private\n");
assert.equal(run.result.status, "0");
run = await execute("say stream('temporary.txt','c','query exists')\nexit 0\n");
assert.equal(run.stdout, "\n", "new execution must not inherit private files");
run = await execute(Uint8Array.of(255));
assert.notEqual(run.code, 0);
assert.match(run.error, /UTF-8/);
assert.throws(() => parseRunResult("LLB1\nindicator=N\n"), /Invalid/);
console.log(
  "PASS actual Wasm Classic interpreter: linked image, routines/stems/loops/INTERPRET, Unicode, separate argument positions, delayed stdin, EOF, typed failures, private files and UTF-8 admission",
);
