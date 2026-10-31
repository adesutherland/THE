#!/usr/bin/env python3
"""Narrow typed LLM form operations against the real editor owner."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="THE LLM forms ") as directory:
    root = Path(directory)
    sample = root / "source.txt"
    original = b"alpha\nbeta\n"
    sample.write_bytes(original)
    env = dict(os.environ, THE_HOME_DIR=str(binary.parent / "release"), THE_DRIVER_PATH=str(binary.parent))
    process = subprocess.Popen([str(binary), "--driver", "llm", "-n", str(sample)],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8", env=env)

    def send(text):
        process.stdin.write(text + "\n")
        process.stdin.flush()
        while True:
            line = process.stdout.readline()
            assert line, (text, process.poll(), process.stderr.read())
            if line.startswith("{"):
                return json.loads(line)

    def command(text):
        reply = send("command " + text)
        assert reply["rc"] == 0, (text, reply)
        return reply

    def query(text):
        return json.loads(command("query workbench " + text)["last_message"])

    def metadata():
        return query("forms")["records"][0]

    def values(submitted=False):
        return {field["id"]: field["value"] for field in query("form public.person result" if submitted else "fields public.person")["records"]}

    def typed(operation, field=None, value=None, expected=None, generation=None):
        state = metadata()
        line = f"form {operation} public.person {generation or state['generation']} {expected or state['draftRevision']}"
        if field is not None:
            line += f" {field} {value.encode('utf-8').hex() or '-'}"
        return send(line)

    try:
        capabilities = send("capabilities")
        assert capabilities["workbench"]["formInput"] == "typed-hex" and capabilities["workbench"]["formValueBytes"] == 1024
        assert "form" in capabilities["inputs"]
        command("workbench begin owner public.forms")
        command("workbench form put public.person")
        command('workbench field put public.person name label "Name" value "Guest" access edit maxcells 1024')
        command('workbench field put public.person protected label "Protected" value "Keep" access readonly maxcells 20')
        command('workbench panel put public.personpane kind fields form public.person')
        command("workbench commit")
        before = send("look full compact")
        first = metadata()
        reply = typed("set", "name", "Alice 😀")
        assert reply["code"] == "ok" and reply["rc"] == 0 and isinstance(reply["draftRevision"], str), reply
        assert values()["name"] == "Alice 😀"
        reply = typed("set", "name", "stale", expected=first["draftRevision"])
        assert reply["code"] == "stale-revision" and reply["rc"] == -3, reply
        reply = typed("set", "protected", "changed")
        assert reply["code"] == "field-protected" and reply["rc"] == 12 and values()["protected"] == "Keep", reply
        reply = typed("submit")
        assert reply["code"] == "ok" and values(True)["name"] == "Alice 😀", reply
        typed("set", "name", 'literal # " ; command delete ALL <img>')
        assert values()["name"] == 'literal # " ; command delete ALL <img>'
        reply = typed("cancel")
        assert reply["code"] == "ok" and values()["name"] == "Alice 😀", reply
        assert typed("set", "name", "")["code"] == "ok" and values()["name"] == ""
        assert typed("set", "name", "x" * 1024)["code"] == "ok"
        stable = metadata()["draftRevision"]
        for encoded in ["00", "ff", "c080", "eda080", "0", "zz", "41" * 1025]:
            reply = send(f"form set public.person {first['generation']} {stable} name {encoded}")
            assert reply["code"] == "invalid-operand" and metadata()["draftRevision"] == stable, reply
        reply = send(f"form set public.person {first['generation']} {stable} name 41 extra")
        assert reply["code"] == "invalid-operand", reply
        reply = send(f"form set public.person {first['generation']} {stable} name " + "41" * 3000)
        assert reply["code"] == "invalid-operand", reply
        assert send("capabilities")["driver"] == "llm", "oversized form input leaked into following protocol line"
        reply = typed("set", "name", "wrong generation", generation=str(int(first["generation"]) + 1))
        assert reply["code"] == "stale-generation", reply
        after = send("look full compact")
        assert before["buffer"] == after["buffer"] and before["focus"] == after["focus"], (before, after)
        assert sample.read_bytes() == original
        send("quit")
        process.wait(timeout=10)
        assert process.returncode == 0, process.stderr.read()
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=5)
print("typed LLM forms: SET/Submit/Cancel, Unicode, protection, bounds, stale tokens and editor preservation passed")
