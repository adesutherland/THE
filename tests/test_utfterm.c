#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "textpos.h"
#include "utfterm.h"

static int failures = 0;

static void expect_int(const char *name, int got, int want)
{
   if (got != want)
   {
      fprintf(stderr, "%s: got %d want %d\n", name, got, want);
      failures++;
   }
}

static void expect_size(const char *name, size_t got, size_t want)
{
   if (got != want)
   {
      fprintf(stderr, "%s: got %zu want %zu\n", name, got, want);
      failures++;
   }
}

static void expect_string(const char *name, const char *got, const char *want)
{
   if (strcmp(got, want) != 0)
   {
      fprintf(stderr, "%s: got %s want %s\n", name, got, want);
      failures++;
   }
}

static const Utf8TerminalProfileEntry *expect_entry(
   const char *name, Utf8TerminalClass feature_class, Utf8TerminalDisplayMode display)
{
   const Utf8TerminalProfileEntry *entry = utf8_terminal_profile_lookup(feature_class, display);

   if (entry == NULL)
   {
      fprintf(stderr, "%s: missing profile entry\n", name);
      failures++;
   }
   return entry;
}

static void expect_profile(const char *name,
                           Utf8TerminalClass feature_class,
                           Utf8TerminalDisplayMode display,
                           Utf8TerminalOutput output,
                           int advance_width,
                           int cursor_width,
                           Utf8TerminalStrategy cursor_strategy,
                           Utf8TerminalStrategy replacement_strategy)
{
   const Utf8TerminalProfileEntry *entry = expect_entry(name, feature_class, display);

   if (entry == NULL)
      return;
   expect_int(name, entry->output_method, output);
   expect_int(name, entry->advance_width, advance_width);
   expect_int(name, entry->cursor_width, cursor_width);
   expect_int(name, entry->cursor_strategy, cursor_strategy);
   expect_int(name, entry->replacement_strategy, replacement_strategy);
}

static void expect_repaint(const char *name,
                           Utf8TerminalClass feature_class,
                           Utf8TerminalDisplayMode display,
                           int repaint_width)
{
   const Utf8TerminalProfileEntry *entry = expect_entry(name, feature_class, display);

   if (entry == NULL)
      return;
   expect_int(name, entry->repaint_width, repaint_width);
}

static void expect_widths(const char *name,
                          Utf8TerminalClass feature_class,
                          Utf8TerminalDisplayMode display,
                          int width,
                          int advance_width,
                          int cursor_width,
                          int repaint_width)
{
   const Utf8TerminalProfileEntry *entry = expect_entry(name, feature_class, display);

   if (entry == NULL)
      return;
   expect_int(name, entry->width, width);
   expect_int(name, entry->advance_width, advance_width);
   expect_int(name, entry->cursor_width, cursor_width);
   expect_int(name, entry->repaint_width, repaint_width);
}

static void expect_substitute_codepoint(const char *name,
                                        Utf8TerminalClass feature_class,
                                        Utf8TerminalDisplayMode display,
                                        uint32_t codepoint)
{
   const Utf8TerminalProfileEntry *entry = expect_entry(name, feature_class, display);

   if (entry == NULL)
      return;
   expect_int(name, (int)entry->substitute_codepoint, (int)codepoint);
}

static void expect_mark(const char *name,
                        Utf8TerminalClass feature_class,
                        Utf8TerminalDisplayMode display,
                        Utf8TerminalMark mark)
{
   const Utf8TerminalProfileEntry *entry = expect_entry(name, feature_class, display);

   if (entry == NULL)
      return;
   expect_int(name, entry->mark, mark);
}

static void expect_metrics(const char *name,
                           Utf8TerminalClass feature_class,
                           Utf8TerminalDisplayMode display,
                           Utf8TerminalMetrics metrics)
{
   const Utf8TerminalProfileEntry *entry = expect_entry(name, feature_class, display);

   if (entry == NULL)
      return;
   expect_int(name, entry->metric_method, metrics);
}

static TextCluster cluster_after_leading_ascii(const CHARTYPE *line, size_t len)
{
   TextPos pos = textpos_next_cluster(line, len, textpos_begin());

   return textpos_cluster_at_boundary(line, len, pos);
}

static void test_coded_defaults(void)
{
   utf8_terminal_profile_reset();
   expect_size("entry.count", utf8_terminal_profile_entry_count(),
               (size_t)(UTF8_TERM_CLASS_COUNT * UTF8_TERM_DISPLAY_COUNT));
   expect_int("default.display.mode", utf8_terminal_display_mode(),
              UTF8_TERM_DISPLAY_NORMAL);
   expect_profile("default.ascii", UTF8_TERM_CLASS_ASCII, UTF8_TERM_DISPLAY_NORMAL,
                  UTF8_TERM_OUTPUT_NATIVE, 1, 1,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS);
   expect_profile("default.keycap", UTF8_TERM_CLASS_KEYCAP, UTF8_TERM_DISPLAY_NORMAL,
                  UTF8_TERM_OUTPUT_NATIVE, 2, 2,
                  UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST,
                  UTF8_TERM_STRATEGY_CLEAR_WHOLE_FAST);
   expect_profile("default.regional.indicator",
                  UTF8_TERM_CLASS_REGIONAL_INDICATOR,
                  UTF8_TERM_DISPLAY_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 2, 2,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS);
   expect_widths("default.regional.indicator.widths",
                 UTF8_TERM_CLASS_REGIONAL_INDICATOR,
                 UTF8_TERM_DISPLAY_NORMAL, 2, 2, 2, 2);
   expect_profile("default.flag", UTF8_TERM_CLASS_REGIONAL_FLAG, UTF8_TERM_DISPLAY_NORMAL,
                  UTF8_TERM_OUTPUT_NATIVE, 2, 2,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS);
   expect_profile("default.short.zwj.normal", UTF8_TERM_CLASS_SHORT_ZWJ,
                  UTF8_TERM_DISPLAY_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 2, 2,
                  UTF8_TERM_STRATEGY_LINE, UTF8_TERM_STRATEGY_LINE);
   expect_metrics("default.short.zwj.normal.metrics",
                  UTF8_TERM_CLASS_SHORT_ZWJ, UTF8_TERM_DISPLAY_NORMAL,
                  UTF8_TERM_METRICS_AUTO);
   expect_profile("default.short.zwj.decomposed", UTF8_TERM_CLASS_SHORT_ZWJ,
                  UTF8_TERM_DISPLAY_DECOMPOSED, UTF8_TERM_OUTPUT_COMPONENTS,
                  5, 5, UTF8_TERM_STRATEGY_LINE, UTF8_TERM_STRATEGY_LINE);
   expect_profile("default.keycap.decomposed", UTF8_TERM_CLASS_KEYCAP,
                  UTF8_TERM_DISPLAY_DECOMPOSED, UTF8_TERM_OUTPUT_COMPONENTS,
                  3, 3, UTF8_TERM_STRATEGY_CHANGED_CELLS,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS);
   expect_profile("default.flag.decomposed", UTF8_TERM_CLASS_REGIONAL_FLAG,
                  UTF8_TERM_DISPLAY_DECOMPOSED, UTF8_TERM_OUTPUT_COMPONENTS,
                  5, 5, UTF8_TERM_STRATEGY_CHANGED_CELLS,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS);
   expect_profile("default.keycap.single", UTF8_TERM_CLASS_KEYCAP,
                  UTF8_TERM_DISPLAY_SINGLE, UTF8_TERM_OUTPUT_BASE, 1, 1,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS);
   expect_profile("default.emoji.single", UTF8_TERM_CLASS_EMOJI,
                  UTF8_TERM_DISPLAY_SINGLE, UTF8_TERM_OUTPUT_SUBSTITUTE, 1, 1,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS);
   expect_substitute_codepoint("default.heart.single.codepoint",
                               UTF8_TERM_CLASS_HEART_ZWJ,
                               UTF8_TERM_DISPLAY_SINGLE, 0x2665u);
   expect_substitute_codepoint("default.family.single.codepoint",
                               UTF8_TERM_CLASS_FAMILY_ZWJ,
                               UTF8_TERM_DISPLAY_SINGLE, 0x2302u);
}

static void test_line_parser(void)
{
   utf8_terminal_profile_reset();
   expect_int("line.display.decomposed",
              utf8_terminal_profile_apply_line("SET UTF DISPLAY decomposed"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_int("line.display.decomposed.value", utf8_terminal_display_mode(),
              UTF8_TERM_DISPLAY_DECOMPOSED);
   expect_int("line.display.normal",
              utf8_terminal_profile_apply_line("SET UTF DISPLAY normal"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_int("line.display.normal.value", utf8_terminal_display_mode(),
              UTF8_TERM_DISPLAY_NORMAL);
   expect_int("line.display.single",
              utf8_terminal_profile_apply_line("SET UTF DISPLAY single"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_int("line.display.single.value", utf8_terminal_display_mode(),
              UTF8_TERM_DISPLAY_SINGLE);
   expect_int("line.display.single.toggle",
              utf8_terminal_profile_apply_line("SET UTF DISPLAY TOGGLE"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_int("line.display.single.toggle.value", utf8_terminal_display_mode(),
              UTF8_TERM_DISPLAY_NORMAL);
   expect_int("line.display.toggle.normal",
              utf8_terminal_profile_apply_line("SET UTF DISPLAY TOGGLE"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_int("line.display.toggle.normal.value", utf8_terminal_display_mode(),
              UTF8_TERM_DISPLAY_DECOMPOSED);
   expect_int("line.display.toggle.decomposed",
              utf8_terminal_profile_apply_line("SET UTF DISPLAY TOGGLE"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_int("line.display.toggle.decomposed.value", utf8_terminal_display_mode(),
              UTF8_TERM_DISPLAY_SINGLE);
   expect_int("line.display.toggle.single",
              utf8_terminal_profile_apply_line("SET UTF DISPLAY TOGGLE"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_int("line.display.toggle.single.value", utf8_terminal_display_mode(),
              UTF8_TERM_DISPLAY_NORMAL);
   expect_int("line.display.old.components.invalid",
              utf8_terminal_profile_apply_line("SET UTF DISPLAY components"),
              UTF8_TERMINAL_PROFILE_INVALID);
   expect_int("line.display.old.components.keeps.value", utf8_terminal_display_mode(),
              UTF8_TERM_DISPLAY_NORMAL);
   expect_int("line.display.old.grouped.invalid",
              utf8_terminal_profile_apply_line("SET UTF DISPLAY grouped"),
              UTF8_TERMINAL_PROFILE_INVALID);
   expect_int("line.display.old.grouped.keeps.value", utf8_terminal_display_mode(),
              UTF8_TERM_DISPLAY_NORMAL);
   expect_int("line.old.intent.invalid",
              utf8_terminal_profile_apply_line("SET UTF INTENT components"),
              UTF8_TERMINAL_PROFILE_INVALID);

   expect_int("line.width.advance",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS keycap WIDTH 2 ADVANCE 9 CURSOR 8 REPAINT 9"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_widths("line.keycap.width.advance",
                 UTF8_TERM_CLASS_KEYCAP, UTF8_TERM_DISPLAY_NORMAL,
                 2, 9, 8, 9);
   expect_int("line.rexx.comment",
              utf8_terminal_profile_apply_line("/* generated setting */"),
              UTF8_TERMINAL_PROFILE_IGNORED);
   expect_int("line.rexx.address",
              utf8_terminal_profile_apply_line("address the"),
              UTF8_TERMINAL_PROFILE_IGNORED);
   expect_int("line.rexx.options",
              utf8_terminal_profile_apply_line("options levelb"),
              UTF8_TERMINAL_PROFILE_IGNORED);
   expect_int("line.rexx.quoted",
              utf8_terminal_profile_apply_line(
                 "'SET UTF TERMINAL CLASS keycap WIDTH 3 ADVANCE 7 CURSOR 6 REPAINT 5'"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_profile("line.keycap.quoted", UTF8_TERM_CLASS_KEYCAP,
                  UTF8_TERM_DISPLAY_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 7, 6,
                  UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST,
                  UTF8_TERM_STRATEGY_CLEAR_WHOLE_FAST);
   expect_widths("line.keycap.quoted.widths",
                 UTF8_TERM_CLASS_KEYCAP, UTF8_TERM_DISPLAY_NORMAL,
                 3, 7, 6, 5);
   expect_int("line.width.restore",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS keycap WIDTH 2 ADVANCE 9 CURSOR 8 REPAINT 9"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_profile("line.keycap.advance", UTF8_TERM_CLASS_KEYCAP,
                  UTF8_TERM_DISPLAY_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 9, 8,
                  UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST,
                  UTF8_TERM_STRATEGY_CLEAR_WHOLE_FAST);
   expect_widths("line.keycap.advance.widths",
                 UTF8_TERM_CLASS_KEYCAP, UTF8_TERM_DISPLAY_NORMAL,
                 2, 9, 8, 9);
   expect_int("line.repaint",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS keycap REPAINT 10"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_repaint("line.keycap.repaint", UTF8_TERM_CLASS_KEYCAP,
                UTF8_TERM_DISPLAY_NORMAL, 10);
   expect_int("line.advance",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS keycap ADVANCE 11"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_widths("line.keycap.standalone.advance",
                 UTF8_TERM_CLASS_KEYCAP, UTF8_TERM_DISPLAY_NORMAL,
                 2, 11, 8, 10);
   expect_int("line.repaint.below.cursor",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS keycap REPAINT 7"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_repaint("line.keycap.repaint.below.cursor",
                UTF8_TERM_CLASS_KEYCAP, UTF8_TERM_DISPLAY_NORMAL, 7);
   expect_int("line.display.width.advance.repaint",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS short-zwj DISPLAY decomposed WIDTH 4 ADVANCE 4 CURSOR 3 REPAINT 5"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_profile("line.short.components.width.advance.repaint",
                  UTF8_TERM_CLASS_SHORT_ZWJ,
                  UTF8_TERM_DISPLAY_DECOMPOSED,
                  UTF8_TERM_OUTPUT_COMPONENTS, 4, 3,
                  UTF8_TERM_STRATEGY_LINE,
                  UTF8_TERM_STRATEGY_LINE);
   expect_widths("line.short.components.widths",
                 UTF8_TERM_CLASS_SHORT_ZWJ,
                 UTF8_TERM_DISPLAY_DECOMPOSED, 4, 4, 3, 5);
   expect_int("line.single.width.invalid",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS regional-flag DISPLAY single WIDTH 2 ADVANCE 2 CURSOR 2 REPAINT 2"),
              UTF8_TERMINAL_PROFILE_INVALID);
   expect_int("line.single.width.advance.repaint",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS regional-flag DISPLAY single WIDTH 1 ADVANCE 4 CURSOR 2 REPAINT 3"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_widths("line.single.flag.widths",
                 UTF8_TERM_CLASS_REGIONAL_FLAG,
                 UTF8_TERM_DISPLAY_SINGLE, 1, 4, 2, 3);
   expect_int("line.single.metrics.expanded.invalid",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS regional-flag DISPLAY single METRICS expanded"),
              UTF8_TERMINAL_PROFILE_INVALID);
   expect_int("line.metrics.expanded",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS short-zwj DISPLAY normal METRICS expanded"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_metrics("line.short.metrics.expanded",
                  UTF8_TERM_CLASS_SHORT_ZWJ, UTF8_TERM_DISPLAY_NORMAL,
                  UTF8_TERM_METRICS_EXPANDED);
   expect_int("line.regional.indicator.width",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS regional-indicator WIDTH 1 ADVANCE 1 CURSOR 1 REPAINT 1"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_widths("line.regional.indicator.widths",
                 UTF8_TERM_CLASS_REGIONAL_INDICATOR,
                 UTF8_TERM_DISPLAY_NORMAL, 1, 1, 1, 1);

   expect_int("line.replacement",
              utf8_terminal_profile_apply_line(
                 "terminal class keycap replacestrategy first"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_profile("line.keycap.replacement", UTF8_TERM_CLASS_KEYCAP,
                  UTF8_TERM_DISPLAY_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 11, 8,
                  UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST,
                  UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST);
   expect_widths("line.keycap.replacement.widths",
                 UTF8_TERM_CLASS_KEYCAP, UTF8_TERM_DISPLAY_NORMAL,
                 2, 11, 8, 7);

   expect_int("line.substitute.codepoint",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS short-zwj DISPLAY normal OUTPUT substitute U+0040"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_profile("line.short.normal.substitute", UTF8_TERM_CLASS_SHORT_ZWJ,
                  UTF8_TERM_DISPLAY_NORMAL, UTF8_TERM_OUTPUT_SUBSTITUTE, 1, 1,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS);
   expect_substitute_codepoint("line.short.normal.substitute.codepoint",
                               UTF8_TERM_CLASS_SHORT_ZWJ,
                               UTF8_TERM_DISPLAY_NORMAL, 0x0040u);
   expect_int("line.components.substitute",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS short-zwj DISPLAY decomposed OUTPUT substitute U+0040"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_profile("line.short.components.substitute", UTF8_TERM_CLASS_SHORT_ZWJ,
                  UTF8_TERM_DISPLAY_DECOMPOSED, UTF8_TERM_OUTPUT_SUBSTITUTE,
                  1, 1, UTF8_TERM_STRATEGY_CHANGED_CELLS,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS);
   expect_mark("line.short.components.substitute.mark",
               UTF8_TERM_CLASS_SHORT_ZWJ,
               UTF8_TERM_DISPLAY_DECOMPOSED,
               UTF8_TERM_MARK_SUBSTITUTED);

   expect_int("line.keycap.output.base",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS keycap OUTPUT base"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_profile("line.keycap.base.profile", UTF8_TERM_CLASS_KEYCAP,
                  UTF8_TERM_DISPLAY_NORMAL, UTF8_TERM_OUTPUT_BASE, 11, 8,
                  UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST,
                  UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST);
   expect_int("line.keycap.output.base.codepoint.invalid",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS keycap OUTPUT base U+002A"),
              UTF8_TERMINAL_PROFILE_INVALID);
   expect_int("line.keycap.mark.compressed",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS keycap MARK compressed"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_mark("line.keycap.mark.compressed.profile", UTF8_TERM_CLASS_KEYCAP,
               UTF8_TERM_DISPLAY_NORMAL, UTF8_TERM_MARK_COMPRESSED);
   expect_int("line.keycap.mark.invalid",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS keycap MARK bright"),
              UTF8_TERMINAL_PROFILE_INVALID);
   expect_int("line.short.components.output.components",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS short-zwj DISPLAY decomposed OUTPUT components"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_profile("line.short.components.components",
                  UTF8_TERM_CLASS_SHORT_ZWJ,
                  UTF8_TERM_DISPLAY_DECOMPOSED,
                  UTF8_TERM_OUTPUT_COMPONENTS, 1, 1,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS);
   expect_mark("line.short.components.components.mark",
               UTF8_TERM_CLASS_SHORT_ZWJ,
               UTF8_TERM_DISPLAY_DECOMPOSED,
               UTF8_TERM_MARK_NONE);

   utf8_terminal_profile_reset();
   expect_int("line.short.normal.output.components",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS short-zwj DISPLAY normal OUTPUT components"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_profile("line.short.normal.components",
                  UTF8_TERM_CLASS_SHORT_ZWJ,
                  UTF8_TERM_DISPLAY_NORMAL,
                  UTF8_TERM_OUTPUT_COMPONENTS, 2, 2,
                  UTF8_TERM_STRATEGY_LINE,
                  UTF8_TERM_STRATEGY_LINE);

   utf8_terminal_profile_reset();
   expect_int("line.keycap.substitute",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS keycap OUTPUT substitute U+002A"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_profile("line.keycap.substitute.profile", UTF8_TERM_CLASS_KEYCAP,
                  UTF8_TERM_DISPLAY_NORMAL, UTF8_TERM_OUTPUT_SUBSTITUTE, 1, 1,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS);
   expect_mark("line.keycap.substitute.mark", UTF8_TERM_CLASS_KEYCAP,
               UTF8_TERM_DISPLAY_NORMAL, UTF8_TERM_MARK_SUBSTITUTED);
   expect_substitute_codepoint("line.keycap.substitute.codepoint",
                               UTF8_TERM_CLASS_KEYCAP,
                               UTF8_TERM_DISPLAY_NORMAL, 0x002Au);
   expect_int("line.flag.substitute",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS regional-flag OUTPUT substitute U+25A1"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_profile("line.flag.substitute.profile",
                  UTF8_TERM_CLASS_REGIONAL_FLAG, UTF8_TERM_DISPLAY_NORMAL,
                  UTF8_TERM_OUTPUT_SUBSTITUTE, 1, 1,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS);
   expect_int("line.ascii.substitute",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS ascii OUTPUT substitute U+00B7"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_profile("line.ascii.substitute.profile", UTF8_TERM_CLASS_ASCII,
                  UTF8_TERM_DISPLAY_NORMAL, UTF8_TERM_OUTPUT_SUBSTITUTE, 1, 1,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS);

   expect_int("line.invalid.class",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS made-up WIDTH 1 ADVANCE 1 CURSOR 1 REPAINT 1"),
              UTF8_TERMINAL_PROFILE_INVALID);
   expect_int("line.repaint.invalid",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS keycap REPAINT 0"),
              UTF8_TERMINAL_PROFILE_INVALID);
   expect_int("line.width.advance.repaint.invalid",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS keycap WIDTH 1 ADVANCE 1 CURSOR 1 REPAINT 0"),
              UTF8_TERMINAL_PROFILE_INVALID);
   expect_int("line.old.advance.invalid",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS keycap LAYOUT 4 CURSOR 4 PAINT 2"),
              UTF8_TERMINAL_PROFILE_INVALID);
   expect_int("line.width.repaint.below.cursor",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS keycap WIDTH 2 ADVANCE 4 CURSOR 4 REPAINT 2"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_widths("line.keycap.width.repaint.below.cursor",
                 UTF8_TERM_CLASS_KEYCAP, UTF8_TERM_DISPLAY_NORMAL,
                 2, 4, 4, 2);
}

static void test_strategy_names(void)
{
   expect_int("strategy.cells", utf8_terminal_strategy_from_name("cells"),
              UTF8_TERM_STRATEGY_CHANGED_CELLS);
   expect_int("strategy.line", utf8_terminal_strategy_from_name("line"),
              UTF8_TERM_STRATEGY_LINE);
   expect_int("strategy.suffix", utf8_terminal_strategy_from_name("suffix"),
              UTF8_TERM_STRATEGY_CLEAR_CHANGED_SUFFIX_FAST);
   expect_int("strategy.prev", utf8_terminal_strategy_from_name("prev"),
              UTF8_TERM_STRATEGY_CLEAR_FROM_ONE_PRIOR_CLUSTER);
   expect_int("strategy.first", utf8_terminal_strategy_from_name("first"),
              UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST);
   expect_int("strategy.whole", utf8_terminal_strategy_from_name("whole"),
              UTF8_TERM_STRATEGY_CLEAR_WHOLE_FAST);
   expect_int("strategy.old.changed.invalid",
              utf8_terminal_strategy_from_name("changed_cells"),
              UTF8_TERM_STRATEGY_UNKNOWN);
   expect_int("strategy.old.pause.invalid",
              utf8_terminal_strategy_from_name(
                 "clear_from_first_cluster_pause"),
              UTF8_TERM_STRATEGY_UNKNOWN);
   expect_int("output.base", utf8_terminal_output_from_name("base"),
              UTF8_TERM_OUTPUT_BASE);
   expect_int("output.components", utf8_terminal_output_from_name("components"),
              UTF8_TERM_OUTPUT_COMPONENTS);
   expect_int("metrics.expanded", utf8_terminal_metrics_from_name("expanded"),
              UTF8_TERM_METRICS_EXPANDED);
   expect_int("metrics.profile", utf8_terminal_metrics_from_name("profile"),
              UTF8_TERM_METRICS_PROFILE);
   expect_int("mark.compressed", utf8_terminal_mark_from_name("compressed"),
              UTF8_TERM_MARK_COMPRESSED);
   expect_int("mark.unsafe", utf8_terminal_mark_from_name("unsafe"),
              UTF8_TERM_MARK_UNSAFE);

   expect_string("strategy.name.cells",
                 utf8_terminal_strategy_name(UTF8_TERM_STRATEGY_CHANGED_CELLS),
                 "cells");
   expect_string("strategy.name.suffix",
                 utf8_terminal_strategy_name(
                    UTF8_TERM_STRATEGY_CLEAR_CHANGED_SUFFIX_FAST),
                 "suffix");
   expect_string("strategy.name.prev",
                 utf8_terminal_strategy_name(
                    UTF8_TERM_STRATEGY_CLEAR_FROM_ONE_PRIOR_CLUSTER),
                 "prev");
   expect_string("strategy.name.first",
                 utf8_terminal_strategy_name(
                    UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST),
                 "first");
   expect_string("strategy.name.whole",
                 utf8_terminal_strategy_name(UTF8_TERM_STRATEGY_CLEAR_WHOLE_FAST),
                 "whole");
   expect_string("output.name.base",
                 utf8_terminal_output_name(UTF8_TERM_OUTPUT_BASE), "base");
   expect_string("output.name.components",
                 utf8_terminal_output_name(UTF8_TERM_OUTPUT_COMPONENTS),
                 "components");
   expect_string("metrics.name.expanded",
                 utf8_terminal_metrics_name(UTF8_TERM_METRICS_EXPANDED),
                 "expanded");
   expect_string("mark.name.compressed",
                 utf8_terminal_mark_name(UTF8_TERM_MARK_COMPRESSED),
                 "compressed");
}

static void test_profile_file(const char *profile_path)
{
   int loaded = 0;

   utf8_terminal_profile_reset();
   expect_int("system.file",
              utf8_terminal_profile_apply_file(profile_path, &loaded),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_int("system.loaded", loaded, 90);
   expect_profile("system.modifier", UTF8_TERM_CLASS_MODIFIER,
                  UTF8_TERM_DISPLAY_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 4, 4,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS,
                  UTF8_TERM_STRATEGY_LINE);
   expect_widths("system.modifier.widths", UTF8_TERM_CLASS_MODIFIER,
                 UTF8_TERM_DISPLAY_NORMAL, 2, 4, 4, 4);
   expect_profile("system.keycap", UTF8_TERM_CLASS_KEYCAP,
                  UTF8_TERM_DISPLAY_NORMAL, UTF8_TERM_OUTPUT_BASE, 1, 1,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS);
   expect_mark("system.keycap.mark", UTF8_TERM_CLASS_KEYCAP,
               UTF8_TERM_DISPLAY_NORMAL, UTF8_TERM_MARK_COMPRESSED);
   expect_repaint("system.keycap.repaint", UTF8_TERM_CLASS_KEYCAP,
                UTF8_TERM_DISPLAY_NORMAL, 1);
   expect_widths("system.regional.indicator.widths",
                 UTF8_TERM_CLASS_REGIONAL_INDICATOR,
                 UTF8_TERM_DISPLAY_NORMAL, 2, 2, 2, 2);
   expect_profile("system.regional.flag.normal",
                  UTF8_TERM_CLASS_REGIONAL_FLAG,
                  UTF8_TERM_DISPLAY_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 3, 3,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS);
   expect_widths("system.regional.flag.widths",
                 UTF8_TERM_CLASS_REGIONAL_FLAG,
                 UTF8_TERM_DISPLAY_NORMAL, 2, 3, 3, 3);
   expect_profile("system.short.normal", UTF8_TERM_CLASS_SHORT_ZWJ,
                  UTF8_TERM_DISPLAY_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 2, 2,
                  UTF8_TERM_STRATEGY_LINE,
                  UTF8_TERM_STRATEGY_LINE);
   expect_metrics("system.short.normal.metrics",
                  UTF8_TERM_CLASS_SHORT_ZWJ, UTF8_TERM_DISPLAY_NORMAL,
                  UTF8_TERM_METRICS_EXPANDED);
   expect_profile("system.heart.normal", UTF8_TERM_CLASS_HEART_ZWJ,
                  UTF8_TERM_DISPLAY_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 2, 2,
                  UTF8_TERM_STRATEGY_LINE,
                  UTF8_TERM_STRATEGY_LINE);
   expect_widths("system.heart.normal.widths",
                 UTF8_TERM_CLASS_HEART_ZWJ,
                 UTF8_TERM_DISPLAY_NORMAL, 2, 2, 2, 2);
   expect_metrics("system.heart.normal.metrics",
                  UTF8_TERM_CLASS_HEART_ZWJ, UTF8_TERM_DISPLAY_NORMAL,
                  UTF8_TERM_METRICS_EXPANDED);
   expect_profile("system.family.normal", UTF8_TERM_CLASS_FAMILY_ZWJ,
                  UTF8_TERM_DISPLAY_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 2, 2,
                  UTF8_TERM_STRATEGY_LINE,
                  UTF8_TERM_STRATEGY_LINE);
   expect_metrics("system.family.normal.metrics",
                  UTF8_TERM_CLASS_FAMILY_ZWJ, UTF8_TERM_DISPLAY_NORMAL,
                  UTF8_TERM_METRICS_EXPANDED);
   expect_substitute_codepoint("system.heart.normal.codepoint",
                               UTF8_TERM_CLASS_HEART_ZWJ,
                               UTF8_TERM_DISPLAY_NORMAL, 0x2665u);
   expect_substitute_codepoint("system.family.normal.codepoint",
                               UTF8_TERM_CLASS_FAMILY_ZWJ,
                               UTF8_TERM_DISPLAY_NORMAL, 0x2302u);
   expect_profile("system.short.components", UTF8_TERM_CLASS_SHORT_ZWJ,
                  UTF8_TERM_DISPLAY_DECOMPOSED, UTF8_TERM_OUTPUT_COMPONENTS,
                  5, 5, UTF8_TERM_STRATEGY_LINE,
                  UTF8_TERM_STRATEGY_LINE);
   expect_repaint("system.short.components.repaint", UTF8_TERM_CLASS_SHORT_ZWJ,
                UTF8_TERM_DISPLAY_DECOMPOSED, 5);
}

static void test_terminal_identity(void)
{
   utf8_terminal_profile_reset();
   expect_int("identity.apple",
              utf8_terminal_profile_apply_terminal_identity(
                 "xterm-256color", "Apple_Terminal"),
              0);
   expect_profile("identity.apple.keycap", UTF8_TERM_CLASS_KEYCAP,
                  UTF8_TERM_DISPLAY_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 2, 2,
                  UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST,
                  UTF8_TERM_STRATEGY_CLEAR_WHOLE_FAST);
   expect_mark("identity.apple.keycap.mark", UTF8_TERM_CLASS_KEYCAP,
               UTF8_TERM_DISPLAY_NORMAL, UTF8_TERM_MARK_NONE);
   expect_profile("identity.apple.short.normal", UTF8_TERM_CLASS_SHORT_ZWJ,
                  UTF8_TERM_DISPLAY_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 2, 2,
                  UTF8_TERM_STRATEGY_LINE, UTF8_TERM_STRATEGY_LINE);
   expect_metrics("identity.apple.short.normal.metrics",
                  UTF8_TERM_CLASS_SHORT_ZWJ, UTF8_TERM_DISPLAY_NORMAL,
                  UTF8_TERM_METRICS_AUTO);
}

static void test_cluster_classification(void)
{
#ifdef USE_UTF8PROC
   static const CHARTYPE keycap[] = { 'A', '1',
                                      0xEF, 0xB8, 0x8F,
                                      0xE2, 0x83, 0xA3, 'B' };
   static const CHARTYPE flag[] = { 'A',
                                    0xF0, 0x9F, 0x87, 0xAC,
                                    0xF0, 0x9F, 0x87, 0xA7, 'B' };
   static const CHARTYPE regional_indicator[] = { 'A',
                                                  0xF0, 0x9F, 0x87, 0xAC,
                                                  'B' };
   static const CHARTYPE short_zwj[] = { 'A',
                                         0xF0, 0x9F, 0x91, 0xA9,
                                         0xE2, 0x80, 0x8D,
                                         0xF0, 0x9F, 0x92, 0xBB, 'B' };
   static const CHARTYPE heart_zwj[] = { 'A',
                                         0xF0, 0x9F, 0x91, 0xA9,
                                         0xE2, 0x80, 0x8D,
                                         0xE2, 0x9D, 0xA4,
                                         0xEF, 0xB8, 0x8F,
                                         0xE2, 0x80, 0x8D,
                                         0xF0, 0x9F, 0x91, 0xA8, 'B' };
   static const CHARTYPE family_zwj[] = { 'A',
                                          0xF0, 0x9F, 0x91, 0xA8,
                                          0xE2, 0x80, 0x8D,
                                          0xF0, 0x9F, 0x91, 0xA9,
                                          0xE2, 0x80, 0x8D,
                                          0xF0, 0x9F, 0x91, 0xA7,
                                          0xE2, 0x80, 0x8D,
                                          0xF0, 0x9F, 0x91, 0xA6, 'B' };
   static const CHARTYPE modifier[] = { 'A',
                                        0xF0, 0x9F, 0x91, 0x8D,
                                        0xF0, 0x9F, 0x8F, 0xBB, 'B' };
   static const CHARTYPE text_heart[] = { 'A',
                                          0xE2, 0x99, 0xA5, 'B' };
   static const CHARTYPE text_checkbox[] = { 'A',
                                             0xE2, 0x98, 0x91, 'B' };
   static const CHARTYPE explicit_text_heart[] = { 'A',
                                                   0xE2, 0x99, 0xA5,
                                                   0xEF, 0xB8, 0x8E, 'B' };
   static const CHARTYPE emoji_heart[] = { 'A',
                                           0xE2, 0x99, 0xA5,
                                           0xEF, 0xB8, 0x8F, 'B' };
   static const CHARTYPE default_emoji_check[] = { 'A',
                                                   0xE2, 0x9C, 0x85, 'B' };

   expect_int("class.keycap",
              utf8_terminal_classify_cluster(
                 keycap, sizeof(keycap),
                 cluster_after_leading_ascii(keycap, sizeof(keycap))),
              UTF8_TERM_CLASS_KEYCAP);
   expect_int("class.regional.flag",
              utf8_terminal_classify_cluster(
                 flag, sizeof(flag),
                 cluster_after_leading_ascii(flag, sizeof(flag))),
              UTF8_TERM_CLASS_REGIONAL_FLAG);
   expect_int("class.regional.indicator",
              utf8_terminal_classify_cluster(
                 regional_indicator, sizeof(regional_indicator),
                 cluster_after_leading_ascii(regional_indicator,
                                             sizeof(regional_indicator))),
              UTF8_TERM_CLASS_REGIONAL_INDICATOR);
   expect_int("class.short.zwj",
              utf8_terminal_classify_cluster(
                 short_zwj, sizeof(short_zwj),
                 cluster_after_leading_ascii(short_zwj, sizeof(short_zwj))),
              UTF8_TERM_CLASS_SHORT_ZWJ);
   expect_int("class.heart.zwj",
              utf8_terminal_classify_cluster(
                 heart_zwj, sizeof(heart_zwj),
                 cluster_after_leading_ascii(heart_zwj, sizeof(heart_zwj))),
              UTF8_TERM_CLASS_HEART_ZWJ);
   expect_int("class.family.zwj",
              utf8_terminal_classify_cluster(
                 family_zwj, sizeof(family_zwj),
                 cluster_after_leading_ascii(family_zwj, sizeof(family_zwj))),
              UTF8_TERM_CLASS_FAMILY_ZWJ);
   expect_int("class.modifier",
              utf8_terminal_classify_cluster(
                 modifier, sizeof(modifier),
                 cluster_after_leading_ascii(modifier, sizeof(modifier))),
              UTF8_TERM_CLASS_MODIFIER);
   expect_int("class.text.heart",
              utf8_terminal_classify_cluster(
                 text_heart, sizeof(text_heart),
                 cluster_after_leading_ascii(text_heart, sizeof(text_heart))),
              UTF8_TERM_CLASS_AMBIGUOUS);
   expect_int("class.text.checkbox",
              utf8_terminal_classify_cluster(
                 text_checkbox, sizeof(text_checkbox),
                 cluster_after_leading_ascii(text_checkbox,
                                             sizeof(text_checkbox))),
              UTF8_TERM_CLASS_AMBIGUOUS);
   expect_int("class.explicit.text.heart",
              utf8_terminal_classify_cluster(
                 explicit_text_heart, sizeof(explicit_text_heart),
                 cluster_after_leading_ascii(explicit_text_heart,
                                             sizeof(explicit_text_heart))),
              UTF8_TERM_CLASS_TEXT_VARIATION);
   expect_int("class.emoji.heart",
              utf8_terminal_classify_cluster(
                 emoji_heart, sizeof(emoji_heart),
                 cluster_after_leading_ascii(emoji_heart,
                                             sizeof(emoji_heart))),
              UTF8_TERM_CLASS_EMOJI_VARIATION);
   expect_int("class.default.emoji.check",
              utf8_terminal_classify_cluster(
                 default_emoji_check, sizeof(default_emoji_check),
                 cluster_after_leading_ascii(default_emoji_check,
                                             sizeof(default_emoji_check))),
              UTF8_TERM_CLASS_EMOJI);
#endif
}

static void test_cluster_profile_lookup(void)
{
#ifdef USE_UTF8PROC
   static const CHARTYPE keycap[] = { 'A', '1',
                                      0xEF, 0xB8, 0x8F,
                                      0xE2, 0x83, 0xA3, 'B' };
   static const CHARTYPE short_zwj[] = { 'A',
                                         0xF0, 0x9F, 0x91, 0xA9,
                                         0xE2, 0x80, 0x8D,
                                         0xF0, 0x9F, 0x92, 0xBB, 'B' };
   static const CHARTYPE regional_indicator[] = { 'A',
                                                  0xF0, 0x9F, 0x87, 0xAC,
                                                  'B' };
   static const CHARTYPE flag[] = { 'A',
                                    0xF0, 0x9F, 0x87, 0xAC,
                                    0xF0, 0x9F, 0x87, 0xA7, 'B' };
   TextCluster cluster;
   const Utf8TerminalProfileEntry *entry;

   cluster = cluster_after_leading_ascii(short_zwj, sizeof(short_zwj));
   utf8_terminal_profile_reset();
   entry = utf8_terminal_profile_lookup_cluster(short_zwj, sizeof(short_zwj),
                                                cluster,
                                                UTF8_TERM_DISPLAY_NORMAL);
   if (entry == NULL)
   {
      fprintf(stderr, "lookup.short.normal: missing profile entry\n");
      failures++;
   }
   else
   {
      expect_int("lookup.short.normal.output", entry->output_method,
                 UTF8_TERM_OUTPUT_NATIVE);
      expect_int("lookup.short.normal.advance", entry->advance_width, 2);
   }

   expect_int("lookup.apple.apply",
              utf8_terminal_profile_apply_apple_terminal(), 0);
   cluster = cluster_after_leading_ascii(regional_indicator,
                                         sizeof(regional_indicator));
   entry = utf8_terminal_profile_lookup_cluster(regional_indicator,
                                                sizeof(regional_indicator),
                                                cluster,
                                                UTF8_TERM_DISPLAY_NORMAL);
   if (entry == NULL)
   {
      fprintf(stderr, "lookup.apple.regional.indicator: missing profile entry\n");
      failures++;
   }
   else
   {
      expect_int("lookup.apple.regional.indicator.class",
                 entry->feature_class,
                 UTF8_TERM_CLASS_REGIONAL_INDICATOR);
      expect_int("lookup.apple.regional.indicator.advance",
                 entry->advance_width, 2);
   }
   cluster = cluster_after_leading_ascii(flag, sizeof(flag));
   entry = utf8_terminal_profile_lookup_cluster(flag, sizeof(flag), cluster,
                                                UTF8_TERM_DISPLAY_NORMAL);
   if (entry == NULL)
   {
      fprintf(stderr, "lookup.apple.regional.flag: missing profile entry\n");
      failures++;
   }
   else
   {
      expect_int("lookup.apple.regional.flag.class",
                 entry->feature_class,
                 UTF8_TERM_CLASS_REGIONAL_FLAG);
      expect_int("lookup.apple.regional.flag.output", entry->output_method,
                 UTF8_TERM_OUTPUT_NATIVE);
      expect_int("lookup.apple.regional.flag.advance",
                 entry->advance_width, 2);
   }
   cluster = cluster_after_leading_ascii(short_zwj, sizeof(short_zwj));
   entry = utf8_terminal_profile_lookup_cluster(short_zwj, sizeof(short_zwj),
                                                cluster,
                                                UTF8_TERM_DISPLAY_NORMAL);
   if (entry == NULL)
   {
      fprintf(stderr, "lookup.apple.short.normal: missing profile entry\n");
      failures++;
   }
   else
   {
      expect_int("lookup.apple.short.normal.output", entry->output_method,
                 UTF8_TERM_OUTPUT_NATIVE);
      expect_int("lookup.apple.short.normal.advance", entry->advance_width, 2);
   }

   entry = utf8_terminal_profile_lookup_cluster(short_zwj, sizeof(short_zwj),
                                                cluster,
                                                UTF8_TERM_DISPLAY_DECOMPOSED);
   if (entry == NULL)
   {
      fprintf(stderr, "lookup.apple.short.components: missing profile entry\n");
      failures++;
   }
   else
   {
      expect_int("lookup.apple.short.components.output", entry->output_method,
                 UTF8_TERM_OUTPUT_COMPONENTS);
      expect_int("lookup.apple.short.components.advance", entry->advance_width, 5);
   }

   cluster = cluster_after_leading_ascii(keycap, sizeof(keycap));
   utf8_terminal_profile_reset();
   expect_int("lookup.keycap.apply",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS keycap WIDTH 2 ADVANCE 9 CURSOR 8 REPAINT 9"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_int("lookup.keycap.replacement.apply",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS keycap REPLACESTRATEGY first"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   entry = utf8_terminal_profile_lookup_cluster(keycap, sizeof(keycap),
                                                cluster,
                                                UTF8_TERM_DISPLAY_NORMAL);
   if (entry == NULL)
   {
      fprintf(stderr, "lookup.keycap: missing profile entry\n");
      failures++;
   }
   else
   {
      expect_int("lookup.keycap.display", entry->display_mode,
                 UTF8_TERM_DISPLAY_NORMAL);
      expect_int("lookup.keycap.advance", entry->advance_width, 9);
      expect_int("lookup.keycap.cursor", entry->cursor_width, 8);
      expect_int("lookup.keycap.repaint", entry->repaint_width, 9);
      expect_int("lookup.keycap.cursor.strategy", entry->cursor_strategy,
                 UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST);
      expect_int("lookup.keycap.replacement.strategy",
                 entry->replacement_strategy,
                 UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST);
      expect_int("lookup.keycap.transition.strategy",
                 utf8_terminal_cursor_transition_strategy(
                    utf8_terminal_profile_lookup(UTF8_TERM_CLASS_ASCII,
                                                 UTF8_TERM_DISPLAY_NORMAL),
                    entry),
                 UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST);
   }

   entry = utf8_terminal_profile_lookup_cluster(keycap, sizeof(keycap),
                                                cluster,
                                                UTF8_TERM_DISPLAY_DECOMPOSED);
   if (entry == NULL)
   {
      fprintf(stderr, "lookup.keycap.decomposed: missing profile entry\n");
      failures++;
   }
   else
   {
      expect_int("lookup.keycap.decomposed.display", entry->display_mode,
                 UTF8_TERM_DISPLAY_DECOMPOSED);
      expect_int("lookup.keycap.decomposed.output", entry->output_method,
                 UTF8_TERM_OUTPUT_COMPONENTS);
      expect_int("lookup.keycap.decomposed.advance", entry->advance_width, 3);
   }

   cluster = cluster_after_leading_ascii(short_zwj, sizeof(short_zwj));
   utf8_terminal_profile_reset();
   expect_int("lookup.global.display.components",
              utf8_terminal_profile_apply_line("SET UTF DISPLAY decomposed"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   entry = utf8_terminal_profile_lookup_cluster(short_zwj, sizeof(short_zwj),
                                                cluster,
                                                utf8_terminal_display_mode());
   if (entry == NULL)
   {
      fprintf(stderr, "lookup.global.short.components: missing profile entry\n");
      failures++;
   }
   else
   {
      expect_int("lookup.global.short.components.output", entry->output_method,
                 UTF8_TERM_OUTPUT_COMPONENTS);
      expect_int("lookup.global.short.components.advance", entry->advance_width, 5);
   }
#endif
}

static void test_physical_policy_does_not_change_logical_textpos(void)
{
#ifdef USE_UTF8PROC
   static const CHARTYPE keycap[] = { 'A', '1',
                                      0xEF, 0xB8, 0x8F,
                                      0xE2, 0x83, 0xA3, 'B' };
   TextPos pos;
   TextCluster cluster;

   utf8_terminal_profile_reset();
   expect_int("logical.profile.apply",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS keycap WIDTH 2 ADVANCE 9 CURSOR 9 REPAINT 9"),
              UTF8_TERMINAL_PROFILE_APPLIED);

   pos = textpos_next_cluster(keycap, sizeof(keycap), textpos_begin());
   cluster = textpos_cluster_at(keycap, sizeof(keycap), pos);
   expect_size("logical.keycap.cluster.count",
               textpos_count_clusters(keycap, sizeof(keycap)), 3);
   expect_int("logical.keycap.cluster.width", cluster.cell_width, 1);
#endif
}

int main(int argc, char **argv)
{
   if (argc != 2)
   {
      fprintf(stderr, "usage: %s system-profile.the\n", argv[0]);
      return 2;
   }

   test_coded_defaults();
   test_line_parser();
   test_strategy_names();
   test_profile_file(argv[1]);
   test_terminal_identity();
   test_cluster_classification();
   test_cluster_profile_lookup();
   test_physical_policy_does_not_change_logical_textpos();

   if (failures != 0)
   {
      fprintf(stderr, "utfterm tests failed: %d\n", failures);
      return 1;
   }
   return 0;
}
