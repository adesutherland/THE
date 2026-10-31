#include "workbench.h"
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
   if (failures) return 1;
   puts("workbench registry tests passed"); return 0;
}
