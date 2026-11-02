#!/usr/bin/env python3
"""Qualify an extracted combined browser ZIP before exposing candidate artifacts."""

import argparse
from datetime import datetime, timezone
from functools import partial
from hashlib import sha256
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import shlex
import shutil
import signal
import subprocess
import sys
import tempfile
from threading import Thread
from uuid import uuid4
import zipfile


ROOT = Path(__file__).resolve().parent.parent
REQUIRED_CAPABILITIES = (
    "crexx_vm", "classic_rexx_highlighter", "classic_rexx_interpreter",
    "editor_address", "source_compiler", "profile_source_compiler",
    "common_tool_session", "minibasic_candidate",
)


def chromium_binary():
    configured = os.environ.get("CHROMIUM_BIN")
    candidates = [configured] if configured else [
        "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
        "/Applications/Chromium.app/Contents/MacOS/Chromium",
        str(Path.home() / "Applications/Google Chrome.app/Contents/MacOS/Google Chrome"),
        "chromium", "chromium-browser", "google-chrome", "google-chrome-stable", "chrome",
    ]
    for candidate in candidates:
        executable = shutil.which(os.path.expanduser(candidate))
        if executable:
            return str(Path(executable).resolve())
    raise ValueError("Chromium was not found; set CHROMIUM_BIN to its executable")


def check_source(manifest):
    if manifest.get("source_dirty") is not False:
        raise ValueError("Candidate manifest must record clean tracked source (source_dirty=false)")
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    if manifest.get("source_revision") != revision:
        raise ValueError("Candidate manifest source_revision differs from the current checkout")
    dirty = subprocess.check_output(
        ["git", "status", "--porcelain", "--untracked-files=no"], cwd=ROOT, text=True)
    if dirty:
        raise ValueError("Commit tracked source changes before qualifying a browser candidate")


def stop_process(process):
    if os.name == "posix":
        try:
            os.killpg(process.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
    elif process.poll() is None:
        process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        if os.name == "posix":
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
        else:
            process.kill()
        process.wait()


def run_logged(command, environment, log):
    message = "$ " + shlex.join(command) + "\n"
    print(message, end="", flush=True)
    log.write(message)
    with subprocess.Popen(command, cwd=ROOT, env=environment, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace",
                          bufsize=1, start_new_session=os.name == "posix") as process:
        try:
            for line in process.stdout:
                print(line, end="", flush=True)
                log.write(line)
                log.flush()
            status = process.wait()
        finally:
            stop_process(process)
    if status:
        raise RuntimeError(f"Command failed with status {status}: {shlex.join(command)}")
    return {"command": command, "status": "passed", "returncode": status}


class SiteHandler(SimpleHTTPRequestHandler):
    extensions_map = {**SimpleHTTPRequestHandler.extensions_map,
                      ".mjs": "text/javascript", ".wasm": "application/wasm"}

    def log_message(self, format, *args):
        pass


def qualify(site, fixture_build, output_dir):
    site = site.resolve(strict=True)
    fixture_build = fixture_build.resolve(strict=True)
    output_dir = output_dir.resolve()
    if output_dir == site or site in output_dir.parents:
        raise ValueError("Candidate output must be outside the site directory")
    manifest = json.loads((site / "manifest.json").read_text())
    check_source(manifest)
    capabilities = manifest.get("capabilities", {})
    missing = [name for name in REQUIRED_CAPABILITIES if capabilities.get(name) is not True]
    if missing:
        raise ValueError("Combined browser candidate lacks capabilities: " + ", ".join(missing))
    node = shutil.which("node")
    if not node:
        raise ValueError("Node.js was not found on PATH")
    chromium = chromium_binary()
    for name in ("tool-example.rxbin", "source-file-probe.rxbin"):
        if not (fixture_build / name).is_file():
            raise ValueError(f"Browser QA fixture is missing: {fixture_build / name}")
    environment = {key: value for key, value in os.environ.items()
                   if not (key.startswith("THE_WASM_") and key.endswith("_ONLY"))}
    environment.pop("THE_WASM_SCREENSHOT", None)
    environment.update(CHROMIUM_BIN=chromium, THE_WASM_FIXTURE_BUILD=str(fixture_build))

    with tempfile.TemporaryDirectory(prefix="crxthe-browser-candidate-") as temporary:
        staging = Path(temporary)
        archive = staging / "CRXTHE-browser-candidate.zip"
        log_path = staging / "CRXTHE-browser-candidate.qa.log"
        receipt_path = staging / "CRXTHE-browser-candidate.qa.json"
        with log_path.open("w", encoding="utf-8") as log:
            package = run_logged([sys.executable, str(ROOT / "scripts/zip-wasm-site.py"),
                                  str(site), str(archive)], environment, log)
            extracted = staging / "site"
            with zipfile.ZipFile(archive) as zipped:
                zipped.extractall(extracted)
            packaged_manifest = json.loads((extracted / "manifest.json").read_text())
            if packaged_manifest != manifest:
                raise ValueError("Site changed while packaging the candidate; rebuild and retry")
            manifest = packaged_manifest
            check_source(manifest)
            digest = sha256(archive.read_bytes()).hexdigest()
            server = ThreadingHTTPServer(("127.0.0.1", 0), partial(SiteHandler, directory=str(extracted)))
            thread = Thread(target=server.serve_forever, daemon=True)
            environment["THE_WEB_URL"] = f"http://127.0.0.1:{server.server_port}/"
            tests = []
            try:
                thread.start()
                for name in ("wasm-browser-smoke.mjs", "wasm-paint-smoke.mjs"):
                    tests.append(run_logged([node, str(ROOT / "web/test" / name)], environment, log))
            finally:
                if thread.is_alive():
                    server.shutdown()
                    thread.join()
                server.server_close()
            check_source(manifest)
            log.write(f"PASS extracted browser candidate {digest}\n")
        receipt = {
            "status": "passed", "qualification": "local-extracted-zip-browser",
            "completed_utc": datetime.now(timezone.utc).isoformat(),
            "source_revision": manifest["source_revision"], "source_dirty": False,
            "candidate": archive.name, "candidate_sha256": digest,
            "capabilities": capabilities, "package": package, "tests": tests,
            "test_environment": {key: environment[key] for key in
                                 ("THE_WEB_URL", "CHROMIUM_BIN", "THE_WASM_FIXTURE_BUILD")},
        }
        receipt_path.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
        # Install the complete result as one directory rename. Existing
        # candidates stay immutable even if copying a new artifact fails.
        output_dir.mkdir(parents=True, exist_ok=True)
        generation = output_dir / f"{manifest['source_revision'][:12]}-{digest[:12]}-{uuid4().hex[:8]}"
        with tempfile.TemporaryDirectory(prefix=".qualifying-", dir=output_dir) as pending:
            bundle = Path(pending) / "bundle"
            bundle.mkdir()
            for artifact in (archive, archive.with_suffix(".zip.sha256"), log_path, receipt_path):
                shutil.copyfile(artifact, bundle / artifact.name)
            os.replace(bundle, generation)
        print(f"PASS browser candidate: {generation / archive.name} (SHA-256 {digest})")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--site", required=True, type=Path)
    parser.add_argument("--fixture-build", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args()
    signal.signal(signal.SIGTERM, lambda signum, frame: sys.exit(1))
    qualify(args.site, args.fixture_build, args.output_dir)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError, KeyboardInterrupt) as error:
        print(f"Browser candidate failed: {error}", file=sys.stderr)
        sys.exit(1)
