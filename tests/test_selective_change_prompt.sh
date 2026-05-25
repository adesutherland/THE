#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${THE_BUILD_DIR:-${ROOT_DIR}/cmake-build-debug}"
THE_BIN="${THE_BIN:-${BUILD_DIR}/release/the}"
WORK_DIR="${BUILD_DIR}/selective-change-prompt-test"
PROFILE="${WORK_DIR}/profile.the"
SAMPLE="${WORK_DIR}/sample.txt"
TRANSCRIPT="${WORK_DIR}/combined.txt"

if [[ ! -x "${THE_BIN}" ]]; then
  echo "Skipping selective change prompt test; THE build output is missing" >&2
  exit 77
fi

if grep -aq "CREXX unavailable" "${THE_BIN}"; then
  echo "Skipping selective change prompt test; THE was built without CREXX" >&2
  exit 77
fi

if ! command -v expect >/dev/null 2>&1; then
  echo "Skipping selective change prompt test; expect(1) is missing" >&2
  exit 77
fi

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}"
printf 'alpha beta gamma\n' > "${SAMPLE}"

cat > "${PROFILE}" <<'PROFILE_EOF'
options levelb
import rxfnsb
address the

'cursor file 1 1'
say "SCHANGE_READY_SKIP"
'schange /beta/BETA/ 1'
field = .string[]
address the "extract /field/" expose field[]
say "SCHANGE_SKIP_FIELD=" || field[2] || ":" || field[3] || ":" || field[4]

'cursor file 1 1'
say "SCHANGE_READY_CHANGE"
'schange /beta/BETA/ 1'
field = .string[]
address the "extract /field/" expose field[]
say "SCHANGE_CHANGE_FIELD=" || field[1] || ":" || field[2] || ":" || field[3] || ":" || field[4]

'qquit'
PROFILE_EOF

export THE_BIN PROFILE SAMPLE
TERM="${TERM:-xterm}" expect >"${TRANSCRIPT}" 2>&1 <<'EXPECT_EOF'
set timeout 10
spawn $env(THE_BIN) -p $env(PROFILE) $env(SAMPLE)

expect {
  -re {SCHANGE_READY_SKIP} { after 100; send "n" }
  timeout { exit 2 }
  eof { exit 3 }
}
expect -re {SCHANGE_SKIP_FIELD=b:7:TEXT}

expect {
  -re {SCHANGE_READY_CHANGE} { after 100; send "c" }
  timeout { exit 4 }
  eof { exit 5 }
}
after 100
send "n"
expect -re {SCHANGE_CHANGE_FIELD=alpha BETA gamma:B:7:TEXT}
EXPECT_EOF

grep -q "SCHANGE_SKIP_FIELD=b:7:TEXT" "${TRANSCRIPT}"
grep -q "SCHANGE_CHANGE_FIELD=alpha BETA gamma:B:7:TEXT" "${TRANSCRIPT}"

echo "Selective change prompt test passed."
