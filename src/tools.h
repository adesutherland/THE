#ifndef THE_TOOLS_H
#define THE_TOOLS_H
#include "inputevent.h"
#include "toolhost.h"
#include "workbench.h"

#define THE_TOOLS_FILE_ACTION_COUNT 3
typedef struct {
   char id[16],label[THE_WORKBENCH_LABEL_MAX+1],reason[256];
   char tool[THE_WORKBENCH_ID_MAX+1],target[16];
   int enabled;
   unsigned long generation;
} TheToolsFileActionSnapshot;
typedef struct {
   unsigned source_id,revision;
   char image[THE_TOOL_PATH],name[THE_TOOL_PATH],source_path[THE_TOOL_PATH];
   char role[24],state[16];
} TheToolsBuildSnapshot;
typedef struct {
   unsigned source_id,revision;
   char image[THE_TOOL_PATH],source_path[THE_TOOL_PATH],kind[24];
} TheToolsActiveProfileSnapshot;
typedef struct {
   int enabled;
   char reason[256];
} TheToolsAvailabilitySnapshot;
typedef struct {
   char source_path[THE_TOOL_PATH];
   TheToolsBuildSnapshot build;
   TheToolsActiveProfileSnapshot active;
   TheToolsAvailabilitySnapshot apply_image;
   size_t legacy_entries;
} TheToolsProfileSnapshot;
typedef struct {
   unsigned serial,source_id,revision;
   short rc;
   char action[32],code[40],message[256];
} TheToolsFileResultSnapshot;
typedef struct {
   unsigned source_id,revision;
   char path[THE_TOOL_PATH],role[24];
   TheToolsFileActionSnapshot actions[THE_TOOLS_FILE_ACTION_COUNT];
   TheToolsProfileSnapshot profile;
   TheToolsFileResultSnapshot result;
} TheToolsFileSnapshot;

/* Editor-owner projection with owned strings. Preparing the current source
 * includes its pending record; reading never admits asynchronous completion.
 * Dispatch actions through prepare_lines_bound with these source/generation
 * tokens, which are rechecked on the editor input owner. Returns zero with a
 * zeroed snapshot while no editor view/file exists (including UI startup). */
int the_tools_file_snapshot(TheToolsFileSnapshot *);
/* Owner update: poll the host and admit completed file actions exactly once. */
void the_tools_poll(void);
short the_tools_workbench_run(const char *,const TheWorkbenchArguments *);
void the_tools_workbench_initialize(void);
void the_tools_initialise(const TheToolAdapter *adapter, int available);
void the_tools_fileio_initialize(void);
int the_tools_prepare_lines(const char *, const char *, const char *, const char *,
                            TheInputEvent *);
/* Source and published-action tokens are rechecked on the editor input owner. */
int the_tools_prepare_lines_bound(const char *,const char *,const char *,const char *,
                                  unsigned,unsigned,const char *,TheInputEvent *);
size_t the_tools_format_json(char *,size_t);
short the_tools_file_build_active(const char *,unsigned long);
int the_tools_prepare_input(const char *action, const char *tool, const char *text, int argc,
                            const char *const *argv, TheInputEvent *event);
int the_tools_take_menu_run(TheInputEvent *event);
void the_tools_native_initialise(void);
void the_tools_native_poll(void);
int the_tools_native_language(const TheToolDefinition *);
const char *the_tools_profile_path(void);
/* Primary file purpose is explicit; RX ACTIVATE retains its legacy opt-in. */
const char *the_tools_source_role(const char *path);
/* Logical output basename; hosts add uniqueness before the compound suffix. */
size_t the_tools_build_name(const char *path,char *out,size_t capacity);
void the_tools_profile_built(unsigned id, const char *path, unsigned source_id, unsigned revision);
/* Restore persisted tool definitions before startup profiles validate routes.
 * Metadata only: no language activation, profile execution or program launch. */
void the_tools_restore_metadata(void);
int the_tools_profile_restore(void);
int the_tools_backend_available(void);
#endif
