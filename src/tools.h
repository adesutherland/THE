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
