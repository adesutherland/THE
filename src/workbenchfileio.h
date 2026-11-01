#ifndef THE_WORKBENCHFILEIO_H
#define THE_WORKBENCHFILEIO_H

#include <stddef.h>
#include <stdint.h>

#define THE_WORKBENCH_FILE_PATH_MAX 2048
#define THE_WORKBENCH_FILE_BYTES_MAX 8388608u

typedef struct TheWorkbenchFileJob TheWorkbenchFileJob;
typedef struct {
   const char *input, *output;
   int stream; /* 1 stdout or 2 stderr; zero when there is no output. */
} TheWorkbenchFileRequest;
typedef struct {
   int exists, readonly;
   size_t length;
   char path[THE_WORKBENCH_FILE_PATH_MAX+1], token[128];
} TheWorkbenchFileInfo;
typedef struct {
   unsigned job;
   char code[40], message[192], path[THE_WORKBENCH_FILE_PATH_MAX+1], token[128];
   size_t input_bytes, output_bytes;
   int published;
} TheWorkbenchFileReceipt;
/* Capture the canonical startup cwd once. Changing editor directories never
 * expands this workspace. Unsupported hosts expose an empty root. */
int the_workbench_fileio_initialize(const char *workspace);
const char *the_workbench_fileio_workspace(void);
int the_workbench_fileio_supported(void);
/* Side-effect-free candidate/availability validation. ready=0 permits an
 * absent final input leaf, but requires an authorized canonical parent.
 * ready=1 also checks current input/output readiness and owner policy. The
 * returned stable code is "ok" on success; the last file-job receipt is kept. */
const char *the_workbench_fileio_check(const char *path,int writing,int ready,
   char *error,size_t capacity);
/* Owner callback rechecks session readonly/policy and dirty/staged editor
 * targets at launch and immediately before publication. No callback reenters
 * the tool host. Returning zero must fill error. */
typedef int (*TheWorkbenchFileAuthority)(const char *canonical, int writing,
   int published, char *error, size_t capacity);
void the_workbench_fileio_authority(TheWorkbenchFileAuthority authority);
int the_workbench_fileio_inspect(const char *path, TheWorkbenchFileInfo *info,
   char *error, size_t capacity);
int the_workbench_fileio_inspect_json(const char *path,char *out,size_t capacity);
TheWorkbenchFileJob *the_workbench_fileio_begin(unsigned job,
   const TheWorkbenchFileRequest *request,char *error,size_t capacity);
const void *the_workbench_fileio_input(const TheWorkbenchFileJob *job,size_t *length);
int the_workbench_fileio_output(TheWorkbenchFileJob *job,int fd,const void *bytes,
   size_t length,char *error,size_t capacity);
/* Exactly once per successful begin. success means runtime code/status zero,
 * no condition, and both raw streams have drained. Frees the job in all cases.
 * A post-publication acknowledgement failure reports publication-unknown. */
int the_workbench_fileio_end(TheWorkbenchFileJob *job,int success,
   char *error,size_t capacity);
const TheWorkbenchFileReceipt *the_workbench_fileio_receipt(void);
int the_workbench_fileio_json(char *out,size_t capacity);
#endif
