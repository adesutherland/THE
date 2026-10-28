/* Browser parser state belongs to the editor's C owner. Only queued replies
 * enter here; JavaScript never calls into an executing/suspended editor. */
#include "the.h"
#include "highlighter.h"
#include "editorsource.h"
#include "tools.h"
#include "proto.h"
#include "vars.h"
#include "textpos.h"
#include "panelui.h"
#include "serialization.h"
#include <emscripten.h>
#include <ctype.h>
#include <limits.h>

#define REPLY_LIMIT (8u * 1024u * 1024u)
#define RECORD_LIMIT 200000u

typedef struct {
    size_t pos, length;
    UiSyntaxStyle style;
} HighlightSpan;

typedef struct HighlightDocument {
    FILE_DETAILS *file;
    char image[THE_TOOL_PATH];
    unsigned id, revision, applied, stale;
    char *source;
    size_t length, points, lines;
    size_t *line_bytes, *line_points;
    CB_TokenStream *tokens;
    HighlightSpan *spans;
    size_t span_count;
    const char *state;
    struct HighlightDocument *next;
} HighlightDocument;

static HighlightDocument *documents;
static unsigned next_id;
static int initialized;

EM_JS(void, request_parse, (unsigned id, unsigned revision, const char *source, size_t bytes, size_t lines, const char *image), {
    Module.requestHighlight?.({id, revision, source: HEAPU8.slice(source, source + bytes), lines, image:UTF8ToString(image)});
});
EM_JS(void, forget_parse, (unsigned id), { Module.forgetHighlight?.(id); });

static int eligible(FILE_DETAILS *file)
{
    return file && file->colouring && file->parser && the_tool_for_file((char*)file->fname,"language");
}

static HighlightDocument *document(FILE_DETAILS *file, int create)
{
    HighlightDocument *doc;
    for (doc = documents; doc; doc = doc->next) if (doc->file == file) return doc;
    if (!create) return NULL;
    doc = calloc(1, sizeof(*doc));
    if (!doc) return NULL;
    doc->file = file;
    doc->id = ++next_id;
    doc->state = "pending";
    doc->next = documents;
    documents = doc;
    return doc;
}

static void clear_result(HighlightDocument *doc)
{
    if (doc->tokens) cb_free_token_stream(doc->tokens);
    free(doc->spans);
    doc->tokens = NULL;
    doc->spans = NULL;
    doc->span_count = 0;
    doc->applied = 0;
}

void the_wasm_highlight_forget(FILE_DETAILS *file)
{
    HighlightDocument **link = &documents, *doc;
    while ((doc = *link) != NULL) {
        if (doc->file != file) { link = &doc->next; continue; }
        *link = doc->next;
        forget_parse(doc->id);
        clear_result(doc);
        free(doc->source); free(doc->line_bytes); free(doc->line_points); free(doc);
        return;
    }
}

void the_wasm_highlight_restart(void)
{
    for (HighlightDocument *doc = documents; doc; doc = doc->next) {
        clear_result(doc);
        doc->revision++;
        doc->state = "pending";
        if (doc->source) request_parse(doc->id, doc->revision, doc->source, doc->length, doc->lines,doc->image);
    }
}

/* DSLSH's logical source has LF after every editor record. Include the active
 * working record, which may not yet have been committed to the linked list. */
void the_wasm_highlight_prepare(void)
{
    HighlightDocument *doc;
    const TheEditorSource *owned = the_editor_source_current();
    const TheToolDefinition*tool;
    size_t length = 0, count = 0, offset = 0, points = 0;
    char *source;
    size_t *line_bytes, *line_points;
    if (!CURRENT_VIEW || !eligible(CURRENT_FILE) || !CURRENT_FILE->first_line || the_panel_active || in_macro) return;
    tool=the_tool_for_file((char*)CURRENT_FILE->fname,"language");
    if (!initialized) {
        if (init_parser_thread_utils() != 0) return;
        initialized = 1;
    }
    doc = document(CURRENT_FILE, 1);
    if (!doc) return;
    if (!owned) {
        if (strcmp(doc->state, "refused")) doc->revision++;
        doc->state = "refused"; clear_result(doc);
        free(doc->source); doc->source = NULL; return;
    }
    length = owned->length; count = owned->lines;
    source = malloc(length + 1);
    line_bytes = malloc((count + 1) * sizeof(size_t));
    line_points = malloc((count + 1) * sizeof(size_t));
    if (!source || !line_bytes || !line_points) {
        free(source); free(line_bytes); free(line_points);
        clear_result(doc); doc->state = "error"; doc->revision++;
        free(doc->source); doc->source = NULL; return;
    }
    memcpy(source, owned->bytes, length + 1);
    count = 0;
    while (offset < length) {
        const char *end = memchr(source + offset, '\n', length - offset);
        size_t n = end ? (size_t)(end - source - offset) : length - offset;
        line_bytes[count] = offset; line_points[count++] = points;
        points += textpos_count_codepoints((const CHARTYPE *)source + offset, n) + 1;
        offset += n + 1;
    }
    line_bytes[count] = offset; line_points[count] = points; source[offset] = 0;
    if (!strcmp(doc->image,tool->image) && doc->source && doc->length == length && !memcmp(doc->source, source, length)) {
        free(source); free(line_bytes); free(line_points); return;
    }
    enter_codeblock_critical_section();
    clear_result(doc);
    free(doc->source); free(doc->line_bytes); free(doc->line_points);
    snprintf(doc->image,sizeof(doc->image),"%s",tool->image);
    doc->source = source; doc->length = length; doc->points = points; doc->lines = count;
    doc->line_bytes = line_bytes; doc->line_points = line_points;
    doc->revision++;
    doc->state = "pending";
    if (memchr(source, 0, length)) doc->state = "refused";
    else request_parse(doc->id, doc->revision, source, length, count,doc->image);
    exit_codeblock_critical_section();
}

static UiSyntaxStyle token_style(int type)
{
    switch (type) {
    case LEXER_COMMENT: return UI_SYNTAX_COMMENT;
    case LEXER_STRING_LITERAL: return UI_SYNTAX_STRING;
    case LEXER_NUMBER_LITERAL: return UI_SYNTAX_NUMBER;
    case LEXER_KEYWORD: return UI_SYNTAX_KEYWORD;
    case LEXER_IDENTIFIER: return UI_SYNTAX_IDENTIFIER;
    case LEXER_FUNCTION_IDENTIFIER: return UI_SYNTAX_FUNCTION;
    case LEXER_TYPE_IDENTIFIER: return UI_SYNTAX_TYPE;
    case LEXER_CONSTANT_IDENTIFIER: return UI_SYNTAX_CONSTANT;
    case LEXER_PREPROCESSOR: return UI_SYNTAX_PREPROCESSOR;
    case LEXER_OPERATOR: case LEXER_OPERATOR_ASSIGN:
    case LEXER_OPERATOR_ARITHMETIC: case LEXER_OPERATOR_LOGICAL: return UI_SYNTAX_OPERATOR;
    case LEXER_SEPARATOR: case LEXER_STATEMENT_SEPARATOR: return UI_SYNTAX_PUNCTUATION;
    case LEXER_LH_BLOCK: case LEXER_RH_BLOCK: case LEXER_LH_CODEBLOCK:
    case LEXER_RH_CODEBLOCK: case LEXER_LH_EXPR: case LEXER_RH_EXPR: return UI_SYNTAX_PAREN;
    case LEXER_UNKNOWN: return UI_SYNTAX_UNKNOWN;
    default: return UI_SYNTAX_NONE;
    }
}

/* Admit the bounded wire shape before using the shared DSLSH codec, whose
 * general deserializer assumes a complete token stream. */
static int number(const char **input, size_t maximum, size_t *value, int last)
{
    const char *p = *input;
    size_t n = 0;
    if (!isdigit((unsigned char)*p)) return 0;
    do {
        unsigned digit = *p++ - '0';
        if (n > maximum / 10 || (n == maximum / 10 && digit > maximum % 10)) return 0;
        n = n * 10 + digit;
    } while (isdigit((unsigned char)*p));
    if (*p != (last ? '\0' : '|')) return 0;
    *input = p + !last; *value = n; return 1;
}

static int valid_wire(const char *payload, size_t extent)
{
    const char *p = payload;
    size_t count, type, pos, length, identifier;
    if (!payload || strlen(payload) > REPLY_LIMIT || !number(&p, RECORD_LIMIT, &count, 0) || !count) return 0;
    for (size_t i = 0; i < count; i++) {
        if (!number(&p, 102, &type, 0) || !number(&p, extent, &pos, 0)
            || !number(&p, extent - pos, &length, 0) || !number(&p, INT_MAX, &identifier, 0)) return 0;
        if (!((type >= 33 && type < 70) || type == 71 || type == 100 || type == 101)) return 0;
        if (*p < '0' || *p > '3' || p[1] != '|') return 0;
        p += 2;
        for (int field = 0; field < 2; field++) {
            size_t n = 0;
            while (isxdigit((unsigned char)*p)) { p++; n++; }
            if (n % 2 || n > 2u * 1024u * 1024u) return 0;
            if (field == 0 || i + 1 < count) { if (*p++ != '|') return 0; }
            else if (*p) return 0;
        }
    }
    return *p == 0;
}

void the_wasm_highlight_reply(unsigned id, unsigned revision, const char *payload, int failed)
{
    HighlightDocument *doc;
    CB_TokenStream *tokens = NULL;
    HighlightSpan *spans = NULL;
    size_t cursor = 0, roots = 0, count = 0;
    /* Capture edits applied since the last published frame before comparing. */
    the_wasm_highlight_prepare();
    for (doc = documents; doc && doc->id != id; doc = doc->next) {}
    if (!doc) return;
    if (doc->revision != revision) { doc->stale++; return; }
    enter_codeblock_critical_section();
    clear_result(doc);
    doc->state = failed ? "error" : "refused";
    if (failed || !valid_wire(payload, doc->points)) goto done;
    tokens = cb_deserialize_token_stream(payload);
    if (!tokens) goto done;
    spans = calloc(tokens->count, sizeof(*spans));
    if (!spans) goto done;
    for (size_t i = 0; i < tokens->count; i++) {
        CB_SerializedToken *token = &tokens->tokens[i];
        if (token->type == PARSE_TREE_FILE && token->pos == 0 && token->length == doc->points) roots++;
        if (token->type >= LEXER_WHITESPACE && token->type < PARSE_TREE) {
            if (token->type == LEXER_EOF && token->length == 0 && token->pos == doc->points) continue;
            if (token->pos != cursor || token->length == 0) goto done;
            cursor += token->length;
            UiSyntaxStyle style = token_style(token->type);
            if (style != UI_SYNTAX_NONE) spans[count++] = (HighlightSpan){token->pos, token->length, style};
        }
    }
    if (roots != 1 || cursor != doc->points) goto done;
    doc->tokens = tokens; tokens = NULL;
    doc->spans = spans; spans = NULL;
    doc->span_count = count; doc->applied = revision; doc->state = "ready";
done:
    if (tokens) cb_free_token_stream(tokens);
    free(spans);
    exit_codeblock_critical_section();
}

int the_wasm_highlight_row(FILE_DETAILS *file, UiFrame *frame, size_t index)
{
    HighlightDocument *doc;
    UiFrameRow *row = &frame->row[index];
    size_t start, end, lo = 0, hi;
    if (!eligible(file) || row->role != UI_ROW_FILE) return 0;
    doc = document(file, 0);
    row->style_count = 0; /* Pending/refused results never masquerade as legacy colours. */
    if (!doc || doc->applied != doc->revision || row->line_number < 1 || (size_t)row->line_number > doc->lines) return 1;
    enter_codeblock_critical_section();
    start = doc->line_points[row->line_number - 1];
    end = start + textpos_count_codepoints(row->text, row->text_len);
    hi = doc->span_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (doc->spans[mid].pos + doc->spans[mid].length <= start) lo = mid + 1;
        else hi = mid;
    }
    for (size_t i = lo; i < doc->span_count && doc->spans[i].pos < end; i++) {
        HighlightSpan *span = &doc->spans[i];
        size_t a = span->pos > start ? span->pos : start;
        size_t b = span->pos + span->length < end ? span->pos + span->length : end;
        if (b > a) ui_frame_add_row_style(frame, index, (int)(a - start), (int)(b - a), span->style);
    }
    exit_codeblock_critical_section();
    return 1;
}

int the_wasm_highlight_metadata(char *out, size_t capacity)
{
    HighlightDocument *doc = document(CURRENT_FILE, 0);
    const TheToolDefinition *tool=the_tool_for_file(CURRENT_FILE?(char*)CURRENT_FILE->fname:"","language");
    return snprintf(out, capacity,
        "\"parser\":{\"provider\":\"%s\",\"providerId\":\"%s\",\"state\":\"%s\",\"id\":%u,\"revision\":%u,\"applied\":%u,\"stale\":%u}",
        tool&&!strcmp(tool->id,"classic-highlight")?"Level L Classic Rexx":"DSLSH RXBIN",tool?tool->id:"",
        eligible(CURRENT_FILE) && doc ? doc->state : "inactive", doc ? doc->id : 0,
        doc ? doc->revision : 0, doc ? doc->applied : 0, doc ? doc->stale : 0);
}

short the_wasm_highlight_diagnostics(TheParserDiagnostic **out, int *count)
{
    HighlightDocument *doc = document(CURRENT_FILE, 0);
    size_t capacity = 0;
    *out = NULL; *count = 0;
    if (!eligible(CURRENT_FILE) || !doc || !doc->tokens || doc->applied != doc->revision) return RC_OK;
    enter_codeblock_critical_section();
    for (size_t i = 0; i < doc->tokens->count; i++) {
        CB_SerializedToken *token = &doc->tokens->tokens[i];
        size_t line = 0, column;
        if (token->severity == CB_NONE || !token->message || !*token->message) continue;
        if ((size_t)*count == capacity) {
            capacity = capacity ? capacity * 2 : 8;
            TheParserDiagnostic *next = realloc(*out, capacity * sizeof(**out));
            if (!next) { free(*out); *out = NULL; *count = 0; exit_codeblock_critical_section(); return RC_OUT_OF_MEMORY; }
            *out = next;
        }
        while (line + 1 < doc->lines && doc->line_points[line + 1] <= token->pos) line++;
        column = token->pos - doc->line_points[line];
        size_t bytes = doc->lines ? doc->line_bytes[line + 1] - doc->line_bytes[line] - 1 : 0;
        TextPos position = textpos_from_codepoint((CHARTYPE *)doc->source + doc->line_bytes[line], bytes, column);
        TheParserDiagnostic *diagnostic = &(*out)[(*count)++];
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->line = line + 1; diagnostic->column = position.cell_column + 1;
        snprintf(diagnostic->severity, sizeof(diagnostic->severity), "%s", token->severity == CB_ERROR ? "ERROR" : token->severity == CB_WARNING ? "WARNING" : "INFORMATION");
        snprintf(diagnostic->code, sizeof(diagnostic->code), "%s", token->message_code ? token->message_code : "");
        snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", token->message);
    }
    exit_codeblock_critical_section();
    return RC_OK;
}
