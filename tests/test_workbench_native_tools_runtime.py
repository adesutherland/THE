#!/usr/bin/env python3
"""Owner-bound native source tools through the real editor, without RX helpers."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

binary = Path(sys.argv[1]).resolve()


def quoted(value):
    return '"' + str(value).replace('"', '""') + '"'


class Session:
    def __init__(self, root, source, engine='NONE', profile=None, extra_env=None):
        self.message = ''
        self.env = dict(os.environ, THE_REXX_INTERPRETER=engine,
                        THE_TOOL_ROOT=str(root / 'no-rxbin-helper'),
                        THE_TOOL_CONFIG_DIR=str(root / 'config'),
                        THE_CREXX_CACHE_DIR=str(root / 'cache'))
        self.env.update(extra_env or {})
        self.process = subprocess.Popen(
            [str(binary), '--driver', 'llm'] +
            (['-p', str(profile)] if profile else ['-n']) + [str(source)],
            cwd=root, env=self.env, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)

    def send(self, request):
        data = request if isinstance(request, bytes) else request.encode('utf-8')
        self.process.stdin.write(data + b'\n')
        self.process.stdin.flush()
        while True:
            line = self.process.stdout.readline()
            assert line, (request, self.process.poll(), self.process.stderr.read())
            if line.startswith(b'{'):
                record = json.loads(line)
                self.message = record.get('last_message', self.message)
                return record

    def command(self, text, rc=0):
        record = self.send('command ' + text)
        assert record.get('rc') == rc, (text, record)
        return record

    def query(self, section='summary'):
        self.command('query workbench ' + section)
        return json.loads(self.message)

    def refusal(self, command, code, rc=-3):
        self.command(command, rc)
        assert self.query()['lastResult']['code'] == code, (command, self.query())

    def run(self, command):
        self.command(command)
        self.send('toolwait 10000')
        return self.send('tools')

    def close(self):
        if self.process.poll() is None:
            self.send('quit')
            self.process.wait(timeout=10)
        assert self.process.returncode == 0, self.process.stderr.read()


with tempfile.TemporaryDirectory(prefix='THE native workbench ') as directory:
    root = Path(directory)
    source = root / 'source with spaces Ω.py'
    program = ("PENDING = 'disk'\n"
               "import json,sys\nfrom pathlib import Path\n"
               "with Path('executed.marker').open('ab') as marker: marker.write((PENDING+'\\n').encode())\n"
               "sys.stdout.buffer.write(('ARGV:'+json.dumps(sys.argv[1:],ensure_ascii=False)+'\\n').encode())\n"
               "sys.stdout.buffer.write(b'RAW:\\x00\\xff:'+sys.stdin.buffer.read())\n"
               "sys.stderr.buffer.write(b'ERR:\\x00\\xff\\n')\n"
               "sys.exit(7 if sys.argv[1:2]==['fail'] else 0)\n")
    source.write_text(program, encoding='utf-8')
    original = source.read_bytes()
    input_bytes = b'a\x00\xffb\n'
    (root / 'input.bin').write_bytes(input_bytes)
    captured = root / 'captured.bin'
    marker = root / 'executed.marker'
    s = Session(root, source)
    try:
        capability = s.query('capabilities')
        assert capability['tools'] and capability['nativeCompletion'] == 'process-exit-only'
        if not capability['nativeProcess']:
            s.command('workbench begin owner public.native')
            s.refusal('workbench tool put public.python role runner adapter native-process '
                      f'executable {quoted(sys.executable)} source snapshot', 'unsupported-host')
            s.command('workbench abort')
            s.refusal('workbench run public.python source active', 'unsupported-host')
            assert not marker.exists() and source.read_bytes() == original
            s.close()
            print('Native tools explicitly unsupported on this unqualified host')
            raise SystemExit(0)

        assert capability['nativeLimits']['joinedArgvBytes'] == 8191
        assert not any(t['available'] for t in s.send('tools')['catalog'])
        builtin_action_count = s.query('actions')['total']
        assert builtin_action_count == 10
        args = ['Mixed Case', '', 'quoted"back\\', 'Zoë😀', '$(touch never); & |']
        s.command('workbench begin owner public.native')
        s.command('workbench tool put public.python role runner adapter native-process '
                  f'executable {quoted(sys.executable)} source snapshot')
        s.command('workbench route put public.stdin from FILE "input.bin" '
                  'to tool:public.python/stdin format bytes eof end')
        s.command('workbench route put public.capture from tool:public.python/stdout '
                  'to FILE "captured.bin" format bytes mode CREATE publish success')
        s.command('workbench panel put public.logpane kind log model public.log')
        s.command('workbench route put public.logroute from tool:public.python/stderr to log:public.log')
        s.command('workbench action put public.run label "Run native" tool public.python source active ' +
                  ' '.join('ARG ' + quoted(value) for value in args))
        assert s.query('tools')['total'] == 0
        assert not marker.exists() and not captured.exists()
        s.command('workbench commit')
        tool = s.query('tools')['records'][0]
        assert tool['id'] == 'public.python' and tool['available'] and tool['reason'] == 'ok'
        assert tool['executable'] == sys.executable and tool['source'] == 'snapshot'
        assert s.query('routes')['total'] == 3
        action_page = s.query(f'actions offset {builtin_action_count} limit 8')
        assert action_page['total'] == builtin_action_count + 1
        assert len(action_page['records']) == 1
        assert action_page['records'][0]['id'] == 'public.run'
        assert action_page['records'][0]['argumentCount'] == len(args)
        assert not marker.exists() and not captured.exists()

        # Pending working-record capture, not Save or a disk reread.
        assert s.send("text PENDING = 'captured'")['rc'] == 0
        before = s.send('look full compact')
        result = s.run('workbench invoke public.run')
        assert result['job']['state'] == 'Completed · status 0', result
        assert result['job']['tool'] == 'public.python' and result['job']['sourceId']
        expected = ('ARGV:' + json.dumps(args, ensure_ascii=False) + '\n').encode() + b'RAW:\x00\xff:' + input_bytes
        assert captured.read_bytes() == expected, captured.read_bytes()
        assert marker.read_bytes() == b'captured\n' and not (root / 'never').exists()
        after = s.send('look full compact')
        assert before['buffer'] == after['buffer'] and before['focus'] == after['focus']
        assert source.read_bytes() == original and before['buffer']['dirty']

        # All complete references resolve the candidate graph and its owner.
        s.command('workbench begin owner public.native')
        s.command('workbench tool remove public.python')
        s.refusal('workbench commit', 'object-in-use')
        s.command('workbench abort')
        assert s.query('tools')['records'][0]['generation'] == tool['generation']
        s.command('workbench begin owner public.other')
        s.command('workbench action put public.steal label "Denied" tool public.python source active')
        s.refusal('workbench commit', 'policy-denied')
        s.command('workbench abort')
        s.command('workbench begin owner public.other')
        s.command('workbench route put public.steal from FILE "input.bin" '
                  'to tool:public.python/stdin format bytes eof end')
        s.refusal('workbench commit', 'policy-denied')
        s.command('workbench abort')
        s.command('workbench begin owner public.native')
        s.refusal('workbench tool put classic-rexx role runner adapter native-process '
                  f'executable {quoted(sys.executable)} source snapshot', 'object-in-use')
        s.command('workbench abort')

        # RX REGISTER may claim a candidate ID while the WB transaction is
        # open; COMMIT must recheck and preserve the legacy registration.
        s.command('workbench begin owner public.native')
        s.command('workbench tool put candidatecollision role runner adapter native-process '
                  f'executable {quoted(sys.executable)} source snapshot')
        s.command(f'RX REGISTER candidatecollision program {quoted(root / "unused.rxbin")}')
        s.refusal('workbench commit', 'object-in-use')
        s.command('workbench abort')
        assert all(t['id'] != 'candidatecollision' for t in s.query('tools')['records'])
        assert any(t['id'] == 'candidatecollision' for t in s.send('tools')['catalog'])

        # Runtime refusals do not poison an otherwise valid definition transaction.
        s.command('workbench begin owner public.native')
        s.command('workbench tool put public.missing role runner adapter native-process '
                  f'executable {quoted(root / "missing interpreter")} source snapshot')
        s.command('workbench commit')
        missing = next(t for t in s.query('tools')['records'] if t['id'] == 'public.missing')
        assert not missing['available'] and missing['reason'] == 'capability-unavailable'
        (root / 'missing interpreter').symlink_to(sys.executable)
        assert next(t for t in s.query('tools')['records'] if t['id'] == 'public.missing')['available']
        (root / 'missing interpreter').unlink()
        s.command('workbench begin owner public.native')
        s.refusal('workbench run public.missing source active', 'capability-unavailable')
        s.command('workbench commit')
        assert marker.read_bytes() == b'captured\n'
        s.command('workbench begin owner public.native')
        s.refusal('workbench tool put public.relative role runner adapter native-process '
                  'executable "python" source snapshot', 'invalid-operand', 5)
        s.command('workbench abort')

        # Malformed/misspelt argv and invalid UTF-8 cannot reach exec.
        s.refusal('workbench run public.python source active ARGS "bad"', 'invalid-operand', 5)
        bad = s.send(b'command workbench run public.python source active ARG "\xff"')
        assert bad['rc'] == 5 and s.query()['lastResult']['code'] == 'invalid-operand'
        assert marker.read_bytes() == b'captured\n'

        captured.unlink()
        failed = s.run('workbench run public.python source active ARG "fail"')
        assert failed['job']['state'] == 'Failed', failed
        assert not captured.exists() and not s.query('fileio')['published']
        assert marker.read_bytes() == b'captured\ncaptured\n'

        # Maximum argv count exercises the compact tokenizer, retaining empties.
        many = [''] * 128
        s.command('workbench begin owner public.native')
        s.command('workbench action put public.run label "128 arguments" tool public.python source active ' +
                  ' '.join('ARG ""' for _ in many))
        s.command('workbench commit')
        result = s.run('workbench invoke public.run')
        assert result['job']['state'] == 'Completed · status 0'
        assert captured.read_bytes().startswith(('ARGV:' + json.dumps(many) + '\n').encode())
        assert source.read_bytes() == original
        s.command('workbench begin owner public.native')
        s.command('workbench reset owner public.native')
        s.command('workbench commit')
        assert s.query('tools')['total'] == 0 and s.query('routes')['total'] == 0
    finally:
        s.close()

    if len(sys.argv) > 2 and sys.argv[2] == 'CREXX':
        profile = root / 'rollback.the'
        setup = ("options levelb\naddress THE\n"
                 "'workbench begin owner public.profile';\n"
                 f"'workbench tool put public.profiletool role runner adapter native-process executable {quoted(sys.executable)} source snapshot';\n"
                 "'workbench action put public.profileaction label \"Profile action\" tool public.profiletool source active';\n"
                 "'workbench commit';\n")
        marker_before = marker.read_bytes()
        profile.write_text(setup + 'return 9\n')
        probe = Session(root, source, engine='CREXX', profile=profile)
        try:
            assert probe.query('tools')['total'] == 0
            assert marker.read_bytes() == marker_before
        finally:
            probe.close()
        profile.write_text(setup + 'return 0\n')
        probe = Session(root, source, engine='CREXX', profile=profile)
        try:
            assert probe.query('tools')['total'] == 1
            assert marker.read_bytes() == marker_before
            macro = root / 'refuse-run.the'
            macro.write_text("options levelb\naddress THE\n'workbench run public.profiletool source active';\nreturn 0\n")
            probe.command('macro ' + quoted(macro))
            assert probe.query()['lastResult']['code'] == 'busy-modal'
            assert marker.read_bytes() == marker_before
        finally:
            probe.close()

    # Optional dependency qualification: an actual installed/build ooRexx and
    # its matching adjacent rxapi, isolated from any user's existing provider.
    oorexx = os.environ.get('THE_TEST_OOREXX_EXECUTABLE')
    if oorexx:
        # Unix socket names are short; macOS's default /var/folders TMPDIR plus
        # the test directory can exceed sockaddr_un.sun_path's 104 bytes.
        runtime_context = tempfile.TemporaryDirectory(prefix='THE-ooapi-', dir='/tmp')
        runtime = Path(runtime_context.name)
        ooenv = {'XDG_RUNTIME_DIR': str(runtime)}
        provider = subprocess.Popen([str(Path(oorexx).parent / 'rxapi')],
                                    env=dict(os.environ, **ooenv), stdout=subprocess.DEVNULL,
                                    stderr=subprocess.DEVNULL)
        try:
            deadline = time.monotonic() + 10
            while not list(runtime.glob('*.service')) and time.monotonic() < deadline:
                assert provider.poll() is None
                time.sleep(.01)
            assert list(runtime.glob('*.service'))
            oo_source = root / 'object.rexx'
            oo_source.write_text("say .Greeter~new~greet('ready')\nsay .SYSCARGS[1]\n"
                                 "say c2x(charin(,,5))\nexit 0\n::class Greeter\n"
                                 "::method greet\nuse arg text\nreturn 'object:'||text\n")
            oo = Session(root, oo_source, extra_env=ooenv)
            try:
                oo.command('workbench begin owner public.oo')
                oo.command('workbench tool put public.oorexx role runner adapter native-process '
                           f'executable {quoted(oorexx)} source snapshot')
                oo.command('workbench route put public.oo.in from FILE "input.bin" '
                           'to tool:public.oorexx/stdin format bytes eof end')
                oo.command('workbench commit')
                before = oo.send('look full compact')
                result = oo.run('workbench run public.oorexx source active ARG "Zoë😀"')
                output = ''.join(c['text'] for c in result['chunks'] if c['fd'] == 1)
                assert result['job']['state'] == 'Completed · status 0' and output == 'object:ready\nZoë😀\n6100FF620A\n', result
                after = oo.send('look full compact')
                assert before['buffer'] == after['buffer'] and before['focus'] == after['focus']
            finally:
                oo.close()
        finally:
            provider.terminate()
            provider.wait(timeout=10)
            runtime_context.cleanup()
        print('Actual configured ooRexx: object source, argv, finite binary IO and editor preservation passed')
    else:
        print('Actual ooRexx executable not configured; dependency qualification omitted')
print('Native workbench tools: ownership, candidates, argv, routes, source and failures passed')
