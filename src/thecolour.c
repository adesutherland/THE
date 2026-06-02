#include "thecolour.h"

typedef struct
{
   int used;
   uint32_t rgb;
} TheLogicalRgbColour;

static TheLogicalRgbColour rgb_colours[THE_RENDER_COLOR_LOGICAL_COUNT];
static int next_rgb_colour = THE_RENDER_COLOR_RGB_FIRST;

static int rgb_component_valid(int component)
{
   return component >= 0 && component <= 255;
}

int the_render_color_from_rgb(int red, int green, int blue)
{
   if (!rgb_component_valid(red)
   ||  !rgb_component_valid(green)
   ||  !rgb_component_valid(blue))
      return THE_COLOR_UNSPECIFIED;
   return the_render_color_from_rgb24(((uint32_t)red << 16)
                                    | ((uint32_t)green << 8)
                                    | (uint32_t)blue);
}

int the_render_color_from_rgb24(uint32_t rgb)
{
   int i;

   rgb &= UINT32_C(0x00ffffff);
   for (i = THE_RENDER_COLOR_RGB_FIRST; i < next_rgb_colour; i++)
   {
      if (rgb_colours[i].used && rgb_colours[i].rgb == rgb)
         return i;
   }
   if (next_rgb_colour >= (int)THE_RENDER_COLOR_LOGICAL_COUNT)
      return THE_COLOR_UNSPECIFIED;
   rgb_colours[next_rgb_colour].used = 1;
   rgb_colours[next_rgb_colour].rgb = rgb;
   return next_rgb_colour++;
}

int the_render_color_is_rgb(int colour)
{
   return colour >= THE_RENDER_COLOR_RGB_FIRST
       && colour < (int)THE_RENDER_COLOR_LOGICAL_COUNT
       && rgb_colours[colour].used;
}

int the_render_color_rgb(int colour, int *red, int *green, int *blue)
{
   uint32_t rgb;

   if (!the_render_color_is_rgb(colour))
      return 0;
   rgb = rgb_colours[colour].rgb;
   if (red != 0)
      *red = (int)((rgb >> 16) & UINT32_C(0xff));
   if (green != 0)
      *green = (int)((rgb >> 8) & UINT32_C(0xff));
   if (blue != 0)
      *blue = (int)(rgb & UINT32_C(0xff));
   return 1;
}

uint32_t the_render_color_rgb24(int colour)
{
   if (!the_render_color_is_rgb(colour))
      return 0;
   return rgb_colours[colour].rgb;
}
