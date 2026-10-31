#include "the.h"
#include "proto.h"
#include "workbench.h"
#include "frontendpolicy.h"
#include "inputdispatch.h"
#include "panelui.h"
#include "query.h"

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char text[THE_WORKBENCH_OPERAND_MAX + 1]; int quoted; } Token;
typedef struct { Token values[32]; size_t count; } Tokens;

static short bad(void)
{ return the_workbench_result("invalid-operand",RC_INVALID_OPERAND,"Invalid workbench operands or quoting"); }

static int keyword(const Token *t, const char *s)
{
   const unsigned char *p = (const unsigned char *)t->text;
   if (t->quoted) return 0;
   while (*p && *s) if (toupper(*p++) != *s++) return 0;
   return !*p && !*s;
}

static int tokenize(const char *s, Tokens *tokens)
{
   const unsigned char *p = (const unsigned char *)s;
   memset(tokens,0,sizeof(*tokens));
   if (!s || strlen(s) > THE_WORKBENCH_COMMAND_MAX) return 0;
   while (*p)
   {
      Token *t; size_t n = 0; int closed = 0;
      while (*p == ' ' || *p == '\t') p++;
      if (!*p) break;
      if (tokens->count == 32) return 0;
      t = &tokens->values[tokens->count++]; t->quoted = *p == '"';
      if (t->quoted) p++;
      while (*p)
      {
         if (!t->quoted && (*p == ' ' || *p == '\t')) break;
         if (*p < 32 || *p == 127) return 0;
         if (t->quoted && *p == '"')
         {
            p++;
            if (*p != '"') { closed = 1; break; }
         }
         else if (!t->quoted && isspace(*p)) break;
         else if (!t->quoted && *p == '"') return 0;
         if (n == THE_WORKBENCH_OPERAND_MAX) return 0;
         t->text[n++] = (char)*p++;
      }
      if (t->quoted && (!closed || (*p && *p != ' ' && *p != '\t'))) return 0;
      t->text[n] = '\0';
   }
   return 1;
}

static int generation_value(const Token *t, unsigned long *value)
{
   const char *p = t->text; unsigned long n = 0;
   if (t->quoted || !*p) return 0;
   for (; *p; p++)
   {
      if (*p < '0' || *p > '9' || n > (ULONG_MAX-(unsigned)(*p-'0'))/10) return 0;
      n = n*10 + (unsigned)(*p-'0');
   }
   if (!n) return 0;
   *value = n; return 1;
}

static int order_value(const Token *t, int *value)
{
   const char *p = t->text; unsigned long n = 0;
   if (t->quoted || !*p) return 0;
   for (; *p; p++)
   {
      if (*p < '0' || *p > '9' || n > ((unsigned long)INT_MAX-(unsigned)(*p-'0'))/10) return 0;
      n = n*10 + (unsigned)(*p-'0');
   }
   *value = (int)n; return 1;
}

int the_workbench_restricted(void)
{
#ifdef THE_WASM
   return 1;
#else
   return the_frontend_policy_enabled();
#endif
}

const char *the_workbench_action_reason(const TheWorkbenchAction *action)
{
   if (!action) return "missing-action";
   if (the_panel_active) return "busy-modal";
   if (action->kind == THE_WORKBENCH_ACTION_BUILTIN)
   {
      const TheFrontendActionDefinition *d = the_frontend_action_definition(action->builtin);
      if (!d) return "unsupported-feature";
      /* An unbound path is acquired by the frontend at explicit invocation. */
      if (!(d->requires_argument && !action->operand[0]))
      {
         TheFrontendAction prepared;
         char error[256];
         if (!the_frontend_action_init(action->builtin,action->operand,&prepared)) return "invalid-operand";
         if (!the_frontend_policy_prepare_action(&prepared,error,sizeof(error))) return "policy-denied";
      }
      return "ok";
   }
   if (the_workbench_restricted()) return "policy-denied";
   if (action->kind == THE_WORKBENCH_ACTION_MACRO && !rexx_support) return "capability-unavailable";
   if (action->kind != THE_WORKBENCH_ACTION_COMMAND && action->kind != THE_WORKBENCH_ACTION_MACRO)
      return "unsupported-feature";
   return "ok";
}

static short invoke_action(const char *id, unsigned long generation,
                           const char *argument)
{
   TheWorkbenchAction copy;
   TheFrontendAction builtin;
   const TheWorkbenchAction *found;
   short rc;
   if (!id) return bad();
   found = the_workbench_action_find(id);
   if (!found) return the_workbench_result("missing-action",RC_INVALID_OPERAND,"No published workbench action has that ID");
   copy = *found; /* Explicit execution may publish or remove definitions. */
   if (generation && generation != copy.generation)
      return the_workbench_result("stale-generation",RC_INVALID_ENVIRON,"The workbench action generation changed");
   if (the_workbench_activation_active() && strncmp(copy.owner,"the.",4))
      return the_workbench_result("activation-pending",RC_INVALID_ENVIRON,"Profile actions cannot run before activation completes");
   if (the_panel_active) return the_workbench_result("busy-modal",RC_INVALID_ENVIRON,"A modal READ owns editor input");
   if (copy.kind == THE_WORKBENCH_ACTION_BUILTIN)
   {
      if (!the_frontend_action_init(copy.builtin,argument ? argument : copy.operand,&builtin)) return bad();
      {
         char error[256];
         TheFrontendAction admitted = builtin;
         if (!the_frontend_policy_prepare_action(&admitted,error,sizeof(error)))
            return the_workbench_result("policy-denied",RC_INVALID_ENVIRON,"The typed action is denied by current workspace policy");
      }
      rc = the_input_dispatch_action(&builtin);
   }
   else
   {
      if (argument && *argument) return bad();
      if (the_workbench_restricted())
         return the_workbench_result("policy-denied",RC_INVALID_ENVIRON,"Command and macro actions are unavailable in restricted sessions");
      if (copy.kind == THE_WORKBENCH_ACTION_COMMAND)
         rc = the_input_dispatch_command(copy.operand,0);
      else if (copy.kind == THE_WORKBENCH_ACTION_MACRO)
      {
         char name[THE_WORKBENCH_OPERAND_MAX + 3];
         if (!rexx_support) return the_workbench_result("capability-unavailable",RC_INVALID_ENVIRON,"No macro interpreter is available");
         if (strchr(copy.operand,'"')) return bad();
         snprintf(name,sizeof(name),"\"%s\"",copy.operand);
         rc = Macro((CHARTYPE *)name);
      }
      else return the_workbench_result("unsupported-feature",RC_INVALID_ENVIRON,"This action kind is not implemented");
   }
   return the_workbench_result(rc == RC_OK ? "ok" : "command-failed",rc,
                               rc == RC_OK ? "" : "The explicit workbench action returned a nonzero THE result");
}

static unsigned long invocation_serial;
static unsigned int invocation_depth;
static char invocation_ids[THE_WORKBENCH_INVOKE_DEPTH_MAX][THE_WORKBENCH_ID_MAX+1];
static const char *invocation_refusal;
unsigned long the_workbench_invocation_serial(void) { return invocation_serial; }
short the_workbench_invoke(const char *id, unsigned long generation, const char *argument)
{
   unsigned int i;
   short rc;
   if (!invocation_depth) invocation_refusal = NULL;
   if (!id || strlen(id) > THE_WORKBENCH_ID_MAX)
   {
      if (!invocation_depth) invocation_serial++;
      return bad();
   }
   if (invocation_depth >= THE_WORKBENCH_INVOKE_DEPTH_MAX)
   {
      invocation_refusal = "limit-exceeded";
      return the_workbench_result(invocation_refusal,RC_OUT_OF_MEMORY,"Workbench invocation nesting limit exceeded");
   }
   for (i = 0; i < invocation_depth; i++) if (!strcmp(invocation_ids[i],id))
   {
      invocation_refusal = "object-in-use";
      return the_workbench_result(invocation_refusal,RC_INVALID_ENVIRON,"Workbench action is already being invoked");
   }
   strcpy(invocation_ids[invocation_depth],id);
   invocation_depth++;
   rc = invoke_action(id,generation,argument);
   invocation_depth--;
   if (!invocation_depth)
   {
      /* A command or macro may recursively invoke another action. Preserve
       * the admission refusal while unwinding instead of hiding it behind
       * a generic command-failed result from each enclosing action. */
      if (invocation_refusal)
         rc = the_workbench_result(invocation_refusal,
               !strcmp(invocation_refusal,"limit-exceeded") ? RC_OUT_OF_MEMORY : RC_INVALID_ENVIRON,
               !strcmp(invocation_refusal,"limit-exceeded") ? "Workbench invocation nesting limit exceeded"
                                                          : "Workbench action is already being invoked");
      invocation_serial++;
   }
   return rc;
}

static short command_operands(const char *operands)
{
   Tokens tokens;
   Token *t = tokens.values;
   size_t n;
   if (!tokenize(operands,&tokens)) return bad();
   n = tokens.count;
   if (!n) return bad();
   if (keyword(&t[0],"BEGIN") && n == 3 && keyword(&t[1],"OWNER") && !t[2].quoted)
      return the_workbench_begin(t[2].text);
   if (keyword(&t[0],"COMMIT") && n == 1) return the_workbench_commit();
   if (keyword(&t[0],"ABORT") && n == 1) return the_workbench_abort();
   if (keyword(&t[0],"RESET") && n == 3 && keyword(&t[1],"OWNER") && !t[2].quoted)
      return the_workbench_reset_owner(t[2].text);
   if (keyword(&t[0],"INVOKE") && !t[1].quoted)
   {
      unsigned long expected = 0;
      if (n == 4 && keyword(&t[2],"EXPECT") && generation_value(&t[3],&expected)) {}
      else if (n != 2) return bad();
      return the_workbench_invoke(t[1].text,expected,NULL);
   }
   if (keyword(&t[0],"ACTION"))
   {
      TheWorkbenchAction a; memset(&a,0,sizeof(a));
      if (n == 3 && keyword(&t[1],"REMOVE") && !t[2].quoted) return the_workbench_remove_action(t[2].text);
      if (n < 7 || !keyword(&t[1],"PUT") || t[2].quoted || strlen(t[2].text) > THE_WORKBENCH_ID_MAX
      || !keyword(&t[3],"LABEL") || !t[4].quoted || strlen(t[4].text) > THE_WORKBENCH_LABEL_MAX) return bad();
      strcpy(a.id,t[2].text); strcpy(a.label,t[4].text);
      if (keyword(&t[5],"BUILTIN"))
      {
         a.kind = THE_WORKBENCH_ACTION_BUILTIN;
         if (t[6].quoted || !the_frontend_action_id_from_name(t[6].text,&a.builtin)) return bad();
         if (n == 9 && keyword(&t[7],"ARGUMENT") && t[8].quoted) strcpy(a.operand,t[8].text);
         else if (n != 7) return bad();
      }
      else if (keyword(&t[5],"COMMAND") || keyword(&t[5],"MACRO"))
      {
         if (the_workbench_restricted()) return the_workbench_result("policy-denied",RC_INVALID_ENVIRON,"Restricted sessions may define only typed builtin actions");
         if (n > 7 && keyword(&t[7],"ARG"))
            return the_workbench_result("unsupported-feature",RC_INVALID_ENVIRON,"Macro argument vectors are outside phase one");
         if (n != 7 || !t[6].quoted) return bad();
         a.kind = keyword(&t[5],"COMMAND") ? THE_WORKBENCH_ACTION_COMMAND : THE_WORKBENCH_ACTION_MACRO;
         if (a.kind == THE_WORKBENCH_ACTION_MACRO && strchr(t[6].text,'"')) return bad();
         strcpy(a.operand,t[6].text);
      }
      else if (keyword(&t[5],"TOOL") || keyword(&t[5],"PANEL"))
         return the_workbench_result("unsupported-feature",RC_INVALID_ENVIRON,"Tool and panel actions are outside phase one");
      else return bad();
      return the_workbench_put_action(&a);
   }
   if (keyword(&t[0],"MENU"))
   {
      TheWorkbenchMenu menu; memset(&menu,0,sizeof(menu));
      if (n == 3 && keyword(&t[1],"REMOVE") && !t[2].quoted) return the_workbench_remove_menu(t[2].text);
      if ((n != 5 && n != 7) || !keyword(&t[1],"PUT") || t[2].quoted || strlen(t[2].text) > THE_WORKBENCH_ID_MAX
      || !keyword(&t[3],"LABEL") || !t[4].quoted || strlen(t[4].text) > THE_WORKBENCH_LABEL_MAX) return bad();
      strcpy(menu.id,t[2].text); strcpy(menu.label,t[4].text);
      if (n == 7 && (!keyword(&t[5],"ORDER") || !order_value(&t[6],&menu.order))) return bad();
      return the_workbench_put_menu(&menu);
   }
   if (keyword(&t[0],"ITEM"))
   {
      TheWorkbenchItem item; size_t option;
      memset(&item,0,sizeof(item));
      if (n == 4 && keyword(&t[1],"REMOVE") && !t[2].quoted && !t[3].quoted)
         return the_workbench_remove_item(t[2].text,t[3].text);
      if (n < 5 || !keyword(&t[1],"PUT") || t[2].quoted || t[3].quoted
      || strlen(t[2].text) > THE_WORKBENCH_ID_MAX || strlen(t[3].text) > THE_WORKBENCH_ID_MAX) return bad();
      strcpy(item.menu,t[2].text); strcpy(item.id,t[3].text);
      if (keyword(&t[4],"SEPARATOR")) { item.separator = 1; option = 5; }
      else
      {
         if (n < 8 || !keyword(&t[4],"LABEL") || !t[5].quoted || strlen(t[5].text) > THE_WORKBENCH_LABEL_MAX
         || !keyword(&t[6],"ACTION") || t[7].quoted || strlen(t[7].text) > THE_WORKBENCH_ID_MAX) return bad();
         strcpy(item.label,t[5].text); strcpy(item.action,t[7].text); option = 8;
      }
      if (n == option+2 && keyword(&t[option],"ORDER") && order_value(&t[option+1],&item.order)) {}
      else if (n != option) return bad();
      return the_workbench_put_item(&item);
   }
   if (keyword(&t[0],"PANEL") || keyword(&t[0],"FORM") || keyword(&t[0],"FIELD")
   || keyword(&t[0],"TOOL") || keyword(&t[0],"ROUTE") || keyword(&t[0],"RUN") || keyword(&t[0],"JOB"))
      return the_workbench_result("unsupported-feature",RC_INVALID_ENVIRON,"This workbench feature is outside phase one");
   return bad();
}

short the_workbench_command(const char *operands)
{
   const char *p = operands ? operands : "";
   const char *word = "INVOKE";
   int invocation = 1;
   short rc;
   while (*p == ' ' || *p == '\t') p++;
   while (*word) if (toupper((unsigned char)*p++) != *word++) { invocation = 0; break; }
   if (invocation && *p && *p != ' ' && *p != '\t') invocation = 0;
   rc = command_operands(operands);
   if (rc != RC_OK && !invocation) the_workbench_poison_definition();
   return rc;
}

short Workbench(CHARTYPE *parameters)
{
   short rc = the_workbench_command((const char *)parameters);
   if (rc != RC_OK)
   {
      char error[256]; const TheWorkbenchResult *result = the_workbench_last_result();
      snprintf(error,sizeof(error),"WORKBENCH %s: %s",result->code,result->message);
      display_error(0,(CHARTYPE *)error,FALSE);
   }
   return rc;
}

static short query_bad(void)
{
   bad();
   return EXTRACT_ARG_ERROR;
}

/* One bounded JSON record per extraction avoids item_values[] overflow. */
short extract_workbench(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
{
   static char json[THE_WORKBENCH_JSON_MAX];
   Tokens tokens; size_t offset = 0, count = THE_WORKBENCH_PAGE_MAX, n; int value;
   const char *section = "SUMMARY";
   int seen_offset = 0, seen_limit = 0;
   if (!tokenize(itemargs ? (const char *)itemargs : "",&tokens)) return query_bad();
   if (tokens.count)
   {
      if (tokens.values[0].quoted) return query_bad();
      for (n = 0; tokens.values[0].text[n]; n++) tokens.values[0].text[n] = (char)toupper((unsigned char)tokens.values[0].text[n]);
      section = tokens.values[0].text;
      for (n = 1; n < tokens.count; n += 2)
      {
         if (n+1 >= tokens.count || !order_value(&tokens.values[n+1],&value)) return query_bad();
         if (keyword(&tokens.values[n],"OFFSET") && !seen_offset) { offset = (size_t)value; seen_offset = 1; }
         else if (keyword(&tokens.values[n],"LIMIT") && !seen_limit) { count = (size_t)value; seen_limit = 1; }
         else return query_bad();
      }
   }
   if (!the_workbench_json(json,sizeof(json),section,offset,count,1)) return query_bad();
   if (query_type == QUERY_QUERY)
   {
      display_error(0,(CHARTYPE *)json,TRUE);
      return EXTRACT_VARIABLES_SET;
   }
   item_values[1].value = (CHARTYPE *)json;
   item_values[1].len = strlen(json);
   return 1;
}
