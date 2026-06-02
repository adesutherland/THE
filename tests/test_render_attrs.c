#include <stdint.h>
#include <stdio.h>

#include "thecolour.h"

static int failures = 0;

static void expect_int(const char *name, int got, int want)
{
   if (got != want)
   {
      fprintf(stderr, "%s: got %d want %d\n", name, got, want);
      failures++;
   }
}

static void expect_uint32(const char *name, uint32_t got, uint32_t want)
{
   if (got != want)
   {
      fprintf(stderr, "%s: got 0x%06x want 0x%06x\n", name,
              (unsigned int)got, (unsigned int)want);
      failures++;
   }
}

static void expect_true(const char *name, int value)
{
   if (!value)
   {
      fprintf(stderr, "%s: expression was false\n", name);
      failures++;
   }
}

static void test_rgb_colour_registry(void)
{
   int red;
   int green;
   int blue;
   int colour = the_render_color_from_rgb(0x12, 0x34, 0x56);
   int again = the_render_color_from_rgb24(UINT32_C(0x123456));

   expect_true("rgb.logical.range",
               colour >= THE_RENDER_COLOR_RGB_FIRST
            && colour < (int)THE_RENDER_COLOR_LOGICAL_COUNT);
   expect_int("rgb.logical.intern", again, colour);
   expect_int("rgb.logical.is-rgb", the_render_color_is_rgb(colour), 1);
   expect_uint32("rgb.logical.rgb24", the_render_color_rgb24(colour),
                 UINT32_C(0x123456));
   expect_int("rgb.logical.components",
              the_render_color_rgb(colour, &red, &green, &blue), 1);
   expect_int("rgb.logical.red", red, 0x12);
   expect_int("rgb.logical.green", green, 0x34);
   expect_int("rgb.logical.blue", blue, 0x56);
   expect_int("rgb.invalid.red", the_render_color_from_rgb(256, 0, 0),
              THE_COLOR_UNSPECIFIED);
}

static void test_rgb_attr_and_cell_roundtrip(void)
{
   int fg = the_render_color_from_rgb24(UINT32_C(0xff8844));
   int bg = the_render_color_from_rgb24(UINT32_C(0x001122));
   TheRenderAttr attr = the_render_attr_make(
      fg, bg, THE_STYLE_BOLD | THE_STYLE_UNDERLINE);
   TheDriverCell cell = the_driver_cell_make('R', attr);
   TheRenderAttr stored = the_driver_cell_attr(cell);

   expect_int("attr.has-colour", the_render_attr_has_color(attr), 1);
   expect_int("attr.fg.logical", the_render_attr_fg(attr), fg);
   expect_int("attr.bg.logical", the_render_attr_bg(attr), bg);
   expect_int("attr.style", (int)the_render_attr_style(attr),
              THE_STYLE_BOLD | THE_STYLE_UNDERLINE);
   expect_int("cell.codepoint", (int)the_driver_cell_codepoint(cell), 'R');
   expect_int("cell.attr", stored == attr, 1);
   expect_uint32("cell.fg.rgb", the_render_color_rgb24(the_render_attr_fg(stored)),
                 UINT32_C(0xff8844));
   expect_uint32("cell.bg.rgb", the_render_color_rgb24(the_render_attr_bg(stored)),
                 UINT32_C(0x001122));
}

static void test_attr_mutators_preserve_rgb(void)
{
   int fg = the_render_color_from_rgb24(UINT32_C(0xabcdef));
   int bg = the_render_color_from_rgb24(UINT32_C(0x102030));
   int replacement = the_render_color_from_rgb24(UINT32_C(0x654321));
   TheRenderAttr attr = the_render_attr_make(fg, bg, THE_STYLE_REVERSE);
   TheRenderAttr merged = the_render_attr_merge_style(attr, THE_STYLE_BLINK);
   TheRenderAttr replaced = the_render_attr_replace_fg(merged, replacement);

   expect_uint32("merge.fg.rgb",
                 the_render_color_rgb24(the_render_attr_fg(merged)),
                 UINT32_C(0xabcdef));
   expect_uint32("merge.bg.rgb",
                 the_render_color_rgb24(the_render_attr_bg(merged)),
                 UINT32_C(0x102030));
   expect_int("merge.style", (int)the_render_attr_style(merged),
              THE_STYLE_REVERSE | THE_STYLE_BLINK);
   expect_uint32("replace.fg.rgb",
                 the_render_color_rgb24(the_render_attr_fg(replaced)),
                 UINT32_C(0x654321));
   expect_uint32("replace.bg.rgb",
                 the_render_color_rgb24(the_render_attr_bg(replaced)),
                 UINT32_C(0x102030));
}

int main(void)
{
   test_rgb_colour_registry();
   test_rgb_attr_and_cell_roundtrip();
   test_attr_mutators_preserve_rgb();

   if (failures != 0)
   {
      fprintf(stderr, "render attr tests failed: %d\n", failures);
      return 1;
   }
   return 0;
}
