Browser presentation regression receipts

The latest focused fixture passed against `/tmp/the-alignment-reader-ui`. Its
production browser UI bundle uses the native WebSocket transport with a
deterministic semantic-protocol server. It does not run the actual Wasm editor,
compiler or language engines. Independent actual-Wasm UI/aesthetic findings and
candidate qualification remain in the neighbouring candidate evidence packs.
This is not a replay or acceptance claim for all 29 catalogue scenarios.

`reader-viewport-fixture-pass.log` preserves exact stdout from the existing
successful tool run (exec session 67326, exit 0). That run was not tee'd to disk;
the log was transcribed from the retained tool result without rerunning it.
The following commands produced that bundle and result:

```sh
# cwd /Users/adrian/CLionProjects/THE/web
./node_modules/.bin/vite build --outDir /tmp/the-alignment-reader-ui --emptyOutDir
# cwd /Users/adrian/CLionProjects/THE
THE_WEB_ASSETS=/tmp/the-alignment-reader-ui CHROMIUM_BIN='/Applications/Google Chrome.app/Contents/MacOS/Google Chrome' node web/test/workbench-smoke.mjs
```

The C04 reader patch remembers each job's last visible top/left/follow state,
ignores zero geometry reported while hidden, suspends following while an owned
reader selection is active, and restores the viewport after restoring native
selection/focus. Clear and new jobs reset that state. The strict fixture kept
its real native selection `output 37`, separate input draft `unsent input` and
`scrollTop: 710` after a hidden same-job output append. It also exercised a
Tools native-field edit while the reader was hidden, independent field/source
state, and stale-range refusal after new-job and clear/replacement transitions.
The same successful run covers the existing responsive/profile-footer/focus
fixture cases plus the new coarse-pointer and Settings layout branches.

`asset-source-hashes.json` records SHA-256 for the exact temporary bundles,
embedded build-time TypeScript source from their source maps, current source
and test files, and the base commit. The successful reader bundle predates only
the final explanatory hidden-geometry comment in `rexx-console.tsx`; the receipt
records both hashes and the verified comment-only difference. The earlier
coarse/Settings bundle predates the C04 viewport repair and is not reader-fix
qualification. The initial coarse-before path was mutable and no native
per-asset hash was retained at measurement time; the linked candidate02 input
receipt supplies source context without inventing an asset-cohort claim.

The existing coarse and Settings JSON/PNG proofs are copied here, separately
from the UI lead's actual-Wasm captures. Coarse emulation used CDP
`Emulation.setTouchEmulationEnabled` and recorded `pointer:coarse=true`,
`pointer:fine=false`, `hover=false`, `maxTouchPoints=1` and zoom 1 at 390x844.
Actual `Input.dispatchTouchEvent` touchStart/touchEnd taps opened Tools,
associations, Files and Menu with resulting-state assertions. Before the
coarse-only CSS rule, undersized button heights ranged from 12 to 42 CSS
pixels. Afterwards all measured visible buttons met the 44 CSS pixel minimum
across Console, Profiles, associations, Files and Menu. Editor prefix cells are
excluded from that UI-control criterion. This is emulated input, not a physical
touch-device qualification.

```sh
# cwd /Users/adrian/CLionProjects/THE
THE_WEB_ASSETS=/tmp/the-alignment-native-ui THE_WORKBENCH_COARSE_ONLY=probe THE_WORKBENCH_COARSE_STAGE=before THE_WORKBENCH_COARSE_EVIDENCE=tests/ui-acceptance/alignment/browser/evidence CHROMIUM_BIN='/Applications/Google Chrome.app/Contents/MacOS/Google Chrome' node web/test/workbench-smoke.mjs
# cwd /Users/adrian/CLionProjects/THE/web
./node_modules/.bin/vite build --outDir /tmp/the-alignment-coarse-ui --emptyOutDir
# cwd /Users/adrian/CLionProjects/THE
THE_WEB_ASSETS=/tmp/the-alignment-coarse-ui THE_WORKBENCH_COARSE_ONLY=check THE_WORKBENCH_COARSE_STAGE=after THE_WORKBENCH_COARSE_EVIDENCE=tests/ui-acceptance/alignment/browser/evidence CHROMIUM_BIN='/Applications/Google Chrome.app/Contents/MacOS/Google Chrome' node web/test/workbench-smoke.mjs
THE_WEB_ASSETS=/tmp/the-alignment-coarse-ui THE_WORKBENCH_SETTINGS_ONLY=1 THE_WORKBENCH_LAYOUT_EVIDENCE=tests/ui-acceptance/alignment/browser/evidence CHROMIUM_BIN='/Applications/Google Chrome.app/Contents/MacOS/Google Chrome' node web/test/workbench-smoke.mjs
```

The Settings geometry receipt checks entry bounds and then scrolls the body,
leaving the heading and Done footer fixed. At fine-pointer 820x500 the dialog
spans y20-480 and Done y437-467; its body has 344 pixels available for 405 pixels
of content. At coarse 390x500, Done spans y423-467 and the body has 316 pixels
available for 599 pixels of content. Coarse 390x844 also passed. The PNGs show
the body after the scroll step; entry geometry is retained in the JSON.
`static-checks.txt` records the existing successful TypeScript, Node syntax and
diff-whitespace checks. No test, build or product-source change was performed
while preserving these receipts. Core sanitizer evidence is separate under the
shared evidence pack.
