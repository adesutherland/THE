// A compiler is a fresh command-line Module per invocation, owned by one job.
self.onmessage = async ({ data }) => {
  try {
    let assembly;
    for (const name of ["rxc", "rxas"]) {
      const base = new URL(/* @vite-ignore */ "../compiler/", import.meta.url);
      const url = new URL(name + "_single.mjs", base);
      const { default: create } = await import(/* @vite-ignore */ url.href);
      let status = 0;
      const module = await create({
        noInitialRun: true,
        locateFile: (n) => new URL(n, url).href,
        print: (s) =>
          self.postMessage({
            type: "output",
            fd: 1,
            bytes: new TextEncoder().encode(s + "\n"),
          }),
        printErr: (s) =>
          self.postMessage({
            type: "output",
            fd: 2,
            bytes: new TextEncoder().encode(s + "\n"),
          }),
        onExit: (n) => {
          status = n;
        },
      });
      module.FS.mkdir("/sdk");
      for (const file of ["library.rxbin", "rxcexits.rxbin"]) {
        const response = await fetch(new URL(file, base));
        if (!response.ok) throw Error("Compiler SDK unavailable");
        module.FS.writeFile(
          "/sdk/" + file,
          new Uint8Array(await response.arrayBuffer()),
        );
      }
      module.FS.writeFile(
        "/profile." + (name === "rxc" ? "the" : "rxas"),
        name === "rxc" ? data.source : assembly,
      );
      const result = module.callMain(
        name === "rxc"
          ? ["--no-exe-import", "-i", "/sdk", "-o", "/profile", "/profile.the"]
          : ["-o", "/profile", "/profile"],
      );
      if (status || (typeof result === "number" && result))
        throw Error(`${name} failed with status ${status || result}`);
      if (name === "rxc") assembly = module.FS.readFile("/profile.rxas");
      else {
        const bytes = module.FS.readFile("/profile.rxbin");
        if (new TextDecoder().decode(bytes.slice(0, 8)) !== "cReXx007")
          throw Error("Invalid compiled RXBIN");
        self.postMessage({ type: "built", bytes });
      }
    }
  } catch (error) {
    self.postMessage({ type: "failed", error: String(error) });
  }
};
