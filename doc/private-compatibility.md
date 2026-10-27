# Separately held Shmuel compatibility kit

Shmuel Metz's PFXREXX/PFXHLASM workflows are not part of CRXTHE's bundled
features. All related source is kept in a private review ZIP outside this
repository: unchanged originals, patched Regina copies, minimal patches,
the smaller Regina adaptation and the cREXX Level G ports. The kit also
retains preparation tools, provenance, historical guides and dedicated QA.

The ports follow Shmuel's workflows; porting them does not turn them into
CRXTHE's own feature package. New public installers and portable archives
exclude every PFXREXX/PFXHLASM setup/implementation, patch and preparation
tool. Package checks reject those files. Dedicated workflow/parity tests
have moved to the private kit; routine product QA tests the editor, profiles,
macro runtime and generic panel facilities without this package.

General editor profiles, small find/open/line prompt helpers and the generic
panel examples remain. They are unrelated to the prefix macro package.
Historical releases and Git history are unchanged. Existing installations
may retain old files until separately qualified upgrade cleanup is provided.

Private-kit review, platform qualification and any external handoff are separate
from CRXTHE's public editor documentation and release process.
