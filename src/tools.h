#ifndef THE_TOOLS_H
#define THE_TOOLS_H
#include "inputevent.h"
#include "toolhost.h"
void the_tools_initialise(const TheToolAdapter *adapter, int available);
int the_tools_prepare_lines(const char *, const char *, const char *, const char *,
                            TheInputEvent *);
int the_tools_prepare_input(const char *action, const char *tool, const char *text, int argc,
                            const char *const *argv, TheInputEvent *event);
void the_tools_native_initialise(void);
void the_tools_native_poll(void);
int the_tools_native_language(const TheToolDefinition *);
const char *the_tools_profile_path(void);
void the_tools_profile_built(unsigned id, const char *path, unsigned source_id, unsigned revision);
int the_tools_profile_restore(void);
int the_tools_backend_available(void);
#endif
