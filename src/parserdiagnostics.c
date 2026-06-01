#include "parserdiagnostics.h"

#include <string.h>

#include "the.h"
#include "proto.h"

#ifdef USE_SDSLH
typedef struct ParserDiagnosticsCollectContext
{
   CodeBuffer *cb;
   TheParserDiagnostic **diagnostics;
   int *count;
   int *capacity;
   short rc;
} ParserDiagnosticsCollectContext;

static const char *parser_diagnostics_severity_name(CB_Severity severity)
{
   switch(severity)
   {
      case CB_INFORMATION:
         return "INFORMATION";
      case CB_WARNING:
         return "WARNING";
      case CB_ERROR:
         return "ERROR";
      case CB_NONE:
      default:
         return "NONE";
   }
}

static void parser_diagnostics_copy_field(char *dest, size_t dest_len,
                                          const char *value,
                                          const char *fallback)
{
   const char *source;
   size_t len;

   if (dest == NULL || dest_len == 0)
      return;
   source = (value != NULL && value[0] != '\0') ? value : fallback;
   if (source == NULL)
      source = "";
   len = strlen(source);
   if (len >= dest_len)
      len = dest_len - 1;
   if (len > 0)
      memcpy(dest, source, len);
   dest[len] = '\0';
}

static short parser_diagnostics_append(TheParserDiagnostic **diagnostics,
                                       int *count, int *capacity,
                                       const CB_Node *node,
                                       LINETYPE line, LENGTHTYPE column)
{
   TheParserDiagnostic *resized = NULL;
   TheParserDiagnostic *entry = NULL;
   int next_capacity;

   if (*count == *capacity)
   {
      next_capacity = (*capacity == 0) ? 8 : (*capacity * 2);
      if (*diagnostics == NULL)
         resized = (TheParserDiagnostic *)(*the_malloc)(
            next_capacity * sizeof(TheParserDiagnostic));
      else
         resized = (TheParserDiagnostic *)(*the_realloc)(
            *diagnostics, next_capacity * sizeof(TheParserDiagnostic));

      if (resized == NULL)
         return RC_SYSTEM_ERROR;

      *diagnostics = resized;
      *capacity = next_capacity;
   }

   entry = &(*diagnostics)[*count];
   memset(entry, 0, sizeof(*entry));
   entry->line = line;
   entry->column = column;
   parser_diagnostics_copy_field(
      entry->severity, sizeof(entry->severity),
      parser_diagnostics_severity_name(node->severity), "NONE");
   parser_diagnostics_copy_field(entry->code, sizeof(entry->code),
                                 node->message_code, "-");
   parser_diagnostics_copy_field(entry->message, sizeof(entry->message),
                                 node->message, "");
   (*count)++;
   return RC_OK;
}

static void parser_diagnostics_node_location(CodeBuffer *cb,
                                             const CB_Node *node,
                                             LINETYPE *line,
                                             LENGTHTYPE *column)
{
   size_t found_line = 0;
   size_t found_col = 0;
   size_t current_pos = 0;
   size_t line_idx;
   size_t length;
   size_t offset;

   *line = 0;
   *column = 0;

   if (cb == NULL || node == NULL || cb->line_count == 0)
      return;

   length = (node->length > 0) ? node->length : 1;
   if (get_code_buffer_part(cb, node->pos, length, &found_line, &found_col,
                            NULL) != NULL)
   {
      *line = (LINETYPE)found_line;
      *column = (LENGTHTYPE)found_col + 1;
      return;
   }

   for (line_idx = 0; line_idx < cb->line_count; line_idx++)
   {
      size_t line_length = cb->lines[line_idx].length;
      size_t span = line_length + 1;

      if (node->pos >= current_pos && node->pos <= current_pos + span)
      {
         offset = node->pos - current_pos;
         if (offset > line_length)
            offset = line_length;
         *line = (LINETYPE)line_idx + 1;
         *column = (LENGTHTYPE)offset + 1;
         return;
      }
      current_pos += span;
   }

   *line = (LINETYPE)cb->line_count;
   *column = (LENGTHTYPE)cb->lines[cb->line_count - 1].length + 1;
}

static void parser_diagnostics_collect_node(CB_Node *node, size_t depth,
                                            void *user_data)
{
   ParserDiagnosticsCollectContext *context =
      (ParserDiagnosticsCollectContext *)user_data;
   LINETYPE line = 0;
   LENGTHTYPE column = 0;

   INTENTIONALLY_UNUSED_VARIABLE(depth);

   if (context == NULL || context->rc != RC_OK || node == NULL)
      return;

   if (node->message == NULL
   ||  node->message[0] == '\0'
   ||  node->severity == CB_NONE)
      return;

   parser_diagnostics_node_location(context->cb, node, &line, &column);
   context->rc = parser_diagnostics_append(
      context->diagnostics, context->count, context->capacity,
      node, line, column);
}
#endif

short the_parser_diagnostics_collect(TheParserDiagnostic **diagnostics,
                                     int *count)
{
#ifdef USE_SDSLH
   short rc = RC_OK;
   int capacity = 0;
   CodeBuffer *cb = NULL;
   ParserDiagnosticsCollectContext context;
#endif

   if (diagnostics == NULL || count == NULL)
      return RC_INVALID_OPERAND;
   *diagnostics = NULL;
   *count = 0;

#ifndef USE_SDSLH
   return RC_OK;
#else
   if (CURRENT_FILE == NULL || CURRENT_FILE->cb == NULL)
      return RC_OK;

   if (enter_codeblock_critical_section() != 0)
      return RC_SYSTEM_ERROR;

   cb = CURRENT_FILE->cb;
   if (cb->parse_tree != NULL)
   {
      context.cb = cb;
      context.diagnostics = diagnostics;
      context.count = count;
      context.capacity = &capacity;
      context.rc = RC_OK;
      cb_walk_tree_top_down(cb->parse_tree, parser_diagnostics_collect_node,
                            &context);
      rc = context.rc;
   }

   if (exit_codeblock_critical_section() != 0 && rc == RC_OK)
      rc = RC_SYSTEM_ERROR;

   if (rc != RC_OK)
   {
      the_parser_diagnostics_free(*diagnostics);
      *diagnostics = NULL;
      *count = 0;
   }

   return rc;
#endif
}

void the_parser_diagnostics_free(TheParserDiagnostic *diagnostics)
{
   if (diagnostics != NULL)
      (*the_free)(diagnostics);
}
