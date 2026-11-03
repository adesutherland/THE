/* A test-only driver exercises the production editor owner without a display
 * or a real tool dependency. Actual compiler/profile journeys stay separate. */
#define the_driver_module_ops file_snapshot_base_ops
#define the_driver_module_abi_version file_snapshot_base_abi
#define the_driver_module_lifecycle file_snapshot_base_lifecycle
#include "../src/drivers/llm/headlessdriver.c"
#undef the_driver_module_ops
#undef the_driver_module_abi_version
#undef the_driver_module_lifecycle
#include "vars.h"
#include "tools.h"
#include "editorsource.h"
#include <assert.h>

static unsigned phase,job_id;
static TheToolsFileSnapshot snapshot_initial;
static LogicalCursorState cursor;
static int window;
static TheDriverOps probe_ops;
static TheDriverModuleLifecycle probe_lifecycle;
static const char expected[]="alpha\nZo\303\253\360\237\230\200\n";
static char json[65536];

static int start(const TheToolRequest *request,char *error,size_t capacity)
{
   (void)error;(void)capacity;
   assert(request->source_id==snapshot_initial.source_id && request->revision==snapshot_initial.revision);
   assert(request->source_length==sizeof(expected)-1);
   assert(!memcmp(request->source,expected,sizeof(expected)-1));
   job_id=request->id;the_tool_started(job_id);return 1;
}
static void unchanged(void)
{
   const TheEditorSource *source=the_editor_source_prepare();
   assert(source && source->id==snapshot_initial.source_id && source->revision==snapshot_initial.revision);
   assert(source->length==sizeof(expected)-1 && !memcmp(source->bytes,expected,sizeof(expected)-1));
   assert(!the_editor_buffer_dirty(CURRENT_FILE) && CURRENT_FILE->save_alt==0);
   assert(CURRENT_VIEW->current_window==window);
   assert(!memcmp(&cursor,&CURRENT_VIEW->logical_cursor,sizeof(cursor)));
}
static int probe_input(TheInputEvent *event)
{
   switch(phase++) {
      case 0: {
         TheToolAdapter adapter={start,NULL,NULL,NULL,NULL};
         TheToolDefinition runner={0};TheWorkbenchAction build={0};
         TheToolsFileSnapshot again;
         char generation[32];
         the_tools_native_initialise();
         the_tool_host_adapter(&adapter);
         strcpy(runner.id,"snapshot-runner");strcpy(runner.label,"Snapshot runner");
         strcpy(runner.role,"runner");strcpy(runner.entry,"source-file");
         strcpy(runner.image,"@probe");strcpy(runner.extensions,".probe");runner.available=1;
         assert(the_tool_register(&runner));
         strcpy(build.id,"public.snapshot-build");strcpy(build.label,"Snapshot Build");
         build.kind=THE_WORKBENCH_ACTION_MACRO;strcpy(build.operand,"public-probe");
         build.file_build=1;strcpy(build.file_extension,".probe");
         assert(!the_workbench_begin("public.snapshot"));
         assert(!the_workbench_put_action(&build) && !the_workbench_commit());
         cursor=CURRENT_VIEW->logical_cursor;window=CURRENT_VIEW->current_window;
         assert(the_tools_file_snapshot(&snapshot_initial));
         assert(snapshot_initial.source_id && snapshot_initial.revision && !strcmp(snapshot_initial.role,"other"));
         assert(!strcmp(snapshot_initial.path,getenv("THE_FILE_SNAPSHOT_SOURCE")));
         assert(!strcmp(snapshot_initial.actions[0].id,"build") && !strcmp(snapshot_initial.actions[0].label,"Snapshot Build"));
         assert(!snapshot_initial.actions[0].enabled && !strcmp(snapshot_initial.actions[0].reason,"capability-unavailable"));
         assert(!strcmp(snapshot_initial.actions[0].tool,build.id));
         assert(snapshot_initial.actions[0].generation==the_workbench_action_find(build.id)->generation);
         assert(snapshot_initial.actions[0].generation);
         assert(!strcmp(snapshot_initial.actions[1].id,"run") && !strcmp(snapshot_initial.actions[1].label,"Run"));
         assert(snapshot_initial.actions[1].enabled && !*snapshot_initial.actions[1].reason);
         assert(!strcmp(snapshot_initial.actions[1].tool,runner.id) && !snapshot_initial.actions[1].generation);
         assert(!strcmp(snapshot_initial.actions[1].target,"current-file"));
         assert(!snapshot_initial.actions[2].enabled && !strcmp(snapshot_initial.actions[2].label,"Apply profile"));
         assert(!strcmp(snapshot_initial.actions[2].reason,"This file has no profile activation role"));
         assert(!strcmp(snapshot_initial.profile.build.state,"none") && !*snapshot_initial.profile.build.image);
         assert(!snapshot_initial.result.serial);
         assert(the_tools_file_snapshot(&again) && !memcmp(&snapshot_initial,&again,sizeof(snapshot_initial)));
         unchanged();
         snprintf(generation,sizeof(generation),"%lu",snapshot_initial.actions[1].generation);
         return the_tools_prepare_lines_bound("file-run",snapshot_initial.actions[1].tool,"","",
            snapshot_initial.source_id,snapshot_initial.revision,generation,event);
      }
      case 1: {
         TheToolsFileSnapshot started,finished,again;
         assert(job_id && the_tool_console()->running);
         assert(the_tools_file_snapshot(&started));
         assert(started.result.serial==1 && !strcmp(started.result.code,"started"));
         /* Completion arrives on the owner. Projection alone must not admit it. */
         the_tool_finish(job_id,0,"0",NULL,NULL);
         assert(the_tools_file_snapshot(&again) && !memcmp(&started.result,&again.result,sizeof(again.result)));
         assert(the_tools_format_json(json,sizeof(json)));
         assert(the_tools_file_snapshot(&again) && !memcmp(&started.result,&again.result,sizeof(again.result)));
         the_tools_poll();
         assert(the_tools_file_snapshot(&finished));
         assert(finished.result.serial==started.result.serial+1 && !strcmp(finished.result.code,"completed"));
         assert(finished.result.source_id==snapshot_initial.source_id && finished.result.revision==snapshot_initial.revision);
         assert(!finished.result.rc && !strcmp(finished.result.action,"run"));
         assert(the_tools_format_json(json,sizeof(json)));puts(json);
         the_tools_poll();
         assert(the_tools_file_snapshot(&again) && !memcmp(&finished,&again,sizeof(finished)));
         assert(the_tools_format_json(json,sizeof(json)));
         assert(the_tools_file_snapshot(&again) && !memcmp(&finished,&again,sizeof(finished)));
         unchanged();
         printf("TYPED_FILE_SNAPSHOT %u %u %lu %u %d\n",snapshot_initial.source_id,snapshot_initial.revision,
            snapshot_initial.actions[0].generation,finished.result.serial,finished.result.rc);
         puts("FILE_SNAPSHOT_OWNER_POLL_AND_PROJECTION_PASS");fflush(stdout);
         return the_input_event_from_command("qquit",event);
      }
      default: assert(0);return 0;
   }
}
int the_driver_module_abi_version(void) { return file_snapshot_base_abi(); }
const TheDriverOps *the_driver_module_ops(void)
{
   probe_ops=*file_snapshot_base_ops();probe_ops.read_input_event=probe_input;return &probe_ops;
}
const TheDriverModuleLifecycle *the_driver_module_lifecycle(void)
{
   probe_lifecycle=*file_snapshot_base_lifecycle();probe_lifecycle.name="curses";return &probe_lifecycle;
}
