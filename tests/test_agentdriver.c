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

static void apply_text_chars(AgentDriver *driver, const char *text)
{
   TheInputEvent input;
   const unsigned char *ptr;

   for (ptr = (const unsigned char *)text; *ptr != '\0'; ptr++)
   {
      expect_true(the_input_event_from_text((uint32_t)*ptr, &input),
                  "make text input");
      expect_true(agent_driver_apply_input(driver, &input),
                  "apply text input");
   }
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

   expect_true(the_input_event_from_logical_hit(LOGICAL_CURSOR_ZONE_FILEAREA,
                                                2, 1, 1, &input),
               "make first logical hit");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply first logical hit");
   options.mode = LLM_DRIVER_VIEW_FOCUS;
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"zone\":\"filearea\"", "logical hit filearea focus");
   expect_contains(out, "\"line\":2", "logical hit target line");
   expect_contains(out, "\"cell\":1", "logical hit target cell");
   options.mode = LLM_DRIVER_VIEW_FILEAREA;

   expect_true(the_input_event_from_key_name("right", &input),
               "make right key");
   expect_true(agent_driver_apply_input(&driver, &input), "apply right key");
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"cell\":2", "cursor moved right");

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
   agent_driver_init(&driver, 6, 80);
   expect_true(agent_driver_set_text(&driver, "one\ntwo\n"),
               "set command cursor text");
   expect_true(the_input_event_from_command("focus command", &input),
               "make command focus");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply command focus");
   apply_text_chars(&driver, "goto 2");

   options.mode = LLM_DRIVER_VIEW_FOCUS;
   options.compact = 1;
   options.include_prefix = 0;
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"zone\":\"command\"", "command focus zone");
   expect_contains(out, "\"role\":\"command\"", "command row visible");
   expect_contains(out, "\"cell\":6", "command cursor after text");

   expect_true(the_input_event_from_key_name("left", &input),
               "make command left");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply command left");
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"cell\":5", "command cursor moves left");

   expect_true(the_input_event_from_key_name("right", &input),
               "make command right");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply command right");
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"cell\":6", "command cursor moves right");

   expect_true(the_input_event_from_key_name("enter", &input),
               "make command enter");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply command enter");
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"zone\":\"filearea\"", "enter returns to filearea");
   expect_contains(out, "\"line\":2", "entered command executed");

   expect_true(the_input_event_from_command("sos qcmnd", &input),
               "make sos qcmnd");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply sos qcmnd");
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"zone\":\"command\"", "sos qcmnd focuses command");
   expect_contains(out, "\"cell\":0", "sos qcmnd first command cell");

   expect_true(the_input_event_from_key_name("esc", &input),
               "make esc");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply esc");
   expect_true(the_input_event_from_command("goto 2", &input),
               "make goto for sos navigation");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply goto for sos navigation");
   expect_true(the_input_event_from_command("sos rightedge", &input),
               "make sos rightedge");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply sos rightedge");
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"line\":2", "sos rightedge preserves line");
   expect_contains(out, "\"cell\":3", "sos rightedge goes to line end");

   expect_true(the_input_event_from_command("sos leftedge", &input),
               "make sos leftedge");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply sos leftedge");
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"cell\":0", "sos leftedge goes to first cell");

   expect_true(the_input_event_from_command("sos bottomedge", &input),
               "make sos bottomedge");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply sos bottomedge");
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"line\":2", "sos bottomedge uses last visible file row");

   expect_true(the_input_event_from_command("sos topedge", &input),
               "make sos topedge");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply sos topedge");
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"line\":1", "sos topedge uses first visible file row");

   expect_true(the_input_event_from_command("sos delword", &input),
               "make unsupported command");
   expect_true(!agent_driver_apply_input(&driver, &input),
               "reject unsupported command");
   expect_true(strcmp(agent_driver_status(&driver), "unsupported command") == 0,
               "unsupported command status");

   agent_driver_free(&driver);
   return failures == 0 ? 0 : 1;
}
