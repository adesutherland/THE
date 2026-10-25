"""Run prefix workflows against the real shared editor with isolated files."""
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile


class MacroHarness:
    def __init__(self, binary, macros, engine='REGINA'):
        self.binary = str(Path(binary).resolve())
        self.macros = Path(macros).resolve()
        self.engine = engine

    def aliases(self, package):
        text = (self.macros / (package + '.the')).read_text()
        return list(dict.fromkeys(re.findall(r'SET PREFIX SYNONYM\s+(\S+)\s+' + package,
                                             text, re.IGNORECASE)))

    def run(self, package, prefix, seed='seed', passes=1, commands='', before=''):
        with tempfile.TemporaryDirectory(prefix='THE macro workflow ') as tmp:
            work = Path(tmp)
            sample = work / 'sample.txt'
            sample.write_text(seed + '\ntail\n', encoding='utf-8')
            setup = ['capabilities', 'command set compat xedit xedit xedit',
                     'command set case mixed respect', 'command set prefix on left 12 0',
                     'command set eolout lf']
            for alias in self.aliases(package):
                setup.append(f'command set prefix synonym {alias} {package}')
            setup += [before, 'command :1', 'command cursor goto 1 1',
                      'command set pending on ' + prefix]
            for _ in range(passes):
                setup += ['command sos doprefix', 'look filearea compact']
            setup += [commands, 'look filearea compact', 'look prefix compact', 'command save', 'quit']
            env = dict(os.environ, THE_REXX_INTERPRETER=self.engine,
                       THE_MACRO_PATH=str(self.macros), TERM='')
            proc = subprocess.run([self.binary, '--driver', 'llm', '-n', str(sample)],
                                  input='\n'.join(setup) + '\n', env=env,
                                  text=True, encoding='utf-8', capture_output=True, timeout=20, check=True)
            records = [json.loads(s) for s in proc.stdout.splitlines() if s.startswith('{')]
            caps = next(r for r in records if 'macro_available' in r)
            assert caps['macro_available'] and caps['macro_interpreter'] == self.engine, (caps, proc.stderr)
            return {'text': sample.read_text(encoding='utf-8').splitlines(),
                    'records': records, 'errors': proc.stderr,
                    'snapshots': [r for r in records if 'screen_rows' in r]}


def interpreter_errors(result):
    return re.findall(r'.*(?:Error \d{4}|Invalid prefix|Invalid SET|Invalid operand|REXX interpreter|not yet implemented|not found|Bad arithmetic|Incorrect call|Routine not found).*', result['errors'])
