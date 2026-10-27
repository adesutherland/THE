import { CrexxVMProcess } from "./vm-client.mjs";

const flags = [
  "hasStatus",
  "hasValue",
  "hasPrimary",
  "hasCleanup",
  "truncated",
];
const decodeHex = (value) => {
  if (
    typeof value !== "string" ||
    value.length % 2 ||
    !/^[0-9A-F]*$/.test(value)
  )
    throw new Error("Invalid interpreter text encoding");
  const bytes = new Uint8Array(value.length / 2);
  const nibble = (code) => (code <= 57 ? code - 48 : code - 55);
  for (let i = 0; i < bytes.length; i++)
    bytes[i] =
      nibble(value.charCodeAt(i * 2)) * 16 +
      nibble(value.charCodeAt(i * 2 + 1));
  return new TextDecoder().decode(bytes);
};
export function parseRunResult(wire) {
  if (
    typeof wire !== "string" ||
    wire.length > 3145728 ||
    !wire.startsWith("LLB1\n") ||
    !wire.endsWith("\n")
  )
    throw new Error("Invalid interpreter completion envelope");
  const fields = Object.create(null);
  for (const line of wire.slice(5, -1).split("\n")) {
    const split = line.indexOf("=");
    const key = line.slice(0, split);
    if (split < 1 || Object.hasOwn(fields, key))
      throw new Error("Duplicate or invalid interpreter result field");
    fields[key] = line.slice(split + 1);
  }
  for (const key of flags)
    if (!/^[01]$/.test(fields[key]))
      throw new Error(`Invalid interpreter ${key}`);
  for (const key of [
    "status",
    "primaryCode",
    "primaryModule",
    "primaryAddress",
    "cleanupCode",
    "cleanupModule",
    "cleanupAddress",
  ])
    if (fields[key]?.length > 20 || !/^-?\d+$/.test(fields[key]))
      throw new Error(`Invalid interpreter ${key}`);
  if (!/^[NREXS]$/.test(fields.indicator))
    throw new Error("Invalid interpreter completion indicator");
  if (
    (fields.hasPrimary === "1") !== (fields.hasStatus === "0") ||
    /^[NR]$/.test(fields.indicator) !== (fields.hasPrimary === "0")
  )
    throw new Error("Inconsistent interpreter completion");
  const texts = Object.create(null);
  for (const key of [
    "valueHex",
    "primaryNameHex",
    "primaryMessageHex",
    "cleanupNameHex",
    "cleanupMessageHex",
  ]) {
    if (fields[key]?.length > (key === "valueHex" ? 2097152 : 131072))
      throw new Error("Interpreter text exceeds its field limit");
    texts[key] = decodeHex(fields[key]);
  }
  const condition = (prefix) =>
    fields[`has${prefix[0].toUpperCase() + prefix.slice(1)}`] === "1"
      ? {
          name: texts[`${prefix}NameHex`],
          message: texts[`${prefix}MessageHex`],
          code: fields[`${prefix}Code`],
          module: fields[`${prefix}Module`],
          address: fields[`${prefix}Address`],
        }
      : undefined;
  return {
    indicator: fields.indicator,
    status: fields.hasStatus === "1" ? fields.status : undefined,
    value: fields.hasValue === "1" ? texts.valueHex : undefined,
    primary: condition("primary"),
    cleanup: condition("cleanup"),
    truncated: fields.truncated === "1",
  };
}

let imagePromise;
async function interpreterImage() {
  if (!imagePromise)
    imagePromise = (async () => {
      const metadata = await fetch(
        new URL("./runner/interpreter.json", import.meta.url),
      );
      if (!metadata.ok) throw new Error("Interpreter metadata unavailable");
      const manifest = await metadata.json();
      const response = await fetch(
        new URL("./runner/llcbrowser.rxbin", import.meta.url),
      );
      if (!response.ok) throw new Error("Interpreter bytecode unavailable");
      const image = new Uint8Array(await response.arrayBuffer());
      const digest = [
        ...new Uint8Array(await crypto.subtle.digest("SHA-256", image)),
      ]
        .map((v) => v.toString(16).padStart(2, "0"))
        .join("");
      if (manifest.version !== 1 || digest !== manifest.sha256)
        throw new Error("Interpreter image does not match its manifest");
      return image;
    })().catch((error) => {
      imagePromise = undefined;
      throw error;
    });
  return imagePromise;
}

export async function startRexx({
  source,
  filename,
  arguments: args,
  stdout,
  stderr,
  inputState,
  signal,
}) {
  const image = await interpreterImage();
  if (signal.aborted) throw signal.reason;
  return CrexxVMProcess.start({
    images: [image],
    program: { source, filename, arguments: args },
    stdout,
    stderr,
    inputState,
    signal,
    outputLimit: 8388608,
    workerUrl: new URL("./runner/vm-worker.mjs", import.meta.url),
  });
}
