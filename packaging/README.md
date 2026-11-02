# CRXTHE packaging

The maintained [packaging guide](../doc/packaging.md) describes dependencies,
portable archives, native installers, runtime selection, signing and
qualification. It is installed with the other product manuals.

Start with [Windows installation and first use](../doc/Windows-Readme.txt) for
end-user setup. Current limitations are in [known issues](../doc/known-issues.md)
and planned work in the [roadmap](../doc/roadmap.md).

## Browser static site candidate

Build a configured WebAssembly editor site as described in the
[browser build guide](../doc/browser-wasm.md#build-and-run). For the full
examples candidate, enable cREXX, Classic highlighting and interpreter,
the matching profile compiler, and the reviewed MiniBASIC images as described
there. Build the site, then package it from the repository root:

```sh
cmake --build build/wasm-review-candidate --parallel 8
python3 scripts/zip-wasm-site.py \
  build/wasm-review-candidate/site build/browser-static-site-candidate.zip
cd build
shasum -a 256 -c browser-static-site-candidate.zip.sha256
unzip -t browser-static-site-candidate.zip
```

Replace `build/wasm-review-candidate` with the configured build directory.
The script checks every site's file size and SHA-256 against `manifest.json`,
rejects missing/extra files and symlinks, writes a deterministic ZIP containing
the whole site, reads it back and writes the ZIP's SHA-256 sidecar. It does not
tag, sign, upload or publish anything. The manifest identifies a dirty source
tree when the source has uncommitted tracked changes.

Before distributing the GPL-covered WebAssembly editor publicly, make the
corresponding source and build scripts for the manifest's exact source commit
available to recipients, alongside the separate dependency notices. A
`source_url` field is provenance metadata; check that it resolves to the
packaged commit before relying on it. Keep candidates built from unpublished
local commits local until their source is supplied.

For a local HTTP check, serve the **unpacked ZIP** at the origin root:

```sh
mkdir -p /tmp/crxthe-site-check
unzip -q build/browser-static-site-candidate.zip -d /tmp/crxthe-site-check
python3 -m http.server 8766 --bind 127.0.0.1 --directory /tmp/crxthe-site-check
```

Open `http://127.0.0.1:8766/` and check `Examples.txt` through Files > Open.
The archive's `HOSTING.txt` states the root deployment, HTTPS, MIME, Web Locks,
IndexedDB and content security policy requirements. The browser UI and
interpreter checks in [browser verification](../doc/browser-wasm.md#verification)
remain separate from archive validation. This is a local review candidate,
not release qualification.
