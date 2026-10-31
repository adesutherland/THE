#ifndef THE_WORKBENCHLOG_H
#define THE_WORKBENCHLOG_H

#include <stddef.h>
#include <stdint.h>

#define THE_WORKBENCH_LOG_MAX 32
#define THE_WORKBENCH_LOG_ID_MAX 63
#define THE_WORKBENCH_LOG_TEXT_MAX 262144u
#define THE_WORKBENCH_LOG_SESSION_TEXT_MAX 8388608u
#define THE_WORKBENCH_LOG_RECORD_MAX 128
#define THE_WORKBENCH_LOG_PAGE_MAX 8
#define THE_WORKBENCH_LOG_APPEND_MAX 8388608u

typedef enum
{
   THE_WORKBENCH_LOG_OK = 0,
   THE_WORKBENCH_LOG_INVALID,
   THE_WORKBENCH_LOG_MISSING,
   THE_WORKBENCH_LOG_STALE,
   THE_WORKBENCH_LOG_LIMIT,
   THE_WORKBENCH_LOG_NOMEM
} TheWorkbenchLogResult;

typedef struct
{
   char id[THE_WORKBENCH_LOG_ID_MAX + 1];
   uint64_t generation, revision;
   uint64_t accepted_bytes, accepted_records;
   uint64_t omitted_bytes, omitted_records;
   uint64_t first_cursor, next_cursor;
   size_t bytes, records;
} TheWorkbenchLogInfo;

typedef struct
{
   uint64_t cursor, job, sequence;
   int stream; /* 1 stdout, 2 stderr. */
   size_t offset, length, original_length;
   const char *text;
} TheWorkbenchLogSlice;

typedef struct
{
   TheWorkbenchLogInfo log;
   TheWorkbenchLogSlice slices[THE_WORKBENCH_LOG_PAGE_MAX];
   size_t count, bytes;
   uint64_t next_cursor;
   size_t next_offset;
   int more, omitted; /* Requested history was evicted or explicitly cleared. */
} TheWorkbenchLogPage;

/* Session-owned data only: these functions never dispatch, launch or render.
 * IDs follow the workbench ASCII syntax and reserve the.* for built-ins.
 * create is idempotent; removal/recreation obtains a new generation. */
TheWorkbenchLogResult the_workbench_log_create(const char *id);
TheWorkbenchLogResult the_workbench_log_remove(const char *id, uint64_t generation);
TheWorkbenchLogResult the_workbench_log_clear(const char *id, uint64_t generation);
size_t the_workbench_log_count(void);
size_t the_workbench_log_bytes(void);
const TheWorkbenchLogInfo *the_workbench_log_at(size_t index);
const TheWorkbenchLogInfo *the_workbench_log_find(const char *id);

/* Append one already decoded, complete UTF-8 text event. Stream decoding and
 * job admission belong to the caller. Job/sequence must be nonzero and are
 * preserved verbatim; cursor is a separate model-local paging identity.
 * Control characters, including NUL, are data: renderers must escape them and
 * use lengths, never terminal/HTML interpretation or strlen. Invalid/incomplete
 * UTF-8 is rejected without mutation. This is not an exact-byte file sink.
 * Oldest text/records may be evicted; even an oversized event is validated in
 * full before retaining its UTF-8-aligned tail. Empty text is a no-op. */
TheWorkbenchLogResult the_workbench_log_append(const char *id, uint64_t generation,
   uint64_t job, int stream, uint64_t sequence, const void *text, size_t length);

/* Inclusive cursor plus original-event byte offset. cursor=0 starts at the
 * oldest retained text and requires offset=0. Continue with next_cursor/offset.
 * Record and byte budgets are required (1..8 and 1..256 KiB respectively).
 * Slices end at scalar boundaries; an insufficient budget for the first scalar
 * returns LIMIT. A stale history cursor resumes at retained text with omitted=1.
 * All returned pointers expire on the next log mutation. Results are untouched
 * on failure. Clear records its discarded history in lifetime omission totals;
 * cursors and counters remain monotonic until removal/reset. */
TheWorkbenchLogResult the_workbench_log_page(const char *id, uint64_t generation,
   uint64_t cursor, size_t offset, size_t records, size_t bytes,
   TheWorkbenchLogPage *page);
void the_workbench_log_reset(void);

#endif
