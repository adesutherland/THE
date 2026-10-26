# Separately held compatibility kit

New CRXTHE packages do not contain Shmuel Metz's original macros, the minimal
patches for those originals, or the preparation/download tool. They are held
in a separate private review kit for Adrian to send directly to Shmuel after
the agreed testing and review. Preparing that ZIP does not send it.

The kit retains unchanged originals, hash/provenance information, patched
Regina copies, the patch inventory, setup macros and the reproducible
preparation script. It is outside the public source checkout and installer
payload. Earlier experimental THE releases and Git history are historical;
this change does not rewrite or retract them.

CRXTHE's independently authored prefix implementations and panel examples
remain bundled. `macro pfxsetupg REXX` and `macro pfxsetupg HLASM` use cREXX;
the smaller `macro pfxsetup` implementation uses Regina. These are distinct
from Shmuel's supplied source and its compatibility patches.

Public QA tests all 121 independent cREXX workflows without downloading the
private kit. Maintainers can separately set `THE_SHMUEL_MACRO_DIR` to an
explicitly supplied prepared directory for original-Regina and parity tests.
Installed-payload QA accepts the same external directory through `--macros`;
it no longer fetches or installs compatibility material itself.
