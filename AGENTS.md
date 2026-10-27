# THE repository instructions

Read `AGENT_README.md` for the editor's engineering and testing contracts.

## Main line

- Work on `main`. Commit and integrate changes there. Create another branch or
  worktree only when Adrian explicitly requests it for the current task.
- Publish to `origin/main` when pushing is authorized. Preserve existing remote
  history and reconcile concurrent changes before pushing.
- Before deleting any branch, inspect its commits and patches against `main`.
  Report useful unmerged work and merge it when authorized, or preserve it in a
  verified recovery bundle and obtain an explicit decision about its disposal.
  A request to tidy branch names is not permission to discard unique work.
- Check linked worktrees before removing branch references. Preserve their
  uncommitted files and any commits not yet integrated into `main`.

## Releases

- Build releases from a commit already published on `origin/main`. Verify the
  exact tag commit against freshly fetched `origin/main` before building,
  signing or publishing a new release.
- Create or push release tags, sign/upload assets and publish releases only
  with explicit authorization for that release. Source commits and ordinary
  CI qualification do not authorize a release.
- Keep development versions clearly marked, currently `0.10.dev`. Change to a
  release version only as part of an authorized release preparation.
- Preserve existing tags and release assets unless Adrian explicitly requests
  their modification. Historical branch-based releases do not establish the
  workflow for future releases.
