"""Build the same runtime-choice PKG for unsigned staging and signed releases."""
from pathlib import Path
import json
import re
import shutil
import subprocess
from xml.sax.saxutils import escape


def build_installer(payload, output, version, work, run=None, identity=None, keychain=None):
    if run is None:
        def run(*command):
            subprocess.run(list(map(str, command)), check=True)
    numeric = re.sub(r'\.dev$', '.0', version.split('-')[0].split('+')[0])
    root = work / 'pkgroot'
    manifest = json.loads((payload / 'manifest.json').read_text()) if (payload / 'manifest.json').exists() else {}
    product = manifest.get('display_version', manifest.get('product', 'THE cREXX Edition'))
    destination = root / 'usr/local/the-crexx'
    destination.parent.mkdir(parents=True)
    shutil.copytree(payload, destination, symlinks=True)
    links = root / 'usr/local/bin'
    links.mkdir()
    (links / 'the').symlink_to('../the-crexx/bin/the')
    if (payload / 'bin/crxthe').exists():
        (links / 'crxthe').symlink_to('../the-crexx/bin/crxthe')
    if not (payload / 'libexec/the/the-launcher').exists():
        component = work / 'component.pkg'
        run('pkgbuild', '--root', root, '--identifier', 'org.crexx.the',
            '--version', numeric, '--install-location', '/', component)
        command = ['productbuild', '--package', component]
        if identity:
            command += ['--sign', identity, '--keychain', keychain, '--timestamp']
        run(*command, output)
        return
    core_scripts = work / 'core-scripts'
    installed_scripts = work / 'installed-scripts'
    core_scripts.mkdir()
    installed_scripts.mkdir()
    for directory, command in (
            (core_scripts, 'rm -f "$target/usr/local/the-crexx/share/the/runtime-preference"'),
            (installed_scripts, 'printf "INSTALLED\\n" > "$target/usr/local/the-crexx/share/the/runtime-preference"')):
        script = directory / 'postinstall'
        script.write_text('#!/bin/sh\nset -eu\ntarget=${3:-/}\n' + command + '\n')
        script.chmod(0o755)
    core = work / 'the-core.pkg'
    installed = work / 'the-installed-runtime.pkg'
    run('pkgbuild', '--root', root, '--scripts', core_scripts, '--identifier', 'org.crexx.the',
        '--version', numeric, '--install-location', '/', core)
    run('pkgbuild', '--nopayload', '--scripts', installed_scripts,
        '--identifier', 'org.crexx.the.runtime-installed', '--version', numeric, installed)
    distribution = work / 'Distribution.xml'
    distribution.write_text('''<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>''' + escape(product) + '''</title>
  <options customize="always" require-scripts="false"/>
  <domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>
  <choices-outline><line choice="core"/><line choice="installed-crexx"/></choices-outline>
  <choice id="core" title="''' + escape(product) + ''' and bundled cREXX (default)" enabled="false" selected="true"
    description="Includes the editor and its matching cREXX runtime. The bundled runtime is the default.">
    <pkg-ref id="org.crexx.the"/>
  </choice>
  <choice id="installed-crexx" title="Use an installed cREXX when available" start_selected="false"
    description="The editor checks for a compatible installed cREXX at startup. If none is available, it uses its bundled copy automatically. Regina is configured separately.">
    <pkg-ref id="org.crexx.the.runtime-installed"/>
  </choice>
  <pkg-ref id="org.crexx.the" version="''' + escape(numeric) + '''">the-core.pkg</pkg-ref>
  <pkg-ref id="org.crexx.the.runtime-installed" version="''' + escape(numeric) + '''">the-installed-runtime.pkg</pkg-ref>
</installer-gui-script>
''')
    command = ['productbuild', '--distribution', distribution, '--package-path', work]
    if identity:
        command += ['--sign', identity, '--keychain', keychain, '--timestamp']
    run(*command, output)
