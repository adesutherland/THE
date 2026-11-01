#include "the.h"
#include "proto.h"
#include "workbench.h"
#include "workbenchruntime.h"
#include "frontendpolicy.h"
#include "inputdispatch.h"
#include "panelui.h"
#include "query.h"
#include "editordocument.h"
#include "workbenchform.h"
#include "workbenchfilefields.h"
#include "workbenchfileio.h"
#include "editorgroup.h"
#include "tools.h"

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define TOKEN_MAX (2 * THE_WORKBENCH_ARG_MAX + 16)
typedef struct { char *text; int quoted; } Token;
typedef struct { Token values[TOKEN_MAX]; char arena[THE_WORKBENCH_COMMAND_MAX+TOKEN_MAX+1]; size_t count; } Tokens;

static short bad(void)
{ return the_workbench_result("invalid-operand",RC_INVALID_OPERAND,"Invalid workbench operands or quoting"); }

static int keyword(const Token *t, const char *s)
{
   const unsigned char *p = (const unsigned char *)t->text;
   if (!p || t->quoted) return 0;
   while (*p && *s) if (toupper(*p++) != *s++) return 0;
   return !*p && !*s;
}

static int tokenize(const char *s, Tokens *tokens)
{
   const unsigned char *p = (const unsigned char *)s;
   size_t used=0;
   memset(tokens,0,sizeof(*tokens));
   if (!s || strlen(s) > THE_WORKBENCH_COMMAND_MAX) return 0;
   while (*p)
   {
      Token *t; size_t n = 0; int closed = 0;
      while (*p == ' ' || *p == '\t') p++;
      if (!*p) break;
      if (tokens->count == TOKEN_MAX) return 0;
      t = &tokens->values[tokens->count++]; t->quoted = *p == '"';
      t->text=tokens->arena+used;
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
      used+=n+1;
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

static int path_below(const char *path,const char *root)
{
   size_t n;
   if (!path || !root) return 0;
   n=strlen(root);
   if (n==1 && root[0]=='/') return path[0]=='/' && path[1]!='\0';
   return n && !strncmp(path,root,n) && path[n]=='/';
}

static const char *file_build_path_reason(const char *path,int macro)
{
   char resolved[THE_FRONTEND_POLICY_PATH_MAX+1],error[256];
   const char *workspace;
   int readonly=0;
   size_t n;
   if (!the_workbench_restricted()) return "ok";
   if (!the_frontend_policy_enabled()) return "policy-denied";
   if (!path || !(workspace=the_frontend_policy_root_at(0,NULL))
   || !the_frontend_policy_resolve_path(path,macro ? 0 : 1,resolved,sizeof(resolved),
                                        &readonly,error,sizeof(error))
   || !path_below(resolved,workspace)) return "policy-denied";
   if (macro) {
      n=strlen(resolved);
      if (n<6 || strcmp(resolved+n-6,".rxbin")) return "policy-denied";
   }
   return "ok";
}

static void invocation_completed(void);
static short panel_operation(const char *id,unsigned long generation,TheWorkbenchPanelOperation operation)
{
   if (the_workbench_activation_active()) return the_workbench_result("activation-pending",RC_INVALID_ENVIRON,"Panel runtime operations require completed profile activation");
   if (the_panel_active) return the_workbench_result("busy-modal",RC_INVALID_ENVIRON,"A modal READ owns editor input");
   return the_workbench_runtime_panel(id,generation,operation);
}

short the_workbench_panel_operation(const char *id,unsigned long generation,TheWorkbenchPanelOperation operation)
{
   short rc=panel_operation(id,generation,operation);
   invocation_completed();
   return rc;
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
   if (action->kind == THE_WORKBENCH_ACTION_PANEL)
   {
      const TheWorkbenchPanel *p=the_workbench_panel_find(action->panel);
      const TheWorkbenchPanelState *state=the_workbench_panel_state(action->panel);
      if (!p || !state) return "missing-object";
      if (action->panel_operation == THE_WORKBENCH_PANEL_FOCUS && !state->visible) return "capability-unavailable";
      return "ok";
   }
   if(action->kind==THE_WORKBENCH_ACTION_TOOL) {
      const TheWorkbenchTool *tool=the_workbench_tool_find(action->tool);
      const char *host=the_workbench_native_host_reason();
      if(strcmp(host,"ok")) return host;
      if(!tool) return "missing-object";
      if(in_macro) return "busy-modal";
      if(the_tool_console()->running) return "busy-job";
      return the_workbench_native_tool_reason(tool);
   }
   if (the_workbench_restricted()) {
      if (action->kind!=THE_WORKBENCH_ACTION_MACRO || !action->file_build)
         return "policy-denied";
      if (!rexx_support) return "capability-unavailable";
      return file_build_path_reason(action->operand,1);
   }
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
   if (copy.file_build) {
      if (argument && *argument) return bad();
      rc=the_tools_file_build_active(copy.id,copy.generation);
      return the_workbench_result(rc==RC_OK ? "ok" : "command-failed",rc,
         rc==RC_OK ? "" : "File Build failed; inspect the Build result in Tools & Profiles");
   }
   if (copy.kind == THE_WORKBENCH_ACTION_PANEL)
   {
      if (argument && *argument) return bad();
      return the_workbench_panel_operation(copy.panel,0,copy.panel_operation);
   }
   if(copy.kind==THE_WORKBENCH_ACTION_TOOL) {
      if(argument && *argument) return bad();
      return the_tools_workbench_run(copy.tool,&copy.arguments);
   }
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
      if (the_workbench_restricted() && !copy.file_build)
         return the_workbench_result("policy-denied",RC_INVALID_ENVIRON,"Command and macro actions are unavailable in restricted sessions");
      if (copy.kind == THE_WORKBENCH_ACTION_COMMAND)
         rc = the_input_dispatch_command(copy.operand,0);
      else if (copy.kind == THE_WORKBENCH_ACTION_MACRO)
      {
         char name[THE_WORKBENCH_OPERAND_MAX + 3];
         if (!rexx_support) return the_workbench_result("capability-unavailable",RC_INVALID_ENVIRON,"No macro interpreter is available");
         if (strchr(copy.operand,'"')) return bad();
         if (copy.file_build)
            return the_workbench_result("invalid-operand",RC_INVALID_OPERAND,"File Build requires an explicit selected target");
         snprintf(name,sizeof(name),"\"%s\"",copy.operand);
         rc = Macro((CHARTYPE *)name);
      }
      else return the_workbench_result("unsupported-feature",RC_INVALID_ENVIRON,"This action kind is not implemented");
   }
   return the_workbench_result(rc == RC_OK ? "ok" : "command-failed",rc,
                               rc == RC_OK ? "" : "The explicit workbench action returned a nonzero THE result");
}

short the_workbench_file_build_invoke(const char *id,unsigned long generation,
                                      const char *target)
{
   const TheWorkbenchAction *found;
   TheWorkbenchAction action;
   char invocation[2*THE_WORKBENCH_OPERAND_MAX+8];
   const char *reason;
   short rc;
   if (!id || !target || !*target || !generation) return bad();
   found=the_workbench_action_find(id);
   if (!found || !found->file_build) return the_workbench_result("missing-action",RC_INVALID_OPERAND,"No file Build action has that ID");
   action=*found;
   if (action.generation!=generation) return the_workbench_result("stale-generation",RC_INVALID_ENVIRON,"File Build action changed");
   if (the_workbench_file_build(target)!=found) return the_workbench_result("invalid-operand",RC_INVALID_OPERAND,"Build action does not belong to this file type");
   if (the_workbench_activation_active()) return the_workbench_result("activation-pending",RC_INVALID_ENVIRON,"File Build requires a completed profile activation");
   if (the_panel_active || in_macro) return the_workbench_result("busy-modal",RC_INVALID_ENVIRON,"Editor input is busy");
   reason=file_build_path_reason(action.operand,1);
   if (strcmp(reason,"ok")) return the_workbench_result(reason,RC_INVALID_ENVIRON,"Build macro is not in the browser workspace");
   reason=file_build_path_reason(target,0);
   if (strcmp(reason,"ok")) return the_workbench_result(reason,RC_INVALID_ENVIRON,"Build target is not in the browser workspace");
   if (!rexx_support) return the_workbench_result("capability-unavailable",RC_INVALID_ENVIRON,"No macro interpreter is available");
   if (strchr(action.operand,'"') || strchr(target,'\n') || strchr(target,'\r')) return bad();
   if (snprintf(invocation,sizeof(invocation),"\"%s\" %s",action.operand,target)>=(int)sizeof(invocation)) return bad();
   rc=Macro((CHARTYPE *)invocation);
   return the_workbench_result(rc==RC_OK ? "ok" : "command-failed",rc,
      rc==RC_OK ? "" : "The file Build macro returned a nonzero THE result");
}

static unsigned long invocation_serial;
static unsigned int invocation_depth;
static char invocation_ids[THE_WORKBENCH_INVOKE_DEPTH_MAX][THE_WORKBENCH_ID_MAX+1];
static const char *invocation_refusal;
static void invocation_completed(void) { if (!invocation_depth) invocation_serial++; }
unsigned long the_workbench_invocation_serial(void) { return invocation_serial; }
short the_workbench_invoke_bound(const char *id,unsigned long generation,
   const char *argument,int pinned,uint64_t document)
{
   const TheWorkbenchAction *action=the_workbench_action_find(id);
   if(action && action->kind==THE_WORKBENCH_ACTION_TOOL
   && (!generation || generation==action->generation)
   && (!pinned || !document || document!=the_editor_document_active())) {
      invocation_completed();
      return the_workbench_result("stale-revision",RC_INVALID_ENVIRON,"Queued TOOL source changed or closed before dispatch");
   }
   return the_workbench_invoke(id,generation,argument);
}
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

static TheWorkbenchPanelOperation panel_operation_token(const Token *t)
{
   if (keyword(t,"SHOW")) return THE_WORKBENCH_PANEL_SHOW;
   if (keyword(t,"HIDE")) return THE_WORKBENCH_PANEL_HIDE;
   if (keyword(t,"FOCUS")) return THE_WORKBENCH_PANEL_FOCUS;
   return 0;
}
static TheWorkbenchPlacement placement_token(const Token *t)
{
   if (keyword(t,"RIGHT")) return THE_WORKBENCH_PLACE_RIGHT;
   if (keyword(t,"BELOW")) return THE_WORKBENCH_PLACE_BELOW;
   if (keyword(t,"TAB")) return THE_WORKBENCH_PLACE_TAB;
   return 0;
}
static int fallback_tokens(const Token *t,TheWorkbenchPlacement *out,size_t *count)
{
   const char *p=t->text; *count=0;
   if (t->quoted || !*p) return 0;
   while (*p)
   {
      char value[9]={0};Token word={value,0};size_t n=0;
      while (*p && *p != ',') { if (n == 8) return 0; word.text[n++]=*p++; }
      if (!n || *count == 3 || !(out[*count]=placement_token(&word))) return 0;
      (*count)++;
      if (*p == ',' && !*++p) return 0;
   }
   return 1;
}

static int unsigned64_value(const Token *t,uint64_t *value);
static int edit_number(const Token *t,uint64_t *value)
{ return !(t->text[0]=='0' && t->text[1]) && unsigned64_value(t,value) && *value; }
static short edit_request_tokens(const Tokens *tokens)
{
   const Token *t=tokens->values; size_t n=tokens->count,i,count=0;
   TheDocumentWriteToken expected; TheDocumentLineEdit edits[4];
   TheEditGroupPlan *plan=NULL; TheEditGroupReceipt receipt; uint64_t group,line; short rc;
   memset(&expected,0,sizeof(expected)); memset(edits,0,sizeof(edits));
   the_editor_edit_group_initialize();
   if(n==7 && keyword(t+1,"UNDO") && edit_number(t+2,&group) && keyword(t+3,"DOCUMENT")
   && edit_number(t+4,&expected.document_id) && keyword(t+5,"EXPECT") && edit_number(t+6,&expected.write_revision))
      rc=the_edit_group_undo(group,expected,&receipt);
   else {
      if(n<12 || !keyword(t+1,"APPLY") || !keyword(t+2,"DOCUMENT")
      || !edit_number(t+3,&expected.document_id) || !keyword(t+4,"EXPECT")
      || !edit_number(t+5,&expected.write_revision) || (n-6)%6) return bad();
      for(i=6;i<n;i+=6) {
         if(count==4 || !keyword(t+i,"LINE") || !edit_number(t+i+1,&line) || line>SIZE_MAX
         || !keyword(t+i+2,"BEFORE") || !t[i+3].quoted || !keyword(t+i+4,"AFTER") || !t[i+5].quoted) return bad();
         edits[count].line=(size_t)line; edits[count].before=(const unsigned char *)t[i+3].text;
         edits[count].before_length=strlen(t[i+3].text); edits[count].after=(const unsigned char *)t[i+5].text;
         edits[count].after_length=strlen(t[i+5].text); count++;
      }
      rc=the_edit_group_prepare(expected,edits,count,&plan);
      if(!rc) { rc=the_edit_group_commit(plan,&receipt); if(!rc) plan=NULL; }
      the_edit_group_abort(plan);
   }
   return the_workbench_result(rc ? the_edit_group_error() : "ok",rc,rc ? "Edit group refused; editor state preserved" : "");
}
static int form_modal(void) { return the_panel_active!=NULL; }
static short form_request_tokens(const Tokens *tokens)
{
   const Token *t=tokens->values; size_t n=tokens->count,i; TheWorkbenchFormRequest r; const TheWorkbenchFormState *s;
   memset(&r,0,sizeof(r));
   if(n<3 || t[2].quoted || strlen(t[2].text)>THE_WORKBENCH_ID_MAX) return bad();
   strcpy(r.form,t[2].text);
   if(keyword(t,"FIELD"))
   {
      if(n<8 || t[3].quoted || strlen(t[3].text)>THE_WORKBENCH_ID_MAX || !keyword(t+4,"VALUE") || !t[5].quoted) return bad();
      r.operation=THE_WORKBENCH_FORM_SET; strcpy(r.field,t[3].text); r.length=strlen(t[5].text);
      if(r.length>THE_WORKBENCH_FIELD_VALUE_MAX) return the_workbench_result("limit-exceeded",94,"Field value byte limit exceeded");
      memcpy(r.value,t[5].text,r.length+1); i=6;
   }
   else { r.operation=keyword(t+1,"SUBMIT") ? THE_WORKBENCH_FORM_SUBMIT : THE_WORKBENCH_FORM_CANCEL; i=3; }
   for(;i<n;i+=2)
   {
      if(i+1>=n) return bad();
      if(keyword(t+i,"EXPECT") && !r.draft_revision) { if(!unsigned64_value(t+i+1,&r.draft_revision) || !r.draft_revision) return bad(); }
      else if(keyword(t+i,"GENERATION") && !r.form_generation) { if(!generation_value(t+i+1,&r.form_generation)) return bad(); }
      else return bad();
   }
   if(!r.draft_revision && r.operation!=THE_WORKBENCH_FORM_CANCEL) return bad();
   s=the_workbench_form_state(r.form);
   if(s) { if(!r.form_generation) r.form_generation=s->generation; if(!r.draft_revision) r.draft_revision=s->draft_revision; }
   the_workbench_forms_modal_lookup(form_modal);
   return the_workbench_form_request(&r);
}
static short filefields_request_tokens(const Tokens *tokens)
{
   const Token *t=tokens->values; size_t n=tokens->count,i=3; TheWorkbenchFileFieldsRequest r; const TheWorkbenchPanel *p; const TheWorkbenchFileFieldsState *s;
   memset(&r,0,sizeof(r)); r.cell=-1;
   if(n<3 || t[2].quoted || strlen(t[2].text)>THE_WORKBENCH_ID_MAX) return bad();
   strcpy(r.panel,t[2].text);
   r.operation=keyword(t+1,"SET") ? THE_WORKBENCH_FILEFIELDS_SET : keyword(t+1,"APPLY") ? THE_WORKBENCH_FILEFIELDS_APPLY : keyword(t+1,"RELOAD") ? THE_WORKBENCH_FILEFIELDS_RELOAD : THE_WORKBENCH_FILEFIELDS_CANCEL;
   if(r.operation==THE_WORKBENCH_FILEFIELDS_SET) {
      if(n<9 || !keyword(t+3,"FIELD") || t[4].quoted || strlen(t[4].text)>THE_WORKBENCH_ID_MAX || !keyword(t+5,"VALUE") || !t[6].quoted) return bad();
      strcpy(r.field,t[4].text); r.length=strlen(t[6].text); if(r.length>THE_WORKBENCH_FIELD_VALUE_MAX) return the_workbench_result("limit-exceeded",94,"Field byte limit exceeded");
      memcpy(r.value,t[6].text,r.length+1); i=7;
   }
   for(;i<n;i+=2) {
      if(i+1>=n) return bad();
      if(keyword(t+i,"EXPECT") && !r.draft_revision) { if(!unsigned64_value(t+i+1,&r.draft_revision) || !r.draft_revision) return bad(); }
      else if(keyword(t+i,"GENERATION") && !r.panel_generation) { if(!generation_value(t+i+1,&r.panel_generation)) return bad(); }
      else return bad();
   }
   if(!r.draft_revision && r.operation!=THE_WORKBENCH_FILEFIELDS_CANCEL && r.operation!=THE_WORKBENCH_FILEFIELDS_RELOAD) return bad();
   p=the_workbench_panel_find(r.panel); s=the_workbench_filefields_state(r.panel);
   if(p && !r.panel_generation) r.panel_generation=p->generation;
   if(s && !r.draft_revision) r.draft_revision=s->draft_revision;
   the_editor_filefields_initialize(); return the_workbench_filefields_request(&r);
}
static short command_operands(const char *operands)
{
   Tokens tokens;
   Token *t = tokens.values;
   size_t n;
   the_tools_workbench_initialize();
   if (!tokenize(operands,&tokens)) return bad();
   n = tokens.count;
   if (!n) return bad();
   if(keyword(t,"TOOL")) {
      TheWorkbenchTool tool={0};
      if(n==3 && keyword(t+1,"REMOVE") && !t[2].quoted) return the_workbench_remove_tool(t[2].text);
      if(n!=11 || !keyword(t+1,"PUT") || t[2].quoted || strlen(t[2].text)>THE_WORKBENCH_ID_MAX
      || !keyword(t+3,"ROLE") || !keyword(t+4,"RUNNER") || !keyword(t+5,"ADAPTER")
      || !keyword(t+6,"NATIVE-PROCESS") || !keyword(t+7,"EXECUTABLE") || !t[8].quoted
      || strlen(t[8].text)>=sizeof(tool.executable) || !keyword(t+9,"SOURCE") || !keyword(t+10,"SNAPSHOT")) return bad();
      strcpy(tool.id,t[2].text);strcpy(tool.executable,t[8].text);return the_workbench_put_tool(&tool);
   }
   if(keyword(t,"RUN")) {
      TheWorkbenchArguments args={0};size_t i;
      if(n<4 || t[1].quoted || strlen(t[1].text)>THE_WORKBENCH_ID_MAX
      || !keyword(t+2,"SOURCE") || !keyword(t+3,"ACTIVE") || (n-4)%2) return bad();
      for(i=4;i<n;i+=2) if(!keyword(t+i,"ARG") || !t[i+1].quoted
      || !the_workbench_arguments_append(&args,t[i+1].text)) return bad();
      return the_tools_workbench_run(t[1].text,&args);
   }
   if(keyword(t,"EDIT")) return edit_request_tokens(&tokens);
   if(keyword(t,"FORM"))
   {
      TheWorkbenchForm form;
      if(n>=3 && (keyword(t+1,"SUBMIT") || keyword(t+1,"CANCEL"))) return form_request_tokens(&tokens);
      if(n!=3 || t[2].quoted || strlen(t[2].text)>THE_WORKBENCH_ID_MAX) return bad();
      if(keyword(t+1,"REMOVE")) return the_workbench_remove_form(t[2].text);
      if(!keyword(t+1,"PUT")) return bad();
      memset(&form,0,sizeof(form)); strcpy(form.id,t[2].text); return the_workbench_put_form(&form);
   }
   if(keyword(t,"FIELD"))
   {
      TheWorkbenchField field;
      if(n>=2 && keyword(t+1,"SET")) return form_request_tokens(&tokens);
      if(n==4 && keyword(t+1,"REMOVE") && !t[2].quoted && !t[3].quoted) return the_workbench_remove_field(t[2].text,t[3].text);
      if(n!=12 || !keyword(t+1,"PUT") || t[2].quoted || t[3].quoted
      || strlen(t[2].text)>THE_WORKBENCH_ID_MAX || strlen(t[3].text)>THE_WORKBENCH_ID_MAX
      || !keyword(t+4,"LABEL") || !t[5].quoted || strlen(t[5].text)>THE_WORKBENCH_LABEL_MAX
      || !keyword(t+6,"VALUE") || !t[7].quoted || !keyword(t+8,"ACCESS") || !keyword(t+10,"MAXCELLS")) return bad();
      if(strlen(t[7].text)>THE_WORKBENCH_FIELD_VALUE_MAX) return the_workbench_result("limit-exceeded",94,"Field value byte limit exceeded");
      memset(&field,0,sizeof(field)); strcpy(field.form,t[2].text); strcpy(field.id,t[3].text); strcpy(field.label,t[5].text); strcpy(field.value,t[7].text);
      if(keyword(t+9,"EDIT")) field.access=THE_WORKBENCH_FIELD_EDIT;
      else if(keyword(t+9,"READONLY")) field.access=THE_WORKBENCH_FIELD_READONLY; else return bad();
      if(!order_value(t+11,&field.maxcells)) return bad();
      return the_workbench_put_field(&field);
   }
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
         if (n > 7 && keyword(&t[7],"ARG"))
            return the_workbench_result("unsupported-feature",RC_INVALID_ENVIRON,"Macro argument vectors are outside phase one");
         if ((n != 7 && n != 11) || !t[6].quoted) return bad();
         a.kind = keyword(&t[5],"COMMAND") ? THE_WORKBENCH_ACTION_COMMAND : THE_WORKBENCH_ACTION_MACRO;
         if (a.kind == THE_WORKBENCH_ACTION_MACRO && strchr(t[6].text,'"')) return bad();
         strcpy(a.operand,t[6].text);
         if (n==11) {
            if (a.kind!=THE_WORKBENCH_ACTION_MACRO || !keyword(&t[7],"FILEEXT")
            || !t[8].quoted || strlen(t[8].text)>THE_WORKBENCH_FILEEXT_MAX
            || !keyword(&t[9],"PURPOSE") || !keyword(&t[10],"BUILD")) return bad();
            a.file_build=1;strcpy(a.file_extension,t[8].text);
         }
         if (the_workbench_restricted()) {
            const char *reason;
            if (!a.file_build) return the_workbench_result("policy-denied",RC_INVALID_ENVIRON,"Restricted sessions require a typed file Build macro");
            reason=file_build_path_reason(a.operand,1);
            if (strcmp(reason,"ok")) return the_workbench_result(reason,RC_INVALID_ENVIRON,"Build macro must be an existing workspace RXBIN");
         }
      }
      else if (keyword(&t[5],"PANEL"))
      {
         if (n != 9 || t[6].quoted || strlen(t[6].text) > THE_WORKBENCH_ID_MAX
         || !keyword(&t[7],"OP") || !(a.panel_operation=panel_operation_token(t+8))) return bad();
         a.kind=THE_WORKBENCH_ACTION_PANEL; strcpy(a.panel,t[6].text);
      }
      else if (keyword(&t[5],"TOOL"))
      {
         size_t i;
         if(n<9 || t[6].quoted || strlen(t[6].text)>THE_WORKBENCH_ID_MAX
         || !keyword(t+7,"SOURCE") || !keyword(t+8,"ACTIVE") || (n-9)%2) return bad();
         a.kind=THE_WORKBENCH_ACTION_TOOL;strcpy(a.tool,t[6].text);
         for(i=9;i<n;i+=2) if(!keyword(t+i,"ARG") || !t[i+1].quoted
         || !the_workbench_arguments_append(&a.arguments,t[i+1].text)) return bad();
      }
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
   if (keyword(&t[0],"PANEL"))
   {
      TheWorkbenchPanel p; TheWorkbenchPanelOperation op;
      if(n>=2 && (keyword(t+1,"SET") || keyword(t+1,"APPLY") || keyword(t+1,"RELOAD") || keyword(t+1,"CANCEL"))) return filefields_request_tokens(&tokens);
      if (n >= 2 && (op=panel_operation_token(t+1)))
      {
         if (n != 3 || t[2].quoted) return bad();
         return the_workbench_panel_operation(t[2].text,0,op);
      }
      if (n == 3 && keyword(t+1,"REMOVE") && !t[2].quoted) return the_workbench_remove_panel(t[2].text);
      memset(&p,0,sizeof(p));
      if (n >= 5 && keyword(t+1,"PUT") && keyword(t+3,"KIND") && !keyword(t+4,"LOG"))
      {
         if(keyword(t+4,"FIELDS") && n==7 && keyword(t+5,"FORM") && !t[2].quoted && !t[6].quoted
         && strlen(t[2].text)<=THE_WORKBENCH_ID_MAX && strlen(t[6].text)<=THE_WORKBENCH_ID_MAX)
         { p.kind=THE_WORKBENCH_PANEL_FIELDS; strcpy(p.id,t[2].text); strcpy(p.form,t[6].text); return the_workbench_put_panel(&p); }
         if(keyword(t+4,"FIELDS") && n==11 && !t[2].quoted && strlen(t[2].text)<=THE_WORKBENCH_ID_MAX
         && keyword(t+5,"BUFFER") && keyword(t+6,"ACTIVE") && keyword(t+7,"MARKUP") && keyword(t+8,"CTLCHAR") && keyword(t+9,"ACCESS") && keyword(t+10,"STAGED"))
         { p.kind=THE_WORKBENCH_PANEL_FIELDS; strcpy(p.id,t[2].text); p.document_id=the_editor_document_active(); the_editor_filefields_initialize();
           if(!p.document_id) return the_workbench_result("document-unavailable",-3,"No active document can be bound"); return the_workbench_put_panel(&p); }
         if (!keyword(t+4,"DOCUMENT") || (n>5 && keyword(t+5,"FILE")))
            return the_workbench_result("unsupported-feature",RC_INVALID_ENVIRON,"Only live read-only buffer documents are implemented");
         if (n!=9 || t[2].quoted || strlen(t[2].text)>THE_WORKBENCH_ID_MAX
         || !keyword(t+5,"BUFFER") || !keyword(t+6,"ACTIVE") || !keyword(t+7,"ACCESS") || !keyword(t+8,"READONLY")) return bad();
         p.kind=THE_WORKBENCH_PANEL_DOCUMENT; strcpy(p.id,t[2].text);
         p.document_id=the_editor_document_active();
         if (!p.document_id) return the_workbench_result("document-unavailable",RC_INVALID_ENVIRON,"No active document can be bound");
         return the_workbench_put_panel(&p);
      }
      if (n == 7 && keyword(t+1,"PUT") && !t[2].quoted && strlen(t[2].text) <= THE_WORKBENCH_ID_MAX
      && keyword(t+3,"KIND") && keyword(t+4,"LOG") && keyword(t+5,"MODEL")
      && !t[6].quoted && strlen(t[6].text) <= THE_WORKBENCH_ID_MAX)
      {
         strcpy(p.id,t[2].text); strcpy(p.model,t[6].text); p.kind=THE_WORKBENCH_PANEL_LOG;
         return the_workbench_put_panel(&p);
      }
      if (n == 8 && keyword(t+1,"PLACE") && !t[2].quoted && (p.placement=placement_token(t+3))
      && keyword(t+4,"FALLBACK") && fallback_tokens(t+5,p.fallbacks,&p.fallback_count)
      && keyword(t+6,"MINCOLS") && order_value(t+7,&p.mincols))
         return the_workbench_place_panel(t[2].text,p.placement,p.fallbacks,p.fallback_count,p.mincols);
      return bad();
   }
   if (keyword(&t[0],"ROUTE"))
   {
      TheWorkbenchRoute r; const char *slash; size_t length;
      if (n == 3 && keyword(t+1,"REMOVE") && !t[2].quoted) return the_workbench_remove_route(t[2].text);
      if(n>=4 && keyword(t+1,"PUT") && !t[2].quoted && strlen(t[2].text)<=THE_WORKBENCH_ID_MAX && keyword(t+3,"FROM")) {
         const Token *endpoint=NULL;
         memset(&r,0,sizeof(r)); strcpy(r.id,t[2].text);
         if(n==12 && keyword(t+4,"FILE") && t[5].quoted && keyword(t+6,"TO")
         && keyword(t+8,"FORMAT") && keyword(t+9,"BYTES") && keyword(t+10,"EOF") && keyword(t+11,"END")) {
            r.kind=THE_WORKBENCH_ROUTE_FILE_INPUT; strcpy(r.path,t[5].text); endpoint=t+7;
         } else if((n==14 || n==16) && keyword(t+5,"TO") && keyword(t+6,"FILE") && t[7].quoted
         && keyword(t+8,"FORMAT") && keyword(t+9,"BYTES") && keyword(t+10,"MODE")) {
            if(keyword(t+11,"REPLACE") || keyword(t+11,"APPEND"))
               return the_workbench_result("unsupported-feature",RC_INVALID_ENVIRON,"REPLACE and APPEND require qualified conditional atomic publication");
            if(n!=14 || !keyword(t+11,"CREATE") || !keyword(t+12,"PUBLISH") || !keyword(t+13,"SUCCESS")) return bad();
            r.kind=THE_WORKBENCH_ROUTE_FILE_OUTPUT; strcpy(r.path,t[7].text); endpoint=t+4;
         }
         if(endpoint) {
            if(endpoint->quoted || strncmp(endpoint->text,"tool:",5) || !(slash=strchr(endpoint->text+5,'/'))
            || !(length=(size_t)(slash-endpoint->text-5)) || length>THE_WORKBENCH_ID_MAX) return bad();
            memcpy(r.tool,endpoint->text+5,length);
            if(r.kind==THE_WORKBENCH_ROUTE_FILE_INPUT) { if(strcmp(slash+1,"stdin")) return bad(); }
            else if(!strcmp(slash+1,"stdout")) r.stream=1;
            else if(!strcmp(slash+1,"stderr")) r.stream=2; else return bad();
            return the_workbench_put_route(&r);
         }
      }
      if (n != 7 || !keyword(t+1,"PUT") || t[2].quoted || strlen(t[2].text) > THE_WORKBENCH_ID_MAX
      || !keyword(t+3,"FROM") || !keyword(t+5,"TO") || t[4].quoted || t[6].quoted) return bad();
      if (strncmp(t[4].text,"tool:",5) || strncmp(t[6].text,"log:",4))
         return the_workbench_result("unsupported-feature",RC_INVALID_ENVIRON,"Only tool stdout/stderr to log routes are implemented");
      slash=strchr(t[4].text+5,'/');
      if (!slash || !(length=(size_t)(slash-t[4].text-5)) || length > THE_WORKBENCH_ID_MAX
      || strlen(t[6].text+4) > THE_WORKBENCH_ID_MAX) return bad();
      memset(&r,0,sizeof(r)); strcpy(r.id,t[2].text); memcpy(r.tool,t[4].text+5,length); strcpy(r.model,t[6].text+4);
      if (!strcmp(slash+1,"stdout")) r.stream=1;
      else if (!strcmp(slash+1,"stderr")) r.stream=2;
      else return the_workbench_result("unsupported-feature",RC_INVALID_ENVIRON,"Only stdout/stderr routes are implemented");
      return the_workbench_put_route(&r);
   }
   if(keyword(t,"FILE")) {
      static char json[THE_WORKBENCH_JSON_MAX];
      if(n!=3 || !keyword(t+1,"INSPECT") || !t[2].quoted) return bad();
      if(the_workbench_restricted() || !the_workbench_fileio_supported()) return the_workbench_result("unsupported-feature",RC_INVALID_ENVIRON,"Native FILE inspection is unavailable in this host");
      if(!the_workbench_fileio_inspect_json(t[2].text,json,sizeof(json))) {
         const TheWorkbenchFileReceipt *r=the_workbench_fileio_receipt();
         return the_workbench_result(r->code,RC_INVALID_ENVIRON,r->message);
      }
      display_error(0,(CHARTYPE *)json,TRUE);return the_workbench_result("ok",RC_OK,"");
   }
   if (keyword(&t[0],"FORM") || keyword(&t[0],"FIELD")
   || keyword(&t[0],"TOOL") || keyword(&t[0],"RUN") || keyword(&t[0],"JOB"))
      return the_workbench_result("unsupported-feature",RC_INVALID_ENVIRON,"This workbench feature is outside phase one");
   return bad();
}

short the_workbench_command(const char *operands)
{
   const char *p=operands ? operands : "";
   char first_text[16]={0},second_text[16]={0};
   Token first={first_text,0},second={second_text,0}; size_t n=0;
   int runtime; short rc;
   while (*p == ' ' || *p == '\t') p++;
   while (*p && *p != ' ' && *p != '\t' && n < 15) first.text[n++]=*p++;
   while (*p == ' ' || *p == '\t') p++;
   n=0; while (*p && *p != ' ' && *p != '\t' && n < 15) second.text[n++]=*p++;
   runtime=keyword(&first,"RUN") || keyword(&first,"INVOKE") || keyword(&first,"EDIT") || (keyword(&first,"FILE") && keyword(&second,"INSPECT")) || (keyword(&first,"PANEL") && (panel_operation_token(&second) || keyword(&second,"SET") || keyword(&second,"APPLY") || keyword(&second,"RELOAD") || keyword(&second,"CANCEL")))
      || (keyword(&first,"FIELD") && keyword(&second,"SET"))
      || (keyword(&first,"FORM") && (keyword(&second,"SUBMIT") || keyword(&second,"CANCEL")));
   rc=command_operands(operands);
   if (rc != RC_OK && !runtime) the_workbench_poison_definition();
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

static int unsigned64_value(const Token *t,uint64_t *value)
{
   const char *p=t->text; uint64_t n=0;
   if (t->quoted || !*p) return 0;
   for (;*p;p++)
   {
      if (*p < '0' || *p > '9' || n > (UINT64_MAX-(unsigned)(*p-'0'))/10) return 0;
      n=n*10+(unsigned)(*p-'0');
   }
   *value=n; return 1;
}
static int query_log(const Tokens *tokens,char *json,size_t capacity)
{
   uint64_t generation=0,cursor=0,value;
   size_t offset=0,records=8,bytes=4096,n;
   unsigned seen=0,bit;
   const TheWorkbenchModelState *model;
   if (tokens->count < 2 || tokens->values[1].quoted) return 0;
   for (n=2;n<tokens->count;n+=2)
   {
      if (n+1 >= tokens->count || !unsigned64_value(tokens->values+n+1,&value)) return 0;
      if (keyword(tokens->values+n,"EXPECT")) { bit=1; if(!value) return 0; generation=value; }
      else if (keyword(tokens->values+n,"CURSOR")) { bit=2; cursor=value; }
      else if (keyword(tokens->values+n,"OFFSET")) { bit=4; if(value > SIZE_MAX) return 0; offset=(size_t)value; }
      else if (keyword(tokens->values+n,"LIMIT")) { bit=8; if(!value || value>8) return 0; records=(size_t)value; }
      else if (keyword(tokens->values+n,"BYTES")) { bit=16; if(!value || value>THE_WORKBENCH_LOG_JSON_BYTES_MAX) return 0; bytes=(size_t)value; }
      else return 0;
      if (seen & bit) return 0;
      seen|=bit;
   }
   model=the_workbench_model_state(tokens->values[1].text);
   if (!model) { the_workbench_result("missing-object",RC_INVALID_OPERAND,"No published log has that ID"); return -1; }
   if (!generation) generation=model->generation;
   return the_workbench_log_json(json,capacity,model->id,generation,cursor,offset,records,bytes) == RC_OK ? 1 : -1;
}
static int query_document(const Tokens *tokens,char *json,size_t capacity)
{
   uint64_t expected=0,value; size_t line=1,offset=0,records=8,bytes=4096,n;
   unsigned seen=0,bit;
   if (tokens->count<2 || tokens->values[1].quoted) return 0;
   for (n=2;n<tokens->count;n+=2)
   {
      if (n+1>=tokens->count || !unsigned64_value(tokens->values+n+1,&value)) return 0;
      if (keyword(tokens->values+n,"EXPECT")) { bit=1; if(!value) return 0; expected=value; }
      else if (keyword(tokens->values+n,"LINE")) { bit=2; if(!value || value>SIZE_MAX) return 0; line=(size_t)value; }
      else if (keyword(tokens->values+n,"OFFSET")) { bit=4; if(value>SIZE_MAX) return 0; offset=(size_t)value; }
      else if (keyword(tokens->values+n,"LIMIT")) { bit=8; if(!value || value>8) return 0; records=(size_t)value; }
      else if (keyword(tokens->values+n,"BYTES")) { bit=16; if(!value || value>THE_WORKBENCH_DOCUMENT_BYTES_MAX) return 0; bytes=(size_t)value; }
      else return 0;
      if (seen & bit) return 0; seen|=bit;
   }
   return the_workbench_document_json(json,capacity,tokens->values[1].text,expected,line,offset,records,bytes)==RC_OK ? 1 : -1;
}
static int query_form(const Tokens *tokens,char *json,size_t capacity)
{
   size_t n,offset=0,limit=8,bytes=8192; uint64_t value; unsigned seen=0,bit; int submitted=keyword(tokens->values,"FORM");
   if(tokens->count<2 || tokens->values[1].quoted) return 0;
   n=2;
   if(submitted) { if(tokens->count<3 || !keyword(tokens->values+2,"RESULT")) return 0; n=3; }
   for(;n<tokens->count;n+=2)
   {
      if(n+1>=tokens->count || !unsigned64_value(tokens->values+n+1,&value) || value>SIZE_MAX) return 0;
      if(keyword(tokens->values+n,"OFFSET")) { bit=1; offset=(size_t)value; }
      else if(keyword(tokens->values+n,"LIMIT")) { bit=2; limit=(size_t)value; }
      else if(keyword(tokens->values+n,"BYTES")) { bit=4; bytes=(size_t)value; }
      else return 0;
      if(seen & bit) return 0; seen|=bit;
   }
   return the_workbench_form_json(json,capacity,tokens->values[1].text,offset,limit,bytes,submitted)==RC_OK ? 1 : -1;
}

static int query_filefields(const Tokens *tokens,char *json,size_t capacity)
{
   size_t n,offset=0,limit=8,bytes=8192; uint64_t value; unsigned seen=0,bit;
   if(tokens->count<2 || tokens->values[1].quoted) return 0;
   for(n=2;n<tokens->count;n+=2) {
      if(n+1>=tokens->count || !unsigned64_value(tokens->values+n+1,&value) || value>SIZE_MAX) return 0;
      if(keyword(tokens->values+n,"OFFSET")) { bit=1; offset=(size_t)value; }
      else if(keyword(tokens->values+n,"LIMIT")) { bit=2; limit=(size_t)value; }
      else if(keyword(tokens->values+n,"BYTES")) { bit=4; bytes=(size_t)value; }
      else return 0;
      if(seen & bit) return 0; seen|=bit;
   }
   return the_workbench_filefields_json(json,capacity,tokens->values[1].text,offset,limit,bytes)==RC_OK ? 1 : -1;
}
static unsigned long inspection_refusal_serial;
unsigned long the_workbench_inspection_refusal_serial(void)
{ return inspection_refusal_serial; }

static short query_bad(void)
{
   bad();
   inspection_refusal_serial++;
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
      if(!strcmp(section,"FILE")) {
         if(tokens.count!=2 || !tokens.values[1].quoted) return query_bad();
         if(the_workbench_restricted() || !the_workbench_fileio_supported()) { the_workbench_result("unsupported-feature",RC_INVALID_ENVIRON,"Native FILE inspection is unavailable in this host"); inspection_refusal_serial++; return EXTRACT_ARG_ERROR; }
         if(!the_workbench_fileio_inspect_json(tokens.values[1].text,json,sizeof(json))) { const TheWorkbenchFileReceipt *r=the_workbench_fileio_receipt(); the_workbench_result(r->code,RC_INVALID_ENVIRON,r->message); inspection_refusal_serial++; return EXTRACT_ARG_ERROR; }
         goto formatted;
      }
      if(!strcmp(section,"WRITE")) {
         uint64_t id=0; short rc;
         if(tokens.count!=1 && (tokens.count!=3 || !keyword(tokens.values+1,"DOCUMENT")
         || !edit_number(tokens.values+2,&id))) return query_bad();
         rc=the_editor_edit_group_json(json,sizeof(json),id);
         if(rc) { the_workbench_result(the_edit_group_error(),rc,"Write token unavailable"); inspection_refusal_serial++; return EXTRACT_ARG_ERROR; }
         goto formatted;
      }
      if(!strcmp(section,"FILEFIELDS")) {
         int admitted=query_filefields(&tokens,json,sizeof(json));
         if(admitted<0) { inspection_refusal_serial++; return EXTRACT_ARG_ERROR; }
         if(!admitted) return query_bad(); goto formatted;
      }
      if(!strcmp(section,"FIELDS") || !strcmp(section,"FORM"))
      {
         int admitted=query_form(&tokens,json,sizeof(json));
         if(admitted<0) { inspection_refusal_serial++; return EXTRACT_ARG_ERROR; }
         if(!admitted) return query_bad();
         goto formatted;
      }
      if (!strcmp(section,"LOG") || !strcmp(section,"DOCUMENT"))
      {
         int admitted=!strcmp(section,"LOG") ? query_log(&tokens,json,sizeof(json)) : query_document(&tokens,json,sizeof(json));
         if (admitted < 0) { inspection_refusal_serial++; return EXTRACT_ARG_ERROR; }
         if (!admitted) return query_bad();
         goto formatted;
      }
      for (n = 1; n < tokens.count; n += 2)
      {
         if (n+1 >= tokens.count || !order_value(&tokens.values[n+1],&value)) return query_bad();
         if (keyword(&tokens.values[n],"OFFSET") && !seen_offset) { offset = (size_t)value; seen_offset = 1; }
         else if (keyword(&tokens.values[n],"LIMIT") && !seen_limit) { count = (size_t)value; seen_limit = 1; }
         else return query_bad();
      }
   }
   if (!the_workbench_json(json,sizeof(json),section,offset,count,1)) return query_bad();
formatted:
   if (query_type == QUERY_QUERY)
   {
      display_error(0,(CHARTYPE *)json,TRUE);
      return EXTRACT_VARIABLES_SET;
   }
   item_values[1].value = (CHARTYPE *)json;
   item_values[1].len = strlen(json);
   return 1;
}
