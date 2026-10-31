#include "workbenchlog.h"

#include <stdlib.h>
#include <string.h>

typedef struct
{
   uint64_t cursor, job, sequence;
   int stream;
   char *text;
   size_t length, prefix;
} LogRecord;

typedef struct
{
   TheWorkbenchLogInfo info;
   LogRecord records[THE_WORKBENCH_LOG_RECORD_MAX];
} Log;

static Log logs[THE_WORKBENCH_LOG_MAX];
static size_t log_count;
static uint64_t generation_serial;

static int letter(unsigned char c)
{
   return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static int valid_id(const char *id)
{
   size_t i;
   if (!id || !letter((unsigned char)id[0])) return 0;
   for (i = 1; id[i]; i++)
      if (i >= THE_WORKBENCH_LOG_ID_MAX ||
          !(letter((unsigned char)id[i]) || (id[i] >= '0' && id[i] <= '9') ||
            id[i] == '.' || id[i] == '_' || id[i] == '-')) return 0;
   return strncmp(id, "the.", 4) != 0;
}

static Log *find(const char *id)
{
   size_t i;
   if (!id) return NULL;
   for (i = 0; i < log_count; i++)
      if (!strcmp(logs[i].info.id, id)) return logs + i;
   return NULL;
}

static TheWorkbenchLogResult select_log(const char *id, uint64_t generation, Log **log)
{
   if (!valid_id(id) || !generation) return THE_WORKBENCH_LOG_INVALID;
   *log = find(id);
   if (!*log) return THE_WORKBENCH_LOG_MISSING;
   if ((*log)->info.generation != generation) return THE_WORKBENCH_LOG_STALE;
   return THE_WORKBENCH_LOG_OK;
}

static int continuation(unsigned char c) { return (c & 0xc0) == 0x80; }

static int utf8(const unsigned char *text, size_t length)
{
   size_t i = 0;
   while (i < length)
   {
      unsigned c = text[i], value;
      size_t width, j;
      if (c < 128) { i++; continue; }
      width = c >= 0xc2 && c <= 0xdf ? 2 :
              c >= 0xe0 && c <= 0xef ? 3 :
              c >= 0xf0 && c <= 0xf4 ? 4 : 0;
      if (!width || length - i < width) return 0;
      value = c & ((1u << (7 - width)) - 1u);
      for (j = 1; j < width; j++)
      {
         if (!continuation(text[i + j])) return 0;
         value = (value << 6) | (text[i + j] & 63);
      }
      if (value < ((unsigned[]){0, 0, 128, 2048, 65536})[width] ||
          value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return 0;
      i += width;
   }
   return 1;
}

static void first_cursor(Log *log)
{
   log->info.first_cursor = log->info.records ? log->records[0].cursor : log->info.next_cursor;
}

static void discard_first(Log *log)
{
   LogRecord *record = log->records;
   log->info.omitted_bytes += record->length;
   log->info.omitted_records++;
   log->info.bytes -= record->length;
   free(record->text);
   memmove(log->records, log->records + 1,
           (--log->info.records) * sizeof(*log->records));
}

size_t the_workbench_log_count(void) { return log_count; }

size_t the_workbench_log_bytes(void)
{
   size_t i, bytes = 0;
   for (i = 0; i < log_count; i++) bytes += logs[i].info.bytes;
   return bytes;
}

const TheWorkbenchLogInfo *the_workbench_log_at(size_t index)
{
   return index < log_count ? &logs[index].info : NULL;
}

const TheWorkbenchLogInfo *the_workbench_log_find(const char *id)
{
   Log *log = find(id);
   return log ? &log->info : NULL;
}

TheWorkbenchLogResult the_workbench_log_create(const char *id)
{
   Log *log;
   if (!valid_id(id)) return THE_WORKBENCH_LOG_INVALID;
   if (find(id)) return THE_WORKBENCH_LOG_OK;
   if (log_count == THE_WORKBENCH_LOG_MAX || generation_serial == UINT64_MAX)
      return THE_WORKBENCH_LOG_LIMIT;
   log = logs + log_count++;
   memset(log, 0, sizeof(*log));
   strcpy(log->info.id, id);
   log->info.generation = ++generation_serial;
   log->info.revision = 1;
   log->info.first_cursor = log->info.next_cursor = 1;
   return THE_WORKBENCH_LOG_OK;
}

TheWorkbenchLogResult the_workbench_log_remove(const char *id, uint64_t generation)
{
   Log *log;
   size_t i;
   TheWorkbenchLogResult result = select_log(id, generation, &log);
   if (result != THE_WORKBENCH_LOG_OK) return result;
   for (i = 0; i < log->info.records; i++) free(log->records[i].text);
   memmove(log, log + 1, (logs + --log_count - log) * sizeof(*log));
   memset(logs + log_count, 0, sizeof(*log));
   return THE_WORKBENCH_LOG_OK;
}

TheWorkbenchLogResult the_workbench_log_clear(const char *id, uint64_t generation)
{
   Log *log;
   TheWorkbenchLogResult result = select_log(id, generation, &log);
   if (result != THE_WORKBENCH_LOG_OK) return result;
   if (log->info.revision == UINT64_MAX) return THE_WORKBENCH_LOG_LIMIT;
   if (!log->info.records) return THE_WORKBENCH_LOG_OK;
   while (log->info.records) discard_first(log);
   first_cursor(log);
   log->info.revision++;
   return THE_WORKBENCH_LOG_OK;
}

TheWorkbenchLogResult the_workbench_log_append(const char *id, uint64_t generation,
   uint64_t job, int stream, uint64_t sequence, const void *text, size_t length)
{
   Log *log;
   LogRecord *record;
   const unsigned char *input = text;
   char *owned;
   size_t prefix, retained;
   TheWorkbenchLogResult result = select_log(id, generation, &log);
   if (result != THE_WORKBENCH_LOG_OK) return result;
   if (!job || !sequence || (stream != 1 && stream != 2) || (!text && length))
      return THE_WORKBENCH_LOG_INVALID;
   if (length > THE_WORKBENCH_LOG_APPEND_MAX) return THE_WORKBENCH_LOG_LIMIT;
   if (!utf8(input, length)) return THE_WORKBENCH_LOG_INVALID;
   if (!length) return THE_WORKBENCH_LOG_OK;
   if (log->info.revision == UINT64_MAX || log->info.next_cursor == UINT64_MAX ||
       log->info.accepted_bytes > UINT64_MAX - length ||
       log->info.accepted_records == UINT64_MAX) return THE_WORKBENCH_LOG_LIMIT;
   prefix = length > THE_WORKBENCH_LOG_TEXT_MAX ? length - THE_WORKBENCH_LOG_TEXT_MAX : 0;
   while (prefix < length && continuation(input[prefix])) prefix++;
   retained = length - prefix;
   owned = malloc(retained + 1);
   if (!owned) return THE_WORKBENCH_LOG_NOMEM;
   memcpy(owned, input + prefix, retained);
   owned[retained] = 0;
   /* Allocation and complete input validation precede any eviction. */
   if (log->info.records == THE_WORKBENCH_LOG_RECORD_MAX) discard_first(log);
   while (log->info.bytes + retained > THE_WORKBENCH_LOG_TEXT_MAX)
   {
      LogRecord *first = log->records;
      size_t trim = log->info.bytes + retained - THE_WORKBENCH_LOG_TEXT_MAX;
      if (trim >= first->length) discard_first(log);
      else
      {
         while (trim < first->length && continuation((unsigned char)first->text[trim])) trim++;
         memmove(first->text, first->text + trim, first->length - trim + 1);
         first->length -= trim;
         first->prefix += trim;
         log->info.bytes -= trim;
         log->info.omitted_bytes += trim;
      }
   }
   record = log->records + log->info.records++;
   record->cursor = log->info.next_cursor++;
   record->job = job;
   record->stream = stream;
   record->sequence = sequence;
   record->text = owned;
   record->length = retained;
   record->prefix = prefix;
   log->info.bytes += retained;
   log->info.accepted_bytes += length;
   log->info.accepted_records++;
   log->info.omitted_bytes += prefix;
   log->info.revision++;
   first_cursor(log);
   return THE_WORKBENCH_LOG_OK;
}

TheWorkbenchLogResult the_workbench_log_page(const char *id, uint64_t generation,
   uint64_t cursor, size_t offset, size_t records, size_t bytes,
   TheWorkbenchLogPage *page)
{
   Log *log;
   TheWorkbenchLogPage out;
   size_t index = 0;
   TheWorkbenchLogResult result = select_log(id, generation, &log);
   if (result != THE_WORKBENCH_LOG_OK) return result;
   if (!page || !records || records > THE_WORKBENCH_LOG_PAGE_MAX || !bytes ||
       bytes > THE_WORKBENCH_LOG_TEXT_MAX || (!cursor && offset) ||
       cursor > log->info.next_cursor) return THE_WORKBENCH_LOG_INVALID;
   memset(&out, 0, sizeof(out));
   out.log = log->info;
   if (cursor)
   {
      while (index < log->info.records && log->records[index].cursor < cursor) index++;
      out.omitted = cursor < log->info.first_cursor;
   }
   if (index < log->info.records)
   {
      LogRecord *record = log->records + index;
      if (!cursor || record->cursor != cursor) offset = record->prefix;
      else if (offset < record->prefix) { offset = record->prefix; out.omitted = 1; }
      if (offset > record->prefix + record->length ||
          (offset < record->prefix + record->length &&
           continuation((unsigned char)record->text[offset - record->prefix])))
         return THE_WORKBENCH_LOG_INVALID;
      if (offset == record->prefix + record->length)
      {
         index++;
         offset = index < log->info.records ? log->records[index].prefix : 0;
      }
   }
   else if (offset && !out.omitted) return THE_WORKBENCH_LOG_INVALID;
   while (index < log->info.records && out.count < records && out.bytes < bytes)
   {
      LogRecord *record = log->records + index;
      TheWorkbenchLogSlice *slice = out.slices + out.count;
      size_t relative = offset - record->prefix;
      size_t available = record->length - relative;
      size_t take = available < bytes - out.bytes ? available : bytes - out.bytes;
      while (take < available && take &&
             continuation((unsigned char)record->text[relative + take])) take--;
      if (!take)
      {
         if (!out.count) return THE_WORKBENCH_LOG_LIMIT;
         break;
      }
      slice->cursor = record->cursor;
      slice->job = record->job;
      slice->stream = record->stream;
      slice->sequence = record->sequence;
      slice->offset = offset;
      slice->original_length = record->prefix + record->length;
      slice->text = record->text + relative;
      slice->length = take;
      out.bytes += take;
      out.count++;
      if (take < available) { offset += take; break; }
      index++;
      offset = index < log->info.records ? log->records[index].prefix : 0;
   }
   out.more = index < log->info.records;
   out.next_cursor = out.more ? log->records[index].cursor : log->info.next_cursor;
   out.next_offset = out.more ? offset : 0;
   *page = out;
   return THE_WORKBENCH_LOG_OK;
}

void the_workbench_log_reset(void)
{
   size_t i, j;
   for (i = 0; i < log_count; i++)
      for (j = 0; j < logs[i].info.records; j++) free(logs[i].records[j].text);
   memset(logs, 0, sizeof(logs));
   log_count = 0;
}
