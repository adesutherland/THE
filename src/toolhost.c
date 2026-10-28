/* THE session state. Adapters own OS/Worker resources, never this model. */
#include "toolhost.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static TheToolDefinition catalog[THE_TOOL_LIMIT];
static size_t catalog_count;
static TheToolConsole console;
static TheToolAdapter backend;
static unsigned serial;
static unsigned char pending[4][4];
static size_t pending_length[4];

static int identifier(const char *s) {
   size_t n = 0;
   if (!s || !*s)
      return 0;
   for (; *s; s++, n++)
      if (n >= 63 || !(isalnum((unsigned char)*s) || *s == '-' || *s == '_'))
         return 0;
   return 1;
}
static void changed(void) { console.version++; }
void the_tool_clear(void) {
   size_t i;
   for (i = 0; i < console.count; i++)
      free(console.chunks[i].text);
   console.count = console.bytes = 0;
   console.omitted = 0;
   changed();
}
void the_tool_host_reset(void) {
   the_tool_stop("Stopped");
   the_tool_clear();
   memset(&console, 0, sizeof(console));
   strcpy(console.state, "Ready");
   memset(&backend, 0, sizeof(backend));
   catalog_count = 0;
   memset(pending_length, 0, sizeof(pending_length));
}
void the_tool_host_adapter(const TheToolAdapter *adapter) {
   if (adapter)
      backend = *adapter;
}
const TheToolConsole *the_tool_console(void) { return &console; }
size_t the_tool_count(void) { return catalog_count; }
const TheToolDefinition *the_tool_at(size_t i) { return i < catalog_count ? &catalog[i] : NULL; }
const TheToolDefinition *the_tool_find(const char *id) {
   size_t i;
   for (i = 0; i < catalog_count; i++)
      if (!strcmp(catalog[i].id, id))
         return &catalog[i];
   return NULL;
}
int the_tool_register(const TheToolDefinition *t) {
   size_t i;
   if (!t || !memchr(t->id, 0, sizeof(t->id)) || !memchr(t->role, 0, sizeof(t->role)) ||
       !memchr(t->label, 0, sizeof(t->label)) || !identifier(t->id) || !*t->label ||
       !memchr(t->image, 0, sizeof(t->image)) || !memchr(t->extensions, 0, sizeof(t->extensions)) ||
       !memchr(t->entry, 0, sizeof(t->entry)))
      return 0;
   if (strcmp(t->role, "program") && strcmp(t->role, "runner") && strcmp(t->role, "profile") &&
       strcmp(t->role, "compiler") && strcmp(t->role, "language"))
      return 0;
   for (i = 0; i < catalog_count; i++)
      if (!strcmp(catalog[i].id, t->id))
         break;
   if (i == THE_TOOL_LIMIT)
      return 0;
   catalog[i] = *t;
   if (i == catalog_count)
      catalog_count++;
   changed();
   return 1;
}
const TheToolDefinition *the_tool_for_file(const char *name, const char *role) {
   const char *ext = strrchr(name ? name : "", '.');
   size_t i;
   if (!ext)
      return NULL;
   for (i = catalog_count; i-- > 0;)
      if (catalog[i].available && !strcmp(catalog[i].role, role)) {
         const char *p = catalog[i].extensions;
         while (*p) {
            const char *end = strchr(p, ',');
            size_t n = end ? (size_t)(end - p) : strlen(p);
            if (strlen(ext) == n) {
               size_t j;
               for (j = 0; j < n && tolower((unsigned char)p[j]) == tolower((unsigned char)ext[j]);
                    j++) {
               }
               if (j == n)
                  return &catalog[i];
            }
            if (!end)
               break;
            p = end + 1;
         }
      }
   return NULL;
}
int the_tool_launch(const TheToolRequest *r, char *error, size_t capacity) {
   TheToolRequest owned;
   size_t total = 0;
   int i;
   if (console.running) {
      snprintf(error, capacity, "Stop the current job first");
      return 0;
   }
   if (!r || !r->tool.available || !backend.start || r->argc < 0 || r->argc > THE_TOOL_ARGS ||
       r->source_length > 2097152) {
      snprintf(error, capacity, "Tool or host capability unavailable");
      return 0;
   }
   for (i = 0; i < r->argc; i++) {
      size_t n = r->argv[i] ? strlen(r->argv[i]) : 1048577;
      if (n > 1048576 - total) {
         snprintf(error, capacity, "Arguments exceed 1 MiB");
         return 0;
      }
      total += n;
   }
   the_tool_clear();
   memset(pending_length, 0, sizeof(pending_length));
   console.id = ++serial;
   console.source_id = r->source_id;
   console.revision = r->revision;
   console.running = 1;
   console.waiting = console.eof = 0;
   console.total = 0;
   console.visible = 1;
   console.input_length = 0;
   console.input[0] = 0;
   console.status[0] = 0;
   snprintf(console.state, sizeof(console.state), "Starting…");
   snprintf(console.filename, sizeof(console.filename), "%s", r->filename ? r->filename : "");
   snprintf(console.tool, sizeof(console.tool), "%s", r->tool.id);
   changed();
   owned = *r;
   owned.id = console.id;
   if (!backend.start(&owned, error, capacity)) {
      console.running = 0;
      snprintf(console.state, sizeof(console.state), "Failed");
      changed();
      return 0;
   }
   return 1;
}
void the_tool_started(unsigned id) {
   if (console.running && console.id == id) {
      strcpy(console.state, "Running");
      changed();
   }
}
void the_tool_waiting(unsigned id, int value) {
   if (console.running && console.id == id) {
      console.waiting = !!value;
      strcpy(console.state, value ? "Waiting for input" : "Running");
      changed();
   }
}
static void append(int fd, const char *text, size_t length) {
   TheToolChunk *last;
   char *bytes;
   if (!length)
      return;
   if (console.count && console.chunks[console.count - 1].fd == fd)
      last = &console.chunks[console.count - 1];
   else {
      if (console.count == 128) {
         console.bytes -= console.chunks[0].length;
         free(console.chunks[0].text);
         memmove(console.chunks, console.chunks + 1, 127 * sizeof(*console.chunks));
         console.count--;
         console.omitted = 1;
      }
      last = &console.chunks[console.count++];
      memset(last, 0, sizeof(*last));
      last->fd = fd;
   }
   bytes = realloc(last->text, last->length + length + 1);
   if (!bytes)
      return;
   last->text = bytes;
   memcpy(bytes + last->length, text, length);
   last->length += length;
   bytes[last->length] = 0;
   console.bytes += length;
   while (console.bytes > THE_TOOL_OUTPUT && console.count) {
      TheToolChunk *first = &console.chunks[0];
      size_t trim = console.bytes - THE_TOOL_OUTPUT;
      if (trim >= first->length) {
         console.bytes -= first->length;
         free(first->text);
         memmove(console.chunks, console.chunks + 1, --console.count * sizeof(*first));
      } else {
         while (trim < first->length && ((unsigned char)first->text[trim] & 0xc0) == 0x80)
            trim++;
         memmove(first->text, first->text + trim, first->length - trim + 1);
         first->length -= trim;
         console.bytes -= trim;
      }
      console.omitted = 1;
   }
   changed();
}
/* Validate complete UTF-8 scalars; preserve partial stdout/stderr separately. */
static void decode(int fd, const unsigned char *s, size_t n, int final) {
   unsigned char *all;
   size_t size = pending_length[fd - 1] + n, i = 0, start = 0;
   all = malloc(size ? size : 1);
   if (!all)
      return;
   memcpy(all, pending[fd - 1], pending_length[fd - 1]);
   if (n)
      memcpy(all + pending_length[fd - 1], s, n);
   pending_length[fd - 1] = 0;
   while (i < size) {
      unsigned c = all[i], value;
      size_t width, j;
      if (c < 128) {
         i++;
         continue;
      }
      width = c >= 0xc2 && c <= 0xdf   ? 2
              : c >= 0xe0 && c <= 0xef ? 3
              : c >= 0xf0 && c <= 0xf4 ? 4
                                       : 0;
      if (width && size - i < width && !final) {
         append(fd > 2 ? fd - 2 : fd, (char *)all + start, i - start);
         pending_length[fd - 1] = size - i;
         memcpy(pending[fd - 1], all + i, size - i);
         free(all);
         return;
      }
      value = width ? c & ((1u << (7 - width)) - 1u) : 0;
      for (j = 1; width && j < width && i + j < size; j++) {
         if ((all[i + j] & 0xc0) != 0x80)
            break;
         value = (value << 6) | (all[i + j] & 63);
      }
      if (width && j == width && value >= ((unsigned[]){0, 0, 128, 2048, 65536})[width] &&
          value <= 0x10ffff && !(value >= 0xd800 && value <= 0xdfff)) {
         i += width;
         continue;
      }
      append(fd > 2 ? fd - 2 : fd, (char *)all + start, i - start);
      append(fd > 2 ? fd - 2 : fd, "\xef\xbf\xbd", 3);
      start = ++i;
   }
   append(fd > 2 ? fd - 2 : fd, (char *)all + start, size - start);
   free(all);
}
void the_tool_output(unsigned id, int fd, const void *s, size_t n) {
   if (!console.running || console.id != id || (fd != 1 && fd != 2))
      return;
   if (n > THE_TOOL_OUTPUT_LIMIT - console.total) {
      the_tool_stop("Output limit reached");
      return;
   }
   console.total += n;
   decode(fd, s, n, 0);
}
void the_tool_profile_output(int fd, const void *bytes, size_t n) {
   if (fd != 1 && fd != 2)
      return;
   if (n > THE_TOOL_OUTPUT) {
      bytes = (const unsigned char *)bytes + n - THE_TOOL_OUTPUT;
      n = THE_TOOL_OUTPUT;
      console.omitted = 1;
   }
   console.visible = 1;
   decode(fd + 2, bytes, n, 1);
   changed();
}
static int field(const char *wire, const char *key, char *out, size_t cap) {
   size_t n = strlen(key);
   const char *p = wire;
   while (p && *p) {
      if (!strncmp(p, key, n) && p[n] == '=') {
         const char *end = strchr(p + n + 1, '\n');
         size_t count = end ? (size_t)(end - p - n - 1) : strlen(p + n + 1);
         if (count >= cap)
            return 0;
         memcpy(out, p + n + 1, count);
         out[count] = 0;
         return 1;
      }
      p = strchr(p, '\n');
      if (p)
         p++;
   }
   return 0;
}
static char *unhex(const char *s) {
   size_t n = strlen(s), i;
   char *out;
   if (n % 2 || n > 131072)
      return NULL;
   out = malloc(n / 2 + 1);
   if (!out)
      return NULL;
   for (i = 0; i < n; i += 2) {
      unsigned a = (unsigned char)s[i], b = (unsigned char)s[i + 1];
      if (!isxdigit(a) || !isxdigit(b)) {
         free(out);
         return NULL;
      }
      a = isdigit(a) ? a - '0' : toupper(a) - 'A' + 10;
      b = isdigit(b) ? b - '0' : toupper(b) - 'A' + 10;
      out[i / 2] = (char)(a * 16 + b);
   }
   out[n / 2] = 0;
   return out;
}
static int valid_llb(const char *wire) {
   const char *names[] = {"indicator",      "hasStatus",         "status",
                          "hasValue",       "valueHex",          "hasPrimary",
                          "primaryNameHex", "primaryMessageHex", "primaryCode",
                          "primaryModule",  "primaryAddress",    "hasCleanup",
                          "cleanupNameHex", "cleanupMessageHex", "cleanupCode",
                          "cleanupModule",  "cleanupAddress",    "truncated"};
   unsigned seen = 0;
   const char *p;
   size_t length = strlen(wire);
   int has_status = -1, primary = -1, normal = -1;
   if (length > 3145728 || length < 6 || strncmp(wire, "LLB1\n", 5) || wire[length - 1] != '\n')
      return 0;
   for (p = wire + 5; *p;) {
      const char *eq = strchr(p, '='), *end = strchr(p, '\n');
      size_t n;
      int i;
      if (!eq || !end || eq > end)
         return 0;
      for (i = 0; i < 18; i++)
         if ((size_t)(eq - p) == strlen(names[i]) && !strncmp(p, names[i], eq - p))
            break;
      if (i == 18 || (seen & (1u << i)))
         return 0;
      seen |= 1u << i;
      n = (size_t)(end - eq - 1);
      eq++;
      if (i == 0) {
         if (n != 1 || !strchr("NREXS", *eq))
            return 0;
         normal = *eq == 'N' || *eq == 'R';
      } else if (i == 1 || i == 3 || i == 5 || i == 11 || i == 17) {
         if (n != 1 || (*eq != '0' && *eq != '1'))
            return 0;
         if (i == 1)
            has_status = *eq - '0';
         if (i == 5)
            primary = *eq - '0';
      } else if (i == 4 || i == 6 || i == 7 || i == 12 || i == 13) {
         size_t j;
         if (n % 2 || n > (i == 4 ? 2097152u : 131072u))
            return 0;
         for (j = 0; j < n; j++)
            if (!isdigit((unsigned char)eq[j]) && (eq[j] < 'A' || eq[j] > 'F'))
               return 0;
      } else {
         size_t j = *eq == '-' ? 1 : 0, digits = n - j;
         const char *limit = j ? "9223372036854775808" : "9223372036854775807";
         if (!digits || digits > 19)
            return 0;
         for (; j < n; j++)
            if (!isdigit((unsigned char)eq[j]))
               return 0;
         if (digits == 19 && strncmp(eq + (*eq == '-'), limit, 19) > 0)
            return 0;
      }
      p = end + 1;
   }
   return seen == ((1u << 18) - 1) && has_status == !primary && normal == !primary;
}
static void result_condition(const char *llb, const char *prefix, const char *label) {
   char key[64], *encoded = malloc(131073), *text;
   if (!encoded)
      return;
   append(2, label, strlen(label));
   snprintf(key, sizeof(key), "%sNameHex", prefix);
   if (field(llb, key, encoded, 131073) && (text = unhex(encoded))) {
      decode(2, (unsigned char *)text, strlen(text), 1);
      free(text);
   }
   append(2, ": ", 2);
   snprintf(key, sizeof(key), "%sMessageHex", prefix);
   if (field(llb, key, encoded, 131073) && (text = unhex(encoded))) {
      decode(2, (unsigned char *)text, strlen(text), 1);
      free(text);
   }
   append(2, "\n", 1);
   free(encoded);
}
void the_tool_finish(unsigned id, int code, const char *status, const char *error,
                     const char *llb) {
   char flag[4], value[32];
   int condition = 0;
   if (!console.running || console.id != id)
      return;
   decode(1, NULL, 0, 1);
   decode(2, NULL, 0, 1);
   if (llb && *llb && !code) {
      if (!valid_llb(llb)) {
         code = -1;
         error = "Invalid interpreter result";
      } else {
         field(llb, "hasPrimary", flag, sizeof(flag));
         condition = !strcmp(flag, "1");
         if (condition)
            result_condition(llb, "primary", "\n");
         else {
            field(llb, "status", value, sizeof(value));
            status = value;
         }
         field(llb, "hasCleanup", flag, sizeof(flag));
         if (!strcmp(flag, "1"))
            result_condition(llb, "cleanup", "\nCleanup: ");
         field(llb, "truncated", flag, sizeof(flag));
         if (!strcmp(flag, "1"))
            append(2, "\nInterpreter result text was truncated.\n",
                   strlen("\nInterpreter result text was truncated.\n"));
      }
   }
   if (code || (error && *error)) {
      strcpy(console.state, "Failed");
      if (error) {
         append(2, error, strlen(error));
         append(2, "\n", 1);
      }
   } else if (condition)
      strcpy(console.state, "Runtime condition");
   else {
      snprintf(console.status, sizeof(console.status), "%s", status ? status : "0");
      snprintf(console.state, sizeof(console.state), "Completed · status %s", console.status);
   }
   console.running = console.waiting = 0;
   changed();
}

void the_tool_stop(const char *reason) {
   if (console.running) {
      unsigned id = console.id;
      console.running = console.waiting = 0;
      if (backend.stop)
         backend.stop(id);
      decode(1, NULL, 0, 1);
      decode(2, NULL, 0, 1);
      snprintf(console.state, sizeof(console.state), "%s", reason ? reason : "Stopped");
      changed();
   }
}
int the_tool_input(const void *s, size_t n) {
   if (!console.running || console.eof || n > 1048576 || !backend.input)
      return 0;
   if (!backend.input(console.id, s, n))
      return 0;
   append(0, "› ", 4);
   append(0, s, n);
   return 1;
}
int the_tool_eof(void) {
   if (!console.running || console.eof || !backend.eof || !backend.eof(console.id))
      return 0;
   console.eof = 1;
   changed();
   return 1;
}
void the_tool_poll(void) {
   if (backend.poll)
      backend.poll();
}
void the_tool_console_visibility(int visible, int focused) {
   console.visible = !!visible;
   console.focused = !!focused;
   changed();
}
int the_tool_console_key(uint32_t c, int key) {
   if (key == 27) {
      console.focused = 0;
      changed();
      return 1;
   }
   if (key == 3) {
      the_tool_stop("Stopped");
      return 1;
   }
   if (key == 4)
      return the_tool_eof();
   if (key == 10 || key == 13) {
      if (console.input_length < sizeof(console.input) - 1) {
         console.input[console.input_length] = '\n';
         if (!the_tool_input(console.input, console.input_length + 1))
            return 0;
         console.input_length = 0;
         console.input[0] = 0;
         changed();
         return 1;
      }
      return 0;
   }
   if (key == 8 || key == 127) {
      if (console.input_length) {
         console.input_length--;
         while (console.input_length &&
                ((unsigned char)console.input[console.input_length] & 0xc0) == 0x80)
            console.input_length--;
         console.input[console.input_length] = 0;
      }
      changed();
      return 1;
   }
   if (c >= 32 && c <= 0x10ffff && !(c >= 0xd800 && c <= 0xdfff)) {
      unsigned char b[4];
      size_t n;
      if (c < 128) {
         b[0] = c;
         n = 1;
      } else if (c < 2048) {
         b[0] = 0xc0 | (c >> 6);
         b[1] = 0x80 | (c & 63);
         n = 2;
      } else if (c < 65536) {
         b[0] = 0xe0 | (c >> 12);
         b[1] = 0x80 | ((c >> 6) & 63);
         b[2] = 0x80 | (c & 63);
         n = 3;
      } else {
         b[0] = 0xf0 | (c >> 18);
         b[1] = 0x80 | ((c >> 12) & 63);
         b[2] = 0x80 | ((c >> 6) & 63);
         b[3] = 0x80 | (c & 63);
         n = 4;
      }
      if (n >= sizeof(console.input) - console.input_length)
         return 0;
      memcpy(console.input + console.input_length, b, n);
      console.input_length += n;
      console.input[console.input_length] = 0;
      changed();
      return 1;
   }
   return 0;
}
typedef struct {
   char *out;
   size_t cap, used;
   int failed;
} Writer;
static void writef(Writer *w, const char *format, ...) {
   int n;
   va_list a;
   if (w->failed)
      return;
   va_start(a, format);
   n = vsnprintf(w->out + w->used, w->cap - w->used, format, a);
   va_end(a);
   if (n < 0 || (size_t)n >= w->cap - w->used)
      w->failed = 1;
   else
      w->used += n;
}
static void quoted(Writer *w, const char *s, size_t n) {
   size_t i;
   writef(w, "\"");
   for (i = 0; i < n; i++) {
      unsigned c = (unsigned char)s[i];
      if (c == '"' || c == '\\')
         writef(w, "\\%c", c);
      else if (c < 32)
         writef(w, "\\u%04x", c);
      else
         writef(w, "%c", c);
   }
   writef(w, "\"");
}
#define Q(w, s) quoted(w, s, strlen(s))
size_t the_tool_format_json(char *out, size_t cap) {
   Writer w = {out, cap, 0, 0};
   size_t i;
   writef(&w,
          "{\"type\":\"tools\",\"version\":%u,\"job\":{\"id\":%u,\"sourceId\":%u,\"revision\":%u,"
          "\"running\":%s,\"waiting\":%s,\"eof\":%s,\"state\":",
          console.version, console.id, console.source_id, console.revision,
          console.running ? "true" : "false", console.waiting ? "true" : "false",
          console.eof ? "true" : "false");
   Q(&w, console.state);
   writef(&w, ",\"filename\":");
   Q(&w, console.filename);
   writef(&w, ",\"tool\":");
   Q(&w, console.tool);
   writef(&w, "},\"visible\":%s,\"omitted\":%s,\"chunks\":[", console.visible ? "true" : "false",
          console.omitted ? "true" : "false");
   for (i = 0; i < console.count; i++) {
      writef(&w, "%s{\"fd\":%d,\"text\":", i ? "," : "", console.chunks[i].fd);
      quoted(&w, console.chunks[i].text, console.chunks[i].length);
      writef(&w, "}");
   }
   writef(&w, "],\"catalog\":[");
   for (i = 0; i < catalog_count; i++) {
      TheToolDefinition *t = &catalog[i];
      writef(&w, "%s{\"id\":", i ? "," : "");
      Q(&w, t->id);
      writef(&w, ",\"label\":");
      Q(&w, t->label);
      writef(&w, ",\"role\":");
      Q(&w, t->role);
      writef(&w, ",\"image\":");
      Q(&w, t->image);
      writef(&w, ",\"extensions\":");
      Q(&w, t->extensions);
      writef(&w, ",\"available\":%s}", t->available ? "true" : "false");
   }
   writef(&w, "]}");
   return w.failed ? 0 : w.used;
}
size_t the_tool_console_text(char *out, size_t cap) {
   size_t i, n = 0;
   if (!cap)
      return 0;
   for (i = 0; i < console.count; i++) {
      size_t k = console.chunks[i].length;
      if (k > cap - 1 - n)
         k = cap - 1 - n;
      memcpy(out + n, console.chunks[i].text, k);
      n += k;
      if (n == cap - 1)
         break;
   }
   out[n] = 0;
   return n;
}
