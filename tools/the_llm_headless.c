#include <stdio.h>
#include <string.h>

#include "agentdriver.h"
#include "transientui.h"

static int apply_command(AgentDriver *driver, const char *command)
{
   TheInputEvent input;

   if (!the_input_event_from_command(command, &input))
      return 0;
   return agent_driver_apply_input(driver, &input);
}

static void print_transient_demo(void)
{
   TransientUiReadvState readv;
   TransientUiSnapshot snapshot;
   char out[4096];

   transient_ui_readv_state_init(&readv, "sample command", -1, 0, 80);
   transient_ui_snapshot_build_readv(&snapshot, 0, 0, 80, &readv);
   transient_ui_format_snapshot(&snapshot, out, sizeof(out));
   printf("%s\n", out);
}

static int run_mini_session(const char *path)
{
   AgentDriver driver;
   LlmDriverFormatOptions options;
   char out[16384];

   agent_driver_init(&driver, 8, 100);
   if (path != NULL && !agent_driver_load_file(&driver, path))
   {
      fprintf(stderr, "the_llm_headless: unable to load %s\n", path);
      agent_driver_free(&driver);
      return 1;
   }
   if (path == NULL)
      agent_driver_set_text(&driver, "alpha beta gamma\n");

   apply_command(&driver, "find beta");
   apply_command(&driver, "replace beta delta");
   apply_command(&driver, "appendline edited by the_llm_headless");
   if (path != NULL && !apply_command(&driver, "save"))
   {
      fprintf(stderr, "the_llm_headless: mini-session save failed: %s\n",
              agent_driver_status(&driver));
      agent_driver_free(&driver);
      return 1;
   }

   llm_driver_format_options_init(&options);
   options.mode = LLM_DRIVER_VIEW_FULL;
   options.compact = 1;
   options.max_text_cols = 120;
   agent_driver_format(&driver, &options, out, sizeof(out));
   printf("the_llm_headless mini-session status: %s\n",
          agent_driver_status(&driver));
   printf("%s\n", out);
   agent_driver_free(&driver);
   return 0;
}

int main(int argc, char **argv)
{
   AgentDriver driver;
   LlmDriverFormatOptions options;
   char out[8192];

   if (argc > 1 && strcmp(argv[1], "--transient-demo") == 0)
   {
      print_transient_demo();
      return 0;
   }
   if (argc > 1 && strcmp(argv[1], "--mini-session") == 0)
      return run_mini_session(argc > 2 ? argv[2] : NULL);

   agent_driver_init(&driver, 24, 80);
   if (argc > 1 && !agent_driver_load_file(&driver, argv[1]))
   {
      fprintf(stderr, "the_llm_headless: unable to load %s\n", argv[1]);
      agent_driver_free(&driver);
      return 1;
   }
   if (argc == 1)
      agent_driver_set_text(&driver, "");

   llm_driver_format_options_init(&options);
   options.mode = LLM_DRIVER_VIEW_FULL;
   options.compact = 1;
   options.max_text_cols = 120;
   agent_driver_format(&driver, &options, out, sizeof(out));
   printf("the_llm_headless capabilities: screen transient-ui input-events\n");
   printf("%s\n", out);
   agent_driver_free(&driver);
   return 0;
}
