#!/usr/bin/env python3
"""Validate a built browser site and create a deterministic static-hosting ZIP candidate."""

import argparse
from hashlib import sha256
import json
from pathlib import Path
import zipfile


def package(site: Path, output: Path) -> None:
    site = site.resolve(strict=True)
    output = output.resolve()
    if site in output.parents:
        raise ValueError("ZIP output must be outside the site directory")
    manifest = json.loads((site / "manifest.json").read_text())
    if manifest.get("product") != "CRXTHE" or manifest.get("variant") != "wasm-editor-preview":
        raise ValueError("Expected a CRXTHE browser editor manifest")
    listed = manifest.get("files")
    if not isinstance(listed, dict):
        raise ValueError("Manifest file index is missing")
    required = {"index.html", "HOSTING.txt", "manifest.json",
                "wasm/crxthe_wasm.mjs", "wasm/crxthe_wasm.wasm",
                "licenses/CRXTHE-GPL-2.0.txt", "licenses/Emscripten.txt"}
    included = {}
    for path in site.rglob("*"):
        if path.is_symlink():
            raise ValueError(f"Site contains a symlink: {path}")
        if path.is_file() and path.name != ".build-stamp":
            included[path.relative_to(site).as_posix()] = path
    if missing := required - included.keys():
        raise ValueError(f"Site missing required files: {sorted(missing)}")
    if set(listed) != set(included) - {"manifest.json"}:
        missing = sorted(set(listed) - set(included))
        extra = sorted(set(included) - set(listed) - {"manifest.json"})
        raise ValueError(f"Manifest/site mismatch; missing={missing}, extra={extra}")
    for name, receipt in listed.items():
        if not isinstance(receipt, dict) or not isinstance(receipt.get("bytes"), int):
            raise ValueError(f"Invalid manifest receipt: {name}")
        data = included[name].read_bytes()
        if len(data) != receipt["bytes"] or sha256(data).hexdigest() != receipt.get("sha256"):
            raise ValueError(f"Manifest checksum mismatch: {name}")
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, path in sorted(included.items()):
            info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            archive.writestr(info, path.read_bytes(), compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)
    with zipfile.ZipFile(output) as archive:
        if archive.testzip() is not None or set(archive.namelist()) != set(included):
            raise ValueError("ZIP read-back failed")
    digest = sha256(output.read_bytes()).hexdigest()
    output.with_suffix(output.suffix + ".sha256").write_text(f"{digest}  {output.name}\n")
    print(f"{output} ({len(included)} files; SHA-256 {digest})")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("site", type=Path, help="Built CMake site directory")
    parser.add_argument("output", type=Path, help="Candidate ZIP path outside the site")
    args = parser.parse_args()
    package(args.site, args.output)


if __name__ == "__main__":
    main()
