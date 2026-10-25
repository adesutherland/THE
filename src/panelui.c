/* Shared fixed-width panel fields. No terminal, protocol or macro callbacks. */
#include "panelui.h"
#include "textpos.h"
#include "thekeys.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

ThePanelState *the_panel_active;

void panel_ui_init(ThePanelState *p, int rows, int cols)
{
   memset(p, 0, sizeof(*p));
   p->rows = rows;
   p->cols = cols;
   p->active = -1;
}

static int cells(const char *text)
{
   return textpos_from_byte((const CHARTYPE *)text, strlen(text), strlen(text)).cell_column;
}

int panel_ui_add(ThePanelState *p, int row, int cell, int width,
                  int editable, char kind, LINETYPE line, const char *text,
                  size_t length, size_t source_start)
{
   ThePanelField *f;
   int n;
   if (width <= 0) return 1;
   if (p->count >= THE_PANEL_MAX_FIELDS || length > THE_PANEL_TEXT || width > THE_PANEL_TEXT / 4)
      return 0;
   f = &p->fields[p->count];
   f->row = row; f->cell = cell; f->width = width;
   f->editable = editable; f->kind = kind; f->line = line;
   f->source_start = source_start; f->source_length = length;
   memcpy(f->text, text, length); f->text[length] = 0;
   n = cells(f->text);
   if (n > width) return 0;
   if (editable)
   {
      while (n++ < width) f->text[length++] = ' ';
      f->text[length] = 0;
   }
   strcpy(f->original, f->text);
   if (editable && p->active < 0) p->active = p->count;
   p->count++;
   return 1;
}

int panel_ui_hit(ThePanelState *p, int row, int cell)
{
   int i;
   for (i = 0; i < p->count; i++)
   {
      ThePanelField *f = &p->fields[i];
      if (f->editable && row == f->row && cell >= f->cell && cell < f->cell + f->width)
      {
         p->active = i;
         p->cursor = textpos_from_cell((CHARTYPE *)f->text, strlen(f->text), cell-f->cell, TEXT_SNAP_BACKWARD).cell_column;
         return 1;
      }
   }
   return 0;
}

static void next_field(ThePanelState *p, int direction)
{
   int i, n = p->active;
   for (i = 0; i < p->count; i++)
   {
      n = (n + direction + p->count) % p->count;
      if (p->fields[n].editable) { p->active = n; p->cursor = 0; return; }
   }
}

static int replace_span(ThePanelField *f, int start, int end, const char *text, size_t length)
{
   size_t oldlen = strlen(f->text), a, b;
   char result[THE_PANEL_TEXT + 1];
   int width;
   TextPos left = textpos_from_cell((CHARTYPE *)f->text, oldlen, start, TEXT_SNAP_BACKWARD);
   TextPos right = textpos_from_cell((CHARTYPE *)f->text, oldlen, end, TEXT_SNAP_BACKWARD);
   if (right.cell_column < end) right=textpos_next_cluster((CHARTYPE *)f->text,oldlen,right);
   a = left.byte_offset; b = right.byte_offset;
   if (a + length + oldlen - b > THE_PANEL_TEXT) return 0;
   memcpy(result, f->text, a);
   memcpy(result+a, text, length);
   memcpy(result+a+length, f->text+b, oldlen-b+1);
   width = cells(result);
   if (width > f->width) return 0;
   oldlen = strlen(result);
   while (width++ < f->width) result[oldlen++] = ' ';
   result[oldlen] = 0;
   strcpy(f->text, result);
   f->changed = strcmp(f->text, f->original) != 0;
   return 1;
}

int panel_ui_event(ThePanelState *p, const TheInputEvent *e)
{
   ThePanelField *f = p->active >= 0 ? &p->fields[p->active] : NULL;
   int key, i;
   if (!e || p->done) return 0;
   if (e->kind==THE_INPUT_TEXT && (e->codepoint==9 || e->codepoint==10 || e->codepoint==13 ||
       e->codepoint==27 || e->codepoint==8 || e->codepoint==127))
   {
      TheInputEvent key_event=*e;
      key_event.kind=THE_INPUT_KEY; key_event.key_code=(int)e->codepoint;
      return panel_ui_event(p,&key_event);
   }
   if (e->kind == THE_INPUT_LOGICAL_HIT)
      return panel_ui_hit(p, e->target.row, e->target.cell);
   if (e->kind == THE_INPUT_TEXT)
   {
      CHARTYPE encoded[4];
      int width = text_codepoint_cell_width(e->codepoint);
      size_t len;
      if (!f || !f->editable || e->codepoint < 32 || e->codepoint == 127 || width < 0 || p->cursor + width > f->width)
         return 0;
      len = text_utf8_encode(e->codepoint, encoded);
      if (!replace_span(f, p->cursor, p->cursor + width, (char *)encoded, len)) return 0;
      p->cursor += width;
      return 1;
   }
   if (e->kind != THE_INPUT_KEY) return 0;
   key = e->key_code;
   if (key == THE_KEY_ESC) { p->done = p->cancelled = 1; return 1; }
   if (key == THE_KEY_ENTER || key == 13 || key == 10 ||
       (key >= THE_KEY_F(1) && key <= THE_KEY_F(24)) ||
       (key >= THE_KEY_PA1 && key <= THE_KEY_PA3))
   { p->done = 1; p->key = key; return 1; }
   if (key == THE_KEY_CLEAR)
   {
      for (i=0; i<p->count; i++) { strcpy(p->fields[i].text, p->fields[i].original); p->fields[i].changed=0; }
      p->cursor=0; return 1;
   }
   if (key == 9 || key == THE_KEY_BTAB) { next_field(p, key == 9 ? 1 : -1); return 1; }
   if (!f) return 0;
   if (key == THE_KEY_HOME) p->cursor = 0;
   else if (key == THE_KEY_END)
   {
      char trimmed[THE_PANEL_TEXT+1]; size_t n;
      strcpy(trimmed, f->text); n=strlen(trimmed);
      while (n && trimmed[n-1]==' ') trimmed[--n]=0;
      p->cursor=cells(trimmed);
   }
   else if (key == THE_KEY_LEFT)
      p->cursor = textpos_prev_cluster((CHARTYPE *)f->text, strlen(f->text), textpos_from_cell((CHARTYPE *)f->text, strlen(f->text), p->cursor, TEXT_SNAP_BACKWARD)).cell_column;
   else if (key == THE_KEY_RIGHT)
      p->cursor = textpos_next_cluster((CHARTYPE *)f->text, strlen(f->text), textpos_from_cell((CHARTYPE *)f->text, strlen(f->text), p->cursor, TEXT_SNAP_BACKWARD)).cell_column;
   else if (key == THE_KEY_UP || key == THE_KEY_DOWN)
   {
      int step = key == THE_KEY_UP ? -1 : 1, row;
      for (row=f->row+step; row>=0 && row<p->rows; row+=step)
         if (panel_ui_hit(p, row, f->cell + p->cursor)) return 1;
   }
   else if (key == THE_KEY_BACKSPACE || key == 127 || key == 8)
   {
      int start = textpos_prev_cluster((CHARTYPE *)f->text, strlen(f->text), textpos_from_cell((CHARTYPE *)f->text, strlen(f->text), p->cursor, TEXT_SNAP_BACKWARD)).cell_column;
      if (start == p->cursor) return 0;
      if (!replace_span(f, start, p->cursor, "", 0)) return 0;
      p->cursor = start;
   }
   else if (key == THE_KEY_DC)
   {
      int end = textpos_next_cluster((CHARTYPE *)f->text, strlen(f->text), textpos_from_cell((CHARTYPE *)f->text, strlen(f->text), p->cursor, TEXT_SNAP_BACKWARD)).cell_column;
      return replace_span(f, p->cursor, end, "", 0);
   }
   else if (key == THE_KEY_EOL)
   {
      return replace_span(f, p->cursor, f->width, "", 0);
   }
   else return 0;
   if (p->cursor > f->width) p->cursor = f->width;
   return 1;
}

typedef struct { char *out; size_t size, used; int ok; } Writer;
static void emit(Writer *w, const char *format, ...)
{
   int n; va_list args;
   if (!w->ok) return;
   va_start(args, format); n=vsnprintf(w->out+w->used, w->size-w->used, format, args); va_end(args);
   if (n<0 || (size_t)n>=w->size-w->used) { w->ok=0; return; }
   w->used+=(size_t)n;
}
static void quoted(Writer *w, const char *s)
{
   const unsigned char *p=(const unsigned char *)s;
   emit(w,"\"");
   for (; *p; p++)
      if (*p=='"' || *p=='\\') emit(w,"\\%c",*p);
      else if (*p<32) emit(w,"\\u%04x",*p);
      else emit(w,"%c",*p);
   emit(w,"\"");
}
size_t panel_ui_json(const ThePanelState *p, char *out, size_t size)
{
   Writer w={out,size,0,1}; int i;
   if (!size) return 0;
   if (!p) emit(&w,"null");
   else
   {
      emit(&w,"{\"rows\":%d,\"cols\":%d,\"active\":%d,\"cursor\":%d,\"fields\":[",p->rows,p->cols,p->active,p->cursor);
      for (i=0;i<p->count;i++)
      {
         const ThePanelField *f=&p->fields[i];
         emit(&w,"%s{\"row\":%d,\"cell\":%d,\"width\":%d,\"line\":%ld,\"editable\":%d,\"changed\":%d,\"kind\":\"%c\",\"text\":",i?",":"",f->row,f->cell,f->width,(long)f->line,f->editable,f->changed,f->kind);
         quoted(&w,f->text); emit(&w,"}");
      }
      emit(&w,"]}");
   }
   if (!w.ok) { out[0]=0; return 0; }
   return w.used;
}
