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

   expect_true(the_input_event_from_logical_target(THE_INPUT_TARGET_PREFIX,
                                                   2, 1, 2, 0, -1, &input),
               "make prefix logical hit");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply prefix logical hit");
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"zone\":\"prefix\"", "logical hit prefix focus");
   expect_contains(out, "\"line\":2", "logical hit prefix line");
   expect_contains(out, "\"cell\":2", "logical hit prefix cell");

   expect_true(the_input_event_from_logical_hit(LOGICAL_CURSOR_ZONE_FILEAREA,
                                                2, 1, 1, &input),
               "make second logical hit");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply second logical hit");
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

   expect_true(the_input_event_from_logical_target(THE_INPUT_TARGET_COMMAND,
                                                   0, 4, 2, 0, -1, &input),
               "make command logical hit");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply command logical hit");
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"zone\":\"command\"", "logical hit command focus");
   expect_contains(out, "\"cell\":2", "logical hit command cell");

   expect_true(the_input_event_from_key_name("left", &input),
               "make command left");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply command left");
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"cell\":1", "command cursor moves left");

   expect_true(the_input_event_from_key_name("right", &input),
               "make command right");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply command right");
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"cell\":2", "command cursor moves right");

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

   expect_true(the_input_event_from_command("goto 1", &input),
               "make goto for sos edit");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply goto for sos edit");
   expect_true(the_input_event_from_command("sos rightedge", &input),
               "make sos edit rightedge");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply sos edit rightedge");
   expect_true(the_input_event_from_command("sos delback", &input),
               "make sos delback");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply sos delback");
   options.mode = LLM_DRIVER_VIEW_FILEAREA;
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"t\":\"on\"", "sos delback edits filearea");

   expect_true(the_input_event_from_command("sos leftedge", &input),
               "make sos edit leftedge");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply sos edit leftedge");
   expect_true(the_input_event_from_command("sos delchar", &input),
               "make sos delchar");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply sos delchar");
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"t\":\"n\"", "sos delchar edits filearea");

   expect_true(the_input_event_from_command("sos delend", &input),
               "make sos delend");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply sos delend");
   expect_true(strcmp(agent_driver_status(&driver), "deleted to end") == 0,
               "sos delend status");

   agent_driver_free(&driver);
   agent_driver_init(&driver, 6, 80);
   expect_true(agent_driver_set_text(&driver, "  trim\n"),
               "set firstchar text");
   expect_true(the_input_event_from_command("sos firstchar", &input),
               "make sos firstchar");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply sos firstchar");
   options.mode = LLM_DRIVER_VIEW_FOCUS;
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"cell\":2", "sos firstchar skips blanks");

   agent_driver_free(&driver);
   agent_driver_init(&driver, 6, 80);
   expect_true(agent_driver_set_text(&driver,
                                     "alpha beta gamma\n"
                                     "second line\n"
                                     "third line\n"),
               "set delword and tabfield text");
   expect_true(the_input_event_from_logical_target(THE_INPUT_TARGET_FILEAREA,
                                                   1, 1, 6, 0, -1, &input),
               "make delword hit");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply delword hit");
   expect_true(the_input_event_from_command("sos delword", &input),
               "make sos delword");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply sos delword");
   options.mode = LLM_DRIVER_VIEW_FOCUS;
   options.compact = 1;
   options.include_prefix = 0;
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "alpha gamma", "sos delword edits filearea word");
   expect_contains(out, "\"cell\":6", "sos delword keeps deletion start");

   expect_true(the_input_event_from_command("sos prefix", &input),
               "make sos prefix");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply sos prefix");
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"zone\":\"prefix\"", "sos prefix focuses prefix");
   expect_contains(out, "\"cell\":0", "sos prefix first prefix cell");

   expect_true(the_input_event_from_command("sos tabfieldf", &input),
               "make sos tabfieldf");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply sos tabfieldf");
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"zone\":\"filearea\"", "tabfieldf prefix to filearea");
   expect_contains(out, "\"line\":1", "tabfieldf preserves row line");
   expect_contains(out, "\"cell\":0", "tabfieldf goes to field start");

   expect_true(the_input_event_from_command("sos tabfieldb", &input),
               "make sos tabfieldb");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply sos tabfieldb");
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"zone\":\"prefix\"", "tabfieldb filearea to prefix");
   expect_contains(out, "\"line\":1", "tabfieldb preserves row line");

   expect_true(the_input_event_from_command("sos bottomedge", &input),
               "make prefix sos bottomedge");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply prefix sos bottomedge");
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"zone\":\"prefix\"", "prefix bottomedge stays prefix");
   expect_contains(out, "\"line\":3", "prefix bottomedge last visible line");

   expect_true(the_input_event_from_command("sos topedge", &input),
               "make prefix sos topedge");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply prefix sos topedge");
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"zone\":\"prefix\"", "prefix topedge stays prefix");
   expect_contains(out, "\"line\":1", "prefix topedge first visible line");

   expect_true(the_input_event_from_command("sos leftedge", &input),
               "make prefix sos leftedge");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply prefix sos leftedge");
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"zone\":\"filearea\"", "prefix leftedge returns to filearea");
   expect_contains(out, "\"cell\":0", "prefix leftedge first file cell");

   expect_true(the_input_event_from_command("focus command", &input),
               "make command focus for delword");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply command focus for delword");
   apply_text_chars(&driver, "alpha beta");
   expect_true(the_input_event_from_logical_target(THE_INPUT_TARGET_COMMAND,
                                                   0, 4, 6, 0, -1, &input),
               "make command delword hit");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply command delword hit");
   expect_true(the_input_event_from_command("sos delword", &input),
               "make command sos delword");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply command sos delword");
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"zone\":\"command\"", "command delword stays command");
   expect_contains(out, "\"command\":\"alpha \"", "command delword edits command line");

   expect_true(the_input_event_from_command("sos tabfieldf", &input),
               "make command sos tabfieldf");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply command sos tabfieldf");
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"zone\":\"prefix\"", "tabfieldf command to prefix");
   expect_contains(out, "\"line\":1", "tabfieldf command to first visible line");

   expect_true(the_input_event_from_command("sos tabfieldb", &input),
               "make top prefix sos tabfieldb");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply top prefix sos tabfieldb");
   agent_driver_format(&driver, &options, out, sizeof(out));
   expect_contains(out, "\"zone\":\"command\"", "tabfieldb top prefix to command");
   expect_contains(out, "\"cell\":0", "tabfieldb command first cell");

   expect_true(the_input_event_from_logical_target(THE_INPUT_TARGET_STATUS,
                                                   0, 5, 0, 0, -1, &input),
               "make status logical hit");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply status logical hit");
   expect_true(strcmp(agent_driver_status(&driver), "status hit") == 0,
               "status logical hit status");

   expect_true(the_input_event_from_logical_target(THE_INPUT_TARGET_TABLINE,
                                                   0, 0, 0, 0, -1, &input),
               "make tabline logical hit");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply tabline logical hit");
   expect_true(strcmp(agent_driver_status(&driver), "tabline hit") == 0,
               "tabline logical hit status");

   expect_true(the_input_event_from_logical_target(THE_INPUT_TARGET_DIVIDER,
                                                   0, 3, 0, 0, -1, &input),
               "make divider logical hit");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply divider logical hit");
   expect_true(strcmp(agent_driver_status(&driver), "divider hit") == 0,
               "divider logical hit status");

   expect_true(the_input_event_from_logical_target(THE_INPUT_TARGET_WINDOW,
                                                   0, 2, 0, 1, 0, &input),
               "make window logical hit");
   expect_true(agent_driver_apply_input(&driver, &input),
               "apply window logical hit");
   expect_true(strcmp(agent_driver_status(&driver), "window selected") == 0,
               "window logical hit status");

   expect_true(the_input_event_from_command("sos makecurr", &input),
               "make unsupported command");
   expect_true(!agent_driver_apply_input(&driver, &input),
               "reject unsupported command");
   expect_true(strcmp(agent_driver_status(&driver), "unsupported command") == 0,
               "unsupported command status");

   agent_driver_free(&driver);
   return failures == 0 ? 0 : 1;
}
