/* Test-only headless module: drive the production editor event loop without
 * a display or a production automation endpoint. */
#define the_driver_module_ops native_queue_base_ops
#define the_driver_module_abi_version native_queue_base_abi
#define the_driver_module_lifecycle native_queue_base_lifecycle
#include "../src/drivers/llm/headlessdriver.c"
#undef the_driver_module_ops
#undef the_driver_module_abi_version
#undef the_driver_module_lifecycle
#include "vars.h"
#include "tools.h"
#include "editordocument.h"
#include <assert.h>

static unsigned phase, initial_job;
static unsigned long invocation;
static uint64_t original_document, closed_document;
static TheInputQueue probe_queue;
static TheDriverOps probe_ops;
static TheDriverModuleLifecycle probe_lifecycle;

static int probe_input(TheInputEvent *event)
{
   const char *other=getenv("THE_NATIVE_QUEUE_OTHER");
   switch(phase++) {
      case 0: {
         TheWorkbenchTool tool={0};TheWorkbenchAction action={0};TheInputEvent queued;
         uint64_t pin=0;
         assert(other);
         the_tools_workbench_initialize();
         if(strcmp(the_workbench_native_host_reason(),"ok")) {
            puts("NATIVE_QUEUE_UNQUALIFIED_HOST");
            return the_input_event_from_command("qquit",event);
         }
         strcpy(tool.id,"public.queue");strcpy(tool.executable,"/never-executed/native-tool");
         strcpy(action.id,"public.queueaction");strcpy(action.label,"Queued source");
         action.kind=THE_WORKBENCH_ACTION_TOOL;strcpy(action.tool,tool.id);
         assert(!the_workbench_begin("public.queueowner"));
         assert(!the_workbench_put_tool(&tool) && !the_workbench_put_action(&action));
         assert(!the_workbench_commit());
         original_document=the_editor_document_active();assert(original_document);
         initial_job=the_tool_console()->id;
         invocation=the_workbench_invocation_serial();
         the_input_queue_init(&probe_queue);
         assert(the_input_event_from_action("file.open",other,&queued));
         assert(the_input_queue_push(&probe_queue,queued));
         assert(the_input_event_from_workbench_invoke(action.id,
            the_workbench_action_find(action.id)->generation,NULL,&queued));
         assert(the_input_event_workbench_source(&queued,&pin) && pin==original_document);
         assert(the_input_queue_push(&probe_queue,queued));
         return the_input_queue_pop(&probe_queue,event);
      }
      case 1:
         assert(the_editor_document_active()!=original_document);
         return the_input_queue_pop(&probe_queue,event);
      case 2: {
         TheInputEvent queued;
         assert(!strcmp(the_workbench_last_result()->code,"stale-revision"));
         assert(the_tool_console()->id==initial_job && !the_tool_console()->running);
         assert(the_workbench_invocation_serial()==invocation+1);
         closed_document=the_editor_document_active();assert(closed_document!=original_document);
         assert(the_input_event_from_workbench_invoke("public.queueaction",
            the_workbench_action_find("public.queueaction")->generation,NULL,&queued));
         assert(the_input_queue_push(&probe_queue,queued));
         return the_input_event_from_command("qquit",event);
      }
      case 3:
         assert(the_editor_document_active()==original_document);
         return the_input_queue_pop(&probe_queue,event);
      case 4:
         assert(!strcmp(the_workbench_last_result()->code,"stale-revision"));
         assert(the_tool_console()->id==initial_job && !the_tool_console()->running);
         assert(the_workbench_invocation_serial()==invocation+2);
         puts("NATIVE_TOOL_QUEUED_SWITCH_AND_CLOSED_SOURCE_REFUSED_BEFORE_CAPTURE_PASS");
         fflush(stdout);
         return the_input_event_from_command("qquit",event);
      default: assert(0);return 0;
   }
}
int the_driver_module_abi_version(void) { return native_queue_base_abi(); }
const TheDriverOps *the_driver_module_ops(void)
{
   probe_ops=*native_queue_base_ops();probe_ops.read_input_event=probe_input;return &probe_ops;
}
const TheDriverModuleLifecycle *the_driver_module_lifecycle(void)
{
   probe_lifecycle=*native_queue_base_lifecycle();probe_lifecycle.name="curses";return &probe_lifecycle;
}
