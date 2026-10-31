#include "workbenchlog.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t create(const char *id)
{
   assert(the_workbench_log_create(id) == THE_WORKBENCH_LOG_OK);
   assert(the_workbench_log_find(id));
   return the_workbench_log_find(id)->generation;
}

static TheWorkbenchLogInfo info(const char *id)
{
   const TheWorkbenchLogInfo *found = the_workbench_log_find(id);
   assert(found);
   return *found;
}

static void same(const char *id, const TheWorkbenchLogInfo *before)
{
   TheWorkbenchLogInfo after = info(id);
   assert(!memcmp(before, &after, sizeof(after)));
}

static void lifecycle(void)
{
   const char *bad[] = {"", "1bad", "a/b", "a b", "the.private", "a\n", "\xc3\xa9"};
   char id[THE_WORKBENCH_LOG_ID_MAX + 2];
   uint64_t original, recreated;
   size_t i;
   the_workbench_log_reset();
   assert(!the_workbench_log_count() && !the_workbench_log_bytes());
   assert(!the_workbench_log_at(0) && !the_workbench_log_find(NULL));
   assert(the_workbench_log_create(NULL) == THE_WORKBENCH_LOG_INVALID);
   for (i = 0; i < sizeof(bad) / sizeof(*bad); i++)
      assert(the_workbench_log_create(bad[i]) == THE_WORKBENCH_LOG_INVALID);
   memset(id, 'a', sizeof(id));
   id[sizeof(id) - 1] = 0;
   assert(the_workbench_log_create(id) == THE_WORKBENCH_LOG_INVALID);
   id[THE_WORKBENCH_LOG_ID_MAX] = 0;
   create(id);
   original = create("user.output");
   assert(create("user.output") == original);
   create("User.output");
   assert(the_workbench_log_count() == 3);
   assert(the_workbench_log_remove("user.output", 0) == THE_WORKBENCH_LOG_INVALID);
   assert(the_workbench_log_remove("user.output", original + 100) == THE_WORKBENCH_LOG_STALE);
   assert(the_workbench_log_remove("missing", original) == THE_WORKBENCH_LOG_MISSING);
   assert(the_workbench_log_remove("user.output", original) == THE_WORKBENCH_LOG_OK);
   assert(!the_workbench_log_find("user.output"));
   assert(the_workbench_log_find("User.output"));
   recreated = create("user.output");
   assert(recreated != original);
   assert(the_workbench_log_append("user.output", original, 1, 1, 1, "x", 1) == THE_WORKBENCH_LOG_STALE);
   the_workbench_log_reset();
   assert(create("user.output") != recreated);
}

static void admission(void)
{
   static const unsigned char controls[] = {'x', 0, '\n', '\t', 27, '[', 'm', 127};
   static const struct { const char *text; size_t length; } invalid[] = {
      {"\x80", 1}, {"\xc0\xaf", 2}, {"\xc2", 1}, {"\xe2\x82", 2},
      {"\xf0\x9f\x98", 3}, {"\xe2(\xa1", 3}, {"\xe0\x80\x80", 3},
      {"\xed\xa0\x80", 3}, {"\xf0\x80\x80\x80", 4},
      {"\xf4\x90\x80\x80", 4}, {"\xf5\x80\x80\x80", 4}
   };
   uint64_t gen;
   size_t i;
   TheWorkbenchLogInfo before;
   TheWorkbenchLogPage page;
   the_workbench_log_reset();
   gen = create("user.output");
   assert(the_workbench_log_append("user.output", gen, 17, 2, 100,
                                   "Zo\xc3\xab\xf0\x9f\x98\x80", 8) == THE_WORKBENCH_LOG_OK);
   assert(the_workbench_log_append("user.output", gen, 18, 1, 2,
                                   controls, sizeof(controls)) == THE_WORKBENCH_LOG_OK);
   before = info("user.output");
   for (i = 0; i < sizeof(invalid) / sizeof(*invalid); i++)
   {
      assert(the_workbench_log_append("user.output", gen, 19, 1, 3,
               invalid[i].text, invalid[i].length) == THE_WORKBENCH_LOG_INVALID);
      same("user.output", &before);
   }
   assert(the_workbench_log_append("user.output", gen, 0, 1, 1, "x", 1) == THE_WORKBENCH_LOG_INVALID);
   assert(the_workbench_log_append("user.output", gen, 1, 0, 1, "x", 1) == THE_WORKBENCH_LOG_INVALID);
   assert(the_workbench_log_append("user.output", gen, 1, 3, 1, "x", 1) == THE_WORKBENCH_LOG_INVALID);
   assert(the_workbench_log_append("user.output", gen, 1, 1, 0, "x", 1) == THE_WORKBENCH_LOG_INVALID);
   assert(the_workbench_log_append("user.output", gen, 1, 1, 1, NULL, 1) == THE_WORKBENCH_LOG_INVALID);
   assert(the_workbench_log_append("user.output", gen, 1, 1, 1,
                                   "x", THE_WORKBENCH_LOG_APPEND_MAX + 1u) == THE_WORKBENCH_LOG_LIMIT);
   assert(the_workbench_log_append("user.output", gen, 1, 1, 1, NULL, 0) == THE_WORKBENCH_LOG_OK);
   same("user.output", &before);
   assert(the_workbench_log_page("user.output", gen, 0, 0, 8, 100, &page) == THE_WORKBENCH_LOG_OK);
   assert(page.count == 2 && page.bytes == 16 && !page.more && !page.omitted);
   assert(page.slices[0].job == 17 && page.slices[0].stream == 2 && page.slices[0].sequence == 100);
   assert(page.slices[1].job == 18 && page.slices[1].stream == 1 && page.slices[1].sequence == 2);
   assert(!memcmp(page.slices[1].text, controls, sizeof(controls)));
   assert(page.slices[0].cursor == 1 && page.slices[1].cursor == 2);
   assert(page.next_cursor == 3 && !page.next_offset);
   assert(page.log.accepted_bytes == 16 && page.log.accepted_records == 2);
}

static void paging(void)
{
   const char text[] = "A\xe2\x82\xac" "B";
   char collected[100], wanted[100];
   TheWorkbenchLogPage page, before;
   uint64_t gen, cursor = 0;
   size_t i, offset = 0, used = 0, iterations = 0;
   the_workbench_log_reset();
   gen = create("user.paged");
   for (i = 0; i < 10; i++)
   {
      assert(the_workbench_log_append("user.paged", gen, 20 + i, (int)(i % 2 + 1),
                                      10 + i, text, sizeof(text) - 1) == THE_WORKBENCH_LOG_OK);
      memcpy(wanted + i * 5, text, 5);
   }
   do
   {
      assert(++iterations < 50);
      assert(the_workbench_log_page("user.paged", gen, cursor, offset, 2, 4, &page) == THE_WORKBENCH_LOG_OK);
      assert(page.count <= 2 && page.bytes <= 4 && !page.omitted);
      for (i = 0; i < page.count; i++)
      {
         const TheWorkbenchLogSlice *slice = page.slices + i;
         assert(slice->original_length == 5 && slice->offset + slice->length <= 5);
         assert(slice->job == 19 + slice->cursor && slice->sequence == 9 + slice->cursor);
         assert(slice->stream == (int)((slice->cursor - 1) % 2 + 1));
         memcpy(collected + used, slice->text, slice->length);
         used += slice->length;
      }
      cursor = page.next_cursor;
      offset = page.next_offset;
   } while (page.more);
   assert(used == 50 && !memcmp(collected, wanted, used));
   assert(the_workbench_log_page("user.paged", gen, cursor, 0, 8, 100, &page) == THE_WORKBENCH_LOG_OK);
   assert(!page.count && !page.more && page.next_cursor == 11);
   assert(the_workbench_log_page("user.paged", gen, 1, 1, 8, 3, &page) == THE_WORKBENCH_LOG_OK);
   assert(page.count == 1 && page.bytes == 3 && page.next_cursor == 1 && page.next_offset == 4);
   before = page;
   assert(the_workbench_log_page("user.paged", gen, 1, 1, 8, 2, &page) == THE_WORKBENCH_LOG_LIMIT);
   assert(!memcmp(&before, &page, sizeof(page)));
   assert(the_workbench_log_page("user.paged", gen, 1, 2, 8, 100, &page) == THE_WORKBENCH_LOG_INVALID);
   assert(the_workbench_log_page("user.paged", gen, 1, 6, 8, 100, &page) == THE_WORKBENCH_LOG_INVALID);
   assert(the_workbench_log_page("user.paged", gen, 0, 1, 8, 100, &page) == THE_WORKBENCH_LOG_INVALID);
   assert(the_workbench_log_page("user.paged", gen, 12, 0, 8, 100, &page) == THE_WORKBENCH_LOG_INVALID);
   assert(the_workbench_log_page("user.paged", gen, 0, 0, 0, 100, &page) == THE_WORKBENCH_LOG_INVALID);
   assert(the_workbench_log_page("user.paged", gen, 0, 0, 9, 100, &page) == THE_WORKBENCH_LOG_INVALID);
   assert(the_workbench_log_page("user.paged", gen, 0, 0, 8, 0, &page) == THE_WORKBENCH_LOG_INVALID);
   assert(the_workbench_log_page("user.paged", gen, 0, 0, 8,
                                THE_WORKBENCH_LOG_TEXT_MAX + 1u, &page) == THE_WORKBENCH_LOG_INVALID);
   assert(the_workbench_log_page("user.paged", gen + 100, 0, 0, 8, 100, &page) == THE_WORKBENCH_LOG_STALE);
   assert(the_workbench_log_page("user.paged", gen, 0, 0, 8, 100, NULL) == THE_WORKBENCH_LOG_INVALID);
   assert(!memcmp(&before, &page, sizeof(page)));
}

static void retention(void)
{
   const size_t max = THE_WORKBENCH_LOG_TEXT_MAX;
   char *large = malloc(max + 2);
   uint64_t gen;
   size_t i;
   TheWorkbenchLogInfo state;
   TheWorkbenchLogPage page;
   assert(large);
   memset(large, 'a', max + 2);
   memcpy(large, "\xe2\x82\xac", 3);
   the_workbench_log_reset();
   gen = create("user.retained");
   assert(the_workbench_log_append("user.retained", gen, 7, 1, 11, large, max + 2) == THE_WORKBENCH_LOG_OK);
   state = info("user.retained");
   assert(state.bytes == max - 1 && state.omitted_bytes == 3 && !state.omitted_records);
   assert(state.accepted_bytes == max + 2 && state.records == 1);
   assert(the_workbench_log_page("user.retained", gen, 1, 0, 1, 10, &page) == THE_WORKBENCH_LOG_OK);
   assert(page.omitted && page.slices[0].offset == 3 && page.slices[0].original_length == max + 2);
   assert(page.slices[0].job == 7 && page.slices[0].sequence == 11 && page.slices[0].text[0] == 'a');
   assert(the_workbench_log_clear("user.retained", gen) == THE_WORKBENCH_LOG_OK);
   state = info("user.retained");
   assert(!state.bytes && !state.records && state.omitted_bytes == max + 2 && state.omitted_records == 1);
   assert(state.first_cursor == 2 && state.next_cursor == 2);
   assert(the_workbench_log_page("user.retained", gen, 1, 0, 8, 100, &page) == THE_WORKBENCH_LOG_OK);
   assert(page.omitted && !page.count && page.next_cursor == 2);
   assert(the_workbench_log_clear("user.retained", gen) == THE_WORKBENCH_LOG_OK);
   same("user.retained", &state);
   assert(the_workbench_log_remove("user.retained", gen) == THE_WORKBENCH_LOG_OK);
   gen = create("user.retained");
   assert(the_workbench_log_append("user.retained", gen, 9, 2, 22, large, max) == THE_WORKBENCH_LOG_OK);
   assert(the_workbench_log_append("user.retained", gen, 9, 1, 23, "b", 1) == THE_WORKBENCH_LOG_OK);
   state = info("user.retained");
   assert(state.bytes == max - 2 && state.omitted_bytes == 3 && !state.omitted_records);
   assert(state.records == 2);
   assert(the_workbench_log_page("user.retained", gen, 1, 0, 8, 5, &page) == THE_WORKBENCH_LOG_OK);
   assert(page.omitted && page.slices[0].offset == 3 && page.slices[0].original_length == max);
   assert(the_workbench_log_clear("user.retained", gen) == THE_WORKBENCH_LOG_OK);
   assert(the_workbench_log_page("user.retained", gen, 1, 15, 8, 100, &page) == THE_WORKBENCH_LOG_OK);
   assert(page.omitted && !page.count && !page.next_offset);
   for (i = 0; i < THE_WORKBENCH_LOG_RECORD_MAX + 1u; i++)
      assert(the_workbench_log_append("user.retained", gen, 10, 1, 1 + i, "z", 1) == THE_WORKBENCH_LOG_OK);
   state = info("user.retained");
   assert(state.bytes == THE_WORKBENCH_LOG_RECORD_MAX && state.records == THE_WORKBENCH_LOG_RECORD_MAX);
   assert(state.omitted_bytes == max + 2 && state.omitted_records == 3);
   assert(the_workbench_log_page("user.retained", gen, 3, 0, 8, 100, &page) == THE_WORKBENCH_LOG_OK);
   assert(page.omitted && page.slices[0].sequence == 2 && page.slices[0].cursor == 4);
   free(large);
}

static void session_bound(void)
{
   char id[40];
   char *text = malloc(THE_WORKBENCH_LOG_TEXT_MAX);
   size_t i;
   uint64_t gen;
   TheWorkbenchLogPage page;
   assert(text);
   memset(text, 'x', THE_WORKBENCH_LOG_TEXT_MAX);
   the_workbench_log_reset();
   for (i = 0; i < THE_WORKBENCH_LOG_MAX; i++)
   {
      snprintf(id, sizeof(id), "user.log%u", (unsigned)i);
      gen = create(id);
      assert(the_workbench_log_append(id, gen, i + 1, 1, 1, text,
                                     THE_WORKBENCH_LOG_TEXT_MAX) == THE_WORKBENCH_LOG_OK);
      assert(the_workbench_log_bytes() == (i + 1) * THE_WORKBENCH_LOG_TEXT_MAX);
   }
   assert(the_workbench_log_count() == 32);
   assert(the_workbench_log_bytes() == THE_WORKBENCH_LOG_SESSION_TEXT_MAX);
   assert(the_workbench_log_create("user.over") == THE_WORKBENCH_LOG_LIMIT);
   gen = info("user.log0").generation;
   assert(the_workbench_log_append("user.log0", gen, 1, 2, 2, "y", 1) == THE_WORKBENCH_LOG_OK);
   assert(the_workbench_log_bytes() == THE_WORKBENCH_LOG_SESSION_TEXT_MAX);
   gen = info("user.log1").generation;
   assert(the_workbench_log_page("user.log1", gen, 0, 0, 1, 10, &page) == THE_WORKBENCH_LOG_OK);
   assert(!page.log.omitted_bytes && page.slices[0].job == 2 && page.slices[0].stream == 1);
   assert(the_workbench_log_remove("user.log1", gen) == THE_WORKBENCH_LOG_OK);
   assert(the_workbench_log_bytes() == THE_WORKBENCH_LOG_SESSION_TEXT_MAX - THE_WORKBENCH_LOG_TEXT_MAX);
   create("user.replacement");
   assert(the_workbench_log_count() == THE_WORKBENCH_LOG_MAX);
   assert(!the_workbench_log_at(THE_WORKBENCH_LOG_MAX));
   free(text);
   the_workbench_log_reset();
   assert(!the_workbench_log_count() && !the_workbench_log_bytes());
}

int main(void)
{
   lifecycle();
   admission();
   paging();
   retention();
   session_bound();
   puts("Named workbench logs: UTF-8, stream/job/sequence, paging, generations and retention passed");
   return 0;
}
