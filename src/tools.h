#ifndef THE_TOOLS_H
#define THE_TOOLS_H
#include "inputevent.h"
#include "toolhost.h"
#include "workbench.h"
short the_tools_workbench_run(const char *,const TheWorkbenchArguments *);
void the_tools_workbench_initialize(void);
void the_tools_initialise(const TheToolAdapter *adapter, int available);
void the_tools_fileio_initialize(void);
int the_tools_prepare_lines(const char *, const char *, const char *, const char *,
                            TheInputEvent *);
int the_tools_prepare_input(const char *action, const char *tool, const char *text, int argc,
                            const char *const *argv, TheInputEvent *event);
int the_tools_take_menu_run(TheInputEvent *event);
void the_tools_native_initialise(void);
void the_tools_native_poll(void);
int the_tools_native_language(const TheToolDefinition *);
const char *the_tools_profile_path(void);
void the_tools_profile_built(unsigned id, const char *path, unsigned source_id, unsigned revision);
/* Restore persisted tool definitions before startup profiles validate routes.
 * Metadata only: no language activation, profile execution or program launch. */
void the_tools_restore_metadata(void);
int the_tools_profile_restore(void);
int the_tools_backend_available(void);
#endif
