# Controlled CRXTHE 0.9.1 full release

Adrian approved a full public 0.9.1 release on 27 October 2026 after manually
smoke-testing the Windows preview at
`dc2ea4022b264e30d0cdc87e2ece64dcc35ae485`. That source already passed local
macOS, hosted macOS/Windows and native Windows dual-engine form, tab,
Unicode status and syntax-colour checks; see [the follow-up](ui-render-followup.md).

The only subsequent product changes are the 0.9.1 identity/date, matched
profile welcome strings and current release/install documentation. The first
Mac candidate exposed UI test-harness readiness races: the PTY checker
accepted a quiet partial startup paint, and Chrome received a history command
during the test's deliberate disconnect navigation. The harness reads the
editor history before navigating, then waits for the blank document instead
of using a fixed 150 ms delay. Initial page attachment also waits for an
active renderer. The native syntax test also waits for an explicit profile-completion marker
with a successful SDSLHWAIT return code before asserting every token cell
colour, rather than accepting the earlier plain-text paint. All waits are
bounded and retain the rendering assertions.
No editor source changes were needed. Reuse the
manual interaction approval while requiring matching-source automated checks
for the versioned rebuild. Keep the v0.9.0 tag and all its assets unchanged.

1. Commit the consistent 0.9.1 version and documentation. Run local Release
   checks and both clean hosted candidate workflows at the same exact commit.
2. Verify both package manifests (0.9.1, clean source, exact frozen commit)
   and every payload hash. Have the Windows liaison verify that candidate.
3. Tag only that green commit as v0.9.1. Create a private draft containing the
   exact rebuilt installers, archives and checksums.
4. Sign the Windows outer installer locally with SimplySign and a trusted
   timestamp. Run the hosted signed-installer workflow with explicit draft
   access: native Windows installed/lifecycle checks, all Mac code signatures,
   ZIP/PKG notarization, PKG stapling, Gatekeeper and installed/runtime checks.
5. Download the final signed assets, verify checksums/signatures and unchanged
   payload provenance, then publish the draft as a full release and latest:
   draft=false, prerelease=false. Read back the tag, release and all assets.

No third-party macro originals, patches or cREXX ports belong in the package.
No message or material is to be sent to Shmuel as part of this release task.
The browser UI remains experimental; the separate LLM deletion observation
is still open. No upstream THE or live XEDIT compatibility claim is added.

Build/signing receipts and the final release notes record completed gates
without moving the frozen tag for a documentation-only status update.
