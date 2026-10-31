#ifndef THE_WORKBENCHRUNTIME_H
#define THE_WORKBENCHRUNTIME_H

#include "workbench.h"
#include "workbenchlog.h"

#define THE_WORKBENCH_LOG_JSON_BYTES_MAX 8192

typedef struct
{
   char id[THE_WORKBENCH_ID_MAX + 1];
   unsigned long generation;
   int visible, focused;
   TheWorkbenchPlacement effective;
   char reason[64];
} TheWorkbenchPanelState;

typedef struct
{
   unsigned long serial;
   char panel[THE_WORKBENCH_ID_MAX + 1];
   unsigned long generation;
   char previous_panel[THE_WORKBENCH_ID_MAX + 1];
   unsigned long previous_generation;
   int restoring; /* Physical editor/Program origin is retained by the driver. */
} TheWorkbenchFocusRequest;

typedef struct
{
   char id[THE_WORKBENCH_ID_MAX + 1];
   char owner[THE_WORKBENCH_ID_MAX + 1];
   uint64_t generation;
   size_t leases;
   TheWorkbenchLogResult last_error;
   uint64_t error_job, error_sequence;
   int error_stream;
} TheWorkbenchModelState;

typedef struct TheWorkbenchRouteLease TheWorkbenchRouteLease;

/* Publication preparation is synchronous on the editor owner. It does not
 * publish definitions, focus, render, execute, clear or replace existing text.
 * Failure removes only the empty models allocated by this attempt. */
short the_workbench_runtime_publish(TheWorkbenchPanel *panels, size_t count);
void the_workbench_runtime_reset(void);
unsigned long the_workbench_panel_state_revision(void);
const TheWorkbenchPanelState *the_workbench_panel_state(const char *id);
const TheWorkbenchFocusRequest *the_workbench_focus_request(void);
const TheWorkbenchModelState *the_workbench_model_state(const char *id);
const TheWorkbenchModelState *the_workbench_model_at(size_t index);
size_t the_workbench_model_count(void);
/* Internal state operation: use the_workbench_panel_operation for modal and
 * activation admission. SHOW/output never create a focus request. */
short the_workbench_runtime_panel(const char *id, unsigned long generation,
                                TheWorkbenchPanelOperation operation);
/* Driver reports chosen physical placement; no renderer means NONE plus reason. */
int the_workbench_panel_effective(const char *id, unsigned long generation,
   TheWorkbenchPlacement placement, const char *reason);

/* A successful bind returns an allocated immutable lease even with no routes.
 * NULL means refusal; no partial pins survive. One active singleton lease. */
TheWorkbenchRouteLease *the_workbench_routes_bind(const char *tool_id,
   uint64_t job_id, char *error, size_t capacity);
TheWorkbenchLogResult the_workbench_route_output(TheWorkbenchRouteLease *lease,
   int fd, uint64_t sequence, const void *bytes, size_t length);
void the_workbench_routes_release(TheWorkbenchRouteLease *lease);
/* Log tokens are strings in JSON to preserve uint64 precision. Text uses
 * explicit lengths, including NUL/control data. Budget 1..8192 bytes. */
short the_workbench_log_json(char *out, size_t capacity, const char *id,
   uint64_t generation, uint64_t cursor, size_t offset, size_t records, size_t bytes);

#endif
