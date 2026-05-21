#include <stdio.h>
#include <string.h>

#include "getch.h"
#include "inputevent.h"

static int failures = 0;

static void expect_int(const char *name, int got, int want)
{
   if (got != want)
   {
      fprintf(stderr, "%s: got %d want %d\n", name, got, want);
      failures++;
   }
}

static void expect_str(const char *name, const char *got, const char *want)
{
   if (strcmp(got, want) != 0)
   {
      fprintf(stderr, "%s: got %s want %s\n", name, got, want);
      failures++;
   }
}

static void test_named_keys_and_text(void)
{
   TheInputEvent input;
   int key = 0;

   expect_str("kind.key", the_input_kind_name(THE_INPUT_KEY), "key");
   expect_int("key.left.parse",
              the_input_event_from_key_name("left", &input), 1);
   expect_int("key.left.kind", input.kind, THE_INPUT_KEY);
   expect_int("key.left.code", input.key_code, KEY_LEFT);
   expect_int("key.f12.parse",
              the_input_event_from_key_name("F12", &input), 1);
   expect_int("key.f12.code", input.key_code, KEY_F(12));

   expect_int("text.x.parse",
              the_input_event_from_text('x', &input), 1);
   expect_int("text.x.kind", input.kind, THE_INPUT_TEXT);
   expect_int("text.x.legacy",
              the_input_event_to_legacy_key(&input, &key), 1);
   expect_int("text.x.key", key, 'x');
}

static void test_legacy_key_normalization(void)
{
   TheInputEvent input;
   int key = 0;

   expect_int("legacy.ascii.parse",
              the_input_event_from_legacy_key('a', &input), 1);
   expect_int("legacy.ascii.kind", input.kind, THE_INPUT_TEXT);
   expect_int("legacy.ascii.codepoint", input.codepoint, 'a');
   expect_int("legacy.ascii.to.key",
              the_input_event_to_legacy_key(&input, &key), 1);
   expect_int("legacy.ascii.key", key, 'a');

   expect_int("legacy.left.parse",
              the_input_event_from_legacy_key(KEY_LEFT, &input), 1);
   expect_int("legacy.left.kind", input.kind, THE_INPUT_KEY);
   expect_int("legacy.left.to.key",
              the_input_event_to_legacy_key(&input, &key), 1);
   expect_int("legacy.left.key", key, KEY_LEFT);
}

static void test_commands_targets_debug_and_queue(void)
{
   TheInputEvent input;
   TheInputEvent popped;
   TheInputQueue queue;
   int key = 0;

   expect_int("command.parse",
              the_input_event_from_command("next", &input), 1);
   expect_int("command.kind", input.kind, THE_INPUT_COMMAND);
   expect_int("command.not.legacy",
              the_input_event_to_legacy_key(&input, &key), 0);

   expect_int("target.parse",
              the_input_event_from_logical_hit(LOGICAL_CURSOR_ZONE_FILEAREA,
                                               99, 4, 7, &input), 1);
   expect_int("target.kind", input.kind, THE_INPUT_LOGICAL_HIT);
   expect_int("target.cell", input.target.cell, 7);

   expect_int("debug.parse",
              the_input_event_from_debug_command("cursor-mapping", &input), 1);
   expect_int("debug.kind", input.kind, THE_INPUT_DEBUG);
   expect_int("debug.command", input.debug_command,
              THE_INPUT_DEBUG_DUMP_CURSOR_MAPPING);
   expect_str("debug.name",
              the_input_debug_command_name(input.debug_command),
              "dump-cursor-mapping");

   the_input_queue_init(&queue);
   the_input_event_from_legacy_key(KEY_RIGHT, &input);
   expect_int("queue.push.key", the_input_queue_push(&queue, input), 1);
   the_input_event_from_command("next", &input);
   expect_int("queue.push.command", the_input_queue_push(&queue, input), 1);
   expect_int("queue.pop.key",
              the_input_queue_pop_legacy_key(&queue, &key), 1);
   expect_int("queue.pop.key.value", key, KEY_RIGHT);
   expect_int("queue.pop.command",
              the_input_queue_pop(&queue, &popped), 1);
   expect_int("queue.pop.command.kind", popped.kind, THE_INPUT_COMMAND);
}

int main(void)
{
   test_named_keys_and_text();
   test_legacy_key_normalization();
   test_commands_targets_debug_and_queue();

   if (failures != 0)
   {
      fprintf(stderr, "input event tests failed: %d\n", failures);
      return 1;
   }
   return 0;
}
