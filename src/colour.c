/***********************************************************************/
/* COLOUR.C - Colour related functions                                 */
/* This file contains all commands that can be assigned to function    */
/* keys or typed on the command line.                                  */
/***********************************************************************/
/*
 * THE - The Hessling Editor. A text editor similar to VM/CMS xedit.
 * Copyright (C) 1991-2026 Mark Hessling
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 *
 *    The Free Software Foundation, Inc.
 *    675 Mass Ave,
 *    Cambridge, MA 02139 USA.
 *
 *
 * If you make modifications to this software that you feel increases
 * it usefulness for the rest of the community, please email the
 * changes, enhancements, bug fixes as well as any and all ideas to me.
 * This software is going to be maintained and enhanced as deemed
 * necessary by the community.
 *
 * Mark Hessling, mark@rexx.org  http://www.rexx.org/
 */


#include <the.h>
#include <proto.h>
#include "thedriver.h"
#include "extended_colors.h"

TheDriverAttr set_colour(const COLOUR_ATTR *attr)
{
   if (attr == NULL)
      return THE_RENDER_ATTR_NORMAL;
   if (colour_support
   &&  attr->fg != THE_COLOR_UNSPECIFIED
   &&  attr->bg != THE_COLOR_UNSPECIFIED)
      return the_render_attr_make(attr->fg, attr->bg, attr->style);
   return the_render_attr_from_style(attr->mono_style);
}

static int THE_alloc_color(int r, int g, int b) {
    typedef struct {
        int r;
        int g;
        int b;
        int colour;
    } RGB_COLOUR_CACHE;
    static RGB_COLOUR_CACHE colour_cache[256];
    static int colour_cache_count = 0;
    int i;

    for (i = 0; i < colour_cache_count; i++) {
        if (colour_cache[i].r == r
        &&  colour_cache[i].g == g
        &&  colour_cache[i].b == b)
            return colour_cache[i].colour;
    }

    if (!the_driver_can_change_color()) return THE_COLOR_WHITE;
    static int next_color = -1;
    if (next_color == -1) next_color = the_driver_color_count() - 1;
    if (next_color > 15) {
        int c = next_color--;
        // Curses init_color expects 0-1000
        int cr = (r * 1000) / 255;
        int cg = (g * 1000) / 255;
        int cb = (b * 1000) / 255;
        the_driver_init_color(c, cr, cg, cb);
        if (colour_cache_count < (int)(sizeof(colour_cache)/sizeof(colour_cache[0]))) {
            colour_cache[colour_cache_count].r = r;
            colour_cache[colour_cache_count].g = g;
            colour_cache[colour_cache_count].b = b;
            colour_cache[colour_cache_count].colour = c;
            colour_cache_count++;
        }
        return c;
    }
    return THE_COLOR_WHITE;
}
static COLOUR_DEF _THE_FAR the_colours[ATTR_MAX] =
{
   /* foreground   background   modifier  mono                     */
   {THE_COLOR_WHITE   ,THE_COLOR_BLUE  ,THE_STYLE_NORMAL ,THE_STYLE_NORMAL                 }, /* FILEAREA    */
   {THE_COLOR_WHITE   ,THE_COLOR_BLUE  ,THE_STYLE_BOLD   ,THE_STYLE_BOLD                   }, /* CURLINE     */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL ,THE_STYLE_REVERSE                }, /* BLOCK       */
   {THE_COLOR_RED     ,THE_COLOR_WHITE ,THE_STYLE_NORMAL ,THE_STYLE_BOLD|THE_STYLE_REVERSE         }, /* CBLOCK      */
   {THE_COLOR_BLACK   ,THE_COLOR_CYAN  ,THE_STYLE_NORMAL ,THE_STYLE_BOLD                   }, /* CMDLINE     */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL ,THE_STYLE_REVERSE                }, /* IDLINE      */
   {THE_COLOR_RED     ,THE_COLOR_WHITE ,THE_STYLE_NORMAL ,THE_STYLE_BLINK                  }, /* MSGLINE     */
   {THE_COLOR_BLACK   ,THE_COLOR_CYAN  ,THE_STYLE_NORMAL ,THE_STYLE_BOLD                   }, /* ARROW       */
   {THE_COLOR_BLACK   ,THE_COLOR_CYAN  ,THE_STYLE_NORMAL ,THE_STYLE_REVERSE                }, /* PREFIX      */
   {THE_COLOR_BLACK   ,THE_COLOR_CYAN  ,THE_STYLE_NORMAL ,THE_STYLE_REVERSE                }, /* CPREFIX     */
   {THE_COLOR_RED     ,THE_COLOR_WHITE ,THE_STYLE_NORMAL ,THE_STYLE_BOLD|THE_STYLE_REVERSE         }, /* PENDING     */
   {THE_COLOR_YELLOW  ,THE_COLOR_BLUE  ,THE_STYLE_BOLD   ,THE_STYLE_BOLD                   }, /* SCALE       */
   {THE_COLOR_WHITE   ,THE_COLOR_BLUE  ,THE_STYLE_BOLD   ,THE_STYLE_BOLD                   }, /* TOFEOF      */
   {THE_COLOR_WHITE   ,THE_COLOR_BLUE  ,THE_STYLE_BOLD   ,THE_STYLE_BOLD                   }, /* CTOFEOF     */
   {THE_COLOR_YELLOW  ,THE_COLOR_BLUE  ,THE_STYLE_BOLD   ,THE_STYLE_BOLD                   }, /* TABLINE     */
   {THE_COLOR_RED     ,THE_COLOR_WHITE ,THE_STYLE_NORMAL ,THE_STYLE_BOLD                   }, /* SHADOW      */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL ,THE_STYLE_REVERSE                }, /* STATAREA    */
   {THE_COLOR_RED     ,THE_COLOR_WHITE ,THE_STYLE_NORMAL ,THE_STYLE_BOLD                   }, /* DIVIDER     */
   {THE_COLOR_WHITE   ,THE_COLOR_BLACK ,THE_STYLE_NORMAL ,THE_STYLE_BOLD                   }, /* RESERVED    */
   {THE_COLOR_MAGENTA ,THE_COLOR_CYAN  ,THE_STYLE_BLINK  ,THE_STYLE_BLINK|THE_STYLE_REVERSE        }, /* NONDISP     */
   {THE_COLOR_WHITE   ,THE_COLOR_CYAN  ,THE_STYLE_BOLD   ,THE_STYLE_REVERSE                }, /* HIGHLIGHT   */
   {THE_COLOR_YELLOW  ,THE_COLOR_CYAN  ,THE_STYLE_BOLD   ,THE_STYLE_BOLD|THE_STYLE_REVERSE         }, /* CHIGHLIGHT  */
   {THE_COLOR_WHITE   ,THE_COLOR_RED   ,THE_STYLE_BOLD   ,THE_STYLE_BOLD|THE_STYLE_REVERSE         }, /* THIGHLIGHT  */
   {THE_COLOR_BLACK   ,THE_COLOR_CYAN  ,THE_STYLE_BOLD   ,THE_STYLE_BOLD|THE_STYLE_REVERSE         }, /* SLK         */
   {THE_COLOR_BLACK   ,THE_COLOR_CYAN  ,THE_STYLE_NORMAL ,THE_STYLE_REVERSE                }, /* GAP         */
   {THE_COLOR_BLACK   ,THE_COLOR_CYAN  ,THE_STYLE_NORMAL ,THE_STYLE_REVERSE                }, /* CGAP        */
   {THE_COLOR_WHITE   ,THE_COLOR_RED   ,THE_STYLE_NORMAL ,THE_STYLE_REVERSE                }, /* ALERT       */
   {THE_COLOR_BLACK   ,THE_COLOR_WHITE ,THE_STYLE_NORMAL ,THE_STYLE_NORMAL                 }, /* DIALOG      */
   {THE_COLOR_WHITE   ,THE_COLOR_BLACK ,THE_STYLE_BOLD   ,THE_STYLE_NORMAL                 }, /* BOUNDMARK   */
   {THE_COLOR_YELLOW  ,THE_COLOR_BLUE  ,THE_STYLE_BOLD   ,THE_STYLE_NORMAL                 }, /* FILETABS    */
   {THE_COLOR_WHITE   ,THE_COLOR_BLUE  ,THE_STYLE_NORMAL ,THE_STYLE_NORMAL                 }, /* FILETABSDIV */
   {THE_COLOR_WHITE   ,THE_COLOR_BLUE  ,THE_STYLE_NORMAL ,THE_STYLE_NORMAL                 }, /* CURSORLINE  */
   {THE_COLOR_RED     ,THE_COLOR_WHITE ,THE_STYLE_NORMAL ,THE_STYLE_BOLD                   }, /* DIA-BORDER    */
   {THE_COLOR_BLACK   ,THE_COLOR_CYAN  ,THE_STYLE_NORMAL ,THE_STYLE_BOLD                   }, /* DIA-EDITFIELD */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL ,THE_STYLE_REVERSE                }, /* DIA-BUTTON    */
   {THE_COLOR_RED     ,THE_COLOR_WHITE ,THE_STYLE_NORMAL ,THE_STYLE_BOLD|THE_STYLE_REVERSE         }, /* DIA-ABUTTON   */
   {THE_COLOR_RED     ,THE_COLOR_WHITE ,THE_STYLE_NORMAL ,THE_STYLE_BOLD                   }, /* POP-BORDER    */
   {THE_COLOR_WHITE   ,THE_COLOR_BLUE  ,THE_STYLE_NORMAL ,THE_STYLE_NORMAL                 }, /* POP-CURLINE   */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL ,THE_STYLE_REVERSE                }, /* POPUP         */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL ,THE_STYLE_REVERSE                }, /* POP-DIVIDER   */
   {THE_COLOR_WHITE   ,THE_COLOR_RED   ,THE_STYLE_NORMAL ,THE_STYLE_REVERSE                }, /* CBERROR       */
   {THE_COLOR_BLACK   ,THE_COLOR_YELLOW,THE_STYLE_NORMAL ,THE_STYLE_REVERSE                }, /* CBWARN        */
   {THE_COLOR_WHITE   ,THE_COLOR_BLUE  ,THE_STYLE_NORMAL ,THE_STYLE_REVERSE                }, /* CBINFO        */
   {THE_COLOR_WHITE   ,THE_COLOR_RED   ,THE_STYLE_BOLD   ,THE_STYLE_REVERSE                }, /* PMSGERROR     */
   {THE_COLOR_BLACK   ,THE_COLOR_YELLOW,THE_STYLE_NORMAL ,THE_STYLE_REVERSE                }, /* PMSGWARN      */
   {THE_COLOR_WHITE   ,THE_COLOR_BLUE  ,THE_STYLE_NORMAL ,THE_STYLE_REVERSE                }, /* PMSGINFO      */
};

static COLOUR_DEF _THE_FAR kedit_colours[ATTR_MAX] =
{
   /* foreground   background   modifier  mono                     */
   {THE_COLOR_CYAN    ,THE_COLOR_BLUE  ,THE_STYLE_BOLD   ,THE_STYLE_NORMAL                 }, /* FILEAREA    */
   {THE_COLOR_YELLOW  ,THE_COLOR_BLUE  ,THE_STYLE_BOLD   ,THE_STYLE_BOLD                   }, /* CURLINE     */
   {THE_COLOR_CYAN    ,THE_COLOR_WHITE ,THE_STYLE_BOLD   ,THE_STYLE_REVERSE                }, /* BLOCK       */
   {THE_COLOR_YELLOW  ,THE_COLOR_WHITE ,THE_STYLE_BOLD   ,THE_STYLE_REVERSE|THE_STYLE_BOLD         }, /* CBLOCK      */
   {THE_COLOR_YELLOW  ,THE_COLOR_BLUE  ,THE_STYLE_BOLD   ,THE_STYLE_NORMAL                 }, /* CMDLINE     */
   {THE_COLOR_YELLOW  ,THE_COLOR_BLUE  ,THE_STYLE_BOLD   ,THE_STYLE_NORMAL                 }, /* IDLINE      */
   {THE_COLOR_YELLOW  ,THE_COLOR_BLUE  ,THE_STYLE_BOLD   ,THE_STYLE_BOLD                   }, /* MSGLINE     */
   {THE_COLOR_YELLOW  ,THE_COLOR_BLUE  ,THE_STYLE_BOLD   ,THE_STYLE_BOLD                   }, /* ARROW       */
   {THE_COLOR_YELLOW  ,THE_COLOR_BLUE  ,THE_STYLE_NORMAL ,THE_STYLE_NORMAL                 }, /* PREFIX      */
   {THE_COLOR_YELLOW  ,THE_COLOR_BLUE  ,THE_STYLE_NORMAL ,THE_STYLE_NORMAL                 }, /* CPREFIX     */
   {THE_COLOR_WHITE   ,THE_COLOR_BLUE  ,THE_STYLE_BOLD   ,THE_STYLE_BOLD                   }, /* PENDING     */
   {THE_COLOR_YELLOW  ,THE_COLOR_BLUE  ,THE_STYLE_BOLD   ,THE_STYLE_NORMAL                 }, /* SCALE       */
   {THE_COLOR_CYAN    ,THE_COLOR_BLUE  ,THE_STYLE_BOLD   ,THE_STYLE_NORMAL                 }, /* TOFEOF      */
   {THE_COLOR_YELLOW  ,THE_COLOR_BLUE  ,THE_STYLE_BOLD   ,THE_STYLE_BOLD                   }, /* CTOFEOF     */
   {THE_COLOR_YELLOW  ,THE_COLOR_BLUE  ,THE_STYLE_BOLD   ,THE_STYLE_NORMAL                 }, /* TABLINE     */
   {THE_COLOR_YELLOW  ,THE_COLOR_BLUE  ,THE_STYLE_NORMAL ,THE_STYLE_NORMAL                 }, /* SHADOW      */
   {THE_COLOR_YELLOW  ,THE_COLOR_BLUE  ,THE_STYLE_BOLD   ,THE_STYLE_BOLD                   }, /* STATAREA    */
   {THE_COLOR_CYAN    ,THE_COLOR_BLUE  ,THE_STYLE_BOLD   ,THE_STYLE_NORMAL                 }, /* DIVIDER     */
   {THE_COLOR_YELLOW  ,THE_COLOR_BLUE  ,THE_STYLE_BOLD   ,THE_STYLE_NORMAL                 }, /* RESERVED    */
   {THE_COLOR_MAGENTA ,THE_COLOR_CYAN  ,THE_STYLE_BLINK  ,THE_STYLE_BLINK|THE_STYLE_REVERSE        }, /* NONDISP     */
   {THE_COLOR_WHITE   ,THE_COLOR_CYAN  ,THE_STYLE_BOLD   ,THE_STYLE_REVERSE                }, /* HIGHLIGHT   */
   {THE_COLOR_YELLOW  ,THE_COLOR_CYAN  ,THE_STYLE_BOLD   ,THE_STYLE_BOLD|THE_STYLE_REVERSE         }, /* CHIGHLIGHT  */
   {THE_COLOR_WHITE   ,THE_COLOR_RED   ,THE_STYLE_NORMAL ,THE_STYLE_REVERSE                }, /* THIGHLIGHT  */
   {THE_COLOR_BLACK   ,THE_COLOR_CYAN  ,THE_STYLE_BOLD   ,THE_STYLE_BOLD|THE_STYLE_REVERSE         }, /* SLK         */
   {THE_COLOR_CYAN    ,THE_COLOR_BLUE  ,THE_STYLE_BOLD   ,THE_STYLE_NORMAL                 }, /* GAP         */
   {THE_COLOR_CYAN    ,THE_COLOR_BLUE  ,THE_STYLE_BOLD   ,THE_STYLE_NORMAL                 }, /* CGAP        */
   {THE_COLOR_WHITE   ,THE_COLOR_RED   ,THE_STYLE_NORMAL ,THE_STYLE_REVERSE                }, /* ALERT       */
   {THE_COLOR_BLACK   ,THE_COLOR_WHITE ,THE_STYLE_NORMAL ,THE_STYLE_NORMAL                 }, /* DIALOG      */
   {THE_COLOR_WHITE   ,THE_COLOR_BLACK ,THE_STYLE_BOLD   ,THE_STYLE_NORMAL                 }, /* BOUNDMARK   */
   {THE_COLOR_YELLOW  ,THE_COLOR_BLACK ,THE_STYLE_BOLD   ,THE_STYLE_NORMAL                 }, /* FILETABS    */
   {THE_COLOR_WHITE   ,THE_COLOR_BLACK ,THE_STYLE_NORMAL ,THE_STYLE_NORMAL                 }, /* FILETABSDIV */
   {THE_COLOR_CYAN    ,THE_COLOR_BLUE  ,THE_STYLE_BOLD   ,THE_STYLE_NORMAL                 }, /* CURSORLINE   */
   {THE_COLOR_CYAN    ,THE_COLOR_BLUE  ,THE_STYLE_BOLD   ,THE_STYLE_NORMAL                 }, /* DIA-BORDER    */
   {THE_COLOR_YELLOW  ,THE_COLOR_BLUE  ,THE_STYLE_BOLD   ,THE_STYLE_NORMAL                 }, /* DIA-EDITFIELD */
   {THE_COLOR_CYAN    ,THE_COLOR_WHITE ,THE_STYLE_BOLD   ,THE_STYLE_REVERSE                }, /* DIA-BUTTON    */
   {THE_COLOR_YELLOW  ,THE_COLOR_WHITE ,THE_STYLE_BOLD   ,THE_STYLE_REVERSE|THE_STYLE_BOLD         }, /* DIA-ABUTTON   */
   {THE_COLOR_CYAN    ,THE_COLOR_BLUE  ,THE_STYLE_BOLD   ,THE_STYLE_NORMAL                 }, /* POP-BORDER    */
   {THE_COLOR_CYAN    ,THE_COLOR_BLUE  ,THE_STYLE_BOLD   ,THE_STYLE_NORMAL                 }, /* POP-CURLINE   */
   {THE_COLOR_CYAN    ,THE_COLOR_WHITE ,THE_STYLE_BOLD   ,THE_STYLE_REVERSE                }, /* POPUP         */
   {THE_COLOR_YELLOW  ,THE_COLOR_BLUE  ,THE_STYLE_BOLD   ,THE_STYLE_BOLD                   }, /* POP-DIVIDER   */
};

# if defined(USE_WINGUICURSES1)
static COLOUR_DEF _THE_FAR keditw_colours[ATTR_MAX] =
{
   /* foreground   background   modifier          mono                     */
   {THE_COLOR_BLACK   ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_NORMAL                 }, /* FILEAREA    */
   {THE_COLOR_BLACK   ,THE_COLOR_WHITE ,THE_STYLE_BOLD           ,THE_STYLE_BOLD                   }, /* CURLINE     */
   {THE_COLOR_WHITE   ,THE_COLOR_BLACK ,THE_STYLE_BOLD           ,THE_STYLE_REVERSE                }, /* BLOCK       */
   {THE_COLOR_CYAN    ,THE_COLOR_BLACK ,THE_STYLE_NORMAL         ,THE_STYLE_REVERSE|THE_STYLE_BOLD         }, /* CBLOCK      */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_NORMAL                 }, /* CMDLINE     */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_NORMAL                 }, /* IDLINE      */
   {THE_COLOR_BLACK   ,THE_COLOR_CYAN  ,THE_STYLE_NORMAL         ,THE_STYLE_BOLD                   }, /* MSGLINE     */
   {THE_COLOR_CYAN    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_BOLD                   }, /* ARROW       */
   {THE_COLOR_BLACK   ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_NORMAL                 }, /* PREFIX      */
   {THE_COLOR_CYAN    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_NORMAL                 }, /* CPREFIX     */
   {THE_COLOR_RED     ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_BOLD                   }, /* PENDING     */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_NORMAL                 }, /* SCALE       */
   {THE_COLOR_GREEN   ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_NORMAL                 }, /* TOFEOF      */
   {THE_COLOR_GREEN   ,THE_COLOR_WHITE ,THE_STYLE_BOLD           ,THE_STYLE_BOLD                   }, /* CTOFEOF     */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_NORMAL                 }, /* TABLINE     */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_BOLD           ,THE_STYLE_NORMAL                 }, /* SHADOW      */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_BOLD                   }, /* STATAREA    */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_NORMAL                 }, /* DIVIDER     */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_NORMAL                 }, /* RESERVED    */
   {THE_COLOR_MAGENTA ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_REVERSE                }, /* NONDISP     */
   {THE_COLOR_BLACK   ,THE_COLOR_YELLOW,THE_STYLE_BOLD           ,THE_STYLE_REVERSE                }, /* HIGHLIGHT   */
   {THE_COLOR_GREEN   ,THE_COLOR_YELLOW,THE_STYLE_NORMAL         ,THE_STYLE_BOLD|THE_STYLE_REVERSE         }, /* CHIGHLIGHT  */
   {THE_COLOR_BLACK   ,THE_COLOR_GREEN ,THE_STYLE_NORMAL         ,THE_STYLE_BOLD                   }, /* THIGHLIGHT  */
   {THE_COLOR_BLACK   ,THE_COLOR_CYAN  ,THE_STYLE_NORMAL         ,THE_STYLE_BOLD|THE_STYLE_REVERSE         }, /* SLK         */
   {THE_COLOR_BLACK   ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_NORMAL                 }, /* GAP         */
   {THE_COLOR_BLACK   ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_NORMAL                 }, /* CGAP        */
   {THE_COLOR_WHITE   ,THE_COLOR_RED   ,THE_STYLE_BOLD           ,THE_STYLE_REVERSE                }, /* ALERT       */
   {THE_COLOR_BLACK   ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_NORMAL                 }, /* DIALOG      */
   {THE_COLOR_BLACK   ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_NORMAL                 }, /* BOUNDMARK   */
   {THE_COLOR_MAGENTA ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_NORMAL                 }, /* FILETABS    */
   {THE_COLOR_YELLOW  ,THE_COLOR_WHITE ,THE_STYLE_BOLD           ,THE_STYLE_NORMAL                 }, /* FILETABSDIV */
   {THE_COLOR_BLACK   ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_NORMAL                 }, /* CURSORLINE   */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_NORMAL                 }, /* DIA-BORDER    */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_NORMAL                 }, /* DIA-EDITFIELD */
   {THE_COLOR_WHITE   ,THE_COLOR_BLACK ,THE_STYLE_BOLD           ,THE_STYLE_REVERSE                }, /* DIA-BUTTON    */
   {THE_COLOR_CYAN    ,THE_COLOR_BLACK ,THE_STYLE_NORMAL         ,THE_STYLE_REVERSE|THE_STYLE_BOLD         }, /* DIA-ABUTTON   */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_NORMAL                 }, /* POP-BORDER    */
   {THE_COLOR_BLACK   ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_NORMAL                 }, /* POP-CURLINE   */
   {THE_COLOR_WHITE   ,THE_COLOR_BLACK ,THE_STYLE_BOLD           ,THE_STYLE_REVERSE                }, /* POPUP         */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_BOLD                   }, /* POP-DIVIDER   */
   {THE_COLOR_WHITE   ,THE_COLOR_RED   ,THE_STYLE_NORMAL         ,THE_STYLE_REVERSE                }, /* CBERROR       */
   {THE_COLOR_BLACK   ,THE_COLOR_YELLOW,THE_STYLE_NORMAL         ,THE_STYLE_REVERSE                }, /* CBWARN        */
   {THE_COLOR_WHITE   ,THE_COLOR_BLUE  ,THE_STYLE_NORMAL         ,THE_STYLE_REVERSE                }, /* CBINFO        */
   {THE_COLOR_WHITE   ,THE_COLOR_RED   ,THE_STYLE_BOLD           ,THE_STYLE_REVERSE                }, /* PMSGERROR     */
   {THE_COLOR_BLACK   ,THE_COLOR_YELLOW,THE_STYLE_NORMAL         ,THE_STYLE_REVERSE                }, /* PMSGWARN      */
   {THE_COLOR_WHITE   ,THE_COLOR_BLUE  ,THE_STYLE_NORMAL         ,THE_STYLE_REVERSE                }, /* PMSGINFO      */
};
# else
static COLOUR_DEF _THE_FAR keditw_colours[ATTR_MAX] =
{
   /* foreground   background   modifier          mono                     */
   {THE_COLOR_BLACK   ,THE_COLOR_WHITE ,THE_STYLE_NORMAL |THE_STYLE_BLINK,THE_STYLE_NORMAL                 }, /* FILEAREA    */
   {THE_COLOR_BLACK   ,THE_COLOR_WHITE ,THE_STYLE_BOLD   |THE_STYLE_BLINK,THE_STYLE_BOLD                   }, /* CURLINE     */
   {THE_COLOR_WHITE   ,THE_COLOR_BLACK ,THE_STYLE_NORMAL |THE_STYLE_BOLD ,THE_STYLE_REVERSE                }, /* BLOCK       */
   {THE_COLOR_CYAN    ,THE_COLOR_BLACK ,THE_STYLE_NORMAL         ,THE_STYLE_REVERSE|THE_STYLE_BOLD         }, /* CBLOCK      */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL |THE_STYLE_BLINK,THE_STYLE_NORMAL                 }, /* CMDLINE     */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_NORMAL                 }, /* IDLINE      */
   {THE_COLOR_BLACK   ,THE_COLOR_CYAN  ,          THE_STYLE_BLINK,THE_STYLE_BOLD                   }, /* MSGLINE     */
   {THE_COLOR_CYAN    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL |THE_STYLE_BLINK,THE_STYLE_BOLD                   }, /* ARROW       */
   {THE_COLOR_BLACK   ,THE_COLOR_WHITE ,THE_STYLE_NORMAL |THE_STYLE_BLINK,THE_STYLE_NORMAL                 }, /* PREFIX      */
   {THE_COLOR_CYAN    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL |THE_STYLE_BLINK,THE_STYLE_NORMAL                 }, /* CPREFIX     */
   {THE_COLOR_RED     ,THE_COLOR_WHITE ,THE_STYLE_NORMAL |THE_STYLE_BLINK,THE_STYLE_BOLD                   }, /* PENDING     */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL |THE_STYLE_BLINK,THE_STYLE_NORMAL                 }, /* SCALE       */
   {THE_COLOR_GREEN   ,THE_COLOR_WHITE ,THE_STYLE_NORMAL |THE_STYLE_BLINK,THE_STYLE_NORMAL                 }, /* TOFEOF      */
   {THE_COLOR_GREEN   ,THE_COLOR_WHITE ,THE_STYLE_BOLD   |THE_STYLE_BLINK,THE_STYLE_BOLD                   }, /* CTOFEOF     */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL |THE_STYLE_BLINK,THE_STYLE_NORMAL                 }, /* TABLINE     */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_BOLD   |THE_STYLE_BLINK,THE_STYLE_NORMAL                 }, /* SHADOW      */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_BOLD                   }, /* STATAREA    */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_NORMAL                 }, /* DIVIDER     */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL |THE_STYLE_BLINK,THE_STYLE_NORMAL                 }, /* RESERVED    */
   {THE_COLOR_MAGENTA ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_BLINK|THE_STYLE_REVERSE        }, /* NONDISP     */
   {THE_COLOR_BLACK   ,THE_COLOR_YELLOW,THE_STYLE_BOLD   |THE_STYLE_BLINK,THE_STYLE_REVERSE                }, /* HIGHLIGHT   */
   {THE_COLOR_GREEN   ,THE_COLOR_YELLOW,THE_STYLE_NORMAL         ,THE_STYLE_BOLD|THE_STYLE_REVERSE         }, /* CHIGHLIGHT  */
   {THE_COLOR_BLACK   ,THE_COLOR_GREEN ,THE_STYLE_NORMAL |THE_STYLE_BLINK,THE_STYLE_BOLD                   }, /* THIGHLIGHT  */
   {THE_COLOR_BLACK   ,THE_COLOR_CYAN  ,THE_STYLE_NORMAL         ,THE_STYLE_BOLD|THE_STYLE_REVERSE         }, /* SLK         */
   {THE_COLOR_BLACK   ,THE_COLOR_WHITE ,THE_STYLE_NORMAL |THE_STYLE_BLINK,THE_STYLE_NORMAL                 }, /* GAP         */
   {THE_COLOR_BLACK   ,THE_COLOR_WHITE ,THE_STYLE_NORMAL |THE_STYLE_BLINK,THE_STYLE_NORMAL                 }, /* CGAP        */
   {THE_COLOR_WHITE   ,THE_COLOR_RED   ,THE_STYLE_BOLD           ,THE_STYLE_REVERSE                }, /* ALERT       */
   {THE_COLOR_BLACK   ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_NORMAL                 }, /* DIALOG      */
   {THE_COLOR_BLACK   ,THE_COLOR_WHITE ,THE_STYLE_NORMAL |THE_STYLE_BLINK,THE_STYLE_NORMAL                 }, /* BOUNDMARK   */
   {THE_COLOR_MAGENTA ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_NORMAL                 }, /* FILETABS    */
   {THE_COLOR_YELLOW  ,THE_COLOR_WHITE ,THE_STYLE_BOLD           ,THE_STYLE_NORMAL                 }, /* FILETABSDIV */
   {THE_COLOR_BLACK   ,THE_COLOR_WHITE ,THE_STYLE_BOLD   |THE_STYLE_BLINK,THE_STYLE_NORMAL                 }, /* CURSORLINE   */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_NORMAL                 }, /* DIA-BORDER    */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL |THE_STYLE_BLINK,THE_STYLE_NORMAL                 }, /* DIA-EDITFIELD */
   {THE_COLOR_WHITE   ,THE_COLOR_BLACK ,THE_STYLE_NORMAL |THE_STYLE_BOLD ,THE_STYLE_REVERSE                }, /* DIA-BUTTON    */
   {THE_COLOR_CYAN    ,THE_COLOR_BLACK ,THE_STYLE_NORMAL         ,THE_STYLE_REVERSE|THE_STYLE_BOLD         }, /* DIA-ABUTTON   */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_NORMAL                 }, /* POP-BORDER    */
   {THE_COLOR_BLACK   ,THE_COLOR_WHITE ,THE_STYLE_NORMAL |THE_STYLE_BLINK,THE_STYLE_NORMAL                 }, /* POP-CURLINE   */
   {THE_COLOR_WHITE   ,THE_COLOR_BLACK ,THE_STYLE_NORMAL |THE_STYLE_BOLD ,THE_STYLE_REVERSE                }, /* POPUP         */
   {THE_COLOR_BLUE    ,THE_COLOR_WHITE ,THE_STYLE_NORMAL         ,THE_STYLE_BOLD                   }, /* POP-DIVIDER   */
};
# endif

static COLOUR_DEF _THE_FAR xedit_colours[ATTR_MAX] =
{
   /* foreground   background   modifier  mono                     */
   {THE_COLOR_GREEN   ,THE_COLOR_BLACK ,THE_STYLE_NORMAL ,THE_STYLE_NORMAL                 }, /* FILEAREA    */
   {THE_COLOR_CYAN    ,THE_COLOR_BLACK ,THE_STYLE_NORMAL ,THE_STYLE_BOLD                   }, /* CURLINE     */
   {THE_COLOR_BLACK   ,THE_COLOR_GREEN ,THE_STYLE_NORMAL ,THE_STYLE_REVERSE                }, /* BLOCK       */
   {THE_COLOR_CYAN    ,THE_COLOR_GREEN ,THE_STYLE_BOLD   ,THE_STYLE_BOLD|THE_STYLE_REVERSE         }, /* CBLOCK      */
   {THE_COLOR_YELLOW  ,THE_COLOR_BLACK ,THE_STYLE_NORMAL ,THE_STYLE_NORMAL                 }, /* CMDLINE     */
   {THE_COLOR_CYAN    ,THE_COLOR_BLACK ,THE_STYLE_NORMAL ,THE_STYLE_BOLD                   }, /* IDLINE      */
   {THE_COLOR_RED     ,THE_COLOR_BLACK ,THE_STYLE_BOLD   ,THE_STYLE_BOLD                   }, /* MSGLINE     */
   {THE_COLOR_CYAN    ,THE_COLOR_BLACK ,THE_STYLE_NORMAL ,THE_STYLE_BOLD                   }, /* ARROW       */
   {THE_COLOR_GREEN   ,THE_COLOR_BLACK ,THE_STYLE_NORMAL ,THE_STYLE_NORMAL                 }, /* PREFIX      */
   {THE_COLOR_GREEN   ,THE_COLOR_BLACK ,THE_STYLE_NORMAL ,THE_STYLE_NORMAL                 }, /* CPREFIX     */
   {THE_COLOR_GREEN   ,THE_COLOR_BLACK ,THE_STYLE_BOLD   ,THE_STYLE_BOLD                   }, /* PENDING     */
   {THE_COLOR_CYAN    ,THE_COLOR_BLACK ,THE_STYLE_NORMAL ,THE_STYLE_BOLD                   }, /* SCALE       */
   {THE_COLOR_GREEN   ,THE_COLOR_BLACK ,THE_STYLE_NORMAL ,THE_STYLE_NORMAL                 }, /* TOFEOF      */
   {THE_COLOR_GREEN   ,THE_COLOR_BLACK ,THE_STYLE_BOLD   ,THE_STYLE_BOLD                   }, /* CTOFEOF     */
   {THE_COLOR_GREEN   ,THE_COLOR_BLACK ,THE_STYLE_BOLD   ,THE_STYLE_BOLD                   }, /* TABLINE     */
   {THE_COLOR_GREEN   ,THE_COLOR_BLACK ,THE_STYLE_NORMAL ,THE_STYLE_NORMAL                 }, /* SHADOW      */
   {THE_COLOR_CYAN    ,THE_COLOR_BLACK ,THE_STYLE_NORMAL ,THE_STYLE_BOLD                   }, /* STATAREA    */
   {THE_COLOR_GREEN   ,THE_COLOR_BLACK ,THE_STYLE_BOLD   ,THE_STYLE_BOLD                   }, /* DIVIDER     */
   {THE_COLOR_GREEN   ,THE_COLOR_BLACK ,THE_STYLE_BOLD   ,THE_STYLE_BOLD                   }, /* RESERVED    */
   {THE_COLOR_MAGENTA ,THE_COLOR_CYAN  ,THE_STYLE_BLINK  ,THE_STYLE_BLINK|THE_STYLE_REVERSE        }, /* NONDISP     */
   {THE_COLOR_WHITE   ,THE_COLOR_CYAN  ,THE_STYLE_BOLD   ,THE_STYLE_REVERSE                }, /* HIGHLIGHT   */
   {THE_COLOR_YELLOW  ,THE_COLOR_CYAN  ,THE_STYLE_BOLD   ,THE_STYLE_BOLD|THE_STYLE_REVERSE         }, /* CHIGHLIGHT  */
   {THE_COLOR_WHITE   ,THE_COLOR_RED   ,THE_STYLE_NORMAL ,THE_STYLE_REVERSE                }, /* THIGHLIGHT  */
   {THE_COLOR_BLACK   ,THE_COLOR_GREEN ,THE_STYLE_BOLD   ,THE_STYLE_BOLD|THE_STYLE_REVERSE         }, /* SLK         */
   {THE_COLOR_GREEN   ,THE_COLOR_BLACK ,THE_STYLE_NORMAL ,THE_STYLE_NORMAL                 }, /* GAP         */
   {THE_COLOR_GREEN   ,THE_COLOR_BLACK ,THE_STYLE_NORMAL ,THE_STYLE_NORMAL                 }, /* CGAP        */
   {THE_COLOR_WHITE   ,THE_COLOR_RED   ,THE_STYLE_NORMAL ,THE_STYLE_REVERSE                }, /* ALERT       */
   {THE_COLOR_BLACK   ,THE_COLOR_WHITE ,THE_STYLE_NORMAL ,THE_STYLE_NORMAL                 }, /* DIALOG      */
   {THE_COLOR_WHITE   ,THE_COLOR_BLACK ,THE_STYLE_BOLD   ,THE_STYLE_NORMAL                 }, /* BOUNDMARK   */
   {THE_COLOR_GREEN   ,THE_COLOR_WHITE ,THE_STYLE_BOLD   ,THE_STYLE_NORMAL                 }, /* FILETABS    */
   {THE_COLOR_GREEN   ,THE_COLOR_WHITE ,THE_STYLE_NORMAL ,THE_STYLE_NORMAL                 }, /* FILETABSDIV */
   {THE_COLOR_GREEN   ,THE_COLOR_BLACK ,THE_STYLE_NORMAL ,THE_STYLE_NORMAL                 }, /* CURSORLINE   */
   {THE_COLOR_GREEN   ,THE_COLOR_BLACK ,THE_STYLE_BOLD   ,THE_STYLE_BOLD                   }, /* DIA-BORDER    */
   {THE_COLOR_YELLOW  ,THE_COLOR_BLACK ,THE_STYLE_NORMAL ,THE_STYLE_NORMAL                 }, /* DIA-EDITFIELD */
   {THE_COLOR_BLACK   ,THE_COLOR_GREEN ,THE_STYLE_NORMAL ,THE_STYLE_REVERSE                }, /* DIA-BUTTON    */
   {THE_COLOR_CYAN    ,THE_COLOR_GREEN ,THE_STYLE_BOLD   ,THE_STYLE_BOLD|THE_STYLE_REVERSE         }, /* DIA-ABUTTON   */
   {THE_COLOR_GREEN   ,THE_COLOR_BLACK ,THE_STYLE_BOLD   ,THE_STYLE_BOLD                   }, /* POP-BORDER    */
   {THE_COLOR_GREEN   ,THE_COLOR_BLACK ,THE_STYLE_NORMAL ,THE_STYLE_NORMAL                 }, /* POP-CURLINE   */
   {THE_COLOR_BLACK   ,THE_COLOR_GREEN ,THE_STYLE_NORMAL ,THE_STYLE_REVERSE                }, /* POPUP         */
   {THE_COLOR_CYAN    ,THE_COLOR_BLACK ,THE_STYLE_NORMAL ,THE_STYLE_BOLD                   }, /* POP-DIVIDER   */
   {THE_COLOR_WHITE   ,THE_COLOR_RED   ,THE_STYLE_NORMAL ,THE_STYLE_REVERSE                }, /* CBERROR       */
   {THE_COLOR_BLACK   ,THE_COLOR_YELLOW,THE_STYLE_NORMAL ,THE_STYLE_REVERSE                }, /* CBWARN        */
   {THE_COLOR_WHITE   ,THE_COLOR_BLUE  ,THE_STYLE_NORMAL ,THE_STYLE_REVERSE                }, /* CBINFO        */
   {THE_COLOR_WHITE   ,THE_COLOR_RED   ,THE_STYLE_BOLD   ,THE_STYLE_REVERSE                }, /* PMSGERROR     */
   {THE_COLOR_BLACK   ,THE_COLOR_YELLOW,THE_STYLE_NORMAL ,THE_STYLE_REVERSE                }, /* PMSGWARN      */
   {THE_COLOR_WHITE   ,THE_COLOR_BLUE  ,THE_STYLE_NORMAL ,THE_STYLE_REVERSE                }, /* PMSGINFO      */
};
/* A - comments */
/* B - strings */
/* C - numbers */
/* D - keywords */
/* E - labels */
/* F - preprocessor directives */
/* G - header lines */
/* H - extra right paren, matchable keyword */
/* I - level 1 paren */
/* J - level 1 matchable keyword */
/* K - level 1 matchable preprocessor keyword */
/* L - level 2 paren, matchable keyword */
/* M - level 3 paren, matchable keyword */
/* N - level 4 paren, matchable keyword */
/* O - level 5 paren, matchable keyword */
/* P - level 6 paren, matchable keyword */
/* Q - level 7 paren, matchable keyword */
/* R - level 8 paren or higher, matchable keyword */
/* S - incomplete string */
/* T - HTML markup tags */
/* U - HTML character/entity references */
/* V - Builtin functions */
/* W - directory */
/* X - link */
/* Y - extensions */
/* Z - executables */
/* 1 - alternate keyword color 1 */
/* 2 - alternate keyword color 2 */
/* 3 - alternate keyword color 3 */
/* 4 - alternate keyword color 4 */
/* 5 - alternate keyword color 5 */
/* 6 - alternate keyword color 6 */
/* 7 - alternate keyword color 7 */
/* 8 - alternate keyword color 8 */
/* 9 - alternate keyword color 9 */

 static COLOUR_DEF _THE_FAR the_ecolours[ECOLOUR_MAX] =
 {
  /* foreground   background   modifier  mono */
  {THE_COLOR_GREEN,   THE_COLOR_BLUE , THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* A */
  {THE_COLOR_YELLOW,  THE_COLOR_BLUE , THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* B */
  {THE_COLOR_RED,     THE_COLOR_BLUE , THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* C */
  {THE_COLOR_CYAN,    THE_COLOR_BLUE , THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* D */
  {THE_COLOR_RED,     THE_COLOR_BLUE , THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* E */
  {THE_COLOR_RED,     THE_COLOR_BLUE , THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* F */
  {THE_COLOR_RED,     THE_COLOR_BLUE , THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* G */
  {THE_COLOR_BLACK,   THE_COLOR_BLUE , THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* H */
  {THE_COLOR_GREEN,   THE_COLOR_BLUE , THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* I */
  {THE_COLOR_BLUE,    THE_COLOR_BLUE , THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* J */
  {THE_COLOR_RED,     THE_COLOR_BLUE , THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* K */
  {THE_COLOR_GREEN,   THE_COLOR_BLUE , THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* L */
  {THE_COLOR_RED,     THE_COLOR_BLUE , THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* M */
  {THE_COLOR_CYAN,    THE_COLOR_BLUE , THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* N */
  {THE_COLOR_MAGENTA, THE_COLOR_BLUE , THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* O */
  {THE_COLOR_WHITE,   THE_COLOR_BLUE , THE_STYLE_BOLD,   THE_STYLE_NORMAL}, /* P */
  {THE_COLOR_BLUE,    THE_COLOR_BLUE , THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* Q */
  {THE_COLOR_MAGENTA, THE_COLOR_BLUE , THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* R */
  {THE_COLOR_MAGENTA, THE_COLOR_BLUE , THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* S */
  {THE_COLOR_CYAN,    THE_COLOR_BLUE , THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* T */
  {THE_COLOR_RED,     THE_COLOR_BLUE , THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* U */
  {THE_COLOR_MAGENTA, THE_COLOR_BLUE , THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* V */
  {THE_COLOR_YELLOW,  THE_COLOR_BLUE , THE_STYLE_BOLD,   THE_STYLE_NORMAL}, /* W */
  {THE_COLOR_YELLOW,  THE_COLOR_BLUE , THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* X */
  {THE_COLOR_BLACK,   THE_COLOR_BLUE , THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* Y */
  {THE_COLOR_BLACK,   THE_COLOR_BLUE , THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* Z */
  {THE_COLOR_RED,     THE_COLOR_BLUE , THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* 1 */
  {THE_COLOR_YELLOW,  THE_COLOR_BLUE , THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* 2 */
  {THE_COLOR_RED,     THE_COLOR_BLUE , THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* 3 */
  {THE_COLOR_MAGENTA, THE_COLOR_BLUE , THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* 4 */
  {THE_COLOR_GREEN,   THE_COLOR_BLUE , THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* 5 */
  {THE_COLOR_CYAN,    THE_COLOR_BLUE , THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* 6 */
  {THE_COLOR_RED,     THE_COLOR_BLUE , THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* 7 */
  {THE_COLOR_BLACK,   THE_COLOR_BLUE , THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* 8 */
  {THE_COLOR_BLUE,    THE_COLOR_BLUE , THE_STYLE_BOLD ,  THE_STYLE_NORMAL}, /* 9 */
 };

 static COLOUR_DEF _THE_FAR xedit_ecolours[ECOLOUR_MAX] =
 {
  /* foreground   background   modifier  mono */
  {THE_COLOR_GREEN,   THE_COLOR_BLACK, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* A */
  {THE_COLOR_CYAN,    THE_COLOR_BLACK, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* B */
  {THE_COLOR_RED,     THE_COLOR_BLACK, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* C */
  {THE_COLOR_YELLOW,  THE_COLOR_BLACK, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* D */
  {THE_COLOR_RED,     THE_COLOR_BLACK, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* E */
  {THE_COLOR_RED,     THE_COLOR_BLACK, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* F */
  {THE_COLOR_RED,     THE_COLOR_BLACK, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* G */
  {THE_COLOR_BLACK,   THE_COLOR_BLACK, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* H */
  {THE_COLOR_GREEN,   THE_COLOR_BLACK, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* I */
  {THE_COLOR_BLUE,    THE_COLOR_BLACK, THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* J */
  {THE_COLOR_RED,     THE_COLOR_BLACK, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* K */
  {THE_COLOR_GREEN,   THE_COLOR_BLACK, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* L */
  {THE_COLOR_RED,     THE_COLOR_BLACK, THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* M */
  {THE_COLOR_CYAN,    THE_COLOR_BLACK, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* N */
  {THE_COLOR_MAGENTA, THE_COLOR_BLACK, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* O */
  {THE_COLOR_WHITE,   THE_COLOR_BLACK, THE_STYLE_BOLD,   THE_STYLE_NORMAL}, /* P */
  {THE_COLOR_BLUE,    THE_COLOR_BLACK, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* Q */
  {THE_COLOR_MAGENTA, THE_COLOR_BLACK, THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* R */
  {THE_COLOR_MAGENTA, THE_COLOR_BLACK, THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* S */
  {THE_COLOR_BLUE,    THE_COLOR_BLACK, THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* T */
  {THE_COLOR_RED,     THE_COLOR_BLACK, THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* U */
  {THE_COLOR_MAGENTA, THE_COLOR_BLACK, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* V */
  {THE_COLOR_YELLOW,  THE_COLOR_BLACK, THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* W */
  {THE_COLOR_YELLOW,  THE_COLOR_BLACK, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* X */
  {THE_COLOR_BLACK,   THE_COLOR_BLACK, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* Y */
  {THE_COLOR_BLACK,   THE_COLOR_BLACK, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* Z */
  {THE_COLOR_RED,     THE_COLOR_BLACK, THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* 1 */
  {THE_COLOR_BLUE,    THE_COLOR_BLACK, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* 2 */
  {THE_COLOR_RED,     THE_COLOR_BLACK, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* 3 */
  {THE_COLOR_MAGENTA, THE_COLOR_BLACK, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* 4 */
  {THE_COLOR_GREEN,   THE_COLOR_BLACK, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* 5 */
  {THE_COLOR_CYAN,    THE_COLOR_BLACK, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* 6 */
  {THE_COLOR_RED,     THE_COLOR_BLACK, THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* 7 */
  {THE_COLOR_BLACK,   THE_COLOR_BLACK, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* 8 */
  {THE_COLOR_BLUE,    THE_COLOR_BLACK, THE_STYLE_BOLD ,  THE_STYLE_NORMAL}, /* 9 */
 };

 static COLOUR_DEF _THE_FAR kedit_ecolours[ECOLOUR_MAX] =
 {
  /* foreground   background   modifier  mono */
  {THE_COLOR_GREEN,   THE_COLOR_BLUE,  THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* A */
  {THE_COLOR_CYAN,    THE_COLOR_BLUE,  THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* B */
  {THE_COLOR_RED,     THE_COLOR_BLUE,  THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* C */
  {THE_COLOR_BLUE,    THE_COLOR_BLUE,  THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* D */
  {THE_COLOR_RED,     THE_COLOR_BLUE,  THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* E */
  {THE_COLOR_RED,     THE_COLOR_BLUE,  THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* F */
  {THE_COLOR_RED,     THE_COLOR_BLUE,  THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* G */
  {THE_COLOR_BLACK,   THE_COLOR_BLUE,  THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* H */
  {THE_COLOR_GREEN,   THE_COLOR_BLUE,  THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* I */
  {THE_COLOR_BLUE,    THE_COLOR_BLUE,  THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* J */
  {THE_COLOR_RED,     THE_COLOR_BLUE,  THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* K */
  {THE_COLOR_GREEN,   THE_COLOR_BLUE,  THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* L */
  {THE_COLOR_RED,     THE_COLOR_BLUE,  THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* M */
  {THE_COLOR_CYAN,    THE_COLOR_BLUE,  THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* N */
  {THE_COLOR_MAGENTA, THE_COLOR_BLUE,  THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* O */
  {THE_COLOR_WHITE,   THE_COLOR_BLUE,  THE_STYLE_BOLD,   THE_STYLE_NORMAL}, /* P */
  {THE_COLOR_BLUE,    THE_COLOR_BLUE,  THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* Q */
  {THE_COLOR_MAGENTA, THE_COLOR_BLUE,  THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* R */
  {THE_COLOR_MAGENTA, THE_COLOR_BLUE,  THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* S */
  {THE_COLOR_BLUE,    THE_COLOR_BLUE,  THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* T */
  {THE_COLOR_RED,     THE_COLOR_BLUE,  THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* U */
  {THE_COLOR_MAGENTA, THE_COLOR_BLUE,  THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* V */
  {THE_COLOR_YELLOW,  THE_COLOR_BLUE,  THE_STYLE_BOLD,   THE_STYLE_NORMAL}, /* W */
  {THE_COLOR_YELLOW,  THE_COLOR_BLUE,  THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* X */
  {THE_COLOR_BLACK,   THE_COLOR_BLUE,  THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* Y */
  {THE_COLOR_BLACK,   THE_COLOR_BLUE,  THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* Z */
  {THE_COLOR_RED,     THE_COLOR_BLUE,  THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* 1 */
  {THE_COLOR_YELLOW,  THE_COLOR_BLUE , THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* 2 */
  {THE_COLOR_RED,     THE_COLOR_BLUE,  THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* 3 */
  {THE_COLOR_MAGENTA, THE_COLOR_BLUE,  THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* 4 */
  {THE_COLOR_GREEN,   THE_COLOR_BLUE,  THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* 5 */
  {THE_COLOR_CYAN,    THE_COLOR_BLUE,  THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* 6 */
  {THE_COLOR_RED,     THE_COLOR_BLUE,  THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* 7 */
  {THE_COLOR_BLACK,   THE_COLOR_BLUE,  THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* 8 */
  {THE_COLOR_BLUE,    THE_COLOR_BLUE,  THE_STYLE_BOLD ,  THE_STYLE_NORMAL}, /* 9 */
 };

#if defined(USE_WINGUICURSES1)
 static COLOUR_DEF _THE_FAR keditw_ecolours[ECOLOUR_MAX] =
 {
  /* foreground   background   modifier  mono */
  {THE_COLOR_GREEN,   THE_COLOR_WHITE, THE_STYLE_NORMAL|THE_STYLE_ITALIC, THE_STYLE_NORMAL}, /* A */
  {THE_COLOR_CYAN,    THE_COLOR_WHITE, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* B */
  {THE_COLOR_RED,     THE_COLOR_WHITE, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* C */
  {THE_COLOR_BLUE,    THE_COLOR_WHITE, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* D */
  {THE_COLOR_RED,     THE_COLOR_WHITE, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* E */
  {THE_COLOR_RED,     THE_COLOR_WHITE, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* F */
  {THE_COLOR_RED,     THE_COLOR_WHITE, THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* G */
  {THE_COLOR_BLACK,   THE_COLOR_WHITE, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* H */
  {THE_COLOR_GREEN,   THE_COLOR_WHITE, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* I */
  {THE_COLOR_BLUE,    THE_COLOR_WHITE, THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* J */
  {THE_COLOR_RED,     THE_COLOR_WHITE, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* K */
  {THE_COLOR_GREEN,   THE_COLOR_WHITE, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* L */
  {THE_COLOR_RED,     THE_COLOR_WHITE, THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* M */
  {THE_COLOR_CYAN,    THE_COLOR_WHITE, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* N */
  {THE_COLOR_MAGENTA, THE_COLOR_WHITE, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* O */
  {THE_COLOR_WHITE,   THE_COLOR_WHITE, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* P */
  {THE_COLOR_BLUE,    THE_COLOR_WHITE, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* Q */
  {THE_COLOR_MAGENTA, THE_COLOR_WHITE, THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* R */
  {THE_COLOR_MAGENTA, THE_COLOR_WHITE, THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* S */
  {THE_COLOR_BLUE,    THE_COLOR_WHITE, THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* T */
  {THE_COLOR_RED,     THE_COLOR_WHITE, THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* U */
  {THE_COLOR_MAGENTA, THE_COLOR_WHITE, THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* V */
  {THE_COLOR_YELLOW,  THE_COLOR_WHITE, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* W */
  {THE_COLOR_GREEN,   THE_COLOR_WHITE, THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* X */
  {THE_COLOR_RED,     THE_COLOR_WHITE, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* Y */
  {THE_COLOR_BLACK,   THE_COLOR_WHITE, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* Z */
  {THE_COLOR_RED,     THE_COLOR_WHITE, THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* 1 */
  {THE_COLOR_BLUE,    THE_COLOR_WHITE, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* 2 */
  {THE_COLOR_RED,     THE_COLOR_WHITE, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* 3 */
  {THE_COLOR_MAGENTA, THE_COLOR_WHITE, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* 4 */
  {THE_COLOR_GREEN,   THE_COLOR_WHITE, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* 5 */
  {THE_COLOR_CYAN,    THE_COLOR_WHITE, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* 6 */
  {THE_COLOR_YELLOW,  THE_COLOR_WHITE, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* 7 */
  {THE_COLOR_WHITE,   THE_COLOR_WHITE, THE_STYLE_NORMAL, THE_STYLE_NORMAL}, /* 8 */
  {THE_COLOR_BLUE,    THE_COLOR_WHITE, THE_STYLE_BOLD  , THE_STYLE_NORMAL}, /* 9 */
 };
#else
 static COLOUR_DEF _THE_FAR keditw_ecolours[ECOLOUR_MAX] =
 {
  /* foreground   background   modifier  mono */
  {THE_COLOR_GREEN,   THE_COLOR_WHITE, THE_STYLE_NORMAL|THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* A */
  {THE_COLOR_CYAN,    THE_COLOR_WHITE, THE_STYLE_NORMAL|THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* B */
  {THE_COLOR_RED,     THE_COLOR_WHITE, THE_STYLE_NORMAL|THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* C */
  {THE_COLOR_BLUE,    THE_COLOR_WHITE, THE_STYLE_NORMAL|THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* D */
  {THE_COLOR_RED,     THE_COLOR_WHITE, THE_STYLE_NORMAL|THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* E */
  {THE_COLOR_RED,     THE_COLOR_WHITE, THE_STYLE_NORMAL|THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* F */
  {THE_COLOR_RED,     THE_COLOR_WHITE, THE_STYLE_BOLD  |THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* G */
  {THE_COLOR_BLACK,   THE_COLOR_WHITE, THE_STYLE_NORMAL|THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* H */
  {THE_COLOR_GREEN,   THE_COLOR_WHITE, THE_STYLE_NORMAL|THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* I */
  {THE_COLOR_BLUE,    THE_COLOR_WHITE, THE_STYLE_BOLD  |THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* J */
  {THE_COLOR_RED,     THE_COLOR_WHITE, THE_STYLE_NORMAL|THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* K */
  {THE_COLOR_GREEN,   THE_COLOR_WHITE, THE_STYLE_NORMAL|THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* L */
  {THE_COLOR_RED,     THE_COLOR_WHITE, THE_STYLE_BOLD  |THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* M */
  {THE_COLOR_CYAN,    THE_COLOR_WHITE, THE_STYLE_NORMAL|THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* N */
  {THE_COLOR_MAGENTA, THE_COLOR_WHITE, THE_STYLE_NORMAL|THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* O */
  {THE_COLOR_WHITE,   THE_COLOR_WHITE, THE_STYLE_NORMAL|THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* P */
  {THE_COLOR_BLUE,    THE_COLOR_WHITE, THE_STYLE_NORMAL|THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* Q */
  {THE_COLOR_MAGENTA, THE_COLOR_WHITE, THE_STYLE_BOLD  |THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* R */
  {THE_COLOR_MAGENTA, THE_COLOR_WHITE, THE_STYLE_BOLD  |THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* S */
  {THE_COLOR_BLUE,    THE_COLOR_WHITE, THE_STYLE_BOLD  |THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* T */
  {THE_COLOR_RED,     THE_COLOR_WHITE, THE_STYLE_BOLD  |THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* U */
  {THE_COLOR_MAGENTA, THE_COLOR_WHITE, THE_STYLE_BOLD  |THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* V */
  {THE_COLOR_YELLOW,  THE_COLOR_WHITE, THE_STYLE_NORMAL|THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* W */
  {THE_COLOR_GREEN,   THE_COLOR_WHITE, THE_STYLE_BOLD  |THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* X */
  {THE_COLOR_RED,     THE_COLOR_WHITE, THE_STYLE_NORMAL|THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* Y */
  {THE_COLOR_BLACK,   THE_COLOR_WHITE, THE_STYLE_NORMAL|THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* Z */
  {THE_COLOR_RED,     THE_COLOR_WHITE, THE_STYLE_BOLD  |THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* 1 */
  {THE_COLOR_BLUE,    THE_COLOR_WHITE, THE_STYLE_NORMAL|THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* 2 */
  {THE_COLOR_RED,     THE_COLOR_WHITE, THE_STYLE_NORMAL|THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* 3 */
  {THE_COLOR_MAGENTA, THE_COLOR_WHITE, THE_STYLE_NORMAL|THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* 4 */
  {THE_COLOR_GREEN,   THE_COLOR_WHITE, THE_STYLE_NORMAL|THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* 5 */
  {THE_COLOR_CYAN,    THE_COLOR_WHITE, THE_STYLE_NORMAL|THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* 6 */
  {THE_COLOR_YELLOW,  THE_COLOR_WHITE, THE_STYLE_NORMAL|THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* 7 */
  {THE_COLOR_WHITE,   THE_COLOR_WHITE, THE_STYLE_NORMAL|THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* 8 */
  {THE_COLOR_BLUE,    THE_COLOR_WHITE, THE_STYLE_BOLD  |THE_STYLE_BLINK, THE_STYLE_NORMAL}, /* 9 */
 };
#endif

 struct attributes
 {
 CHARTYPE *attrib;
 short attrib_min_len;
 int actual_attrib;
  TheRenderStyle colour_modifier;
  bool attrib_modifier;
  bool attrib_allowed_on_mono;
  bool actual_colour;
 };
 typedef struct attributes ATTRIBS;
 static ATTRIBS _THE_FAR valid_attribs[] =
 {
    {(CHARTYPE *)"black",3,THE_COLOR_BLACK,0,FALSE,TRUE,TRUE},
#if 1
    {(CHARTYPE *)"grey",3,THE_COLOR_WHITE,0,FALSE,FALSE,TRUE},
    {(CHARTYPE *)"gray",3,THE_COLOR_WHITE,0,FALSE,FALSE,TRUE},
    {(CHARTYPE *)"white",1,THE_COLOR_WHITE,THE_STYLE_BOLD,FALSE,TRUE,TRUE},
#else
    {(CHARTYPE *)"white",1,THE_COLOR_WHITE,0,FALSE,TRUE,TRUE},
    {(CHARTYPE *)"grey",3,THE_COLOR_BLACK,THE_STYLE_BOLD,FALSE,FALSE,TRUE},
    {(CHARTYPE *)"gray",3,THE_COLOR_BLACK,THE_STYLE_BOLD,FALSE,FALSE,TRUE},
#endif
    {(CHARTYPE *)"blue",3,THE_COLOR_BLUE,0,FALSE,FALSE,TRUE},
    {(CHARTYPE *)"green",1,THE_COLOR_GREEN,0,FALSE,FALSE,TRUE},
    {(CHARTYPE *)"cyan",1,THE_COLOR_CYAN,0,FALSE,FALSE,TRUE},
    {(CHARTYPE *)"red",3,THE_COLOR_RED,0,FALSE,FALSE,TRUE},
    {(CHARTYPE *)"magenta",1,THE_COLOR_MAGENTA,0,FALSE,FALSE,TRUE},
    {(CHARTYPE *)"pink",1,THE_COLOR_MAGENTA,THE_STYLE_BOLD,FALSE,FALSE,TRUE},
    {(CHARTYPE *)"brown",1,THE_COLOR_YELLOW,0,FALSE,FALSE,TRUE},
    {(CHARTYPE *)"yellow",1,THE_COLOR_YELLOW,THE_STYLE_BOLD,FALSE,FALSE,TRUE},
    {(CHARTYPE *)"turquoise",1,THE_COLOR_CYAN,0,FALSE,FALSE,TRUE},
    {(CHARTYPE *)"normal",3,THE_STYLE_NORMAL,0,TRUE,TRUE,TRUE},
    {(CHARTYPE *)"backbold",8,THE_STYLE_BLINK,0,TRUE,TRUE,FALSE},
    {(CHARTYPE *)"blink",3,THE_STYLE_BLINK,0,TRUE,TRUE,FALSE},
    {(CHARTYPE *)"bold",2,THE_STYLE_BOLD,0,TRUE,TRUE,FALSE},
    {(CHARTYPE *)"bright",3,THE_STYLE_BOLD,0,TRUE,TRUE,FALSE},
    {(CHARTYPE *)"high",1,THE_STYLE_BOLD,0,TRUE,TRUE,FALSE},
    {(CHARTYPE *)"reverse",3,THE_STYLE_REVERSE,0,TRUE,TRUE,FALSE},
    {(CHARTYPE *)"underline",1,THE_STYLE_UNDERLINE,0,TRUE,TRUE,FALSE},
    {(CHARTYPE *)"dark",4,THE_STYLE_NORMAL,0,TRUE,TRUE,FALSE},
    {(CHARTYPE *)"italic",1,THE_STYLE_ITALIC,0,TRUE,TRUE,FALSE},
    {(CHARTYPE *)"rightline",5,THE_STYLE_RIGHTLINE,0,TRUE,TRUE,FALSE},
    {(CHARTYPE *)"leftline",4,THE_STYLE_LEFTLINE,0,TRUE,TRUE,FALSE},
    {(CHARTYPE *)"topline",3,THE_STYLE_TOPLINE,0,TRUE,TRUE,FALSE},
    {(CHARTYPE *)"overline",4,THE_STYLE_OVERLINE,0,TRUE,TRUE,FALSE},
    {(CHARTYPE *)"strikeout",5,THE_STYLE_STRIKEOUT,0,TRUE,TRUE,FALSE},
    {(CHARTYPE *)",",1,8,0,FALSE,TRUE,FALSE},
    {(CHARTYPE *)NULL,0,0,0,FALSE,FALSE,FALSE},
 };

/***********************************************************************/
short parse_colours(CHARTYPE *attrib,COLOUR_ATTR *pattr,CHARTYPE **rem,bool spare,bool *any_colours)
/***********************************************************************/
{
   register short i=0;
   short num_colours=0;
   TheRenderStyle mono=pattr->mono_style;
   TheRenderStyle specified_style=THE_STYLE_NORMAL;
   int fg=pattr->fg;
   int bg=pattr->bg;
   CHARTYPE *string=NULL;
   CHARTYPE *p=NULL,*oldp=NULL;
   bool found=FALSE,any_found=FALSE;
   bool spare_pos=FALSE;
   int offset=0;

   TRACE_FUNCTION("colour.c:  parse_colours");
   /*
    * Get a copy of the passed string and wreck it rather than the passed
    * string.
    */
   if ((string = (CHARTYPE *)my_strdup(attrib)) == NULL)
   {
      display_error(30,(CHARTYPE *)"",FALSE);
      TRACE_RETURN();
      return(RC_OUT_OF_MEMORY);
   }
   oldp = string;
   p = (CHARTYPE *)strtok((DEFCHAR *)string," \t");
   while(p != NULL)
   {
      found = FALSE;
      for ( i = 0; valid_attribs[i].attrib != NULL; i++ )
      {
         if ( equal(valid_attribs[i].attrib, p, valid_attribs[i].attrib_min_len ) )
         {
            any_found = found = TRUE;
            if (!valid_attribs[i].attrib_allowed_on_mono
            &&  !colour_support)
            {
               display_error(61,(CHARTYPE *)p,FALSE);
               (*the_free)(string);
               TRACE_RETURN();
               return(RC_INVALID_OPERAND);
            }
            if (valid_attribs[i].attrib_modifier)
            {
               if (colour_support)
                  specified_style = (valid_attribs[i].actual_attrib==THE_STYLE_NORMAL)
                                  ? THE_STYLE_NORMAL
                                  : specified_style | valid_attribs[i].actual_attrib;
               else
                  mono = (valid_attribs[i].actual_attrib==THE_STYLE_NORMAL)
                       ? THE_STYLE_NORMAL
                       : mono | valid_attribs[i].actual_attrib;
               offset = p-oldp+strlen((DEFCHAR *)p)+1;
               break;
            }
            else
            {
               switch(num_colours)
               {
                  case 0:
                     if (!colour_support
                     &&  valid_attribs[i].actual_attrib != THE_COLOR_WHITE)
                     {
                        display_error(61,(CHARTYPE *)p,FALSE);
                        (*the_free)(string);
                        TRACE_RETURN();
                        return(RC_INVALID_OPERAND);
                     }
                     if (valid_attribs[i].actual_attrib != 8)
                     {
                        fg = valid_attribs[i].actual_attrib;
                        specified_style |= valid_attribs[i].colour_modifier;
                     }
                     num_colours++;
                     offset = p-oldp+strlen((DEFCHAR *)p)+1;
                     break;
                  case 1:
                     if (!colour_support
                     &&  valid_attribs[i].actual_attrib != THE_COLOR_BLACK)
                     {
                        display_error(61,(CHARTYPE *)p,FALSE);
                        (*the_free)(string);
                        TRACE_RETURN();
                        return(RC_INVALID_OPERAND);
                     }
                     if (valid_attribs[i].actual_attrib != 8)
                     {
                        bg = valid_attribs[i].actual_attrib;
                     }
                     num_colours++;
                     offset = p-oldp+strlen((DEFCHAR *)p)+1;
                     break;
                  default:
                     if (spare)
                     {
                        spare_pos = TRUE;
                        *rem = (CHARTYPE *)attrib+offset;
                        break;
                     }
                     display_error(1,(CHARTYPE *)p,FALSE);
                     (*the_free)(string);
                     TRACE_RETURN();
                     return(RC_INVALID_OPERAND);
                     break;
               }
               if (spare_pos)
                  break;
            }
            break;
         }
      }
      if (spare_pos && found)
         break;

      if (!found) {
         int ext_color = is_valid_colour(p);
         if (ext_color != -1) {
            found = any_found = TRUE;
            if (!colour_support) {
               display_error(61,(CHARTYPE *)p,FALSE);
               (*the_free)(string);
               TRACE_RETURN();
               return(RC_INVALID_OPERAND);
            }
            switch(num_colours) {
               case 0: fg = ext_color; num_colours++; break;
               case 1: bg = ext_color; num_colours++; break;
               default:
                  if (spare) {
                     spare_pos = TRUE;
                     *rem = (CHARTYPE *)attrib+offset;
                     break;
                  }
                  display_error(1,(CHARTYPE *)p,FALSE);
                  (*the_free)(string);
                  TRACE_RETURN();
                  return(RC_INVALID_OPERAND);
            }
            offset = p-oldp+strlen((DEFCHAR *)p)+1;
         }
      }

      if (spare_pos && found)
         break;

      if (!found)
      {
         if (equal((CHARTYPE *)"on",p,2)
         && num_colours == 1)
            ;
         else
         {
            if (spare)
            {
               *rem = (CHARTYPE *)attrib+offset;
               break;
            }
            display_error(1,(CHARTYPE *)p,FALSE);
            (*the_free)(string);
            TRACE_RETURN();
            return(RC_INVALID_OPERAND);
         }
      }
      p = (CHARTYPE *)strtok(NULL," \t");
   }

   if (num_colours == 0)
   {
      pattr->fg = THE_COLOR_WHITE;
      pattr->bg = THE_COLOR_BLACK;
   }
   else
   {
      pattr->fg = fg;
      pattr->bg = bg;
   }
   pattr->style = specified_style;
   pattr->mono_style = mono;
   *any_colours = any_found;
   (*the_free)(string);
   TRACE_RETURN();
   return(RC_OK);
}

/***********************************************************************/
short parse_modifiers(CHARTYPE *attrib,COLOUR_ATTR *pattr)
/***********************************************************************/
{
   register short i=0;
   TheRenderStyle mono=pattr->mono_style;
   TheRenderStyle specified_style=THE_STYLE_NORMAL;
   CHARTYPE *string=NULL;
   CHARTYPE *p=NULL,*last_word=NULL;
   bool found=FALSE;

   TRACE_FUNCTION("colour.c:  parse_modifiers");
   /*
    * Get a copy of the passed string and wreck it rather than the passed
    * string.
    */
   if ((string = (CHARTYPE *)my_strdup(attrib)) == NULL)
   {
      display_error(30,(CHARTYPE *)"",FALSE);
      TRACE_RETURN();
      return(RC_OUT_OF_MEMORY);
   }
   p = (CHARTYPE *)strtok((DEFCHAR *)string," \t");
   while(p != NULL)
   {
      found = FALSE;
      for ( i = 0; valid_attribs[i].attrib != NULL; i++ )
      {
         if ( equal( valid_attribs[i].attrib, p, valid_attribs[i].attrib_min_len )
         &&   valid_attribs[i].attrib_modifier )
         {
            found = TRUE;
            if (!valid_attribs[i].attrib_allowed_on_mono
            &&  !colour_support)
            {
               display_error(61,(CHARTYPE *)p,FALSE);
               (*the_free)(string);
               TRACE_RETURN();
               return(RC_INVALID_OPERAND);
            }
            if (colour_support)
               specified_style = (valid_attribs[i].actual_attrib==THE_STYLE_NORMAL)
                               ? THE_STYLE_NORMAL
                               : specified_style | valid_attribs[i].actual_attrib;
            else
               mono = (valid_attribs[i].actual_attrib==THE_STYLE_NORMAL)
                    ? THE_STYLE_NORMAL
                    : mono | valid_attribs[i].actual_attrib;
            break;
         }
      }
      if ( !found )
      {
         if ( equal( (CHARTYPE *)"on", p, 2 )
         ||   equal( (CHARTYPE *)"off", p, 3 ) )
            last_word = p;
         else
         {
            display_error( 1, (CHARTYPE *)p, FALSE );
            (*the_free)( string );
            TRACE_RETURN();
            return(RC_INVALID_OPERAND);
         }
      }
      p = (CHARTYPE *)strtok(NULL," \t");
   }

   if ( equal( (CHARTYPE *)"on", last_word, 2 )
   ||   equal( (CHARTYPE *)"off", last_word, 3 ) )
   {
      (*the_free)(string);
   }
   else
   {
      display_error( 1, (CHARTYPE *)p, FALSE );
      /*
       * Free the memory after we finish referencing it; p points to
       * somewhere in string.
       */
      (*the_free)(string);
      TRACE_RETURN();
      return(RC_INVALID_OPERAND);
   }

   pattr->style = specified_style;
   pattr->mono_style = mono;
   TRACE_RETURN();
   return(RC_OK);
}

/***********************************************************************/
TheDriverAttr merge_curline_colour(COLOUR_ATTR *attr, COLOUR_ATTR *ecolour)
/***********************************************************************/
{
/*
 * Combines the foreground of ecolour colour with the background of the
 * attr colour. Also combines the modifiers from both colours to become
 * the new modifier for the combined colours.
 */
   TheDriverAttr result;
   TheRenderStyle style;

   TRACE_FUNCTION("colour.c:  merge_curline_colour");
   if (attr == NULL || ecolour == NULL)
      result = THE_RENDER_ATTR_NORMAL;
   else if (colour_support)
   {
      style = attr->style | ecolour->style;
      result = the_render_attr_make(ecolour->fg, attr->bg, style);
   }
   else
   {
      style = attr->mono_style | ecolour->mono_style;
      result = the_render_attr_from_style(style);
   }
   TRACE_RETURN();
   return result;
}

static void set_colour_attr_from_def(COLOUR_ATTR *attr,
                                     const COLOUR_DEF *def)
{
   if (attr == NULL || def == NULL)
      return;
   attr->fg = def->fore;
   attr->bg = def->back;
   attr->style = def->style;
   attr->mono_style = def->mono_style;
}
/***********************************************************************/
void set_up_default_colours(FILE_DETAILS *fd,COLOUR_ATTR *attr,int colour_num)
/***********************************************************************/
/* This function is called as part of reading in a new file.           */
/***********************************************************************/
{
   register short i=0;

   TRACE_FUNCTION("colour.c:  set_up_default_colours");
   /*
    * Set up default colours.
    */
   switch(compatible_look)
   {
      case COMPAT_THE:
         if (colour_num == ATTR_MAX)
         {
            for (i=0;i<ATTR_MAX;i++)
            {
               set_colour_attr_from_def(&fd->attr[i], &the_colours[i]);
            }
         }
         else
         {
            set_colour_attr_from_def(attr, &the_colours[colour_num]);
         }
         break;
      case COMPAT_XEDIT:
         if (colour_num == ATTR_MAX)
         {
            for (i=0;i<ATTR_MAX;i++)
            {
               set_colour_attr_from_def(&fd->attr[i], &xedit_colours[i]);
            }
         }
         else
         {
            set_colour_attr_from_def(attr, &xedit_colours[colour_num]);
         }
         break;
      case COMPAT_KEDIT:
         if (colour_num == ATTR_MAX)
         {
            for (i=0;i<ATTR_MAX;i++)
            {
               set_colour_attr_from_def(&fd->attr[i], &kedit_colours[i]);
            }
         }
         else
         {
            set_colour_attr_from_def(attr, &kedit_colours[colour_num]);
         }
         break;
      case COMPAT_KEDITW:
         if (colour_num == ATTR_MAX)
         {
            for (i=0;i<ATTR_MAX;i++)
            {
               set_colour_attr_from_def(&fd->attr[i], &keditw_colours[i]);
            }
         }
         else
         {
            set_colour_attr_from_def(attr, &keditw_colours[colour_num]);
         }
         break;
   }
   TRACE_RETURN();
   return;
}
/***********************************************************************/
void set_up_default_ecolours(FILE_DETAILS *fd)
/***********************************************************************/
/* This function is called as part of reading in a new file.           */
/***********************************************************************/
{
   register short i=0;

   TRACE_FUNCTION("colour.c:  set_up_default_ecolours");
   /*
    * Set up default colours.
    */
   switch(compatible_look)
   {
      case COMPAT_THE:
         for (i=0;i<ECOLOUR_MAX;i++)
         {
            set_colour_attr_from_def(&fd->ecolour[i], &the_ecolours[i]);
         }
         break;
      case COMPAT_XEDIT:
         for (i=0;i<ECOLOUR_MAX;i++)
         {
            set_colour_attr_from_def(&fd->ecolour[i], &xedit_ecolours[i]);
         }
         break;
      case COMPAT_KEDIT:
         for (i=0;i<ECOLOUR_MAX;i++)
         {
            set_colour_attr_from_def(&fd->ecolour[i], &kedit_ecolours[i]);
         }
         break;
      case COMPAT_KEDITW:
         for (i=0;i<ECOLOUR_MAX;i++)
         {
            set_colour_attr_from_def(&fd->ecolour[i], &keditw_ecolours[i]);
         }
         break;
   }
   TRACE_RETURN();
   return;
}
/***********************************************************************/
CHARTYPE *get_colour_strings(COLOUR_ATTR *attr)
/***********************************************************************/
/* This function returns a pointer to an allocated block of memory with*/
/* textual descriptions of the colours associated with the attr.       */
/* The caller is responsible for freeing up the allocated memory.      */
/***********************************************************************/
{
#define GET_MOD 0
#define GET_FG  1
#define GET_BG  2
   register int i=0,j=0;
   CHARTYPE *attr_string=NULL;
   int fg=attr->fg,bg=attr->bg;
   TheRenderStyle mod=attr->mono_style;
   int start_with=0;
   bool colour_only=FALSE;
   int match_value=0;
   TheRenderStyle matched_modifiers=THE_STYLE_NORMAL;

   TRACE_FUNCTION("colour.c:  get_colour_strings");

   start_with = GET_MOD;
   if (colour_support)
   {
      start_with = GET_MOD;
      mod = attr->style;
   }
   attr_string = (CHARTYPE *)(*the_malloc)(sizeof(CHARTYPE)*70);
   if (attr_string == (CHARTYPE *)NULL)
   {
      display_error(30,(CHARTYPE *)"",FALSE);
      TRACE_RETURN();
      return(NULL);
   }
   strcpy((DEFCHAR *)attr_string,"");
   /*
    * If mono, we start with the modifier (GET_MOD)
    * and end with the modifier (GET_MOD).
    * For colour, we start with the modifier (GET_MOD)
    * and end with the background (GET_BG)
    */
   for (j=start_with;j<3;j++)
   {
      switch(j)
      {
         case GET_FG:
            colour_only = TRUE;
            match_value = fg;
            break;
         case GET_BG:
            strcat((DEFCHAR *)attr_string,"on ");
            colour_only = TRUE;
            match_value = bg;
            break;
         default:
            colour_only = FALSE;
            match_value = (int)mod;
            break;
      }
      for ( i = 0; valid_attribs[i].attrib != NULL; i++ )
      {
         if (colour_only)
         {
            /*
             * Foreground or background
             */
            if (!valid_attribs[i].attrib_modifier
            &&  match_value == valid_attribs[i].actual_attrib
            &&  valid_attribs[i].colour_modifier == 0 )
            {
               strcat((DEFCHAR *)attr_string,(DEFCHAR *)valid_attribs[i].attrib);
               strcat((DEFCHAR *)attr_string," ");
               break;
            }
         }
         else
         {
            /*
             * Modifiers only - find all non-duplicate modifiers
             */
            if ( valid_attribs[i].attrib_modifier
            &&  ( match_value & valid_attribs[i].actual_attrib )
            && !( matched_modifiers & valid_attribs[i].actual_attrib ) )
            {
               strcat( (DEFCHAR *)attr_string, (DEFCHAR *)valid_attribs[i].attrib );
               strcat( (DEFCHAR *)attr_string, " " );
               matched_modifiers |= valid_attribs[i].actual_attrib;
            }
         }
      }
   }
   TRACE_RETURN();
   return(attr_string);
}

/***********************************************************************/
int is_valid_colour( CHARTYPE *colour )
/*
 * This function determines if a colour name is passed as the only argument.
 */
/***********************************************************************/
{
   int i;

   TRACE_FUNCTION("colour.c:  is_valid_colour");

   /* Check standard 16 colors first */
   for ( i = 0; valid_attribs[i].attrib != NULL; i++ )
   {
      if ( equal(valid_attribs[i].attrib, colour, valid_attribs[i].attrib_min_len )
      &&  valid_attribs[i].actual_colour )
      {
         TRACE_RETURN();
         return valid_attribs[i].actual_attrib;
      }
   }

   /* Check for hex RGB color (#RRGGBB) */
   if (colour[0] == '#' && strlen((char *)colour) == 7) {
       int r, g, b;
       if (sscanf((char *)colour + 1, "%02x%02x%02x", &r, &g, &b) == 3) {
           int c = THE_alloc_color(r, g, b);
           TRACE_RETURN();
           return c;
       }
   }

   /* Check for extended SVG/X11 colors */
   for (i = 0; extended_colors[i].name != NULL; i++) {
       if (strcasecmp(extended_colors[i].name, (char *)colour) == 0) {
           int c = THE_alloc_color(extended_colors[i].r, extended_colors[i].g, extended_colors[i].b);
           TRACE_RETURN();
           return c;
       }
   }

   TRACE_RETURN();
   return (-1);
}
