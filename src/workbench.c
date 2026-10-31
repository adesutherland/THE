#include "workbench.h"

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
   WorkbenchOwner owners[THE_WORKBENCH_OWNER_MAX];
   size_t nactions, nmenus, nitems, nowners;
} WorkbenchGraph;

static WorkbenchGraph published, activation, draft;
static int initialized, transaction, transaction_failed, activating;
static int activation_failed, activation_changed, activation_trusted;
static char transaction_owner[THE_WORKBENCH_ID_MAX + 1];
static char context[THE_WORKBENCH_OPERAND_MAX + 1];
static unsigned long revision, generation;
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
static int menu_index(const WorkbenchGraph *g, const char *id)
{ size_t i; for (i = 0; i < g->nmenus; i++) if (!strcmp(g->menus[i].id,id)) return (int)i; return -1; }
static int item_index(const WorkbenchGraph *g, const char *id)
{ size_t i; for (i = 0; i < g->nitems; i++) if (!strcmp(g->items[i].id,id)) return (int)i; return -1; }
static int owner_index(const WorkbenchGraph *g, const char *id)
{ size_t i; for (i = 0; i < g->nowners; i++) if (!strcmp(g->owners[i].id,id)) return (int)i; return -1; }

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
const TheWorkbenchAction *the_workbench_action_at(size_t i) { initialize(); return i < published.nactions ? &published.actions[i] : NULL; }
const TheWorkbenchMenu *the_workbench_menu_at(size_t i) { initialize(); return i < published.nmenus ? &published.menus[i] : NULL; }
const TheWorkbenchItem *the_workbench_item_at(size_t i) { initialize(); return i < published.nitems ? &published.items[i] : NULL; }
const TheWorkbenchAction *the_workbench_action_find(const char *id)
{ int i; initialize(); i = id ? action_index(&published,id) : -1; return i >= 0 ? &published.actions[i] : NULL; }
const TheWorkbenchResult *the_workbench_last_result(void) { initialize(); return &last_result; }
const char *the_workbench_action_kind_name(TheWorkbenchActionKind kind)
{ switch (kind) { case THE_WORKBENCH_ACTION_BUILTIN: return "builtin"; case THE_WORKBENCH_ACTION_COMMAND: return "command"; case THE_WORKBENCH_ACTION_MACRO: return "macro"; default: return "unsupported"; } }
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
   if (the_workbench_restricted() && value->kind != THE_WORKBENCH_ACTION_BUILTIN)
      return definition_result("policy-denied",WB_ENVIRON,"Restricted sessions may define only typed builtin actions");
   if (value->kind == THE_WORKBENCH_ACTION_BUILTIN)
   {
      d = the_frontend_action_definition(value->builtin);
      if (!d || ((!d->requires_argument || value->operand[0])
      && !the_frontend_action_init(value->builtin,value->operand,&prepared))) return invalid();
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

static void remove_owner(WorkbenchGraph *g, const char *owner)
{
   size_t n;
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
static short validate(const WorkbenchGraph *g)
{
   size_t n; int i;
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
   return 0;
}

short the_workbench_commit(void)
{
   short rc = require_transaction(); if (rc) return rc;
   if ((rc = validate(&draft))) return rc;
   if (activating) { activation = draft; activation_changed = 1; }
   else { published = draft; revision++; }
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
   if (activation_changed) { published = activation; revision++; }
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
int the_workbench_json(char *out, size_t len, const char *section, size_t offset, size_t count, int candidate)
{
   Json j; const WorkbenchGraph *g; size_t n, total, end; int type;
   initialize();
   if (!out || !len || !count || count > THE_WORKBENCH_PAGE_MAX) return 0;
   out[0] = '\0'; j.out = out; j.len = len; j.used = 0; j.good = 1;
   g = candidate && activating ? &activation : &published;
   if (!section || !*section || !strcmp(section,"SUMMARY")) type = 0;
   else if (!strcmp(section,"CAPABILITIES")) type = 1;
   else if (!strcmp(section,"ACTIONS")) type = 2;
   else if (!strcmp(section,"MENUS")) type = 3;
   else if (!strcmp(section,"ITEMS")) type = 4;
   else return 0;
   js(&j,"{\"schemaVersion\":1,\"revision\":%lu,\"section\":\"%s\"",revision,
      type == 0 ? "summary" : type == 1 ? "capabilities" : type == 2 ? "actions" : type == 3 ? "menus" : "items");
   if (type == 0)
   {
      js(&j,",\"actions\":%lu,\"menus\":%lu,\"items\":%lu,\"transactionActive\":%s,\"activationActive\":%s,\"lastResult\":{\"code\":",
         (unsigned long)g->nactions,(unsigned long)g->nmenus,(unsigned long)g->nitems,transaction ? "true" : "false",activating ? "true" : "false");
      jq(&j,last_result.code); js(&j,",\"rc\":%d,\"message\":",last_result.rc); jq(&j,last_result.message);
      js(&j,"},\"limits\":{\"idBytes\":63,\"labelBytes\":255,\"operandBytes\":2048,\"commandBytes\":8192,\"actions\":128,\"menus\":32,\"items\":256,\"owners\":32,\"pageRecords\":8,\"jsonBytes\":65536,\"invokeDepth\":8}}");
   }
   else if (type == 1)
      js(&j,",\"actions\":true,\"menus\":true,\"panels\":false,\"tools\":false,\"limits\":{\"idBytes\":63,\"labelBytes\":255,\"operandBytes\":2048,\"commandBytes\":8192,\"actions\":128,\"menus\":32,\"items\":256,\"owners\":32,\"pageRecords\":8,\"jsonBytes\":65536,\"invokeDepth\":8}}");
   else
   {
      total = type == 2 ? g->nactions : type == 3 ? g->nmenus : g->nitems;
      end = offset < total ? offset + (count < total-offset ? count : total-offset) : offset;
      js(&j,",\"total\":%lu,\"offset\":%lu,\"nextOffset\":",(unsigned long)total,(unsigned long)offset);
      if (end < total) js(&j,"%lu",(unsigned long)end); else js(&j,"null");
      js(&j,",\"records\":[");
      for (n = offset; n < end; n++)
      {
         if (n != offset) js(&j,",");
         if (type == 2)
         {
            const TheWorkbenchAction *a = &g->actions[n]; const char *reason = the_workbench_action_reason(a);
            common(&j,a->id,a->owner,a->label,a->generation);
            js(&j,",\"kind\":"); jq(&j,the_workbench_action_kind_name(a->kind));
            js(&j,",\"builtin\":"); jq(&j,a->kind == THE_WORKBENCH_ACTION_BUILTIN ? the_frontend_action_name(a->builtin) : "");
            js(&j,",\"operand\":"); jq(&j,a->operand);
            js(&j,",\"requiresArgument\":%s", a->kind == THE_WORKBENCH_ACTION_BUILTIN && the_frontend_action_definition(a->builtin) && the_frontend_action_definition(a->builtin)->requires_argument ? "true" : "false");
            js(&j,",\"enabled\":%s,\"reason\":",!strcmp(reason,"ok") ? "true" : "false"); jq(&j,reason); js(&j,"}");
         }
         else if (type == 3)
         { const TheWorkbenchMenu *m = &g->menus[n]; common(&j,m->id,m->owner,m->label,m->generation); js(&j,",\"order\":%d}",m->order); }
         else
         { const TheWorkbenchItem *i = &g->items[n]; common(&j,i->id,i->owner,i->label,i->generation); js(&j,",\"menu\":"); jq(&j,i->menu); js(&j,",\"action\":"); jq(&j,i->action); js(&j,",\"order\":%d,\"separator\":%s}",i->order,i->separator ? "true" : "false"); }
      }
      js(&j,"]}");
   }
   if (!j.good) out[0] = '\0';
   return j.good;
}
