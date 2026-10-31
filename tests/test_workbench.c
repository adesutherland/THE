#include "workbench.h"
#include "workbenchruntime.h"
#include <stdio.h>
#include <string.h>

static int failures;
static int restricted;
int the_workbench_restricted(void) { return restricted; }
const char *the_workbench_action_reason(const TheWorkbenchAction *action)
{ return action ? "ok" : "missing-action"; }
static void check(int value, const char *message)
{ if (!value) { fprintf(stderr,"FAIL %s\n",message); failures++; } }
static void code(const char *expected)
{ check(!strcmp(the_workbench_last_result()->code,expected),expected); }
static TheWorkbenchAction action(const char *id)
{
   TheWorkbenchAction a; memset(&a,0,sizeof(a));
   strcpy(a.id,id); strcpy(a.label,"Label \" / < > & ");
   a.kind = THE_WORKBENCH_ACTION_COMMAND; strcpy(a.operand,"input DECLARED");
   return a;
}
static int tool_lookup(const char *id,TheWorkbenchToolInfo *out,void *context)
{
   (void)context;
   if (!strcmp(id,"classic-rexx") || !strcmp(id,"rxbin")) { out->role="runner"; out->available=1; return 1; }
   if (!strcmp(id,"disabled")) { out->role="program"; out->available=0; return 1; }
   if (!strcmp(id,"language")) { out->role="language"; out->available=1; return 1; }
   return 0;
}
static void log_panels(void)
{
   TheWorkbenchPanel p={0}, p2;
   TheWorkbenchRoute r={0}, err;
   TheWorkbenchAction a=action("user.show");
   TheWorkbenchRouteLease *lease;
   TheWorkbenchLogPage page;
   const TheWorkbenchLogInfo *log;
   unsigned long rev,ui,focus,gen;
   uint64_t loggen,content;
   char json[THE_WORKBENCH_JSON_MAX],error[256];
   static const char data[]={'A',0,27,'[','m','<','b','>'};
   size_t i;
   the_workbench_reset(); the_workbench_set_tool_lookup(tool_lookup,NULL);
   strcpy(p.id,"user.panel"); strcpy(p.model,"user.log"); p.kind=THE_WORKBENCH_PANEL_LOG;
   strcpy(r.id,"user.stdout"); strcpy(r.tool,"classic-rexx"); strcpy(r.model,p.model); r.stream=1;
   a.kind=THE_WORKBENCH_ACTION_PANEL; a.operand[0]=0; strcpy(a.panel,p.id); a.panel_operation=THE_WORKBENCH_PANEL_SHOW;
   the_workbench_begin("user.logs");
   check(the_workbench_put_action(&a)==0 && the_workbench_put_route(&r)==0 && the_workbench_put_panel(&p)==0,"stage forward panel/model references");
   check(!the_workbench_panel_count() && !the_workbench_log_count(),"draft creates no live panel/log");
   the_workbench_abort();
   the_workbench_activation_begin("/log-profile"); the_workbench_begin("user.logs");
   the_workbench_put_panel(&p); the_workbench_put_route(&r); the_workbench_commit();
   check(!the_workbench_log_count(),"profile inner commit creates no runtime log");
   check(the_workbench_activation_end(0)==-3 && !the_workbench_panel_count() && !the_workbench_log_count(),"profile log rollback");
   the_workbench_begin("user.logs"); the_workbench_put_action(&a); the_workbench_put_route(&r); the_workbench_put_panel(&p);
   err=r; strcpy(err.id,"user.stderr"); err.stream=2; the_workbench_put_route(&err);
   check(the_workbench_commit()==0 && the_workbench_panel_count()==1 && the_workbench_route_count()==2,"publish complete log graph");
   gen=the_workbench_panel_find(p.id)->generation; loggen=the_workbench_panel_find(p.id)->model_generation;
   check(loggen && the_workbench_model_count()==1 && the_workbench_log_count()==1,"one retained model");
   check(the_workbench_json(json,sizeof(json),"PANELS",0,8,0) && strstr(json,"\"modelGeneration\":\"") && strstr(json,"\"visible\":false"),"panel metadata and string tokens");
   the_workbench_begin("user.logs"); the_workbench_remove_panel(p.id);
   check(the_workbench_commit()==-3,"referenced panel removal refused"); code("object-in-use"); the_workbench_abort();
   the_workbench_begin("user.logs");
   check(the_workbench_place_panel(p.id,THE_WORKBENCH_PLACE_RIGHT,NULL,0,0)==5,"zero panel minimum refused"); the_workbench_abort();
   the_workbench_begin("user.logs"); r.stream=1; strcpy(r.id,"user.duplicate"); the_workbench_put_route(&r);
   check(the_workbench_commit()==-3,"ambiguous primary stream refused"); code("object-in-use"); the_workbench_abort();
   rev=the_workbench_revision(); ui=the_workbench_panel_state_revision(); focus=the_workbench_focus_request()->serial;
   check(the_workbench_runtime_panel(p.id,gen,THE_WORKBENCH_PANEL_FOCUS)==-3,"hidden panel does not focus");
   check(the_workbench_panel_state_revision()==ui && the_workbench_focus_request()->serial==focus,"refused focus changes no state");
   check(the_workbench_runtime_panel(p.id,gen,THE_WORKBENCH_PANEL_SHOW)==0,"show panel");
   check(the_workbench_revision()==rev && the_workbench_focus_request()->serial==focus,"show changes neither definitions nor focus");
   check(the_workbench_runtime_panel(p.id,gen+1,THE_WORKBENCH_PANEL_HIDE)==-3 && the_workbench_panel_state(p.id)->visible,"stale panel request refused");
   check(the_workbench_runtime_panel(p.id,gen,THE_WORKBENCH_PANEL_FOCUS)==0 && the_workbench_panel_state(p.id)->focused,"explicit panel focus");
   focus=the_workbench_focus_request()->serial;
   lease=the_workbench_routes_bind("classic-rexx",99,error,sizeof(error)); check(lease!=NULL,"bind immutable streams");
   check(the_workbench_route_output(lease,1,1,data,sizeof(data))==THE_WORKBENCH_LOG_OK,"length-aware routed controls");
   check(the_workbench_route_output(lease,2,2,"error",5)==THE_WORKBENCH_LOG_OK,"stderr remains typed");
   check(the_workbench_focus_request()->serial==focus && the_workbench_panel_state(p.id)->focused,"output never changes focus");
   check(the_workbench_log_page(p.model,loggen,0,0,8,100,&page)==THE_WORKBENCH_LOG_OK && page.count==2 && page.slices[0].job==99 && page.slices[1].stream==2,"job stream sequence retained");
   check(the_workbench_log_json(json,sizeof(json),p.model,loggen,0,0,8,100)==0 && strstr(json,"\\u0000") && strstr(json,"\\u001b"),"log JSON escapes embedded controls");
   content=the_workbench_log_find(p.model)->revision; ui=the_workbench_panel_state_revision();
   check(the_workbench_route_output(lease,1,3,"\x80",1)==THE_WORKBENCH_LOG_INVALID,"invalid routed UTF8 refused");
   check(the_workbench_panel_state_revision()>ui && the_workbench_log_find(p.model)->revision==content,"append failure publishes state without fake content change");
   check(the_workbench_model_state(p.model)->last_error==THE_WORKBENCH_LOG_INVALID,"route error inspectable");
   check(the_workbench_runtime_panel(p.id,gen,THE_WORKBENCH_PANEL_RETURN)==0 && the_workbench_panel_state(p.id)->visible && !the_workbench_panel_state(p.id)->focused,"return preserves visible log");
   check(the_workbench_focus_request()->restoring && !the_workbench_focus_request()->panel[0],"return requests prior origin");
   check(the_workbench_runtime_panel(p.id,gen,THE_WORKBENCH_PANEL_RETURN)==-3,"return requires focus ownership");
   for (i=0;i<2;i++)
   {
      unsigned long previous_generation=gen;
      the_workbench_runtime_panel(p.id,gen,THE_WORKBENCH_PANEL_FOCUS);
      focus=the_workbench_focus_request()->serial; ui=the_workbench_panel_state_revision();
      content=the_workbench_log_find(p.model)->revision;
      the_workbench_begin("user.logs");
      if (i) the_workbench_put_panel(&p);
      else the_workbench_place_panel(p.id,THE_WORKBENCH_PLACE_BELOW,NULL,0,32);
      check(the_workbench_commit()==0,"focused panel definition change publishes");
      gen=the_workbench_panel_find(p.id)->generation;
      check(gen != previous_generation && the_workbench_panel_state(p.id)->visible && !the_workbench_panel_state(p.id)->focused,"replacement retains visibility and returns focus");
      check(the_workbench_focus_request()->serial==focus+1 && the_workbench_focus_request()->restoring && !the_workbench_focus_request()->panel[0]
            && !strcmp(the_workbench_focus_request()->previous_panel,p.id) && the_workbench_focus_request()->previous_generation==previous_generation,"replacement returns old renderer generation");
      check(the_workbench_panel_state_revision()>ui && the_workbench_log_find(p.model)->revision==content,"replacement publishes state without changing text");
      check(the_workbench_runtime_panel(p.id,previous_generation,THE_WORKBENCH_PANEL_RETURN)==-3,"stale return cannot retain focus ownership");
   }
   p2=p; strcpy(p2.id,"user.second"); strcpy(p2.model,"user.next");
   the_workbench_begin("user.logs"); the_workbench_put_panel(&p2);
   r=err; strcpy(r.id,"user.stdout"); r.stream=1; strcpy(r.model,p2.model); the_workbench_put_route(&r); the_workbench_commit();
   check(the_workbench_route_output(lease,1,4,"old",3)==THE_WORKBENCH_LOG_OK && !the_workbench_log_find(p2.model)->bytes,"template replacement cannot retarget a job");
   check(the_workbench_log_find(p.model)->bytes==sizeof(data)+8,"original route receives output once");
   the_workbench_runtime_panel(p.id,gen,THE_WORKBENCH_PANEL_FOCUS);
   the_workbench_begin("user.logs"); the_workbench_reset_owner("user.logs");
   check(the_workbench_commit()==0 && !the_workbench_panel_count() && !the_workbench_route_count(),"owner reset removes graph");
   check(the_workbench_focus_request()->restoring && !the_workbench_focus_request()->panel[0],"focused removal requests return");
   check(the_workbench_model_count()==2 && the_workbench_route_output(lease,1,5,"detached",8)==THE_WORKBENCH_LOG_OK,"removed pane retains pinned model output");
   the_workbench_routes_release(lease);
   check(!the_workbench_model_state(p.model)->leases,"terminal release removes pins");
   the_workbench_begin("other.logs"); the_workbench_put_panel(&p);
   check(the_workbench_commit()==-3,"retained log cannot change owner"); code("policy-denied"); the_workbench_abort();
   /* Force a late runtime resource failure; the earlier candidate allocation
    * must disappear and the published graph stay unchanged. */
   for(i=0;i<29;i++) { char id[32]; snprintf(id,sizeof(id),"raw.log%lu",(unsigned long)i); the_workbench_log_create(id); }
   rev=the_workbench_revision();
   the_workbench_begin("user.logs"); strcpy(p.id,"user.new1"); strcpy(p.model,"user.newlog1"); the_workbench_put_panel(&p);
   strcpy(p.id,"user.new2"); strcpy(p.model,"user.newlog2"); the_workbench_put_panel(&p);
   check(the_workbench_commit()==94 && the_workbench_revision()==rev && !the_workbench_panel_count(),"resource failure publishes no partial graph");
   check(!the_workbench_log_find("user.newlog1") && the_workbench_log_count()==31 && the_workbench_model_count()==2,"candidate log allocations rolled back"); the_workbench_abort();
   the_workbench_reset();
   strcpy(p.id,"user.panel"); strcpy(p.model,"user.log"); r=err; r.stream=1; strcpy(r.id,"user.stdout");
   the_workbench_begin("user.logs"); the_workbench_put_panel(&p); the_workbench_put_route(&r); the_workbench_commit();
   lease=the_workbench_routes_bind("classic-rexx",101,error,sizeof(error)); check(lease!=NULL,"active-reset bind");
   the_workbench_reset();
   check(the_workbench_route_output(lease,1,1,"late",4)==THE_WORKBENCH_LOG_STALE,"reset invalidates observer binding without freeing it");
   the_workbench_begin("user.logs"); the_workbench_put_panel(&p); the_workbench_put_route(&r); the_workbench_commit();
   check(!the_workbench_routes_bind("classic-rexx",102,error,sizeof(error)),"observer retains lifetime until end");
   the_workbench_routes_release(lease); the_workbench_routes_release(lease);
   lease=the_workbench_routes_bind("classic-rexx",102,error,sizeof(error)); check(lease!=NULL,"observer end permits next job");
   check(the_workbench_route_output(lease,1,1,"new",3)==THE_WORKBENCH_LOG_OK,"new job uses new log generation");
   log=the_workbench_log_find(p.model); check(log && log->bytes==3,"stale job did not populate recreated log");
   the_workbench_routes_release(lease); the_workbench_reset();
   restricted=1; the_workbench_activation_begin("/trusted-web-profile"); the_workbench_begin("web.logs");
   the_workbench_put_panel(&p); r=err; r.stream=1; strcpy(r.id,"user.stdout"); the_workbench_put_route(&r);
   check(the_workbench_commit()==0 && the_workbench_activation_end(1)==0,"trusted restricted startup declares safe log route");
   restricted=0; the_workbench_reset();
}

int main(void)
{
   TheWorkbenchAction a = action("user.action");
   TheWorkbenchMenu m = {0}; TheWorkbenchItem item = {0};
   unsigned long rev, gen; char json[THE_WORKBENCH_JSON_MAX];
   the_workbench_reset(); rev = the_workbench_revision();
   check(the_workbench_action_count() == 6,"builtin registry");
   strcpy(m.id,"user.menu"); strcpy(m.label,"Menu");
   strcpy(item.id,"user.item"); strcpy(item.menu,m.id); strcpy(item.action,a.id); strcpy(item.label,"Item");
   check(the_workbench_begin("user.owner") == 0,"begin");
   check(the_workbench_put_item(&item) == 0,"forward action/menu references");
   check(the_workbench_put_action(&a) == 0,"put action");
   check(the_workbench_put_menu(&m) == 0,"put menu");
   check(the_workbench_action_find(a.id) == NULL,"unpublished draft invisible");
   check(the_workbench_commit() == 0,"commit graph");
   check(the_workbench_revision() == rev+1,"single publication");
   gen = the_workbench_action_find(a.id)->generation;
   check(the_workbench_begin("user.owner") == 0,"replace begin");
   strcpy(a.label,"Replacement"); check(the_workbench_put_action(&a) == 0,"replace");
   check(the_workbench_commit() == 0,"replace commit");
   check(the_workbench_action_count() == 7,"idempotent count");
   check(the_workbench_action_find(a.id)->generation > gen,"replacement generation");
   rev = the_workbench_revision();
   check(the_workbench_begin("user.owner") == 0,"remove begin");
   check(the_workbench_remove_action(a.id) == 0,"stage referenced removal");
   check(the_workbench_commit() == -3,"referenced removal refused"); code("object-in-use");
   check(the_workbench_revision() == rev && the_workbench_action_find(a.id),"refusal atomic");
   check(the_workbench_abort() == 0,"abort");
   check(the_workbench_begin("other.owner") == 0,"other owner begin");
   check(the_workbench_put_action(&a) == -3,"owner replacement refused"); code("policy-denied");
   check(the_workbench_commit() == -3,"failed transaction cannot commit"); code("transaction-failed");
   the_workbench_abort();
   check(the_workbench_begin("user.owner") == 0,"missing begin");
   strcpy(item.action,"user.missing"); check(the_workbench_put_item(&item) == 0,"stage missing action");
   check(the_workbench_commit() == 5,"missing reference refused"); code("missing-action"); the_workbench_abort();
   check(the_workbench_activation_begin("/profile-one.the") == 0,"activation begin");
   check(the_workbench_begin("profile.owner") == 0,"profile owner");
   a = action("profile.action"); check(the_workbench_put_action(&a) == 0,"profile action");
   check(the_workbench_commit() == 0,"profile inner commit");
   check(!the_workbench_action_find(a.id),"inner commit remains unpublished");
   check(the_workbench_activation_end(0) == -3,"late activation failure");
   check(!the_workbench_action_find(a.id) && the_workbench_revision() == rev,"late failure rollback");
   check(the_workbench_activation_begin("/profile-one.the") == 0,"successful activation begin");
   the_workbench_begin("profile.owner"); the_workbench_put_action(&a); the_workbench_commit();
   check(the_workbench_activation_end(1) == 0 && the_workbench_action_find(a.id),"successful activation publish");
   rev = the_workbench_revision();
   the_workbench_activation_begin("/profile-two.the");
   check(the_workbench_begin("profile.owner") == -3,"profile owner impersonation denied");
   check(the_workbench_activation_end(1) == -3 && the_workbench_revision() == rev,"failed operation poisons activation");
   check(the_workbench_begin("profile.owner") == -3,"session cannot impersonate profile");
   check(the_workbench_activation_begin("/profile-one.the") == 0,"rerun activation");
   check(the_workbench_activation_end(1) == 0 && !the_workbench_action_find(a.id),"rerun removes omitted definitions");
   check(the_workbench_begin("user.owner") == 0,"atomic removal begin");
   check(the_workbench_remove_menu(m.id) == 0,"menu cascades items");
   check(the_workbench_remove_action("user.action") == 0,"remove unreferenced action");
   check(the_workbench_remove_action("user.absent") == 0,"absent remove idempotent");
   check(the_workbench_commit() == 0 && the_workbench_item_count() == 5,"atomic removal");
   check(the_workbench_begin("the.forged") == -3,"builtin owner reserved");
   check(the_workbench_json(json,sizeof(json),"ACTIONS",0,8,0),"bounded action JSON");
   check(strstr(json,"\"schemaVersion\":1") && strstr(json,"\"records\":["),"JSON schema");
   check(!the_workbench_json(json,20,"ACTIONS",0,8,0) && !json[0],"short JSON buffer fails closed");
   check(!the_workbench_json(json,sizeof(json),"ACTIONS",0,9,0),"page limit");
   the_workbench_reset();
   check(the_workbench_activation_begin_untrusted("/workspace/imported.rxbin") == 0,"untrusted scope enters without authority");
   check(the_workbench_begin("public.imported") == -3,"untrusted profile cannot declare"); code("policy-denied");
   check(the_workbench_activation_end(1) == -3,"untrusted declaration rolls back");
   restricted = 1;
   the_workbench_begin("public.browser"); a = action("public.command");
   check(the_workbench_put_action(&a) == -3,"typed API cannot bypass restricted policy"); code("policy-denied");
   the_workbench_abort(); restricted = 0;
   the_workbench_begin("public.quota");
   for (gen = 0; gen < THE_WORKBENCH_ACTION_MAX - 6; gen++)
   {
      a = action("public.quota"); snprintf(a.id,sizeof(a.id),"public.quota%lu",gen);
      check(the_workbench_put_action(&a) == 0,"admit action up to quota");
   }
   a = action("public.excess"); check(the_workbench_put_action(&a) == 94,"quota refuses without partial publication"); code("limit-exceeded");
   check(the_workbench_commit() == -3 && the_workbench_action_count() == 6,"quota cannot publish partial draft");
   the_workbench_abort();
   the_workbench_begin("public.invalid"); a = action("public.invalid");
   a.label[0] = (char)0xc0; a.label[1] = (char)0x80; a.label[2] = 0;
   check(the_workbench_put_action(&a) == 5,"reject overlong UTF8");
   the_workbench_abort();
   the_workbench_reset();
   log_panels();
   if (failures) return 1;
   puts("workbench registry tests passed"); return 0;
}
