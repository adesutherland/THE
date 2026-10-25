#!/usr/bin/env python3
"""Exercise packaged runtime selection, real macro execution and cache isolation."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
source = parser.add_mutually_exclusive_group(required=True)
source.add_argument('--build-dir', type=Path)
source.add_argument('--prefix', type=Path)
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='THE runtime selection ') as temporary:
    work = Path(temporary)
    prefix = work / 'THE package with spaces'
    if args.build_dir:
        subprocess.run(['cmake', '--install', str(args.build_dir), '--prefix', str(prefix)],
                       check=True, capture_output=True)
    else:
        shutil.copytree(args.prefix, prefix, symlinks=True)
    home = work / 'user home'
    home.mkdir()
    local = work / 'local app data'
    local.mkdir()
    env = {k: v for k, v in os.environ.items() if not k.startswith(('THE_', 'CREXX'))}
    env.update(HOME=str(home), USERPROFILE=str(home), LOCALAPPDATA=str(local), TERM='')
    if os.name == 'nt':
        system = Path(env.get('SystemRoot') or env['SYSTEMROOT'])
        env['PATH'] = str(system / 'System32') + os.pathsep + str(system)
    else:
        env['PATH'] = '/usr/bin:/bin'
    suffix = '.exe' if os.name == 'nt' else ''
    launcher = prefix / ('libexec/the/the-launcher' + suffix)
    bundled = prefix / 'libexec/the/crexx'
    external = work / 'external cREXX'
    external_bin = external / 'bin'
    shutil.copytree(bundled, external_bin, symlinks=True)
    # A differently signed/user-built runtime must work with a hardened Mac host.
    if sys.platform == 'darwin':
        for path in external_bin.iterdir():
            if path.is_file() and path.read_bytes()[:4] in (b'\xcf\xfa\xed\xfe', b'\xca\xfe\xba\xbe'):
                subprocess.run(['codesign', '--force', '--sign', '-', str(path)], check=True, capture_output=True)
    preference = prefix / 'share/the/runtime-preference'
    preference.unlink(missing_ok=True)

    def info(**updates):
        result = subprocess.run([str(launcher), '--info', str(prefix)],
                                env=dict(env, **updates), check=True, capture_output=True, text=True, timeout=60)
        return json.loads(result.stdout)

    def selected(expected, directory, **updates):
        actual = info(**updates)
        assert actual['selected'] == expected, actual
        assert Path(actual['directory']).resolve() == directory.resolve(), actual
        return actual

    default = selected('BUNDLED', bundled, PATH=str(external_bin) + os.pathsep + env['PATH'])
    if os.name == 'nt':
        default_cache = local / 'THE-cREXX/Cache/crexxsaa'
        shared_cache = local / 'crexx/crexxsaa'
    elif sys.platform == 'darwin':
        default_cache = home / 'Library/Caches/org.crexx.the/crexxsaa'
        shared_cache = home / 'Library/Caches/crexx/crexxsaa'
    else:
        default_cache = home / '.cache/the/crexxsaa'
        shared_cache = home / '.cache/crexx/crexxsaa'
    assert Path(default['cache']) == default_cache, default
    explicit = work / 'explicit cache'
    generic = work / 'generic explicit cache'
    assert Path(info(THE_CREXX_CACHE_DIR=str(explicit))['cache']) == explicit
    assert Path(info(THE_CREXX_CACHE_DIR=str(explicit), CREXXSAA_CACHE_DIR=str(generic))['cache']) == generic
    selected('INSTALLED', external_bin, THE_CREXX_RUNTIME='INSTALLED', THE_CREXX_HOME=str(external))
    selected('INSTALLED', external_bin, THE_CREXX_RUNTIME='INSTALLED',
             PATH=str(external_bin) + os.pathsep + env['PATH'])
    preference.write_text('INSTALLED\n')
    selected('INSTALLED', external_bin, THE_CREXX_HOME=str(external))
    selected('BUNDLED', bundled, THE_CREXX_RUNTIME='BUNDLED', THE_CREXX_HOME=str(external))
    selected('BUNDLED', bundled, THE_CREXX_HOME=str(work / 'absent runtime'))
    selected('BUNDLED', bundled, THE_REXX_INTERPRETER='NONE', THE_CREXX_HOME=str(external))
    selected('BUNDLED', bundled, THE_REXX_INTERPRETER='REGINA', THE_CREXX_HOME=str(external))

    macro = work / 'cache probe.the'
    macro.write_text("options levelb\naddress THE\n'emsg RUNTIME_CACHE_OK'\n")
    sample = work / 'sample.txt'
    sample.write_text('seed\n')

    def run_macro(**updates):
        commands = ('capabilities\ncommand macro pfxsetupg REXX\ncommand macro pfxsetupg HLASM\n' +
                    f'command macro "{macro.as_posix()}"\n' * 2 + 'quit\n')
        result = subprocess.run([str(launcher), str(prefix), '--driver', 'llm', '-n', str(sample)],
            env=dict(env, CREXXSAA_CACHE_TRACE='1', **updates), input=commands,
            text=True, encoding='utf-8', capture_output=True, check=True, timeout=60)
        assert 'RUNTIME_CACHE_OK' in result.stdout, (result.stdout, result.stderr)
        assert not any(word in result.stderr for word in ('Error ', 'Unable to')), result.stderr
        trace = [line for line in result.stderr.splitlines() if str(macro).replace('\\', '/') in line.replace('\\', '/')]
        assert any('cache hit:' in line for line in trace), trace
        return result

    run_macro(THE_CREXX_HOME=str(external))
    assert list(default_cache.rglob('*.rxbin'))
    assert not shared_cache.exists(), shared_cache
    # Same source/cache, different compiler cohort must compile rather than use
    # the external cohort's object. A following invocation then hits its cache.
    switched = run_macro(THE_CREXX_RUNTIME='BUNDLED')
    assert 'cache stale:' in switched.stderr or 'cache miss:' in switched.stderr, switched.stderr
    fallback = run_macro(THE_CREXX_HOME=str(work / 'absent runtime'))
    assert 'using bundled cREXX' in fallback.stderr

    library = next(p for p in external_bin.iterdir() if 'crexxsaa' in p.name and p.suffix in ('.dll', '.so', '.dylib'))
    library.write_bytes(b'incompatible runtime')
    selected('BUNDLED', bundled, THE_CREXX_HOME=str(external))
    run_macro(THE_CREXX_HOME=str(external))
    print('Runtime policy: bundled default, installed via PATH/home/preference, explicit override, absent/incompatible fallback, NONE/REGINA isolation passed.')
    print('Cache: THE default, explicit and generic overrides, real macro miss/hit and toolchain switch passed; shared cREXX cache untouched.')
