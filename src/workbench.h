#ifndef THE_WORKBENCH_H
#define THE_WORKBENCH_H

#include <stddef.h>
#include <stdint.h>
#include "frontendaction.h"

#define THE_WORKBENCH_VERSION 1
#define THE_WORKBENCH_ID_MAX 63
#define THE_WORKBENCH_LABEL_MAX 255
#define THE_WORKBENCH_OPERAND_MAX 2048
#define THE_WORKBENCH_COMMAND_MAX 8192
#define THE_WORKBENCH_ACTION_MAX 128
#define THE_WORKBENCH_MENU_MAX 32
#define THE_WORKBENCH_ITEM_MAX 256
#define THE_WORKBENCH_OWNER_MAX 32
#define THE_WORKBENCH_PANEL_MAX 32
#define THE_WORKBENCH_ROUTE_MAX 32
#define THE_WORKBENCH_FORM_MAX 32
#define THE_WORKBENCH_FIELDS_PER_FORM_MAX 128
#define THE_WORKBENCH_FIELD_MAX 512
#define THE_WORKBENCH_FIELD_VALUE_MAX 1024
#define THE_WORKBENCH_PAGE_MAX 8
#define THE_WORKBENCH_INVOKE_DEPTH_MAX 8
#define THE_WORKBENCH_JSON_MAX 65536

typedef enum
{
   THE_WORKBENCH_ACTION_BUILTIN = 1,
   THE_WORKBENCH_ACTION_COMMAND,
   THE_WORKBENCH_ACTION_MACRO,
   THE_WORKBENCH_ACTION_TOOL = 4, /* reserved until phase two */
   THE_WORKBENCH_ACTION_PANEL = 5
} TheWorkbenchActionKind;

typedef enum
{
   THE_WORKBENCH_PANEL_SHOW = 1,
   THE_WORKBENCH_PANEL_HIDE,
   THE_WORKBENCH_PANEL_FOCUS,
   THE_WORKBENCH_PANEL_RETURN /* normalized internal request only */
} TheWorkbenchPanelOperation;

typedef enum
{
   THE_WORKBENCH_PLACE_NONE = 0,
   THE_WORKBENCH_PLACE_RIGHT,
   THE_WORKBENCH_PLACE_BELOW,
   THE_WORKBENCH_PLACE_TAB
} TheWorkbenchPlacement;

typedef enum { THE_WORKBENCH_PANEL_LOG = 1, THE_WORKBENCH_PANEL_DOCUMENT = 2, THE_WORKBENCH_PANEL_FIELDS = 3 } TheWorkbenchPanelKind;
typedef enum { THE_WORKBENCH_FIELD_EDIT = 1, THE_WORKBENCH_FIELD_READONLY = 2 } TheWorkbenchFieldAccess;

typedef struct {
   char id[THE_WORKBENCH_ID_MAX+1], owner[THE_WORKBENCH_ID_MAX+1];
   unsigned long generation;
} TheWorkbenchForm;
typedef struct {
   char id[THE_WORKBENCH_ID_MAX+1], owner[THE_WORKBENCH_ID_MAX+1], form[THE_WORKBENCH_ID_MAX+1];
   char label[THE_WORKBENCH_LABEL_MAX+1], value[THE_WORKBENCH_FIELD_VALUE_MAX+1];
   unsigned long generation;
   TheWorkbenchFieldAccess access;
   int maxcells;
} TheWorkbenchField;

typedef enum { THE_WORKBENCH_ROUTE_LOG=0, THE_WORKBENCH_ROUTE_FILE_INPUT,
   THE_WORKBENCH_ROUTE_FILE_OUTPUT } TheWorkbenchRouteKind;
typedef struct
{
   char id[THE_WORKBENCH_ID_MAX + 1];
   char owner[THE_WORKBENCH_ID_MAX + 1];
   char label[THE_WORKBENCH_LABEL_MAX + 1];
   unsigned long generation;
   TheWorkbenchActionKind kind;
   TheFrontendActionId builtin;
   char operand[THE_WORKBENCH_OPERAND_MAX + 1];
   char panel[THE_WORKBENCH_ID_MAX + 1];
   TheWorkbenchPanelOperation panel_operation;
} TheWorkbenchAction;

typedef struct
{
   char id[THE_WORKBENCH_ID_MAX + 1];
   char owner[THE_WORKBENCH_ID_MAX + 1];
   char label[THE_WORKBENCH_LABEL_MAX + 1];
   unsigned long generation;
   int order;
} TheWorkbenchMenu;

typedef struct
{
   char id[THE_WORKBENCH_ID_MAX + 1];
   char owner[THE_WORKBENCH_ID_MAX + 1];
   char menu[THE_WORKBENCH_ID_MAX + 1];
   char action[THE_WORKBENCH_ID_MAX + 1];
   char label[THE_WORKBENCH_LABEL_MAX + 1];
   unsigned long generation;
   int order;
   int separator;
} TheWorkbenchItem;

typedef struct
{
   char id[THE_WORKBENCH_ID_MAX + 1];
   char owner[THE_WORKBENCH_ID_MAX + 1];
   char title[THE_WORKBENCH_LABEL_MAX + 1];
   unsigned long generation;
   TheWorkbenchPanelKind kind;
   char model[THE_WORKBENCH_ID_MAX + 1];
   uint64_t model_generation;
   uint64_t document_id; /* Immutable live-buffer binding; never a path lookup. */
   TheWorkbenchPlacement placement;
   TheWorkbenchPlacement fallbacks[3];
   size_t fallback_count;
   int mincols, minrows;
   char form[THE_WORKBENCH_ID_MAX+1];
   unsigned long form_generation;
} TheWorkbenchPanel;

typedef struct
{
   char id[THE_WORKBENCH_ID_MAX + 1];
   char owner[THE_WORKBENCH_ID_MAX + 1];
   unsigned long generation;
   char tool[THE_WORKBENCH_ID_MAX + 1];
   char model[THE_WORKBENCH_ID_MAX + 1];
   int stream;
   TheWorkbenchRouteKind kind;
   char path[THE_WORKBENCH_OPERAND_MAX+1];
} TheWorkbenchRoute;

/* Lookup is pure metadata and must be registered before profile definitions.
 * Found but unavailable tools remain valid template references. */
typedef struct { const char *role; int available; } TheWorkbenchToolInfo;
typedef int (*TheWorkbenchToolLookup)(const char *id, TheWorkbenchToolInfo *out, void *context);
void the_workbench_set_tool_lookup(TheWorkbenchToolLookup lookup, void *context);
int the_workbench_tool_info(const char *id, TheWorkbenchToolInfo *out);

typedef struct
{
   const char *code;
   const char *message;
   short rc;
   unsigned long revision;
} TheWorkbenchResult;

/* Published objects only. Borrowed pointers expire on the next publication.
 * Definitions never dispatch input. Frontends must explicitly invoke IDs. */
unsigned long the_workbench_revision(void);
size_t the_workbench_action_count(void);
size_t the_workbench_menu_count(void);
size_t the_workbench_item_count(void);
size_t the_workbench_panel_count(void);
size_t the_workbench_route_count(void);
const TheWorkbenchAction *the_workbench_action_at(size_t index);
const TheWorkbenchAction *the_workbench_action_find(const char *id);
const TheWorkbenchMenu *the_workbench_menu_at(size_t index);
const TheWorkbenchItem *the_workbench_item_at(size_t index);
const TheWorkbenchPanel *the_workbench_panel_at(size_t index);
const TheWorkbenchPanel *the_workbench_panel_find(const char *id);
const TheWorkbenchRoute *the_workbench_route_at(size_t index);
size_t the_workbench_form_count(void);
size_t the_workbench_field_count(void);
const TheWorkbenchForm *the_workbench_form_at(size_t index);
const TheWorkbenchForm *the_workbench_form_find(const char *id);
const TheWorkbenchField *the_workbench_field_at(size_t index);
const TheWorkbenchField *the_workbench_field_find(const char *form,const char *id);
const TheWorkbenchResult *the_workbench_last_result(void);
const char *the_workbench_action_kind_name(TheWorkbenchActionKind kind);
/* Runtime admission hint. Invoke always revalidates. */
const char *the_workbench_action_reason(const TheWorkbenchAction *action);

/* A single explicit definition transaction; caller context is session-owned,
 * or inherited from the active profile. IDs cannot change owner/context. */
short the_workbench_begin(const char *owner);
short the_workbench_commit(void);
short the_workbench_abort(void);
short the_workbench_reset_owner(const char *owner);
short the_workbench_put_action(const TheWorkbenchAction *action);
short the_workbench_remove_action(const char *id);
short the_workbench_put_menu(const TheWorkbenchMenu *menu);
short the_workbench_remove_menu(const char *id);
short the_workbench_put_item(const TheWorkbenchItem *item);
short the_workbench_remove_item(const char *menu, const char *id);
short the_workbench_put_panel(const TheWorkbenchPanel *panel);
short the_workbench_place_panel(const char *id, TheWorkbenchPlacement placement,
   const TheWorkbenchPlacement *fallbacks, size_t fallback_count, int mincols);
short the_workbench_remove_panel(const char *id);
short the_workbench_put_route(const TheWorkbenchRoute *route);
short the_workbench_remove_route(const char *id);
short the_workbench_put_form(const TheWorkbenchForm *form);
short the_workbench_remove_form(const char *id);
short the_workbench_put_field(const TheWorkbenchField *field);
short the_workbench_remove_field(const char *form, const char *id);
const char *the_workbench_placement_name(TheWorkbenchPlacement placement);
/* Execution-owner admission; no driver or tool callback may call reentrantly. */
short the_workbench_panel_operation(const char *id, unsigned long generation,
                                   TheWorkbenchPanelOperation operation);
int the_workbench_transaction_active(void);
int the_workbench_activation_active(void);
short the_workbench_activation_begin(const char *profile);
/* Imported/compiled images inherit no declaration trust in restricted hosts. */
short the_workbench_activation_begin_untrusted(const char *profile);
short the_workbench_activation_end(int success);
/* Result reporting alone does not change definition admission. */
short the_workbench_result(const char *code, short rc, const char *message);
void the_workbench_poison_definition(void);

/* Strict operand parser; WORKBENCH itself is not included. Definitions require
 * BEGIN. Quoted operands escape a quote by doubling it; backslashes are data. */
short the_workbench_command(const char *operands);
/* argument overrides the bound operand only for typed builtins (chooser).
 * generation=0 omits the optional optimistic generation check. */
short the_workbench_invoke(const char *id, unsigned long generation,
                           const char *argument);
/* Advances once after the outermost explicit invocation has finished. */
unsigned long the_workbench_invocation_serial(void);
/* Internal Rexx admission marker; advances only on failed QUERY/EXTRACT. */
unsigned long the_workbench_inspection_refusal_serial(void);
/* JSON page; candidate=1 is for the current profile's QUERY/EXTRACT only.
 * sections SUMMARY/CAPABILITIES/ACTIONS/MENUS/ITEMS; page limit 1..8. */
int the_workbench_json(char *out, size_t length, const char *section,
                       size_t offset, size_t limit, int candidate);
/* Test/session teardown; does not execute or alter editor state. */
void the_workbench_reset(void);

#endif
