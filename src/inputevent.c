#include "inputevent.h"

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "getch.h"
static TheInputWorkbenchSourceLookup workbench_source_lookup;
void the_input_workbench_source_lookup(TheInputWorkbenchSourceLookup lookup)
{ workbench_source_lookup=lookup; }

#ifdef THE_KEY_TAB
# define THE_INPUT_KEY_TAB THE_KEY_TAB
#else
# define THE_INPUT_KEY_TAB 0x9
#endif

typedef struct
{
   const char *name;
   int key_code;
} TheInputKeyName;

typedef struct
{
   const char *name;
   TheInputDebugCommand command;
} TheInputDebugName;

static const TheInputKeyName input_key_names[] =
{
   { "left", THE_KEY_LEFT },
   { "right", THE_KEY_RIGHT },
   { "up", THE_KEY_UP },
   { "down", THE_KEY_DOWN },
   { "home", THE_KEY_HOME },
   { "end", THE_KEY_END },
   { "pageup", THE_KEY_PPAGE },
   { "pgup", THE_KEY_PPAGE },
   { "pagedown", THE_KEY_NPAGE },
   { "pgdn", THE_KEY_NPAGE },
   { "enter", THE_KEY_ENTER },
   { "return", THE_KEY_RETURN },
   { "esc", THE_KEY_ESC },
   { "escape", THE_KEY_ESC },
   { "tab", THE_INPUT_KEY_TAB },
   { "backtab", THE_KEY_BTAB },
   { "btab", THE_KEY_BTAB },
   { "shift-left", THE_KEY_SLEFT },
   { "shift-right", THE_KEY_SRIGHT },
   { "shift-up", THE_KEY_SUP },
   { "shift-down", THE_KEY_SDOWN },
   { "shift-home", THE_KEY_SHOME },
   { "shift-end", THE_KEY_SEND },
   { "backspace", THE_KEY_BACKSPACE },
   { "delete", THE_KEY_DC },
   { "del", THE_KEY_DC },
   { "insert", THE_KEY_IC },
   { "clear", THE_KEY_CLEAR },
   { "eraseeof", THE_KEY_EOL },
   { "pa1", THE_KEY_PA1 },
   { "pa2", THE_KEY_PA2 },
   { "pa3", THE_KEY_PA3 },
   { NULL, 0 }
};

static const TheInputDebugName input_debug_names[] =
{
   { "describe-focus", THE_INPUT_DEBUG_DESCRIBE_FOCUS },
   { "focus", THE_INPUT_DEBUG_DESCRIBE_FOCUS },
   { "describe-row", THE_INPUT_DEBUG_DESCRIBE_ROW },
   { "row", THE_INPUT_DEBUG_DESCRIBE_ROW },
   { "list-visible-rows", THE_INPUT_DEBUG_LIST_VISIBLE_ROWS },
   { "visible-rows", THE_INPUT_DEBUG_LIST_VISIBLE_ROWS },
   { "dump-cursor-mapping", THE_INPUT_DEBUG_DUMP_CURSOR_MAPPING },
   { "cursor-mapping", THE_INPUT_DEBUG_DUMP_CURSOR_MAPPING },
   { "dump-driver-ops", THE_INPUT_DEBUG_DUMP_DRIVER_OPS },
   { "driver-ops", THE_INPUT_DEBUG_DUMP_DRIVER_OPS },
   { "explain-last-render", THE_INPUT_DEBUG_EXPLAIN_LAST_RENDER },
   { "last-render", THE_INPUT_DEBUG_EXPLAIN_LAST_RENDER },
   { "utf-display", THE_INPUT_DEBUG_UTF_DISPLAY },
   { "utf", THE_INPUT_DEBUG_UTF_DISPLAY },
   { NULL, THE_INPUT_DEBUG_NONE }
};

static void input_copy_text(char *dest, size_t dest_len, const char *src)
{
   size_t len;

   if (dest == NULL || dest_len == 0)
      return;
   if (src == NULL)
      src = "";
   len = strlen(src);
   if (len >= dest_len)
      len = dest_len - 1;
   if (len > 0)
      memcpy(dest, src, len);
   dest[len] = '\0';
}

static int input_ascii_equal_ci(const char *left, const char *right)
{
   if (left == NULL || right == NULL)
      return 0;
   while (*left != '\0' && *right != '\0')
   {
      if (tolower((unsigned char)*left) != tolower((unsigned char)*right))
         return 0;
      left++;
      right++;
   }
   return *left == '\0' && *right == '\0';
}

const char *the_input_kind_name(TheInputKind kind)
{
   switch (kind)
   {
      case THE_INPUT_TEXT:
         return "text";
      case THE_INPUT_KEY:
         return "key";
      case THE_INPUT_COMMAND:
         return "command";
      case THE_INPUT_ACTION:
         return "action";
      case THE_INPUT_WORKBENCH_INVOKE:
         return "workbench-invoke";
      case THE_INPUT_WORKBENCH_PANEL:
         return "workbench-panel";
      case THE_INPUT_WORKBENCH_FORM:
         return "workbench-form";
      case THE_INPUT_WORKBENCH_FILEFIELDS:
         return "workbench-filefields";
      case THE_INPUT_LOGICAL_HIT:
         return "logical-hit";
      case THE_INPUT_RESIZE:
         return "resize";
      case THE_INPUT_DEBUG:
         return "debug";
      case THE_INPUT_NONE:
      default:
         return "none";
   }
}

const char *the_input_debug_command_name(TheInputDebugCommand command)
{
   switch (command)
   {
      case THE_INPUT_DEBUG_DESCRIBE_FOCUS:
         return "describe-focus";
      case THE_INPUT_DEBUG_DESCRIBE_ROW:
         return "describe-row";
      case THE_INPUT_DEBUG_LIST_VISIBLE_ROWS:
         return "list-visible-rows";
      case THE_INPUT_DEBUG_DUMP_CURSOR_MAPPING:
         return "dump-cursor-mapping";
      case THE_INPUT_DEBUG_DUMP_DRIVER_OPS:
         return "dump-driver-ops";
      case THE_INPUT_DEBUG_EXPLAIN_LAST_RENDER:
         return "explain-last-render";
      case THE_INPUT_DEBUG_UTF_DISPLAY:
         return "utf-display";
      case THE_INPUT_DEBUG_NONE:
      default:
         return "none";
   }
}

const char *the_input_logical_target_kind_name(TheInputLogicalTargetKind kind)
{
   switch (kind)
   {
      case THE_INPUT_TARGET_FILEAREA:
         return "filearea";
      case THE_INPUT_TARGET_PREFIX:
         return "prefix";
      case THE_INPUT_TARGET_COMMAND:
         return "command";
      case THE_INPUT_TARGET_PROMPT:
         return "prompt";
      case THE_INPUT_TARGET_STATUS:
         return "status";
      case THE_INPUT_TARGET_TABLINE:
         return "tabline";
      case THE_INPUT_TARGET_DIVIDER:
         return "divider";
      case THE_INPUT_TARGET_WINDOW:
         return "window";
      case THE_INPUT_TARGET_NONE:
      default:
         return "none";
   }
}

int the_input_logical_target_kind_from_name(const char *name,
                                            TheInputLogicalTargetKind *kind)
{
   TheInputLogicalTargetKind parsed = THE_INPUT_TARGET_NONE;

   if (name == NULL)
      return 0;
   if (input_ascii_equal_ci(name, "filearea")
   ||  input_ascii_equal_ci(name, "file")
   ||  input_ascii_equal_ci(name, "file-area"))
      parsed = THE_INPUT_TARGET_FILEAREA;
   else if (input_ascii_equal_ci(name, "prefix"))
      parsed = THE_INPUT_TARGET_PREFIX;
   else if (input_ascii_equal_ci(name, "command")
   ||       input_ascii_equal_ci(name, "cmd")
   ||       input_ascii_equal_ci(name, "cmdline")
   ||       input_ascii_equal_ci(name, "commandline"))
      parsed = THE_INPUT_TARGET_COMMAND;
   else if (input_ascii_equal_ci(name, "prompt"))
      parsed = THE_INPUT_TARGET_PROMPT;
   else if (input_ascii_equal_ci(name, "status"))
      parsed = THE_INPUT_TARGET_STATUS;
   else if (input_ascii_equal_ci(name, "tabline")
   ||       input_ascii_equal_ci(name, "tabs")
   ||       input_ascii_equal_ci(name, "tab")
   ||       input_ascii_equal_ci(name, "filetabs")
   ||       input_ascii_equal_ci(name, "file-tabs"))
      parsed = THE_INPUT_TARGET_TABLINE;
   else if (input_ascii_equal_ci(name, "divider"))
      parsed = THE_INPUT_TARGET_DIVIDER;
   else if (input_ascii_equal_ci(name, "window")
   ||       input_ascii_equal_ci(name, "win"))
      parsed = THE_INPUT_TARGET_WINDOW;
   else
      return 0;

   if (kind != NULL)
      *kind = parsed;
   return 1;
}

static LogicalCursorZone input_zone_from_target_kind(TheInputLogicalTargetKind kind)
{
   switch (kind)
   {
      case THE_INPUT_TARGET_FILEAREA:
         return LOGICAL_CURSOR_ZONE_FILEAREA;
      case THE_INPUT_TARGET_PREFIX:
         return LOGICAL_CURSOR_ZONE_PREFIX;
      case THE_INPUT_TARGET_COMMAND:
         return LOGICAL_CURSOR_ZONE_COMMAND;
      case THE_INPUT_TARGET_PROMPT:
         return LOGICAL_CURSOR_ZONE_PROMPT;
      case THE_INPUT_TARGET_STATUS:
         return LOGICAL_CURSOR_ZONE_STATUS;
      case THE_INPUT_TARGET_TABLINE:
      case THE_INPUT_TARGET_DIVIDER:
      case THE_INPUT_TARGET_WINDOW:
      case THE_INPUT_TARGET_NONE:
      default:
         return LOGICAL_CURSOR_ZONE_NONE;
   }
}

static TheInputLogicalTargetKind input_target_kind_from_zone(LogicalCursorZone zone)
{
   switch (zone)
   {
      case LOGICAL_CURSOR_ZONE_FILEAREA:
         return THE_INPUT_TARGET_FILEAREA;
      case LOGICAL_CURSOR_ZONE_PREFIX:
         return THE_INPUT_TARGET_PREFIX;
      case LOGICAL_CURSOR_ZONE_COMMAND:
         return THE_INPUT_TARGET_COMMAND;
      case LOGICAL_CURSOR_ZONE_PROMPT:
         return THE_INPUT_TARGET_PROMPT;
      case LOGICAL_CURSOR_ZONE_STATUS:
         return THE_INPUT_TARGET_STATUS;
      case LOGICAL_CURSOR_ZONE_NONE:
      default:
         return THE_INPUT_TARGET_NONE;
   }
}

TheInputEvent the_input_event_none(void)
{
   TheInputEvent input;

   memset(&input, 0, sizeof(input));
   input.kind = THE_INPUT_NONE;
   input.key_code = -1;
   return input;
}

int the_input_event_from_text(uint32_t codepoint, TheInputEvent *out)
{
   if (out == NULL)
      return 0;
   *out = the_input_event_none();
   out->kind = THE_INPUT_TEXT;
   out->codepoint = codepoint;
   if (codepoint <= 0x7Fu)
      out->key_code = (int)codepoint;
   return 1;
}

int the_input_event_from_resize(int rows, int cols, TheInputEvent *out)
{
   if (out == NULL) return 0;
   *out = the_input_event_none();
   if (rows < 12 || rows > 256 || cols < 40 || cols > 1000) return 0;
   out->kind = THE_INPUT_RESIZE;
   out->rows = rows;
   out->cols = cols;
   return 1;
}

int the_input_event_from_key_name(const char *name, TheInputEvent *out)
{
   size_t i;

   if (out == NULL || name == NULL)
      return 0;
   *out = the_input_event_none();
   if (((name[0]=='c' || name[0]=='C') && name[1]=='-' && name[2] && !name[3]) ||
       ((name[0]=='c' || name[0]=='C') && (name[1]=='t' || name[1]=='T') &&
        (name[2]=='r' || name[2]=='R') && (name[3]=='l' || name[3]=='L') &&
        name[4]=='-' && name[5] && !name[6]))
   {
      int letter=toupper((unsigned char)name[strlen(name)-1]);
      if (letter>='A' && letter<='Z')
      {
         out->kind=THE_INPUT_KEY;
         out->key_code=letter-'A'+1;
         return 1;
      }
   }
   if ((name[0] == 'f' || name[0] == 'F') && isdigit((unsigned char)name[1]))
   {
      char *end = NULL;
      long number = strtol(name + 1, &end, 10);

      if (end != NULL && *end == '\0' && number >= 1 && number <= 64)
      {
         out->kind = THE_INPUT_KEY;
         out->key_code = THE_KEY_F((int)number);
         return 1;
      }
   }

   for (i = 0; input_key_names[i].name != NULL; i++)
   {
      if (input_ascii_equal_ci(name, input_key_names[i].name))
      {
         out->kind = THE_INPUT_KEY;
         out->key_code = input_key_names[i].key_code;
         return 1;
      }
   }
   return 0;
}

int the_input_event_from_legacy_key(int key_code, TheInputEvent *out)
{
   if (out == NULL || key_code < 0)
      return 0;
   if (key_code >= 0x20 && key_code <= 0x7e)
      return the_input_event_from_text((uint32_t)key_code, out);
   *out = the_input_event_none();
   out->kind = THE_INPUT_KEY;
   out->key_code = key_code;
   return 1;
}

int the_input_event_from_command(const char *command, TheInputEvent *out)
{
   if (out == NULL)
      return 0;
   *out = the_input_event_none();
   out->kind = THE_INPUT_COMMAND;
   input_copy_text(out->command, sizeof(out->command), command);
   return out->command[0] != '\0';
}

int the_input_event_from_restricted_command(const char *command,
                                            TheInputEvent *out)
{
   if (!the_input_event_from_command(command, out))
      return 0;
   out->restricted_command = 1;
   return 1;
}

int the_input_event_from_action(const char *name, const char *argument,
                                TheInputEvent *out)
{
   if (out == NULL)
      return 0;
   *out = the_input_event_none();
   if (!the_frontend_action_from_name(name, argument, &out->action))
      return 0;
   out->kind = THE_INPUT_ACTION;
   return 1;
}

int the_input_event_from_workbench_invoke(const char *id, unsigned long generation,
                                           const char *argument, TheInputEvent *out)
{
   const unsigned char *p = (const unsigned char *)id;
   if (!out || !p || !((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z'))
   || generation > LONG_MAX || strlen(id) > THE_WORKBENCH_ID_MAX
   || (argument && strlen(argument) > THE_WORKBENCH_OPERAND_MAX)) return 0;
   for (; *p; p++) if (!( (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')
      || (*p >= '0' && *p <= '9') || *p == '.' || *p == '_' || *p == '-')) return 0;
   *out = the_input_event_none();
   out->kind = THE_INPUT_WORKBENCH_INVOKE;
   strcpy(out->command,id); out->target.line_number = (LINETYPE)generation;
   out->rows = argument != NULL;
   if (argument) strcpy(out->action.argument,argument);
   if(workbench_source_lookup) {
      uint64_t source=0;
      if(workbench_source_lookup(id,&source)) {
         if(argument) return 0;
         out->cols=1;memcpy(out->action.argument,&source,sizeof(source));
      }
   }
   return 1;
}
int the_input_event_workbench_source(const TheInputEvent *event,uint64_t *source)
{
   if(!event || !source || event->kind!=THE_INPUT_WORKBENCH_INVOKE || event->cols!=1 || event->rows) return 0;
   memcpy(source,event->action.argument,sizeof(*source));return 1;
}

const char *the_input_event_workbench_id(const TheInputEvent *input)
{ return input && input->kind == THE_INPUT_WORKBENCH_INVOKE ? input->command : NULL; }
unsigned long the_input_event_workbench_generation(const TheInputEvent *input)
{ return input && input->kind == THE_INPUT_WORKBENCH_INVOKE ? (unsigned long)input->target.line_number : 0; }
const char *the_input_event_workbench_argument(const TheInputEvent *input)
{ return input && input->kind == THE_INPUT_WORKBENCH_INVOKE && input->rows ? input->action.argument : NULL; }

int the_input_event_from_workbench_panel(const char *id,unsigned long generation,
   TheWorkbenchPanelOperation operation,TheInputEvent *out)
{
   if (!generation || operation < THE_WORKBENCH_PANEL_SHOW || operation > THE_WORKBENCH_PANEL_RETURN
   || !the_input_event_from_workbench_invoke(id,generation,NULL,out)) return 0;
   out->kind=THE_INPUT_WORKBENCH_PANEL; out->codepoint=(uint32_t)operation;
   return 1;
}
const char *the_input_event_workbench_panel_id(const TheInputEvent *input)
{ return input && input->kind == THE_INPUT_WORKBENCH_PANEL ? input->command : NULL; }
unsigned long the_input_event_workbench_panel_generation(const TheInputEvent *input)
{ return input && input->kind == THE_INPUT_WORKBENCH_PANEL ? (unsigned long)input->target.line_number : 0; }
TheWorkbenchPanelOperation the_input_event_workbench_panel_operation(const TheInputEvent *input)
{ return input && input->kind == THE_INPUT_WORKBENCH_PANEL ? (TheWorkbenchPanelOperation)input->codepoint : 0; }

/* ABI3 private storage for this kind: three fixed ID slots in command,
 * value followed by counted metadata in action.argument. No command parsing. */
typedef struct {
   uint64_t request,revision,form_generation,panel_generation;
   uint32_t magic,operation,edit,codepoint,length;
   int32_t cell;
} FormInput;
static int form_id(const char *id,int empty)
{
   size_t n;
   if (!id || (!*id && !empty)) return 0;
   if(*id && !((*id>='A' && *id<='Z') || (*id>='a' && *id<='z'))) return 0;
   for (n=0;n<=THE_WORKBENCH_ID_MAX;n++)
   {
      unsigned char c=(unsigned char)id[n];
      if (!c) return 1;
      if (!isalpha(c) && !isdigit(c) && c!='.' && c!='_' && c!='-') return 0;
   }
   return 0;
}
int the_input_event_from_workbench_form(const TheWorkbenchFormRequest *r,TheInputEvent *out)
{
   FormInput wire;
   if (!r || !out || !r->form_generation || !r->draft_revision
   || !form_id(r->form,0) || !form_id(r->field,1) || !form_id(r->panel,1)
   || (r->panel[0] && !r->panel_generation) || r->length>THE_WORKBENCH_FIELD_VALUE_MAX
   || r->value[r->length] || memchr(r->value,0,r->length)
   || r->operation<THE_WORKBENCH_FORM_SET || r->operation>THE_WORKBENCH_FORM_CANCEL) return 0;
   memset(&wire,0,sizeof(wire)); wire.magic=UINT32_C(0x464f524d);
   wire.request=r->request_id; wire.revision=r->draft_revision;
   wire.form_generation=r->form_generation; wire.panel_generation=r->panel_generation;
   wire.operation=r->operation; wire.edit=r->edit; wire.codepoint=r->codepoint;
   wire.length=(uint32_t)r->length; wire.cell=r->cell;
   *out=the_input_event_none(); out->kind=THE_INPUT_WORKBENCH_FORM;
   memcpy(out->command,r->form,strlen(r->form)+1);
   memcpy(out->command+64,r->field,strlen(r->field)+1);
   memcpy(out->command+128,r->panel,strlen(r->panel)+1);
   memcpy(out->action.argument,r->value,r->length+1);
   memcpy(out->action.argument+THE_WORKBENCH_FIELD_VALUE_MAX+1,&wire,sizeof(wire));
   return 1;
}
int the_input_event_workbench_form(const TheInputEvent *input,TheWorkbenchFormRequest *out)
{
   FormInput wire;
   if (!input || !out || input->kind!=THE_INPUT_WORKBENCH_FORM
   || !memchr(input->command,0,64) || !memchr(input->command+64,0,64)
   || !memchr(input->command+128,0,64)) return 0;
   memcpy(&wire,input->action.argument+THE_WORKBENCH_FIELD_VALUE_MAX+1,sizeof(wire));
   if (wire.magic!=UINT32_C(0x464f524d) || wire.length>THE_WORKBENCH_FIELD_VALUE_MAX
   || wire.form_generation>ULONG_MAX || wire.panel_generation>ULONG_MAX) return 0;
   memset(out,0,sizeof(*out)); out->operation=(TheWorkbenchFormOperation)wire.operation;
   out->request_id=wire.request; out->draft_revision=wire.revision;
   out->form_generation=(unsigned long)wire.form_generation; out->panel_generation=(unsigned long)wire.panel_generation;
   strcpy(out->form,input->command); strcpy(out->field,input->command+64); strcpy(out->panel,input->command+128);
   out->length=wire.length; memcpy(out->value,input->action.argument,out->length);
   out->edit=(TheWorkbenchFormEdit)wire.edit; out->codepoint=wire.codepoint; out->cell=wire.cell;
   return out->value[out->length]==0 && !memchr(out->value,0,out->length);
}

int the_input_event_from_workbench_filefields(const TheWorkbenchFileFieldsRequest *r,TheInputEvent *out)
{
   TheWorkbenchFormRequest f;
   if(!r || r->operation<THE_WORKBENCH_FILEFIELDS_SET || r->operation>THE_WORKBENCH_FILEFIELDS_RETURN) return 0;
   memset(&f,0,sizeof(f)); f.operation=THE_WORKBENCH_FORM_SET; f.request_id=r->request_id; f.draft_revision=r->draft_revision;
   f.form_generation=r->panel_generation; f.panel_generation=r->panel_generation;
   memcpy(f.form,r->panel,sizeof(f.form)); memcpy(f.panel,r->panel,sizeof(f.panel)); memcpy(f.field,r->field,sizeof(f.field));
   memcpy(f.value,r->value,sizeof(f.value)); f.length=r->length; f.edit=r->edit; f.cell=r->cell; f.codepoint=r->codepoint;
   if(!the_input_event_from_workbench_form(&f,out)) return 0;
   if(r->utf16_offset>THE_WORKBENCH_FIELD_VALUE_MAX) return 0;
   out->kind=THE_INPUT_WORKBENCH_FILEFIELDS; out->rows=(int)r->operation; out->cols=(int)r->utf16_offset; return 1;
}
int the_input_event_workbench_filefields(const TheInputEvent *input,TheWorkbenchFileFieldsRequest *out)
{
   TheInputEvent copy; TheWorkbenchFormRequest f;
   if(!input || !out || input->kind!=THE_INPUT_WORKBENCH_FILEFIELDS || input->rows<THE_WORKBENCH_FILEFIELDS_SET || input->rows>THE_WORKBENCH_FILEFIELDS_RETURN) return 0;
   copy=*input; copy.kind=THE_INPUT_WORKBENCH_FORM;
   if(!the_input_event_workbench_form(&copy,&f)) return 0;
   memset(out,0,sizeof(*out)); out->operation=(TheWorkbenchFileFieldsOperation)input->rows; out->request_id=f.request_id; out->draft_revision=f.draft_revision;
   out->panel_generation=f.panel_generation; strcpy(out->panel,f.panel); strcpy(out->field,f.field); memcpy(out->value,f.value,sizeof(out->value));
   if(input->cols<0 || input->cols>THE_WORKBENCH_FIELD_VALUE_MAX) return 0;
   out->utf16_offset=(size_t)input->cols; out->length=f.length; out->edit=f.edit; out->cell=f.cell; out->codepoint=f.codepoint; return 1;
}
int the_input_event_from_logical_target(TheInputLogicalTargetKind kind,
                                        LINETYPE line_number, int row,
                                        int cell, int screen, int window_id,
                                        TheInputEvent *out)
{
   if (out == NULL)
      return 0;
   *out = the_input_event_none();
   out->kind = THE_INPUT_LOGICAL_HIT;
   out->target.kind = kind;
   out->target.zone = input_zone_from_target_kind(kind);
   out->target.line_number = line_number;
   out->target.row = row;
   out->target.cell = cell;
   out->target.screen = screen;
   out->target.window_id = window_id;
   return kind != THE_INPUT_TARGET_NONE && row >= 0 && cell >= 0;
}

int the_input_event_from_logical_hit(LogicalCursorZone zone,
                                     LINETYPE line_number, int row,
                                     int cell, TheInputEvent *out)
{
   if (out == NULL)
      return 0;
   *out = the_input_event_none();
   out->kind = THE_INPUT_LOGICAL_HIT;
   out->target.kind = input_target_kind_from_zone(zone);
   out->target.zone = zone;
   out->target.line_number = line_number;
   out->target.row = row;
   out->target.cell = cell;
   return out->target.kind != THE_INPUT_TARGET_NONE && row >= 0 && cell >= 0;
}

int the_input_event_from_debug_command(const char *name,
                                       TheInputEvent *out)
{
   size_t i;

   if (out == NULL || name == NULL)
      return 0;
   *out = the_input_event_none();
   for (i = 0; input_debug_names[i].name != NULL; i++)
   {
      if (input_ascii_equal_ci(name, input_debug_names[i].name))
      {
         out->kind = THE_INPUT_DEBUG;
         out->debug_command = input_debug_names[i].command;
         return 1;
      }
   }
   return 0;
}

int the_input_event_to_legacy_key(const TheInputEvent *input, int *key_code)
{
   if (input == NULL || key_code == NULL)
      return 0;
   if (input->kind != THE_INPUT_TEXT
   &&  input->kind != THE_INPUT_KEY)
      return 0;
   if (input->key_code < 0)
      return 0;
   *key_code = input->key_code;
#if defined(_WIN32) || defined(USE_XCURSES)
   /* Keep driver events portable while honoring the legacy BACKSPACE/BKSP
    * key name and profile binding used by the PDCurses command tables. */
   if (input->kind==THE_INPUT_KEY && input->key_code==THE_KEY_BACKSPACE)
      *key_code=THE_KEY_BKSP;
#endif
   return 1;
}

int the_input_legacy_key_is_mouse(int key_code)
{
   return key_code == THE_KEY_MOUSE;
}

void the_input_queue_init(TheInputQueue *queue)
{
   if (queue != NULL)
      memset(queue, 0, sizeof(*queue));
}

int the_input_queue_push(TheInputQueue *queue, TheInputEvent input)
{
   if (queue == NULL || queue->count >= THE_INPUT_QUEUE_MAX)
      return 0;
   queue->items[queue->tail] = input;
   queue->tail = (queue->tail + 1) % THE_INPUT_QUEUE_MAX;
   queue->count++;
   return 1;
}

int the_input_queue_pop(TheInputQueue *queue, TheInputEvent *input)
{
   if (queue == NULL || queue->count == 0)
      return 0;
   if (input != NULL)
      *input = queue->items[queue->head];
   queue->head = (queue->head + 1) % THE_INPUT_QUEUE_MAX;
   queue->count--;
   return 1;
}

int the_input_queue_pop_legacy_key(TheInputQueue *queue, int *key_code)
{
   TheInputEvent input;

   if (queue == NULL || key_code == NULL || queue->count == 0)
      return 0;
   input = queue->items[queue->head];
   if (!the_input_event_to_legacy_key(&input, key_code))
      return 0;
   queue->head = (queue->head + 1) % THE_INPUT_QUEUE_MAX;
   queue->count--;
   return 1;
}
