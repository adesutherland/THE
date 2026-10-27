#!/usr/bin/env python3
"""Release tags on main pass; unmerged source tags must fail before publishing."""
import os
from pathlib import Path
import subprocess
import tempfile

helper = Path(__file__).resolve().parent.parent / 'scripts/check-release-source.sh'
with tempfile.TemporaryDirectory(prefix='THE release source ') as temporary:
    env = dict(os.environ, GIT_AUTHOR_NAME='Release fixture',
               GIT_AUTHOR_EMAIL='fixture@example.invalid',
               GIT_COMMITTER_NAME='Release fixture',
               GIT_COMMITTER_EMAIL='fixture@example.invalid')

    def git(*args, input=None):
        return subprocess.check_output(['git', *args], cwd=temporary, env=env,
                                       input=input, text=True).strip()

    git('init', '--quiet', '--initial-branch=main')
    tree = git('mktree', input='')
    main = git('commit-tree', tree, '-m', 'Published source')
    unmerged = git('commit-tree', tree, '-p', main, '-m', 'Unmerged source')
    git('update-ref', 'refs/heads/main', main)
    git('update-ref', 'refs/remotes/origin/main', main)
    git('tag', 'v1.0.0', main)
    git('tag', 'v1.0.1', unmerged)
    for tag, expected in [('v1.0.0', 0), ('v1.0.1', 1)]:
        result = subprocess.run(['bash', str(helper), tag], cwd=temporary,
                                capture_output=True, text=True)
        assert result.returncode == expected, result
    git('update-ref', 'refs/remotes/origin/main', unmerged)
    result = subprocess.run(['bash', str(helper), 'v1.0.1'], cwd=temporary,
                            capture_output=True, text=True)
    assert result.returncode == 0, result
print('Release source gate accepts main and rejects unmerged tag commits')
