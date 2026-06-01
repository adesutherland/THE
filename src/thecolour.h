#ifndef THE_THECOLOUR_H
#define THE_THECOLOUR_H

#include <stdint.h>

typedef uint64_t TheRenderAttr;
typedef uint64_t TheDriverCell;
typedef uint32_t TheRenderStyle;

enum
{
   THE_COLOR_UNSPECIFIED = -1,
   THE_COLOR_BLACK = 0,
   THE_COLOR_BLUE = 1,
   THE_COLOR_GREEN = 2,
   THE_COLOR_CYAN = 3,
   THE_COLOR_RED = 4,
   THE_COLOR_MAGENTA = 5,
   THE_COLOR_YELLOW = 6,
   THE_COLOR_WHITE = 7
};

enum
{
   THE_STYLE_NORMAL = 0u,
   THE_STYLE_BOLD = 1u << 0,
   THE_STYLE_REVERSE = 1u << 1,
   THE_STYLE_UNDERLINE = 1u << 2,
   THE_STYLE_BLINK = 1u << 3,
   THE_STYLE_DIM = 1u << 4,
   THE_STYLE_ITALIC = 1u << 5,
   THE_STYLE_RIGHTLINE = 1u << 6,
   THE_STYLE_LEFTLINE = 1u << 7,
   THE_STYLE_TOPLINE = 1u << 8,
   THE_STYLE_OVERLINE = 1u << 9,
   THE_STYLE_STRIKEOUT = 1u << 10
};

#define THE_RENDER_ATTR_STYLE_BITS 16u
#define THE_RENDER_ATTR_COLOR_BITS 12u
#define THE_RENDER_ATTR_FG_SHIFT THE_RENDER_ATTR_STYLE_BITS
#define THE_RENDER_ATTR_BG_SHIFT \
   (THE_RENDER_ATTR_FG_SHIFT + THE_RENDER_ATTR_COLOR_BITS)
#define THE_RENDER_ATTR_HAS_COLOR_SHIFT \
   (THE_RENDER_ATTR_BG_SHIFT + THE_RENDER_ATTR_COLOR_BITS)
#define THE_RENDER_ATTR_STYLE_MASK \
   ((TheRenderAttr)((1u << THE_RENDER_ATTR_STYLE_BITS) - 1u))
#define THE_RENDER_ATTR_COLOR_MASK \
   ((TheRenderAttr)((1u << THE_RENDER_ATTR_COLOR_BITS) - 1u))
#define THE_RENDER_ATTR_HAS_COLOR \
   ((TheRenderAttr)1u << THE_RENDER_ATTR_HAS_COLOR_SHIFT)
#define THE_RENDER_ATTR_NORMAL ((TheRenderAttr)THE_STYLE_NORMAL)

#define THE_DRIVER_CELL_CODEPOINT_BITS 21u
#define THE_DRIVER_CELL_CODEPOINT_MASK \
   ((TheDriverCell)((1u << THE_DRIVER_CELL_CODEPOINT_BITS) - 1u))
#define THE_DRIVER_CELL_ALT_FLAG \
   ((TheDriverCell)1u << THE_DRIVER_CELL_CODEPOINT_BITS)
#define THE_DRIVER_CELL_ATTR_SHIFT (THE_DRIVER_CELL_CODEPOINT_BITS + 1u)

static inline TheRenderAttr the_render_attr_make(int fg, int bg,
                                                 TheRenderStyle style)
{
   TheRenderAttr attr = (TheRenderAttr)(style & THE_RENDER_ATTR_STYLE_MASK);

   if (fg >= 0 && bg >= 0)
   {
      attr |= THE_RENDER_ATTR_HAS_COLOR;
      attr |= ((TheRenderAttr)fg & THE_RENDER_ATTR_COLOR_MASK)
            << THE_RENDER_ATTR_FG_SHIFT;
      attr |= ((TheRenderAttr)bg & THE_RENDER_ATTR_COLOR_MASK)
            << THE_RENDER_ATTR_BG_SHIFT;
   }
   return attr;
}

static inline TheRenderAttr the_render_attr_from_style(TheRenderStyle style)
{
   return (TheRenderAttr)(style & THE_RENDER_ATTR_STYLE_MASK);
}

static inline int the_render_attr_has_color(TheRenderAttr attr)
{
   return (attr & THE_RENDER_ATTR_HAS_COLOR) != 0;
}

static inline int the_render_attr_fg(TheRenderAttr attr)
{
   if (!the_render_attr_has_color(attr))
      return THE_COLOR_UNSPECIFIED;
   return (int)((attr >> THE_RENDER_ATTR_FG_SHIFT)
              & THE_RENDER_ATTR_COLOR_MASK);
}

static inline int the_render_attr_bg(TheRenderAttr attr)
{
   if (!the_render_attr_has_color(attr))
      return THE_COLOR_UNSPECIFIED;
   return (int)((attr >> THE_RENDER_ATTR_BG_SHIFT)
              & THE_RENDER_ATTR_COLOR_MASK);
}

static inline TheRenderStyle the_render_attr_style(TheRenderAttr attr)
{
   return (TheRenderStyle)(attr & THE_RENDER_ATTR_STYLE_MASK);
}

static inline TheRenderAttr the_render_attr_merge_style(TheRenderAttr attr,
                                                        TheRenderStyle style)
{
   return (attr & ~THE_RENDER_ATTR_STYLE_MASK)
        | (TheRenderAttr)((the_render_attr_style(attr) | style)
        & THE_RENDER_ATTR_STYLE_MASK);
}

static inline TheRenderAttr the_render_attr_replace_style(TheRenderAttr attr,
                                                          TheRenderStyle style)
{
   return (attr & ~THE_RENDER_ATTR_STYLE_MASK)
        | (TheRenderAttr)(style & THE_RENDER_ATTR_STYLE_MASK);
}

static inline TheRenderAttr the_render_attr_replace_fg(TheRenderAttr attr,
                                                       int fg)
{
   int bg = the_render_attr_bg(attr);

   if (bg == THE_COLOR_UNSPECIFIED)
      return the_render_attr_from_style(the_render_attr_style(attr));
   return the_render_attr_make(fg, bg, the_render_attr_style(attr));
}

static inline TheRenderAttr the_render_attr_replace_bg(TheRenderAttr attr,
                                                       int bg)
{
   int fg = the_render_attr_fg(attr);

   if (fg == THE_COLOR_UNSPECIFIED)
      return the_render_attr_from_style(the_render_attr_style(attr));
   return the_render_attr_make(fg, bg, the_render_attr_style(attr));
}

static inline TheDriverCell the_driver_cell_make(uint32_t codepoint,
                                                 TheRenderAttr attr)
{
   return ((TheDriverCell)(attr) << THE_DRIVER_CELL_ATTR_SHIFT)
        | ((TheDriverCell)codepoint & THE_DRIVER_CELL_CODEPOINT_MASK);
}

static inline TheDriverCell the_driver_cell_make_alternate(
   uint32_t fallback_codepoint, TheRenderAttr attr)
{
   return ((TheDriverCell)(attr) << THE_DRIVER_CELL_ATTR_SHIFT)
        | THE_DRIVER_CELL_ALT_FLAG
        | ((TheDriverCell)fallback_codepoint & THE_DRIVER_CELL_CODEPOINT_MASK);
}

static inline uint32_t the_driver_cell_codepoint(TheDriverCell cell)
{
   return (uint32_t)(cell & THE_DRIVER_CELL_CODEPOINT_MASK);
}

static inline int the_driver_cell_is_alternate(TheDriverCell cell)
{
   return (cell & THE_DRIVER_CELL_ALT_FLAG) != 0;
}

static inline TheRenderAttr the_driver_cell_attr(TheDriverCell cell)
{
   return (TheRenderAttr)(cell >> THE_DRIVER_CELL_ATTR_SHIFT);
}

static inline TheDriverCell the_driver_cell_with_attr(TheDriverCell cell,
                                                      TheRenderAttr attr)
{
   if (the_driver_cell_is_alternate(cell))
      return the_driver_cell_make_alternate(the_driver_cell_codepoint(cell),
                                            attr);
   return the_driver_cell_make(the_driver_cell_codepoint(cell), attr);
}

static inline TheDriverCell the_driver_cell_merge_style(TheDriverCell cell,
                                                        TheRenderStyle style)
{
   return the_driver_cell_with_attr(
      cell, the_render_attr_merge_style(the_driver_cell_attr(cell), style));
}

#endif
