#include <stdio.h>
#include <string.h>

#include "agentdriver.h"

static int failures = 0;

static void expect_true(int condition, const char *message)
{
   if (!condition)
   {
      fprintf(stderr, "FAIL: %s\n", message);
      failures++;
   }
}

static void expect_contains(const char *text, const char *needle,
                            const char *message)
{
   expect_true(text != NULL && strstr(text, needle) != NULL, message);
}

int main(void)
{
   AgentDriver driver;
   LlmDriverFormatOptions options;
   TheInputEvent input;
   char out[8192];

   agent_driver_init(&driver, 6, 80);
   expect_true(agent_driver_set_text(&driver,
                                     "alpha\n"
                                     "A1" "\xef\xb8\x8f" "\xe2\x83\xa3" "B\n"
                                     "omega\n"),
               "set logical text");

   llm_driver_format_options_init(&options);
   options.mode = LLM_DRIVER_VIEW_FILEAREA;
   options.compact = 1;
   options.max_text_cols = 40;
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"mode\":\"filearea\"", "compact filearea view");
   expect_contains(out, "alpha", "first line visible");
   expect_contains(out, "A1", "keycap line visible");

   expect_true(the_input_event_from_key_name("right", &input),
               "make right key");
   expect_true(agent_driver_apply_input(&driver, &input), "apply right key");
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"cell\":1", "cursor moved right");

   expect_true(the_input_event_from_command("goto 2", &input), "make goto");
   expect_true(agent_driver_apply_input(&driver, &input), "apply goto");
   expect_true(the_input_event_from_command("end", &input), "make end");
   expect_true(agent_driver_apply_input(&driver, &input), "apply end");
   expect_true(the_input_event_from_command("insert Z", &input), "make insert");
   expect_true(agent_driver_apply_input(&driver, &input), "apply insert");
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "A1", "line remains visible after insert");
   expect_contains(out, "BZ", "inserted text visible");

   options.mode = LLM_DRIVER_VIEW_FOCUS;
   options.include_prefix = 0;
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"mode\":\"focus\"", "focus view mode");
   expect_contains(out, "\"cur\":1", "focus current row");

   agent_driver_free(&driver);
   return failures == 0 ? 0 : 1;
}
