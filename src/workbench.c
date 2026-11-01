#include "workbench.h"
#include "workbenchruntime.h"
#include "workbenchdocument.h"
#include "workbenchform.h"
#include "workbenchfilefields.h"
#include "workbenchfileio.h"
#include <inttypes.h>

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* The numeric results complement, and deliberately match, the editor RCs. */
#define WB_INVALID 5
#define WB_ENVIRON (-3)
#define WB_LIMIT 94

/* The session adapter supplies policy; the registry enforces it for typed
 * callers as well as the textual command parser. */
extern int the_workbench_restricted(void);

typedef struct
{
   char id[THE_WORKBENCH_ID_MAX + 1];
   char context[THE_WORKBENCH_OPERAND_MAX + 1];
} WorkbenchOwner;

typedef struct
{
   TheWorkbenchAction actions[THE_WORKBENCH_ACTION_MAX];
   TheWorkbenchMenu menus[THE_WORKBENCH_MENU_MAX];
   TheWorkbenchItem items[THE_WORKBENCH_ITEM_MAX];
   TheWorkbenchPanel panels[THE_WORKBENCH_PANEL_MAX];
   TheWorkbenchRoute routes[THE_WORKBENCH_ROUTE_MAX];
   TheWorkbenchTool tools[THE_WORKBENCH_TOOL_MAX];
   TheWorkbenchForm forms[THE_WORKBENCH_FORM_MAX];
   TheWorkbenchField fields[THE_WORKBENCH_FIELD_MAX];
   WorkbenchOwner owners[THE_WORKBENCH_OWNER_MAX];
   size_t nactions, nmenus, nitems, nowners, npanels, nroutes,nforms,nfields,ntools;
} WorkbenchGraph;

static WorkbenchGraph published, activation, draft;
static int initialized, transaction, transaction_failed, activating;
static int activation_failed, activation_changed, activation_trusted;
static char transaction_owner[THE_WORKBENCH_ID_MAX + 1];
static char context[THE_WORKBENCH_OPERAND_MAX + 1];
static unsigned long revision, generation;
static TheWorkbenchToolLookup tool_lookup;
static void *tool_lookup_context;
static TheWorkbenchNativeToolResolver native_tool_resolver;
static TheWorkbenchResult last_result = { "ok", "", 0, 0 };

static int valid_id(const char *s)
{
   size_t n = 0;
   if (!s || !((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z')))
      return 0;
   for (; *s; s++, n++)
      if (n >= THE_WORKBENCH_ID_MAX || !( (*s >= 'A' && *s <= 'Z')
      || (*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9')
      || *s == '.' || *s == '_' || *s == '-')) return 0;
   return 1;
}

/* UTF-8 data, without control characters, overlong forms or surrogates. */
static int valid_text(const char *s, size_t max, int empty)
{
   const unsigned char *p = (const unsigned char *)s;
   size_t n;
   if (!s || (!empty && !*s) || (n = strlen(s)) > max) return 0;
   while (*p)
   {
      unsigned long c;
      int extra, i;
      if (*p < 0x80) { if (*p < 0x20 || *p == 0x7f) return 0; p++; continue; }
      if (*p >= 0xc2 && *p <= 0xdf) { c = *p & 31; extra = 1; }
      else if (*p >= 0xe0 && *p <= 0xef) { c = *p & 15; extra = 2; }
      else if (*p >= 0xf0 && *p <= 0xf4) { c = *p & 7; extra = 3; }
      else return 0;
      p++;
      for (i = 0; i < extra; i++, p++)
      { if ((*p & 0xc0) != 0x80) return 0; c = (c << 6) | (*p & 63); }
      if ((extra == 1 && c < 0x80) || (extra == 2 && c < 0x800)
      || (extra == 3 && c < 0x10000) || c > 0x10ffff
      || (c >= 0xd800 && c <= 0xdfff) || (c >= 0x80 && c <= 0x9f)) return 0;
   }
   return 1;
}

static int action_index(const WorkbenchGraph *g, const char *id)
{ size_t i; for (i = 0; i < g->nactions; i++) if (!strcmp(g->actions[i].id,id)) return (int)i; return -1; }
static int tool_index(const WorkbenchGraph *g,const char *id)
{ size_t i; for(i=0;i<g->ntools;i++) if(!strcmp(g->tools[i].id,id)) return (int)i; return -1; }
int the_workbench_arguments_valid(const TheWorkbenchArguments *args)
{
   size_t i,used=0,joined=0;
   if(!args || args->argc>THE_WORKBENCH_ARG_MAX) return 0;
   for(i=0;i<args->argc;i++) {
      const char *end;
      if(args->offsets[i]!=used || used>=sizeof(args->bytes)
      || !(end=memchr(args->bytes+used,0,sizeof(args->bytes)-used))
      || !valid_text(args->bytes+used,THE_WORKBENCH_OPERAND_MAX,1)) return 0;
      if((size_t)(end-(args->bytes+used))+(i?1u:0u)>8191u-joined) return 0;
      joined+=(size_t)(end-(args->bytes+used))+(i?1u:0u);
      used=(size_t)(end-args->bytes)+1;
   }
   return 1;
}
int the_workbench_arguments_append(TheWorkbenchArguments *args,const char *value)
{
   size_t used,length;
   if(!the_workbench_arguments_valid(args) || args->argc==THE_WORKBENCH_ARG_MAX
   || !valid_text(value,THE_WORKBENCH_OPERAND_MAX,1)) return 0;
   used=args->argc ? args->offsets[args->argc-1]+strlen(args->bytes+args->offsets[args->argc-1])+1 : 0;
   length=strlen(value)+1;
   if(length>sizeof(args->bytes)-used) return 0;
   args->offsets[args->argc++]=(unsigned short)used;
   memcpy(args->bytes+used,value,length); return 1;
}
const char *the_workbench_native_host_reason(void)
{
#if defined(__EMSCRIPTEN__) || defined(THE_WASM)
   return "unsupported-host";
#else
   if(the_workbench_restricted()) return "policy-denied";
#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
   return "ok";
#else
   return "unsupported-host";
#endif
#endif
}
void the_workbench_set_native_tool_resolver(TheWorkbenchNativeToolResolver resolver)
{ native_tool_resolver=resolver; }
const char *the_workbench_native_tool_reason(const TheWorkbenchTool *tool)
{
   const char *host=the_workbench_native_host_reason();
   if(strcmp(host,"ok")) return host;
   return tool && native_tool_resolver ? native_tool_resolver(tool) : "capability-unavailable";
}
static int menu_index(const WorkbenchGraph *g, const char *id)
{ size_t i; for (i = 0; i < g->nmenus; i++) if (!strcmp(g->menus[i].id,id)) return (int)i; return -1; }
static int item_index(const WorkbenchGraph *g, const char *id)
{ size_t i; for (i = 0; i < g->nitems; i++) if (!strcmp(g->items[i].id,id)) return (int)i; return -1; }
static int owner_index(const WorkbenchGraph *g, const char *id)
{ size_t i; for (i = 0; i < g->nowners; i++) if (!strcmp(g->owners[i].id,id)) return (int)i; return -1; }

static int panel_index(const WorkbenchGraph *g, const char *id)
{ size_t i; for (i=0;i<g->npanels;i++) if (!strcmp(g->panels[i].id,id)) return (int)i; return -1; }
static int route_index(const WorkbenchGraph *g, const char *id)
{ size_t i; for (i=0;i<g->nroutes;i++) if (!strcmp(g->routes[i].id,id)) return (int)i; return -1; }
static int form_index(const WorkbenchGraph *g,const char *id)
{ size_t i; for(i=0;i<g->nforms;i++) if(!strcmp(g->forms[i].id,id)) return (int)i; return -1; }
static int field_index(const WorkbenchGraph *g,const char *form,const char *id)
{ size_t i; for(i=0;i<g->nfields;i++) if(!strcmp(g->fields[i].id,id) && !strcmp(g->fields[i].form,form)) return (int)i; return -1; }
void the_workbench_set_tool_lookup(TheWorkbenchToolLookup lookup,void *context)
{ tool_lookup=lookup; tool_lookup_context=context; }
int the_workbench_tool_info(const char *id,TheWorkbenchToolInfo *out)
{
   const TheWorkbenchTool *native=id ? the_workbench_tool_find(id) : NULL;
   if(!out) return 0;
   if(native) { out->role="runner";out->available=!strcmp(the_workbench_native_tool_reason(native),"ok");return 1; }
   return tool_lookup && id ? tool_lookup(id,out,tool_lookup_context) : 0;
}
static int graph_tool_info(const WorkbenchGraph *g,const char *id,TheWorkbenchToolInfo *out)
{
   int i=tool_index(g,id);
   if(i>=0) { out->role="runner";out->available=!strcmp(the_workbench_native_tool_reason(g->tools+i),"ok");return 1; }
   if(tool_index(&published,id)>=0 || (activating && tool_index(&activation,id)>=0)) return 0;
   return tool_lookup ? tool_lookup(id,out,tool_lookup_context) : 0;
}

static void initialize(void)
{
   size_t i;
   if (initialized) return;
   initialized = 1;
   strcpy(published.owners[0].id,"the.builtin");
   strcpy(published.owners[0].context,"builtin");
   published.nowners = 1;
   for (i = 0; i < 2; i++)
   {
      TheWorkbenchMenu *m = &published.menus[published.nmenus++];
      strcpy(m->id,i ? "the.edit" : "the.file");
      strcpy(m->owner,"the.builtin"); strcpy(m->label,i ? "Edit" : "File");
      m->generation = ++generation; m->order = (int)i * 10;
   }
   for (i = 0; i < the_frontend_action_definition_count(); i++)
   {
      const TheFrontendActionDefinition *d = the_frontend_action_definition_at(i);
      TheWorkbenchAction *a = &published.actions[published.nactions++];
      snprintf(a->id,sizeof(a->id),"the.%s",d->name);
      strcpy(a->owner,"the.builtin"); strcpy(a->label,d->label);
      a->kind = THE_WORKBENCH_ACTION_BUILTIN; a->builtin = d->id;
      a->generation = ++generation;
      if (d->menu)
      {
         TheWorkbenchItem *item = &published.items[published.nitems++];
         snprintf(item->id,sizeof(item->id),"the.item.%s",d->name);
         strcpy(item->owner,"the.builtin"); strcpy(item->action,a->id);
         strcpy(item->label,a->label);
         strcpy(item->menu,!strcmp(d->menu,"File") ? "the.file" : "the.edit");
         item->order = (int)i; item->generation = ++generation;
      }
   }
   revision = 1;
   last_result.revision = revision;
}

void the_workbench_reset(void)
{
   the_workbench_runtime_reset();
   the_workbench_forms_reset();
   the_workbench_filefields_reset();
   memset(&published,0,sizeof(published)); memset(&activation,0,sizeof(activation));
   memset(&draft,0,sizeof(draft)); initialized = transaction = transaction_failed = 0;
   activating = activation_failed = activation_changed = 0;
   context[0] = transaction_owner[0] = '\0'; revision = generation = 0;
   last_result.code = "ok"; last_result.message = ""; last_result.rc = 0;
   initialize();
}

short the_workbench_result(const char *code, short rc, const char *message)
{
   initialize(); last_result.code = code; last_result.rc = rc;
   last_result.message = message; last_result.revision = revision;
   return rc;
}
void the_workbench_poison_definition(void)
{
   if (activating) activation_failed = 1;
   if (transaction) transaction_failed = 1;
}
static short definition_result(const char *code, short rc, const char *message)
{
   if (rc) the_workbench_poison_definition();
   return the_workbench_result(code,rc,message);
}
static short ok(void) { return definition_result("ok",0,""); }
static short invalid(void) { return definition_result("invalid-operand",WB_INVALID,"Invalid workbench operand"); }
static short denied(void) { return definition_result("policy-denied",WB_ENVIRON,"Workbench owner is not managed by this context"); }
static short limit(void) { return definition_result("limit-exceeded",WB_LIMIT,"Workbench definition limit exceeded"); }

unsigned long the_workbench_revision(void) { initialize(); return revision; }
size_t the_workbench_action_count(void) { initialize(); return published.nactions; }
size_t the_workbench_menu_count(void) { initialize(); return published.nmenus; }
size_t the_workbench_item_count(void) { initialize(); return published.nitems; }
size_t the_workbench_panel_count(void) { initialize(); return published.npanels; }
size_t the_workbench_route_count(void) { initialize(); return published.nroutes; }
size_t the_workbench_tool_count(void) { initialize();return published.ntools; }
const TheWorkbenchTool *the_workbench_tool_at(size_t i) { initialize();return i<published.ntools ? published.tools+i : NULL; }
const TheWorkbenchTool *the_workbench_tool_find(const char *id)
{ int i;initialize();i=id ? tool_index(&published,id) : -1;return i>=0 ? published.tools+i : NULL; }
size_t the_workbench_form_count(void) { initialize(); return published.nforms; }
size_t the_workbench_field_count(void) { initialize(); return published.nfields; }
const TheWorkbenchForm *the_workbench_form_at(size_t i) { initialize(); return i<published.nforms ? published.forms+i : NULL; }
const TheWorkbenchField *the_workbench_field_at(size_t i) { initialize(); return i<published.nfields ? published.fields+i : NULL; }
const TheWorkbenchForm *the_workbench_form_find(const char *id)
{ int i; initialize(); i=id ? form_index(&published,id) : -1; return i<0 ? NULL : published.forms+i; }
const TheWorkbenchField *the_workbench_field_find(const char *form,const char *id)
{ int i; initialize(); i=form && id ? field_index(&published,form,id) : -1; return i<0 ? NULL : published.fields+i; }
const TheWorkbenchPanel *the_workbench_panel_at(size_t i) { initialize(); return i < published.npanels ? published.panels+i : NULL; }
const TheWorkbenchPanel *the_workbench_panel_find(const char *id)
{ int i; initialize(); i=id ? panel_index(&published,id) : -1; return i < 0 ? NULL : published.panels+i; }
const TheWorkbenchRoute *the_workbench_route_at(size_t i) { initialize(); return i < published.nroutes ? published.routes+i : NULL; }
const char *the_workbench_placement_name(TheWorkbenchPlacement p)
{ switch(p) { case THE_WORKBENCH_PLACE_RIGHT:return "right"; case THE_WORKBENCH_PLACE_BELOW:return "below"; case THE_WORKBENCH_PLACE_TAB:return "tab"; default:return "none"; } }
const TheWorkbenchAction *the_workbench_action_at(size_t i) { initialize(); return i < published.nactions ? &published.actions[i] : NULL; }
const TheWorkbenchMenu *the_workbench_menu_at(size_t i) { initialize(); return i < published.nmenus ? &published.menus[i] : NULL; }
const TheWorkbenchItem *the_workbench_item_at(size_t i) { initialize(); return i < published.nitems ? &published.items[i] : NULL; }
const TheWorkbenchAction *the_workbench_action_find(const char *id)
{ int i; initialize(); i = id ? action_index(&published,id) : -1; return i >= 0 ? &published.actions[i] : NULL; }
const TheWorkbenchResult *the_workbench_last_result(void) { initialize(); return &last_result; }
const char *the_workbench_action_kind_name(TheWorkbenchActionKind kind)
{ switch (kind) { case THE_WORKBENCH_ACTION_BUILTIN: return "builtin"; case THE_WORKBENCH_ACTION_COMMAND: return "command"; case THE_WORKBENCH_ACTION_MACRO: return "macro"; case THE_WORKBENCH_ACTION_PANEL: return "panel"; case THE_WORKBENCH_ACTION_TOOL:return "tool"; default: return "unsupported"; } }
int the_workbench_transaction_active(void) { return transaction; }
int the_workbench_activation_active(void) { return activating; }

short the_workbench_begin(const char *owner)
{
   WorkbenchGraph *base;
   int i;
   initialize();
   if (activating && !activation_trusted) return denied();
   if (transaction) return definition_result("transaction-active",WB_ENVIRON,"A workbench transaction is already active");
   if (!valid_id(owner)) return invalid();
   if (!strncmp(owner,"the.",4)) return denied();
   base = activating ? &activation : &published;
   i = owner_index(base,owner);
   if (i >= 0 && strcmp(base->owners[i].context,activating ? context : "session")) return denied();
   if (i < 0 && base->nowners == THE_WORKBENCH_OWNER_MAX) return limit();
   draft = *base;
   if (i < 0)
   {
      i = (int)draft.nowners++;
      strcpy(draft.owners[i].id,owner);
      strcpy(draft.owners[i].context,activating ? context : "session");
   }
   strcpy(transaction_owner,owner); transaction = 1; transaction_failed = 0;
   return ok();
}

static short require_transaction(void)
{
   if (!transaction) return definition_result("transaction-required",WB_ENVIRON,"WORKBENCH BEGIN is required");
   if (transaction_failed) return definition_result("transaction-failed",WB_ENVIRON,"Abort the failed workbench transaction");
   return 0;
}

static short owned(const char *owner)
{ return !strcmp(owner,transaction_owner) ? 0 : denied(); }

short the_workbench_put_tool(const TheWorkbenchTool *value)
{
   TheWorkbenchTool tool; TheWorkbenchToolInfo legacy; int i;
   const char *host=the_workbench_native_host_reason();
   short rc=require_transaction();if(rc) return rc;
   if(strcmp(host,"ok")) return definition_result(host,WB_ENVIRON,"Native process declarations are unavailable in this host/session");
   if(!value || !valid_id(value->id)
   || !valid_text(value->executable,sizeof(value->executable)-1,0)
   || !(value->executable[0]=='/' || (isalpha((unsigned char)value->executable[0])
        && value->executable[1]==':' && (value->executable[2]=='/' || value->executable[2]=='\\'))
        || (value->executable[0]=='\\' && value->executable[1]=='\\'))) return invalid();
   if(!strncmp(value->id,"the.",4)) return denied();
   if(!strcmp(value->id,"classic-rexx") || !strcmp(value->id,"rxbin")
   || !strcmp(value->id,"profile-compiler") || !strcmp(value->id,"classic-highlight"))
      return definition_result("object-in-use",WB_ENVIRON,"A builtin RX tool owns that ID");
   i=tool_index(&draft,value->id);
   if(i>=0 && (rc=owned(draft.tools[i].owner))) return rc;
   if(tool_lookup && tool_lookup(value->id,&legacy,tool_lookup_context))
      return definition_result("object-in-use",WB_ENVIRON,"A legacy RX tool already owns that ID");
   if((i<0 && draft.ntools==THE_WORKBENCH_TOOL_MAX) || generation==LONG_MAX) return limit();
   tool=*value;strcpy(tool.owner,transaction_owner);tool.generation=++generation;
   if(i<0) i=(int)draft.ntools++;
   draft.tools[i]=tool;return ok();
}
short the_workbench_remove_tool(const char *id)
{
   int i;short rc=require_transaction();if(rc) return rc;
   if(!valid_id(id)) return invalid();if(!strncmp(id,"the.",4)) return denied();
   i=tool_index(&draft,id);if(i<0) return ok();
   if((rc=owned(draft.tools[i].owner))) return rc;
   memmove(draft.tools+i,draft.tools+i+1,(--draft.ntools-(size_t)i)*sizeof(draft.tools[0]));return ok();
}

short the_workbench_put_action(const TheWorkbenchAction *value)
{
   TheWorkbenchAction a;
   TheFrontendAction prepared;
   const TheFrontendActionDefinition *d;
   int i;
   short rc = require_transaction(); if (rc) return rc;
   if (!value || !valid_id(value->id) || !valid_text(value->label,THE_WORKBENCH_LABEL_MAX,0)
   || !valid_text(value->operand,THE_WORKBENCH_OPERAND_MAX,1)) return invalid();
   if (!strncmp(value->id,"the.",4)) return denied();
   if(value->kind==THE_WORKBENCH_ACTION_TOOL) {
      const char *host=the_workbench_native_host_reason();
      if(strcmp(host,"ok")) return definition_result(host,WB_ENVIRON,"Native tool actions are unavailable in this host/session");
   }
   if (the_workbench_restricted() && value->kind != THE_WORKBENCH_ACTION_BUILTIN
   && value->kind != THE_WORKBENCH_ACTION_PANEL)
      return definition_result("policy-denied",WB_ENVIRON,"Restricted sessions may define only typed builtin actions");
   if (value->kind == THE_WORKBENCH_ACTION_BUILTIN)
   {
      d = the_frontend_action_definition(value->builtin);
      if (!d || ((!d->requires_argument || value->operand[0])
      && !the_frontend_action_init(value->builtin,value->operand,&prepared))) return invalid();
   }
   else if (value->kind == THE_WORKBENCH_ACTION_PANEL)
   {
      if (!valid_id(value->panel) || value->operand[0]
      || value->panel_operation < THE_WORKBENCH_PANEL_SHOW || value->panel_operation > THE_WORKBENCH_PANEL_FOCUS) return invalid();
   }
   else if(value->kind==THE_WORKBENCH_ACTION_TOOL) {
      if(!valid_id(value->tool) || value->operand[0] || !the_workbench_arguments_valid(&value->arguments)) return invalid();
   }
   else if (value->kind != THE_WORKBENCH_ACTION_COMMAND && value->kind != THE_WORKBENCH_ACTION_MACRO)
      return definition_result("unsupported-feature",WB_ENVIRON,"This action kind is not implemented");
   else if (!value->operand[0] || (value->kind == THE_WORKBENCH_ACTION_MACRO && strchr(value->operand,'"'))) return invalid();
   i = action_index(&draft,value->id);
   if (i >= 0 && (rc = owned(draft.actions[i].owner))) return rc;
   if (i < 0 && draft.nactions == THE_WORKBENCH_ACTION_MAX) return limit();
   if (generation == LONG_MAX) return limit();
   a = *value; strcpy(a.owner,transaction_owner); a.generation = ++generation;
   if (i < 0) i = (int)draft.nactions++;
   draft.actions[i] = a;
   return ok();
}

short the_workbench_put_menu(const TheWorkbenchMenu *value)
{
   TheWorkbenchMenu m;
   int i; short rc = require_transaction(); if (rc) return rc;
   if (!value || !valid_id(value->id) || value->order < 0
   || !valid_text(value->label,THE_WORKBENCH_LABEL_MAX,0)) return invalid();
   if (!strncmp(value->id,"the.",4)) return denied();
   i = menu_index(&draft,value->id);
   if (i >= 0 && (rc = owned(draft.menus[i].owner))) return rc;
   if (i < 0 && draft.nmenus == THE_WORKBENCH_MENU_MAX) return limit();
   if (generation == LONG_MAX) return limit();
   m = *value; strcpy(m.owner,transaction_owner); m.generation = ++generation;
   if (i < 0) i = (int)draft.nmenus++;
   draft.menus[i] = m; return ok();
}

short the_workbench_put_item(const TheWorkbenchItem *value)
{
   TheWorkbenchItem item;
   int i; short rc = require_transaction(); if (rc) return rc;
   if (!value || !valid_id(value->id) || !valid_id(value->menu) || value->order < 0
   || (!value->separator && (!valid_id(value->action)
   || !valid_text(value->label,THE_WORKBENCH_LABEL_MAX,0)))) return invalid();
   if (!strncmp(value->id,"the.",4)) return denied();
   i = item_index(&draft,value->id);
   if (i >= 0 && (rc = owned(draft.items[i].owner))) return rc;
   if (i < 0 && draft.nitems == THE_WORKBENCH_ITEM_MAX) return limit();
   if (generation == LONG_MAX) return limit();
   item = *value; strcpy(item.owner,transaction_owner); item.generation = ++generation;
   if (item.separator) item.action[0] = item.label[0] = '\0';
   if (i < 0) i = (int)draft.nitems++;
   draft.items[i] = item; return ok();
}

short the_workbench_remove_action(const char *id)
{
   int i; short rc = require_transaction(); if (rc) return rc;
   if (!valid_id(id)) return invalid(); if (!strncmp(id,"the.",4)) return denied();
   i = action_index(&draft,id); if (i < 0) return ok();
   if ((rc = owned(draft.actions[i].owner))) return rc;
   memmove(&draft.actions[i],&draft.actions[i+1],(--draft.nactions-(size_t)i)*sizeof(draft.actions[0]));
   return ok();
}
short the_workbench_remove_item(const char *menu, const char *id)
{
   int i; short rc = require_transaction(); if (rc) return rc;
   if (!valid_id(menu) || !valid_id(id)) return invalid();
   if (!strncmp(id,"the.",4)) return denied();
   i = item_index(&draft,id); if (i < 0) return ok();
   if (strcmp(draft.items[i].menu,menu)) return invalid();
   if ((rc = owned(draft.items[i].owner))) return rc;
   memmove(&draft.items[i],&draft.items[i+1],(--draft.nitems-(size_t)i)*sizeof(draft.items[0]));
   return ok();
}
short the_workbench_remove_menu(const char *id)
{
   int i; size_t n; short rc = require_transaction(); if (rc) return rc;
   if (!valid_id(id)) return invalid(); if (!strncmp(id,"the.",4)) return denied();
   i = menu_index(&draft,id); if (i < 0) return ok();
   if ((rc = owned(draft.menus[i].owner))) return rc;
   for (n = draft.nitems; n > 0; n--) if (!strcmp(draft.items[n-1].menu,id))
   {
      if (strcmp(draft.items[n-1].owner,transaction_owner)) return denied();
      memmove(&draft.items[n-1],&draft.items[n],(draft.nitems-n)*sizeof(draft.items[0])); draft.nitems--;
   }
   memmove(&draft.menus[i],&draft.menus[i+1],(--draft.nmenus-(size_t)i)*sizeof(draft.menus[0]));
   return ok();
}

static int placement_valid(TheWorkbenchPlacement preferred,const TheWorkbenchPlacement *fallbacks,size_t count)
{
   size_t i,j;
   if (preferred < THE_WORKBENCH_PLACE_RIGHT || preferred > THE_WORKBENCH_PLACE_TAB || count > 3) return 0;
   for (i=0;i<count;i++)
   {
      if (fallbacks[i] < THE_WORKBENCH_PLACE_RIGHT || fallbacks[i] > THE_WORKBENCH_PLACE_TAB || fallbacks[i] == preferred) return 0;
      for (j=0;j<i;j++) if (fallbacks[i] == fallbacks[j]) return 0;
   }
   return 1;
}
short the_workbench_put_panel(const TheWorkbenchPanel *value)
{
   TheWorkbenchPanel p;
   int i; short rc=require_transaction(); if (rc) return rc;
   if (!value || !valid_id(value->id)
   || (value->title[0] && !valid_text(value->title,THE_WORKBENCH_LABEL_MAX,0))) return invalid();
   if (value->kind == THE_WORKBENCH_PANEL_LOG)
   { if (!valid_id(value->model) || value->document_id) return invalid(); }
   else if (value->kind == THE_WORKBENCH_PANEL_DOCUMENT)
   {
      TheWorkbenchDocumentInfo info;
      if (value->model[0] || !value->document_id) return invalid();
      if (the_workbench_document_info(value->document_id,&info)==THE_WORKBENCH_DOCUMENT_MISSING)
         return definition_result("document-closed",-3,"The document binding is no longer live");
   }
   else if(value->kind==THE_WORKBENCH_PANEL_FIELDS)
   { if(value->model[0] || (value->document_id ? value->form[0]!=0 : !valid_id(value->form))) return invalid(); }
   else return definition_result("unsupported-feature",-3,"This panel kind is not implemented");
   if (!strncmp(value->id,"the.",4) || !strncmp(value->model,"the.",4)) return denied();
   p=*value;
   if (!p.title[0]) strcpy(p.title,p.id);
   if (!p.placement)
   { p.placement=THE_WORKBENCH_PLACE_RIGHT; p.fallbacks[0]=THE_WORKBENCH_PLACE_BELOW; p.fallbacks[1]=THE_WORKBENCH_PLACE_TAB; p.fallback_count=2; }
   if (!p.mincols) p.mincols=32;
   if (!p.minrows) p.minrows=4;
   if (!placement_valid(p.placement,p.fallbacks,p.fallback_count)
   || p.mincols < 1 || p.mincols > 32767 || p.minrows < 1 || p.minrows > 32767) return invalid();
   i=panel_index(&draft,p.id);
   if (i >= 0 && (rc=owned(draft.panels[i].owner))) return rc;
   if ((i < 0 && draft.npanels == THE_WORKBENCH_PANEL_MAX) || generation == LONG_MAX) return limit();
   if (i < 0) i=(int)draft.npanels++;
   strcpy(p.owner,transaction_owner); p.generation=++generation; p.model_generation=0;
   draft.panels[i]=p; return ok();
}
short the_workbench_place_panel(const char *id,TheWorkbenchPlacement preferred,
   const TheWorkbenchPlacement *fallbacks,size_t count,int mincols)
{
   TheWorkbenchPanel p; int i; short rc=require_transaction(); if (rc) return rc;
   if (!valid_id(id) || (count && !fallbacks) || mincols < 1 || mincols > 32767) return invalid();
   i=panel_index(&draft,id);
   if (i < 0) return definition_result("missing-object",5,"No candidate panel has that ID");
   if ((rc=owned(draft.panels[i].owner))) return rc;
   p=draft.panels[i]; p.placement=preferred; p.fallback_count=count;
   if (count > 3) return invalid();
   if (count) memcpy(p.fallbacks,fallbacks,count*sizeof(*fallbacks));
   p.mincols=mincols;
   return the_workbench_put_panel(&p);
}
short the_workbench_remove_panel(const char *id)
{
   int i; short rc=require_transaction(); if (rc) return rc;
   if (!valid_id(id)) return invalid(); if (!strncmp(id,"the.",4)) return denied();
   i=panel_index(&draft,id); if (i < 0) return ok();
   if ((rc=owned(draft.panels[i].owner))) return rc;
   memmove(draft.panels+i,draft.panels+i+1,(--draft.npanels-(size_t)i)*sizeof(draft.panels[0])); return ok();
}
short the_workbench_put_route(const TheWorkbenchRoute *value)
{
   TheWorkbenchRoute r; int i; short rc=require_transaction(); if (rc) return rc;
   if (!value || !valid_id(value->id) || !valid_id(value->tool)
   || value->kind<THE_WORKBENCH_ROUTE_LOG || value->kind>THE_WORKBENCH_ROUTE_FILE_OUTPUT) return invalid();
   if(value->kind==THE_WORKBENCH_ROUTE_LOG) {
      if(!valid_id(value->model) || (value->stream!=1 && value->stream!=2)) return invalid();
      if(!strncmp(value->model,"the.",4)) return denied();
   } else {
      if(!memchr(value->path,0,sizeof(value->path)) || !value->path[0]
      || (value->kind==THE_WORKBENCH_ROUTE_FILE_INPUT ? value->stream!=0 : (value->stream!=1 && value->stream!=2))) return invalid();
      if(the_workbench_restricted() || !the_workbench_fileio_supported())
         return definition_result("unsupported-feature",WB_ENVIRON,"Native FILE routes are unavailable in this host");
   }
   if (!strncmp(value->id,"the.",4)) return denied();
   i=route_index(&draft,value->id);
   if (i >= 0 && (rc=owned(draft.routes[i].owner))) return rc;
   if ((i < 0 && draft.nroutes == THE_WORKBENCH_ROUTE_MAX) || generation == LONG_MAX) return limit();
   if (i < 0) i=(int)draft.nroutes++;
   r=*value; strcpy(r.owner,transaction_owner); r.generation=++generation; draft.routes[i]=r; return ok();
}
short the_workbench_remove_route(const char *id)
{
   int i; short rc=require_transaction(); if (rc) return rc;
   if (!valid_id(id)) return invalid(); if (!strncmp(id,"the.",4)) return denied();
   i=route_index(&draft,id); if (i < 0) return ok();
   if ((rc=owned(draft.routes[i].owner))) return rc;
   memmove(draft.routes+i,draft.routes+i+1,(--draft.nroutes-(size_t)i)*sizeof(draft.routes[0])); return ok();
}

short the_workbench_put_form(const TheWorkbenchForm *value)
{
   TheWorkbenchForm form; int i; short rc=require_transaction(); if(rc) return rc;
   if(!value || !valid_id(value->id)) return invalid(); if(!strncmp(value->id,"the.",4)) return denied();
   i=form_index(&draft,value->id); if(i>=0 && (rc=owned(draft.forms[i].owner))) return rc;
   if((i<0 && draft.nforms==THE_WORKBENCH_FORM_MAX) || generation==LONG_MAX) return limit();
   if(i<0) i=(int)draft.nforms++;
   form=*value; strcpy(form.owner,transaction_owner); form.generation=++generation; draft.forms[i]=form; return ok();
}
short the_workbench_put_field(const TheWorkbenchField *value)
{
   TheWorkbenchField field; int i,form; size_t n,count=0; short rc=require_transaction(); if(rc) return rc;
   if(!value || !valid_id(value->id) || !valid_id(value->form)
   || !valid_text(value->label,THE_WORKBENCH_LABEL_MAX,1) || !memchr(value->value,0,sizeof(value->value))
   || !the_workbench_field_value_valid(value->value,strlen(value->value),value->maxcells)
   || (value->access!=THE_WORKBENCH_FIELD_EDIT && value->access!=THE_WORKBENCH_FIELD_READONLY)) return invalid();
   if(!strncmp(value->id,"the.",4)) return denied();
   form=form_index(&draft,value->form); if(form<0) return definition_result("missing-object",5,"Create the form before its fields");
   if((rc=owned(draft.forms[form].owner))) return rc;
   i=field_index(&draft,value->form,value->id); if(i>=0 && (rc=owned(draft.fields[i].owner))) return rc;
   for(n=0;n<draft.nfields;n++) if(!strcmp(draft.fields[n].form,value->form)) count++;
   if((i<0 && (draft.nfields==THE_WORKBENCH_FIELD_MAX || count==THE_WORKBENCH_FIELDS_PER_FORM_MAX)) || generation>LONG_MAX-2) return limit();
   if(i<0) i=(int)draft.nfields++;
   field=*value; strcpy(field.owner,transaction_owner); field.generation=++generation; draft.fields[i]=field;
   draft.forms[form].generation=++generation; return ok();
}
short the_workbench_remove_field(const char *form,const char *id)
{
   int i,f; short rc=require_transaction(); if(rc) return rc;
   if(!valid_id(form) || !valid_id(id)) return invalid();
   i=field_index(&draft,form,id); if(i<0) return ok();
   if(strcmp(draft.fields[i].form,form)) return invalid(); if((rc=owned(draft.fields[i].owner))) return rc;
   if(generation==LONG_MAX) return limit();
   f=form_index(&draft,form); if(f>=0) draft.forms[f].generation=++generation;
   memmove(draft.fields+i,draft.fields+i+1,(--draft.nfields-(size_t)i)*sizeof(draft.fields[0])); return ok();
}
short the_workbench_remove_form(const char *id)
{
   int i; size_t n; short rc=require_transaction(); if(rc) return rc;
   if(!valid_id(id)) return invalid(); i=form_index(&draft,id); if(i<0) return ok();
   if((rc=owned(draft.forms[i].owner))) return rc;
   for(n=draft.nfields;n>0;n--) if(!strcmp(draft.fields[n-1].form,id))
   { memmove(draft.fields+n-1,draft.fields+n,(draft.nfields-n)*sizeof(draft.fields[0])); draft.nfields--; }
   memmove(draft.forms+i,draft.forms+i+1,(--draft.nforms-(size_t)i)*sizeof(draft.forms[0])); return ok();
}

static void remove_owner(WorkbenchGraph *g, const char *owner)
{
   size_t n;
   for(n=g->ntools;n>0;n--) if(!strcmp(g->tools[n-1].owner,owner))
   { memmove(g->tools+n-1,g->tools+n,(g->ntools-n)*sizeof(g->tools[0]));g->ntools--; }
   for(n=g->nfields;n>0;n--) if(!strcmp(g->fields[n-1].owner,owner))
   { memmove(g->fields+n-1,g->fields+n,(g->nfields-n)*sizeof(g->fields[0])); g->nfields--; }
   for(n=g->nforms;n>0;n--) if(!strcmp(g->forms[n-1].owner,owner))
   { memmove(g->forms+n-1,g->forms+n,(g->nforms-n)*sizeof(g->forms[0])); g->nforms--; }
   for (n=g->nroutes;n>0;n--) if (!strcmp(g->routes[n-1].owner,owner))
   { memmove(g->routes+n-1,g->routes+n,(g->nroutes-n)*sizeof(g->routes[0])); g->nroutes--; }
   for (n=g->npanels;n>0;n--) if (!strcmp(g->panels[n-1].owner,owner))
   { memmove(g->panels+n-1,g->panels+n,(g->npanels-n)*sizeof(g->panels[0])); g->npanels--; }
   for (n = g->nitems; n > 0; n--) if (!strcmp(g->items[n-1].owner,owner))
   { memmove(&g->items[n-1],&g->items[n],(g->nitems-n)*sizeof(g->items[0])); g->nitems--; }
   for (n = g->nmenus; n > 0; n--) if (!strcmp(g->menus[n-1].owner,owner))
   { memmove(&g->menus[n-1],&g->menus[n],(g->nmenus-n)*sizeof(g->menus[0])); g->nmenus--; }
   for (n = g->nactions; n > 0; n--) if (!strcmp(g->actions[n-1].owner,owner))
   { memmove(&g->actions[n-1],&g->actions[n],(g->nactions-n)*sizeof(g->actions[0])); g->nactions--; }
}
short the_workbench_reset_owner(const char *owner)
{
   short rc = require_transaction(); if (rc) return rc;
   if (!valid_id(owner)) return invalid(); if ((rc = owned(owner))) return rc;
   remove_owner(&draft,owner); return ok();
}

static int reference_allowed(const char *owner, const char *target)
{ return !strcmp(owner,target) || !strcmp(target,"the.builtin"); }
static short validate(WorkbenchGraph *g)
{
   size_t n; int i;
   for(n=0;n<g->ntools;n++) {
      TheWorkbenchToolInfo legacy;
      if(tool_lookup && tool_lookup(g->tools[n].id,&legacy,tool_lookup_context))
         return definition_result("object-in-use",WB_ENVIRON,"A legacy RX tool claimed a candidate native tool ID");
   }
   for(n=0;n<g->nactions;n++) if(g->actions[n].kind==THE_WORKBENCH_ACTION_TOOL) {
      const TheWorkbenchAction *a=g->actions+n;
      i=tool_index(g,a->tool);
      if(i<0) {
         int existed=tool_index(activating ? &activation : &published,a->tool)>=0;
         return definition_result(existed ? "object-in-use" : "missing-object",existed ? WB_ENVIRON : WB_INVALID,"Tool action refers to a missing native tool");
      }
      if(!reference_allowed(a->owner,g->tools[i].owner)) return denied();
   }
   for(n=0;n<g->nfields;n++)
   {
      i=form_index(g,g->fields[n].form);
      if(i<0) return definition_result("missing-object",5,"Field refers to a missing form");
      if(!reference_allowed(g->fields[n].owner,g->forms[i].owner)) return denied();
   }
   for(n=0;n<g->npanels;n++) if(g->panels[n].kind==THE_WORKBENCH_PANEL_FIELDS && !g->panels[n].document_id)
   {
      i=form_index(g,g->panels[n].form);
      if(i<0) return definition_result("object-in-use",-3,"Fields pane refers to a missing form");
      if(!reference_allowed(g->panels[n].owner,g->forms[i].owner)) return denied();
      g->panels[n].form_generation=g->forms[i].generation;
   }
   for (n = 0; n < g->nitems; n++)
   {
      const TheWorkbenchItem *item = &g->items[n];
      i = menu_index(g,item->menu);
      if (i < 0) return definition_result("missing-object",WB_INVALID,"Menu item refers to a missing menu");
      /* Adding to a built-in menu is allowed; it does not change its owner. */
      if (!reference_allowed(item->owner,g->menus[i].owner)) return denied();
      if (item->separator) continue;
      i = action_index(g,item->action);
      if (i < 0)
      {
         int existed = action_index(activating ? &activation : &published,item->action) >= 0;
         return definition_result(existed ? "object-in-use" : "missing-action",existed ? WB_ENVIRON : WB_INVALID,
                                    "Menu item refers to a missing action");
      }
      if (!reference_allowed(item->owner,g->actions[i].owner)) return denied();
   }
   for (n=0;n<g->npanels;n++)
   {
      if (g->panels[n].kind!=THE_WORKBENCH_PANEL_LOG) continue;
      const TheWorkbenchModelState *m=the_workbench_model_state(g->panels[n].model);
      size_t k;
      if (m && strcmp(m->owner,g->panels[n].owner)) return denied();
      for (k=0;k<n;k++) if (g->panels[k].kind==THE_WORKBENCH_PANEL_LOG && !strcmp(g->panels[k].model,g->panels[n].model)
      && strcmp(g->panels[k].owner,g->panels[n].owner)) return denied();
   }
   for (n=0;n<g->nactions;n++) if (g->actions[n].kind == THE_WORKBENCH_ACTION_PANEL)
   {
      i=panel_index(g,g->actions[n].panel);
      if (i < 0)
      {
         int existed=panel_index(activating ? &activation : &published,g->actions[n].panel) >= 0;
         return definition_result(existed ? "object-in-use" : "missing-object",existed ? -3 : 5,"Panel action refers to a missing panel");
      }
      if (!reference_allowed(g->actions[n].owner,g->panels[i].owner)) return denied();
   }
   for (n=0;n<g->nroutes;n++)
   {
      const TheWorkbenchRoute *r=g->routes+n;
      const TheWorkbenchModelState *m=the_workbench_model_state(r->model);
      const char *owner=m ? m->owner : NULL;
      TheWorkbenchToolInfo info; size_t k;
      if(r->kind==THE_WORKBENCH_ROUTE_LOG) {
         for (k=0;k<g->npanels;k++) if (!strcmp(g->panels[k].model,r->model)) { owner=g->panels[k].owner; break; }
         if (!owner) return definition_result("missing-object",5,"Route refers to a missing log model");
         if (!reference_allowed(r->owner,owner)) return denied();
      }
      if (!graph_tool_info(g,r->tool,&info)) {
         int existed=tool_index(activating ? &activation : &published,r->tool)>=0;
         return definition_result(existed?"object-in-use":"missing-object",existed?-3:5,"Route refers to an unknown tool");
      }
      i=tool_index(g,r->tool);
      if(i>=0 && !reference_allowed(r->owner,g->tools[i].owner)) return denied();
      if (!info.role || (strcmp(info.role,"runner") && strcmp(info.role,"program") && strcmp(info.role,"compiler")))
         return definition_result("unsupported-feature",-3,"Only program stdout/stderr log routes are supported");
      if(r->kind!=THE_WORKBENCH_ROUTE_LOG) {
         static char path_error[256];
         const char *code=the_workbench_fileio_check(r->path,r->kind==THE_WORKBENCH_ROUTE_FILE_OUTPUT,0,path_error,sizeof(path_error));
         if(strcmp(code,"ok")) return definition_result(code,!strcmp(code,"invalid-operand")?5:-3,path_error);
      }
      for (k=0;k<n;k++) if (!strcmp(g->routes[k].tool,r->tool)
      && ((g->routes[k].kind==r->kind && (r->kind==THE_WORKBENCH_ROUTE_FILE_OUTPUT || g->routes[k].stream==r->stream))))
         return definition_result("object-in-use",-3,"A tool already has that input, output file or primary log route");
   }
   return 0;
}

static short publish(WorkbenchGraph *graph)
{
   TheWorkbenchFormPublication *forms=NULL; TheWorkbenchFileFieldsPublication *filefields=NULL; short rc;
   rc=the_workbench_forms_prepare(graph->forms,graph->nforms,graph->fields,graph->nfields,&forms);
   if(rc) return rc;
   rc=the_workbench_filefields_prepare(graph->panels,graph->npanels,graph->nfields,&filefields);
   if(rc) { the_workbench_forms_abort(forms); return rc; }
   rc=the_workbench_runtime_publish(graph->panels,graph->npanels);
   if(rc) { the_workbench_forms_abort(forms); the_workbench_filefields_abort(filefields); return rc; }
   the_workbench_forms_commit(forms); the_workbench_filefields_commit(filefields); published=*graph; revision++; return 0;
}
short the_workbench_commit(void)
{
   short rc = require_transaction(); if (rc) return rc;
   if ((rc = validate(&draft))) return rc;
   if (activating) { activation = draft; activation_changed = 1; }
   else
   {
      if ((rc=publish(&draft)))
      { the_workbench_poison_definition(); return rc; }
   }
   transaction = transaction_failed = 0; transaction_owner[0] = '\0';
   return ok();
}
short the_workbench_abort(void)
{
   transaction = transaction_failed = 0; transaction_owner[0] = '\0';
   return ok();
}
static short activation_begin(const char *profile, int trusted)
{
   size_t n;
   initialize();
   if (activating || transaction) return definition_result("transaction-active",WB_ENVIRON,"Profile activation cannot nest a workbench transaction");
   if (!valid_text(profile,THE_WORKBENCH_OPERAND_MAX,0)) return invalid();
   activation = published; strcpy(context,profile); activating = 1;
   activation_failed = activation_changed = 0; activation_trusted = trusted;
   for (n = 0; trusted && n < activation.nowners; n++) if (!strcmp(activation.owners[n].context,context))
   { remove_owner(&activation,activation.owners[n].id); activation_changed = 1; }
   return ok();
}
short the_workbench_activation_begin(const char *profile)
{ return activation_begin(profile,1); }
short the_workbench_activation_begin_untrusted(const char *profile)
{ return activation_begin(profile,0); }
short the_workbench_activation_end(int success)
{
   short rc = 0;
   if (!activating) return definition_result("invalid-operand",WB_INVALID,"No workbench activation is active");
   if (success && !activation_failed && !transaction) rc = validate(&activation);
   if (!success || activation_failed || transaction || rc)
   {
      transaction = transaction_failed = 0; activating = 0; context[0] = '\0';
      return definition_result("activation-failed",WB_ENVIRON,"Profile workbench definitions were not published");
   }
   if (activation_changed)
   {
      rc=publish(&activation);
      if (rc) { activating=0; context[0]=0; return definition_result("activation-failed",-3,"Profile workbench resources could not be published"); }
   }
   activating = 0; context[0] = '\0'; return ok();
}

/* Bounded JSON writer. UTF-8 data is passed through; controls are escaped. */
typedef struct { char *out; size_t len, used; int good; } Json;
static void js(Json *j, const char *format, ...)
{
   va_list ap; int n;
   if (!j->good) return;
   va_start(ap,format); n = vsnprintf(j->out+j->used,j->len-j->used,format,ap); va_end(ap);
   if (n < 0 || (size_t)n >= j->len-j->used) { j->good = 0; return; }
   j->used += (size_t)n;
}
static void jq(Json *j, const char *s)
{
   const unsigned char *p = (const unsigned char *)s;
   js(j,"\"");
   for (; *p; p++) if (*p == '"' || *p == '\\') js(j,"\\%c",*p);
      else if (*p < 32 || *p == 127) js(j,"\\u%04x",*p); else js(j,"%c",*p);
   js(j,"\"");
}
static void common(Json *j, const char *id, const char *owner, const char *label, unsigned long gen)
{
   js(j,"{\"id\":"); jq(j,id); js(j,",\"owner\":"); jq(j,owner);
   js(j,",\"label\":"); jq(j,label); js(j,",\"generation\":%lu",gen);
}
static const char *log_result_code(TheWorkbenchLogResult result)
{
   switch (result)
   {
      case THE_WORKBENCH_LOG_OK:return "ok";
      case THE_WORKBENCH_LOG_MISSING:return "missing-object";
      case THE_WORKBENCH_LOG_STALE:return "stale-generation";
      case THE_WORKBENCH_LOG_LIMIT:case THE_WORKBENCH_LOG_NOMEM:return "limit-exceeded";
      default:return "invalid-operand";
   }
}
static void json_limits(Json *j)
{
   js(j,",\"limits\":{\"idBytes\":63,\"labelBytes\":255,\"operandBytes\":2048,\"commandBytes\":8192,\"actions\":128,\"menus\":32,\"items\":256,\"owners\":32,\"panels\":32,\"routes\":32,\"logs\":32,\"logBytes\":262144,\"logSessionBytes\":8388608,\"logPageBytes\":8192,\"pageRecords\":8,\"jsonBytes\":65536,\"invokeDepth\":8,\"forms\":32,\"fields\":512,\"fieldsPerForm\":128,\"fieldValueBytes\":1024,\"fieldMaxCells\":1024,\"fileFieldProjections\":32,\"fileFieldRecords\":128,\"fileFieldSpans\":128,\"fileFieldBytes\":262144,\"fileFieldSessionBytes\":8388608}");
}
int the_workbench_json(char *out, size_t len, const char *section, size_t offset, size_t count, int candidate)
{
   Json j; const WorkbenchGraph *g; size_t n,total,end; int type;
   initialize();
   if (!out || !len || !count || count > THE_WORKBENCH_PAGE_MAX) return 0;
   if(section && !strcmp(section,"FILEIO")) {
      if(the_workbench_restricted()) {
         int n=snprintf(out,len,"{\"schemaVersion\":1,\"section\":\"FILEIO\",\"available\":false,\"create\":false,\"replace\":false,\"append\":false,\"code\":\"unsupported-feature\"}");
         return n>=0 && (size_t)n<len;
      }
      return the_workbench_fileio_json(out,len);
   }
   out[0]=0; j.out=out; j.len=len; j.used=0; j.good=1;
   g=candidate && activating ? &activation : &published;
   if (!section || !*section || !strcmp(section,"SUMMARY")) type=0;
   else if (!strcmp(section,"CAPABILITIES")) type=1;
   else if (!strcmp(section,"ACTIONS")) type=2;
   else if (!strcmp(section,"MENUS")) type=3;
   else if (!strcmp(section,"ITEMS")) type=4;
   else if (!strcmp(section,"PANELS")) type=5;
   else if (!strcmp(section,"ROUTES")) type=6;
   else if (!strcmp(section,"LOGS")) type=7;
   else if (!strcmp(section,"FORMS")) type=8;
   else if (!strcmp(section,"TOOLS")) type=9;
   else return 0;
   js(&j,"{\"schemaVersion\":1,\"revision\":%lu,\"section\":\"%s\"",revision,
      type == 0 ? "summary" : type == 1 ? "capabilities" : type == 2 ? "actions" : type == 3 ? "menus" : type == 4 ? "items" : type == 5 ? "panels" : type == 6 ? "routes" : type==7 ? "logs" : type==8 ? "forms" : "tools");
   if (type == 0)
   {
      const TheWorkbenchFocusRequest *focus=the_workbench_focus_request();
      js(&j,",\"actions\":%lu,\"menus\":%lu,\"items\":%lu,\"panels\":%lu,\"routes\":%lu,\"logs\":%lu,\"panelStateRevision\":%lu,\"transactionActive\":%s,\"activationActive\":%s,\"lastResult\":{\"code\":",
         (unsigned long)g->nactions,(unsigned long)g->nmenus,(unsigned long)g->nitems,
         (unsigned long)g->npanels,(unsigned long)g->nroutes,(unsigned long)the_workbench_model_count(),
         the_workbench_panel_state_revision(),transaction ? "true" : "false",activating ? "true" : "false");
      jq(&j,last_result.code); js(&j,",\"rc\":%d,\"message\":",last_result.rc); jq(&j,last_result.message); js(&j,"}");
      js(&j,",\"forms\":%lu,\"fields\":%lu,\"formStateRevision\":\"%" PRIu64 "\"",(unsigned long)g->nforms,(unsigned long)g->nfields,the_workbench_form_state_revision());
      js(&j,",\"tools\":%lu",(unsigned long)g->ntools);
      js(&j,",\"focusRequest\":{\"serial\":%lu,\"panel\":",focus->serial); jq(&j,focus->panel);
      js(&j,",\"generation\":\"%lu\",\"restoring\":%s,\"previousPanel\":",focus->generation,focus->restoring ? "true" : "false"); jq(&j,focus->previous_panel); js(&j,",\"previousGeneration\":\"%lu\"}",focus->previous_generation);
      json_limits(&j); js(&j,"}");
   }
   else if (type == 1)
   {
      js(&j,",\"actions\":true,\"menus\":true,\"panels\":true,\"logPanels\":true,\"documentPanels\":true,\"forms\":true,\"editableFileFields\":true,\"routes\":true,\"tools\":true");
      js(&j,",\"nativeProcess\":%s,\"nativeProcessReason\":",!strcmp(the_workbench_native_host_reason(),"ok")?"true":"false");
      jq(&j,the_workbench_native_host_reason());
      js(&j,",\"nativeProcessQualification\":\"macos-arm64\",\"nativeSource\":\"snapshot\",\"nativeWorkingDirectory\":\"session\",\"nativeEnvironment\":\"inherit\",\"nativeEncoding\":\"captured-bytes-lf\",\"nativeIncludes\":\"interpreter-search-from-staged-source\",\"nativeCompletion\":\"process-exit-only\",\"nativeMacroInvocation\":false");
      js(&j,",\"nativeLimits\":{\"tools\":32,\"argvEntries\":128,\"joinedArgvBytes\":8191,\"sourceBytes\":2097152,\"executableBytes\":2047}");
      js(&j,",\"fileInput\":%s,\"fileCreate\":%s,\"fileReplace\":false,\"fileAppend\":false,\"fileWorkspace\":",
         !the_workbench_restricted() && the_workbench_fileio_supported()?"true":"false",
         !the_workbench_restricted() && the_workbench_fileio_supported()?"true":"false");
      jq(&j,the_workbench_restricted()?"":the_workbench_fileio_workspace());
      json_limits(&j); js(&j,"}");
   }
   else
   {
      total=type == 2 ? g->nactions : type == 3 ? g->nmenus : type == 4 ? g->nitems : type == 5 ? g->npanels : type == 6 ? g->nroutes : type==7 ? the_workbench_model_count() : type==8 ? g->nforms : g->ntools;
      end=offset < total ? offset + (count < total-offset ? count : total-offset) : offset;
      js(&j,",\"total\":%lu,\"offset\":%lu,\"nextOffset\":",(unsigned long)total,(unsigned long)offset);
      if (end < total) js(&j,"%lu",(unsigned long)end); else js(&j,"null");
      js(&j,",\"records\":[");
      for (n=offset;n<end;n++)
      {
         if (n != offset) js(&j,",");
         if (type == 2)
         {
            const TheWorkbenchAction *a=g->actions+n; const char *reason=the_workbench_action_reason(a);
            common(&j,a->id,a->owner,a->label,a->generation);
            js(&j,",\"kind\":"); jq(&j,the_workbench_action_kind_name(a->kind));
            js(&j,",\"builtin\":"); jq(&j,a->kind == THE_WORKBENCH_ACTION_BUILTIN ? the_frontend_action_name(a->builtin) : "");
            js(&j,",\"operand\":"); jq(&j,a->operand);
            js(&j,",\"requiresArgument\":%s",a->kind == THE_WORKBENCH_ACTION_BUILTIN && the_frontend_action_definition(a->builtin) && the_frontend_action_definition(a->builtin)->requires_argument ? "true" : "false");
            if (a->kind == THE_WORKBENCH_ACTION_PANEL)
            { js(&j,",\"panel\":"); jq(&j,a->panel); js(&j,",\"operation\":"); jq(&j,a->panel_operation == THE_WORKBENCH_PANEL_SHOW ? "show" : a->panel_operation == THE_WORKBENCH_PANEL_HIDE ? "hide" : "focus"); }
            if(a->kind==THE_WORKBENCH_ACTION_TOOL) {
               js(&j,",\"tool\":");jq(&j,a->tool);
               js(&j,",\"source\":\"active\",\"argumentCount\":%u",a->arguments.argc);
            }
            js(&j,",\"enabled\":%s,\"reason\":",!strcmp(reason,"ok") ? "true" : "false"); jq(&j,reason); js(&j,"}");
         }
         else if (type == 3)
         { const TheWorkbenchMenu *m=g->menus+n; common(&j,m->id,m->owner,m->label,m->generation); js(&j,",\"order\":%d}",m->order); }
         else if (type == 4)
         { const TheWorkbenchItem *i=g->items+n; common(&j,i->id,i->owner,i->label,i->generation); js(&j,",\"menu\":"); jq(&j,i->menu); js(&j,",\"action\":"); jq(&j,i->action); js(&j,",\"order\":%d,\"separator\":%s}",i->order,i->separator ? "true" : "false"); }
         else if (type == 5)
         {
            const TheWorkbenchPanel *p=g->panels+n;
            const TheWorkbenchPanelState *state=the_workbench_panel_state(p->id);
            const TheWorkbenchLogInfo *log=the_workbench_log_find(p->model);
            const TheWorkbenchModelState *model=the_workbench_model_state(p->model);
            size_t k;
            if (state && state->generation != p->generation) state=NULL;
            js(&j,"{\"id\":"); jq(&j,p->id); js(&j,",\"owner\":"); jq(&j,p->owner); js(&j,",\"title\":"); jq(&j,p->title);
            js(&j,",\"generation\":\"%lu\",\"kind\":\"%s\",\"model\":",p->generation,p->kind==THE_WORKBENCH_PANEL_FIELDS ? (p->document_id ? "file-fields" : "fields") : p->kind==THE_WORKBENCH_PANEL_DOCUMENT ? "document" : "log"); jq(&j,p->model);
            if(p->kind==THE_WORKBENCH_PANEL_FIELDS && !p->document_id)
            {
               const TheWorkbenchFormState *form=the_workbench_form_state(p->form);
               if(form && form->generation!=p->form_generation) form=NULL;
               js(&j,",\"form\":"); jq(&j,p->form);
               js(&j,",\"formGeneration\":\"%lu\",\"draftRevision\":\"%" PRIu64 "\",\"resultRevision\":\"%" PRIu64 "\",\"formDirty\":%s,\"submitted\":%s,\"fieldCount\":%lu",p->form_generation,form ? form->draft_revision : 0,form ? form->result_revision : 0,form && form->dirty ? "true" : "false",form && form->submitted ? "true" : "false",(unsigned long)(form ? form->fields : 0));
            }
            if(p->kind==THE_WORKBENCH_PANEL_FIELDS && p->document_id)
            {
               const TheWorkbenchFileFieldsState *projection=the_workbench_filefields_state(p->id);
               js(&j,",\"documentId\":\"%" PRIu64 "\",\"writeRevision\":\"%" PRIu64 "\",\"draftRevision\":\"%" PRIu64 "\",\"projectionDirty\":%s,\"projectionActive\":%s,\"fieldCount\":%lu,\"rowCount\":%lu",p->document_id,projection ? projection->write_revision : 0,projection ? projection->draft_revision : 0,projection && projection->dirty ? "true" : "false",projection && projection->active ? "true" : "false",(unsigned long)(projection ? projection->spans : 0),(unsigned long)(projection ? projection->records : 0));
            }
            if (p->kind==THE_WORKBENCH_PANEL_DOCUMENT)
            {
               TheWorkbenchDocumentInfo info;
               the_workbench_document_info(p->document_id,&info);
               js(&j,",\"readonly\":true,\"documentId\":\"%" PRIu64 "\",\"documentRevision\":\"%" PRIu64 "\",\"documentAvailable\":%s,\"documentReason\":",p->document_id,info.revision,info.available ? "true" : "false"); jq(&j,info.reason);
               js(&j,",\"lineCount\":%lu,\"dirty\":%s",(unsigned long)info.lines,info.dirty ? "true" : "false");
            }
            js(&j,",\"modelGeneration\":\"%" PRIu64 "\",\"placement\":",p->model_generation); jq(&j,the_workbench_placement_name(p->placement));
            js(&j,",\"fallbacks\":["); for (k=0;k<p->fallback_count;k++) { if(k) js(&j,","); jq(&j,the_workbench_placement_name(p->fallbacks[k])); }
            js(&j,"],\"mincols\":%d,\"minrows\":%d,\"visible\":%s,\"focused\":%s,\"effectivePlacement\":",p->mincols,p->minrows,state && state->visible ? "true" : "false",state && state->focused ? "true" : "false");
            jq(&j,the_workbench_placement_name(state ? state->effective : THE_WORKBENCH_PLACE_NONE));
            js(&j,",\"reason\":"); jq(&j,state ? state->reason : "activation-pending"); js(&j,",\"contentRevision\":\"%" PRIu64 "\",\"modelError\":",log && log->generation == p->model_generation ? log->revision : 0); jq(&j,model ? log_result_code(model->last_error) : "ok"); js(&j,"}");
         }
         else if (type == 6)
         {
            const TheWorkbenchRoute *r=g->routes+n;
            const TheWorkbenchModelState *model=the_workbench_model_state(r->model);
            TheWorkbenchToolInfo info; int available=the_workbench_tool_info(r->tool,&info) && info.available;
            js(&j,"{\"id\":"); jq(&j,r->id); js(&j,",\"owner\":"); jq(&j,r->owner); js(&j,",\"generation\":%lu,\"tool\":",r->generation); jq(&j,r->tool);
            js(&j,",\"stream\":"); jq(&j,r->stream == 0 ? "stdin" : r->stream == 1 ? "stdout" : "stderr");
            if(r->kind!=THE_WORKBENCH_ROUTE_LOG) {
               char reason[256];const char *code=the_workbench_fileio_check(r->path,r->kind==THE_WORKBENCH_ROUTE_FILE_OUTPUT,1,reason,sizeof(reason));
               js(&j,",\"kind\":");jq(&j,r->kind==THE_WORKBENCH_ROUTE_FILE_INPUT?"file-input":"file-output");
               js(&j,",\"path\":");jq(&j,r->path);js(&j,",\"format\":\"bytes\",\"mode\":");jq(&j,r->kind==THE_WORKBENCH_ROUTE_FILE_INPUT?"finite":"create");
               js(&j,",\"available\":%s,\"reason\":",available && !the_workbench_restricted() && the_workbench_fileio_supported() && !strcmp(code,"ok")?"true":"false");
               jq(&j,the_workbench_restricted()?"unsupported-feature":!available?"capability-unavailable":code);js(&j,"}");continue;
            }
            js(&j,",\"model\":"); jq(&j,r->model);
            js(&j,",\"modelGeneration\":\"%" PRIu64 "\",\"available\":%s,\"lastError\":",model ? model->generation : 0,available ? "true" : "false"); jq(&j,model ? log_result_code(model->last_error) : "activation-pending"); js(&j,"}");
         }
         else if(type==9) {
            const TheWorkbenchTool *tool=g->tools+n;
            const char *reason=the_workbench_native_tool_reason(tool);
            js(&j,"{\"id\":");jq(&j,tool->id);js(&j,",\"owner\":");jq(&j,tool->owner);
            js(&j,",\"generation\":%lu,\"role\":\"runner\",\"adapter\":\"native-process\",\"source\":\"snapshot\",\"executable\":",tool->generation);jq(&j,tool->executable);
            js(&j,",\"available\":%s,\"reason\":",!strcmp(reason,"ok")?"true":"false");jq(&j,reason);
            js(&j,",\"completion\":\"process-exit-only\",\"joinedArgvBytes\":8191}");
         }
         else if(type==8)
         {
            const TheWorkbenchForm *f=g->forms+n; const TheWorkbenchFormState *state=the_workbench_form_state(f->id); size_t k,fields=0;
            if(state && state->generation!=f->generation) state=NULL;
            for(k=0;k<g->nfields;k++) if(!strcmp(g->fields[k].form,f->id)) fields++;
            js(&j,"{\"id\":"); jq(&j,f->id); js(&j,",\"owner\":"); jq(&j,f->owner);
            js(&j,",\"generation\":\"%lu\",\"draftRevision\":\"%" PRIu64 "\",\"resultRevision\":\"%" PRIu64 "\",\"dirty\":%s,\"submitted\":%s,\"fieldCount\":%lu}",f->generation,state ? state->draft_revision : 0,state ? state->result_revision : 0,state && state->dirty ? "true" : "false",state && state->submitted ? "true" : "false",(unsigned long)fields);
         }
         else
         {
            const TheWorkbenchModelState *m=the_workbench_model_at(n);
            const TheWorkbenchLogInfo *log=the_workbench_log_find(m->id);
            js(&j,"{\"id\":"); jq(&j,m->id); js(&j,",\"owner\":"); jq(&j,m->owner);
            js(&j,",\"generation\":\"%" PRIu64 "\",\"contentRevision\":\"%" PRIu64 "\",\"bytes\":%lu,\"records\":%lu,\"omittedBytes\":\"%" PRIu64 "\",\"omittedRecords\":\"%" PRIu64 "\",\"leases\":%lu,\"lastError\":",m->generation,log ? log->revision : 0,(unsigned long)(log ? log->bytes : 0),(unsigned long)(log ? log->records : 0),log ? log->omitted_bytes : 0,log ? log->omitted_records : 0,(unsigned long)m->leases);
            jq(&j,log_result_code(m->last_error)); js(&j,",\"errorJob\":\"%" PRIu64 "\",\"errorSequence\":\"%" PRIu64 "\",\"errorStream\":%d}",m->error_job,m->error_sequence,m->error_stream);
         }
      }
      js(&j,"]}");
   }
   if (!j.good) out[0]=0;
   return j.good;
}
