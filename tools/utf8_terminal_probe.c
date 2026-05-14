#define _XOPEN_SOURCE 700
#define _XOPEN_SOURCE_EXTENDED 1

#include <curses.h>
#include <errno.h>
#include <locale.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#if defined(_WIN32)
# include <io.h>
# define isatty _isatty
#else
# include <sys/ioctl.h>
# include <sys/select.h>
# include <termios.h>
# include <unistd.h>
#endif

#ifndef CCHARW_MAX
# define CCHARW_MAX 5
#endif

#define UTF8_TERMINAL_PROBE_VERSION "2026-05-13-poc34"

#define U8_COMBINING_E_ACUTE "e\xCC\x81"
#define U8_COMBINING_STACK "a\xCC\x81\xCC\xA7"
#define U8_CJK_HAN "\xE6\xBC\xA2"
#define U8_AMBIGUOUS_MIDDOT "\xC2\xB7"
#define U8_GRIN "\xF0\x9F\x98\x80"
#define U8_HEART_TEXT "\xE2\x9D\xA4"
#define U8_HEART_EMOJI "\xE2\x9D\xA4\xEF\xB8\x8F"
#define U8_CHECK_EMOJI "\xE2\x9C\x94\xEF\xB8\x8F"
#define U8_THUMBS_SKIN "\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBB"
#define U8_KEYCAP_1 "1\xEF\xB8\x8F\xE2\x83\xA3"
#define U8_KEYCAP_HASH "#\xEF\xB8\x8F\xE2\x83\xA3"
#define U8_KEYCAP_STAR "*\xEF\xB8\x8F\xE2\x83\xA3"
#define U8_FLAG_US "\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8"
#define U8_FLAG_CA "\xF0\x9F\x87\xA8\xF0\x9F\x87\xA6"
#define U8_WOMAN_LAPTOP "\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x92\xBB"
#define U8_PERSON_ROCKET "\xF0\x9F\xA7\x91\xE2\x80\x8D\xF0\x9F\x9A\x80"
#define U8_WOMAN_HEART_MAN "\xF0\x9F\x91\xA9\xE2\x80\x8D\xE2\x9D\xA4\xEF\xB8\x8F\xE2\x80\x8D\xF0\x9F\x91\xA8"
#define U8_FAMILY "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7\xE2\x80\x8D\xF0\x9F\x91\xA6"
#define U8_TAG_FLAG_ENGLAND "\xF0\x9F\x8F\xB4\xF3\xA0\x81\xA7\xF3\xA0\x81\xA2\xF3\xA0\x81\xA5\xF3\xA0\x81\xAE\xF3\xA0\x81\xA7\xF3\xA0\x81\xBF"
#define U8_PRIVATE_USE_E0B0 "\xEE\x82\xB0"

typedef struct
{
   char *name;
   char *klass;
   char *utf8;
   int expected_policy_width;
} ProbeSample;

typedef enum
{
   METHOD_WADDWSTR,
   METHOD_WADD_WCH_EACH,
   METHOD_WADD_WCHNSTR_EACH,
   METHOD_CCHAR_CLUSTER,
   METHOD_RAW_UTF8,
   METHOD_COUNT
} ProbeMethod;

typedef enum
{
   MODE_CLUSTER_ONLY,
   MODE_WRAPPED_NATURAL,
   MODE_WRAPPED_FORCED_POLICY,
   MODE_COUNT
} ProbeMode;

typedef enum
{
   POC_CELL_REPAINT,
   POC_SPAN_REPAINT
} PocRepaintMode;

typedef struct
{
   FILE *report;
   const char *report_path;
   const char *profile_path;
   const char *cases_path;
   ProbeSample *samples;
   size_t sample_count;
   int pause;
   int no_visual;
   int run_matrix;
   int run_motion;
   int run_poc;
   int raw_poc;
   int curses_poc;
   int utfvis;
   const char *utfvis_selector;
   int testcursor;
   const char *testcursor_selector;
   const char *testcursor_mode;
   int testcursor_layout_width;
   int testcursor_cursor_width;
   int testchain;
   const char *testchain_selector;
   const char *testchain_mode;
   int testchain_layout_width;
   int testchain_cursor_width;
   int calibrate;
   const char *calibrate_selector;
   int timeout_ms;
   int data_col;
   int row;
} ProbeConfig;

typedef struct
{
   const ProbeSample *sample;
   const char *feature_class;
   const char *display_intent;
   const char *output_method;
   const struct CalibrationDefault *defaults;
   int layout_width;
   int cursor_width;
   const char *cursor_strategy;
   const char *replacement_strategy;
} CalibrationEntry;

typedef struct
{
   const char *name;
   int layout_width;
   int cursor_width;
} ViewCandidate;

typedef struct
{
   const char *name;
   const char *label;
   int preference_score;
} StrategyCandidate;

typedef struct CalibrationDefault
{
   const char *feature_class;
   const char *display_intent;
   const char *output_method;
   int layout_width;
   int cursor_width;
   const char *cursor_strategy;
   const char *replacement_strategy;
} CalibrationDefault;

static const CalibrationDefault calibration_defaults[] =
{
   { "ascii", "normal", "native", 1, 1, "cell", "cell" },
   { "combining", "normal", "native", 1, 1, "line", "line" },
   { "combining-stack", "normal", "native", 1, 1, "line", "line" },
   { "wide", "normal", "native", 2, 2, "cell", "cell" },
   { "ambiguous", "normal", "native", 1, 1, "cell", "cell" },
   { "emoji", "normal", "native", 2, 2, "line", "line" },
   { "text-variation", "normal", "native", 1, 1, "line", "line" },
   { "emoji-variation", "normal", "native", 2, 2, "line", "line" },
   { "modifier", "normal", "native", 2, 2, "line", "line" },
   { "keycap", "normal", "native", 2, 2, "flashfirstfast", "flashwhole" },
   { "regional-flag", "normal", "native", 3, 3, "cell", "suffix" },
   { "short-zwj", "group", "native", 2, 2, "line", "line" },
   { "short-zwj", "components", "expanded", 4, 4, "line", "line" },
   { "heart-zwj", "group", "native", 6, 6, "line", "line" },
   { "heart-zwj", "components", "expanded", 6, 6, "line", "line" },
   { "family-zwj", "group", "native", 6, 6, "line", "line" },
   { "family-zwj", "components", "expanded", 8, 8, "line", "line" },
   { "tag-flag", "normal", "native", 2, 2, "line", "line" },
   { "private-use", "normal", "native", 1, 1, "line", "line" }
};

#define ZWJ_UTF8 "\xE2\x80\x8D"
#define U8_ZWJ_SUBSTITUTE "@"
#define MAX_VIEW_CANDIDATES 32

static const ProbeSample samples[] =
{
   { (char *)"ascii-x", (char *)"ascii", (char *)"x", 1 },
   { (char *)"combining-e-acute", (char *)"combining", (char *)U8_COMBINING_E_ACUTE, 1 },
   { (char *)"combining-stack", (char *)"combining-stack", (char *)U8_COMBINING_STACK, 1 },
   { (char *)"cjk-han", (char *)"wide", (char *)U8_CJK_HAN, 2 },
   { (char *)"ambiguous-middot", (char *)"ambiguous", (char *)U8_AMBIGUOUS_MIDDOT, 1 },
   { (char *)"emoji-grin", (char *)"emoji", (char *)U8_GRIN, 2 },
   { (char *)"heart-text", (char *)"text-variation", (char *)U8_HEART_TEXT, 1 },
   { (char *)"heart-emoji", (char *)"emoji-variation", (char *)U8_HEART_EMOJI, 2 },
   { (char *)"check-emoji", (char *)"emoji-variation", (char *)U8_CHECK_EMOJI, 2 },
   { (char *)"thumbs-skin", (char *)"modifier", (char *)U8_THUMBS_SKIN, 2 },
   { (char *)"keycap-1", (char *)"keycap", (char *)U8_KEYCAP_1, 2 },
   { (char *)"keycap-hash", (char *)"keycap", (char *)U8_KEYCAP_HASH, 2 },
   { (char *)"keycap-star", (char *)"keycap", (char *)U8_KEYCAP_STAR, 2 },
   { (char *)"flag-us", (char *)"regional-flag", (char *)U8_FLAG_US, 2 },
   { (char *)"flag-ca", (char *)"regional-flag", (char *)U8_FLAG_CA, 2 },
   { (char *)"woman-laptop", (char *)"short-zwj", (char *)U8_WOMAN_LAPTOP, 2 },
   { (char *)"person-rocket", (char *)"short-zwj", (char *)U8_PERSON_ROCKET, 2 },
   { (char *)"woman-heart-man", (char *)"heart-zwj", (char *)U8_WOMAN_HEART_MAN, 6 },
   { (char *)"family", (char *)"family-zwj", (char *)U8_FAMILY, 6 },
   { (char *)"tag-flag-england", (char *)"tag-flag", (char *)U8_TAG_FLAG_ENGLAND, 2 },
   { (char *)"private-use-e0b0", (char *)"private-use", (char *)U8_PRIVATE_USE_E0B0, 1 }
};

static const char *method_name(ProbeMethod method)
{
   switch (method)
   {
      case METHOD_WADDWSTR:
         return "waddwstr";
      case METHOD_WADD_WCH_EACH:
         return "wadd_wch_each";
      case METHOD_WADD_WCHNSTR_EACH:
         return "wadd_wchnstr_each";
      case METHOD_CCHAR_CLUSTER:
         return "cchar_cluster";
      case METHOD_RAW_UTF8:
         return "raw_utf8";
      default:
         return "unknown";
   }
}

static const char *mode_name(ProbeMode mode)
{
   switch (mode)
   {
      case MODE_CLUSTER_ONLY:
         return "cluster_only";
      case MODE_WRAPPED_NATURAL:
         return "A_cluster_B_natural";
      case MODE_WRAPPED_FORCED_POLICY:
         return "A_cluster_B_forced_policy";
      default:
         return "unknown";
   }
}

static void reportf(ProbeConfig *cfg, const char *fmt, ...)
{
   va_list ap;

   if (cfg->report == NULL)
      return;
   va_start(ap, fmt);
   vfprintf(cfg->report, fmt, ap);
   va_end(ap);
   fflush(cfg->report);
}

static char *probe_strdup(const char *text)
{
   size_t len;
   char *copy;

   if (text == NULL)
      text = "";
   len = strlen(text);
   copy = (char *)malloc(len + 1);
   if (copy != NULL)
      memcpy(copy, text, len + 1);
   return copy;
}

static char *trim_field(char *text)
{
   char *end;

   while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')
      text++;
   end = text + strlen(text);
   while (end > text
   &&    (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
      *--end = '\0';
   return text;
}

static size_t append_utf8_codepoint(char *out, size_t used, size_t out_cap,
                                    uint32_t codepoint)
{
   if (codepoint <= 0x7Fu)
   {
      if (used + 1 >= out_cap)
         return 0;
      out[used++] = (char)codepoint;
   }
   else if (codepoint <= 0x7FFu)
   {
      if (used + 2 >= out_cap)
         return 0;
      out[used++] = (char)(0xC0u | (codepoint >> 6));
      out[used++] = (char)(0x80u | (codepoint & 0x3Fu));
   }
   else if (codepoint <= 0xFFFFu)
   {
      if (used + 3 >= out_cap)
         return 0;
      out[used++] = (char)(0xE0u | (codepoint >> 12));
      out[used++] = (char)(0x80u | ((codepoint >> 6) & 0x3Fu));
      out[used++] = (char)(0x80u | (codepoint & 0x3Fu));
   }
   else if (codepoint <= 0x10FFFFu)
   {
      if (used + 4 >= out_cap)
         return 0;
      out[used++] = (char)(0xF0u | (codepoint >> 18));
      out[used++] = (char)(0x80u | ((codepoint >> 12) & 0x3Fu));
      out[used++] = (char)(0x80u | ((codepoint >> 6) & 0x3Fu));
      out[used++] = (char)(0x80u | (codepoint & 0x3Fu));
   }
   else
   {
      return 0;
   }
   out[used] = '\0';
   return used;
}

static char *parse_codepoint_field(const char *field)
{
   char out[512];
   size_t used = 0;
   int count = 0;

   out[0] = '\0';
   while (*field != '\0')
   {
      char *end = NULL;
      unsigned long codepoint;

      while (*field == ' ' || *field == '\t' || *field == ',' || *field == '+')
         field++;
      if (*field == '\0' || *field == '#')
         break;
      if ((field[0] == 'U' || field[0] == 'u') && field[1] == '+')
         field += 2;
      else if (field[0] == '0' && (field[1] == 'x' || field[1] == 'X'))
         field += 2;
      codepoint = strtoul(field, &end, 16);
      if (end == field)
         return NULL;
      used = append_utf8_codepoint(out, used, sizeof(out), (uint32_t)codepoint);
      if (used == 0)
         return NULL;
      count++;
      field = end;
   }

   return (count > 0) ? probe_strdup(out) : NULL;
}

static int load_probe_cases(ProbeConfig *cfg, const char *path)
{
   FILE *fp;
   ProbeSample *loaded = NULL;
   size_t count = 0;
   size_t cap = 0;
   char line[1024];
   int lineno = 0;

   if (path == NULL)
   {
      cfg->samples = (ProbeSample *)samples;
      cfg->sample_count = sizeof(samples) / sizeof(samples[0]);
      return 0;
   }

   fp = fopen(path, "r");
   if (fp == NULL)
   {
      fprintf(stderr, "%s: %s\n", path, strerror(errno));
      return -1;
   }

   while (fgets(line, sizeof(line), fp) != NULL)
   {
      char *name;
      char *klass;
      char *width_text;
      char *codepoints;
      char *utf8;
      ProbeSample *next;

      lineno++;
      name = trim_field(line);
      if (*name == '\0' || *name == '#')
         continue;
      klass = strchr(name, '\t');
      if (klass == NULL)
         goto bad_line;
      *klass++ = '\0';
      width_text = strchr(klass, '\t');
      if (width_text == NULL)
         goto bad_line;
      *width_text++ = '\0';
      codepoints = strchr(width_text, '\t');
      if (codepoints == NULL)
         goto bad_line;
      *codepoints++ = '\0';

      name = trim_field(name);
      klass = trim_field(klass);
      width_text = trim_field(width_text);
      codepoints = trim_field(codepoints);
      utf8 = parse_codepoint_field(codepoints);
      if (utf8 == NULL)
         goto bad_line;

      if (count == cap)
      {
         cap = (cap == 0) ? 16 : cap * 2;
         next = (ProbeSample *)realloc(loaded, cap * sizeof(*loaded));
         if (next == NULL)
         {
            free(utf8);
            fclose(fp);
            return -1;
         }
         loaded = next;
      }
      loaded[count].name = probe_strdup(name);
      loaded[count].klass = probe_strdup(klass);
      loaded[count].utf8 = utf8;
      loaded[count].expected_policy_width = atoi(width_text);
      if (loaded[count].name == NULL || loaded[count].klass == NULL)
      {
         fclose(fp);
         return -1;
      }
      count++;
      continue;

bad_line:
      fprintf(stderr, "%s:%d: expected name<TAB>class<TAB>policy_width<TAB>U+...\n",
              path, lineno);
      fclose(fp);
      return -1;
   }

   fclose(fp);
   if (count == 0)
   {
      fprintf(stderr, "%s: no probe cases loaded\n", path);
      return -1;
   }
   cfg->samples = loaded;
   cfg->sample_count = count;
   return 0;
}

static int utf8_to_wide(const char *src, wchar_t *dst, size_t dst_cap)
{
   mbstate_t state;
   const char *cursor = src;
   size_t len;

   if (dst_cap == 0)
      return -1;
   memset(&state, 0, sizeof(state));
   len = mbsrtowcs(dst, &cursor, dst_cap - 1, &state);
   if (len == (size_t)-1)
   {
      dst[0] = L'\0';
      return -1;
   }
   dst[len] = L'\0';
   return (int)len;
}

static int utf8_next_codepoint(const unsigned char **cursor, uint32_t *codepoint)
{
   const unsigned char *s = *cursor;

   if (*s == '\0')
      return 0;
   if (s[0] < 0x80u)
   {
      *codepoint = s[0];
      *cursor = s + 1;
      return 1;
   }
   if ((s[0] & 0xE0u) == 0xC0u && (s[1] & 0xC0u) == 0x80u)
   {
      *codepoint = ((uint32_t)(s[0] & 0x1Fu) << 6)
                 |  (uint32_t)(s[1] & 0x3Fu);
      *cursor = s + 2;
      return 1;
   }
   if ((s[0] & 0xF0u) == 0xE0u
   &&  (s[1] & 0xC0u) == 0x80u
   &&  (s[2] & 0xC0u) == 0x80u)
   {
      *codepoint = ((uint32_t)(s[0] & 0x0Fu) << 12)
                 | ((uint32_t)(s[1] & 0x3Fu) << 6)
                 |  (uint32_t)(s[2] & 0x3Fu);
      *cursor = s + 3;
      return 1;
   }
   if ((s[0] & 0xF8u) == 0xF0u
   &&  (s[1] & 0xC0u) == 0x80u
   &&  (s[2] & 0xC0u) == 0x80u
   &&  (s[3] & 0xC0u) == 0x80u)
   {
      *codepoint = ((uint32_t)(s[0] & 0x07u) << 18)
                 | ((uint32_t)(s[1] & 0x3Fu) << 12)
                 | ((uint32_t)(s[2] & 0x3Fu) << 6)
                 |  (uint32_t)(s[3] & 0x3Fu);
      *cursor = s + 4;
      return 1;
   }
   *codepoint = 0xFFFDu;
   *cursor = s + 1;
   return 1;
}

static void utf8_codepoints(const char *utf8, char *out, size_t out_cap)
{
   const unsigned char *cursor = (const unsigned char *)utf8;
   size_t used = 0;
   int first = 1;

   if (out_cap == 0)
      return;
   out[0] = '\0';
   while (*cursor != '\0')
   {
      uint32_t codepoint;
      char one[24];
      int n;

      if (!utf8_next_codepoint(&cursor, &codepoint))
         break;
      n = snprintf(one, sizeof(one), "%sU+%X", first ? "" : "+", codepoint);
      first = 0;
      if (n < 0 || used + (size_t)n + 1 >= out_cap)
      {
         snprintf(out + used, out_cap - used, "...");
         return;
      }
      memcpy(out + used, one, (size_t)n + 1);
      used += (size_t)n;
   }
}

static int terminal_write(const char *text)
{
#if defined(_WIN32)
   return _write(_fileno(stdout), text, (unsigned)strlen(text)) >= 0 ? 0 : -1;
#else
   return write(STDOUT_FILENO, text, strlen(text)) >= 0 ? 0 : -1;
#endif
}

static int terminal_write_attr(const char *text, attr_t attr)
{
   int rc;

   if ((attr & A_REVERSE) != 0)
      terminal_write("\033[7m");
   if ((attr & A_UNDERLINE) != 0)
      terminal_write("\033[4m");
   rc = terminal_write(text);
   if ((attr & (A_REVERSE | A_UNDERLINE)) != 0)
      terminal_write("\033[0m");
   return rc;
}

static void terminal_move(int row, int col)
{
   char seq[64];

   snprintf(seq, sizeof(seq), "\033[%d;%dH", row + 1, col + 1);
   terminal_write(seq);
}

static void terminal_clear_screen(void)
{
   terminal_write("\033[2J\033[H");
}

static void terminal_show_cursor(int show)
{
   terminal_write(show ? "\033[?25h" : "\033[?25l");
}

static void sleep_ms(int ms)
{
   struct timespec ts;

   if (ms < 0)
      ms = 0;
   ts.tv_sec = ms / 1000;
   ts.tv_nsec = (long)(ms % 1000) * 1000000L;
   nanosleep(&ts, NULL);
}

static int query_terminal_cursor(int timeout_ms, int *row, int *col)
{
#if defined(_WIN32)
   (void)timeout_ms;
   (void)row;
   (void)col;
   return -1;
#else
   char buf[64];
   size_t used = 0;
   struct timeval tv;
   int fd = STDIN_FILENO;

   *row = -1;
   *col = -1;
   if (!isatty(fd))
      return -1;
   if (terminal_write("\033[6n") != 0)
      return -1;
   fflush(stdout);

   while (used + 1 < sizeof(buf))
   {
      fd_set rfds;
      int rc;
      char ch;

      FD_ZERO(&rfds);
      FD_SET(fd, &rfds);
      tv.tv_sec = timeout_ms / 1000;
      tv.tv_usec = (timeout_ms % 1000) * 1000;
      rc = select(fd + 1, &rfds, NULL, NULL, &tv);
      if (rc <= 0)
         break;
      if (read(fd, &ch, 1) != 1)
         break;
      buf[used++] = ch;
      if (ch == 'R')
         break;
   }
   buf[used] = '\0';
   return sscanf(buf, "\033[%d;%dR", row, col) == 2 ? 0 : -1;
#endif
}

#if !defined(_WIN32)
static int enable_raw_input(struct termios *saved)
{
   struct termios raw;

   if (!isatty(STDIN_FILENO))
      return -1;
   if (tcgetattr(STDIN_FILENO, saved) != 0)
      return -1;
   raw = *saved;
   raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
   raw.c_cc[VMIN] = 1;
   raw.c_cc[VTIME] = 0;
   return tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

static void restore_raw_input(const struct termios *saved)
{
   tcsetattr(STDIN_FILENO, TCSANOW, saved);
}
#endif

static void raw_pause_or_sleep(ProbeConfig *cfg)
{
   if (cfg->pause)
   {
      char ch;

#if defined(_WIN32)
      (void)ch;
#else
      (void)read(STDIN_FILENO, &ch, 1);
#endif
   }
   else
      sleep_ms(350);
}

static int write_utf8_with_method(ProbeMethod method, const char *utf8, attr_t attr)
{
   wchar_t wide[128];
   int wide_len;
   int i;
   int rc;

   if (method == METHOD_RAW_UTF8)
      return terminal_write_attr(utf8, attr);

   wide_len = utf8_to_wide(utf8, wide, sizeof(wide) / sizeof(wide[0]));
   if (wide_len < 0)
      return -2;

   switch (method)
   {
      case METHOD_WADDWSTR:
         wattrset(stdscr, attr);
         rc = waddwstr(stdscr, wide);
         wattrset(stdscr, A_NORMAL);
         return rc;

      case METHOD_WADD_WCH_EACH:
         for (i = 0; i < wide_len; i++)
         {
            cchar_t cell;
            wchar_t one[2];

            one[0] = wide[i];
            one[1] = L'\0';
            if (setcchar(&cell, one, attr, 0, NULL) == ERR)
               return -3;
            if (wadd_wch(stdscr, &cell) == ERR)
            {
               wattrset(stdscr, A_NORMAL);
               return -4;
            }
         }
         wattrset(stdscr, A_NORMAL);
         return OK;

      case METHOD_WADD_WCHNSTR_EACH:
      {
         cchar_t cells[128];

         if (wide_len > (int)(sizeof(cells) / sizeof(cells[0])))
            return -5;
         for (i = 0; i < wide_len; i++)
         {
            wchar_t one[2];

            one[0] = wide[i];
            one[1] = L'\0';
            if (setcchar(&cells[i], one, attr, 0, NULL) == ERR)
               return -3;
         }
         rc = wadd_wchnstr(stdscr, cells, wide_len);
         wattrset(stdscr, A_NORMAL);
         return rc;
      }

      case METHOD_CCHAR_CLUSTER:
      {
         cchar_t cell;

         if (wide_len >= CCHARW_MAX)
            return -6;
         if (setcchar(&cell, wide, attr, 0, NULL) == ERR)
            return -3;
         rc = wadd_wch(stdscr, &cell);
         wattrset(stdscr, A_NORMAL);
         return rc;
      }

      default:
         return -9;
   }
}

static void clear_probe_row(int row)
{
   move(row, 0);
   clrtoeol();
}

static void start_probe_cell(int row, int col, ProbeMethod method)
{
   if (method == METHOD_RAW_UTF8)
   {
      move(row, col);
      refresh();
      terminal_move(row, col);
   }
   else
   {
      move(row, col);
   }
}

static void write_wrapped(ProbeMethod method, ProbeMode mode,
                          const ProbeSample *sample, int row, int col,
                          attr_t attr)
{
   start_probe_cell(row, col, method);
   if (mode == MODE_CLUSTER_ONLY)
   {
      write_utf8_with_method(method, sample->utf8, attr);
      return;
   }

   write_utf8_with_method(method, "A", attr);
   write_utf8_with_method(method, sample->utf8, attr);
   if (mode == MODE_WRAPPED_FORCED_POLICY)
   {
      if (method == METHOD_RAW_UTF8)
         terminal_move(row, col + 1 + sample->expected_policy_width);
      else
         move(row, col + 1 + sample->expected_policy_width);
   }
   write_utf8_with_method(method, "B", attr);
}

static void maybe_page(ProbeConfig *cfg)
{
   int max_rows = LINES > 8 ? LINES - 3 : LINES;

   if (cfg->no_visual)
      return;
   if (cfg->row < max_rows)
      return;
   mvprintw(LINES - 1, 0, cfg->pause ? "press any key for next page" : "next page");
   refresh();
   if (cfg->pause)
      getch();
   else
      napms(250);
   erase();
   cfg->row = 2;
}

static void finish_probe_page(ProbeConfig *cfg)
{
   if (cfg->no_visual)
      return;
   mvprintw(LINES - 1, 0, cfg->pause ? "press any key to continue" : "probe complete");
   clrtoeol();
   refresh();
   if (cfg->pause)
      getch();
   else
      napms(400);
}

static void log_environment(ProbeConfig *cfg)
{
   time_t now = time(NULL);

   reportf(cfg, "# THE UTF-8 terminal probe\n");
   reportf(cfg, "probe_version=%s\n", UTF8_TERMINAL_PROBE_VERSION);
   reportf(cfg, "time=%s", ctime(&now));
   reportf(cfg, "locale=%s\n", setlocale(LC_CTYPE, NULL));
   reportf(cfg, "MB_CUR_MAX=%d\n", (int)MB_CUR_MAX);
   reportf(cfg, "sizeof_wchar_t=%d\n", (int)sizeof(wchar_t));
   reportf(cfg, "TERM=%s\n", getenv("TERM") ? getenv("TERM") : "");
   reportf(cfg, "TERM_PROGRAM=%s\n", getenv("TERM_PROGRAM") ? getenv("TERM_PROGRAM") : "");
   reportf(cfg, "COLORTERM=%s\n", getenv("COLORTERM") ? getenv("COLORTERM") : "");
   reportf(cfg, "cases=%s\n", cfg->cases_path ? cfg->cases_path : "built-in");
   reportf(cfg, "case_count=%zu\n", cfg->sample_count);
#ifdef NCURSES_VERSION
   reportf(cfg, "ncurses_version=%s\n", curses_version());
#endif
   reportf(cfg, "screen_rows=%d\n", LINES);
   reportf(cfg, "screen_cols=%d\n", COLS);
   reportf(cfg, "\n");
}

static void log_raw_environment(ProbeConfig *cfg)
{
   time_t now = time(NULL);
   int rows = -1;
   int cols = -1;
#if !defined(_WIN32)
   struct winsize ws;

   if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0)
   {
      rows = ws.ws_row;
      cols = ws.ws_col;
   }
#endif

   reportf(cfg, "# THE UTF-8 terminal probe\n");
   reportf(cfg, "probe_version=%s\n", UTF8_TERMINAL_PROBE_VERSION);
   reportf(cfg, "mode=raw_poc\n");
   reportf(cfg, "time=%s", ctime(&now));
   reportf(cfg, "locale=%s\n", setlocale(LC_CTYPE, NULL));
   reportf(cfg, "MB_CUR_MAX=%d\n", (int)MB_CUR_MAX);
   reportf(cfg, "sizeof_wchar_t=%d\n", (int)sizeof(wchar_t));
   reportf(cfg, "TERM=%s\n", getenv("TERM") ? getenv("TERM") : "");
   reportf(cfg, "TERM_PROGRAM=%s\n", getenv("TERM_PROGRAM") ? getenv("TERM_PROGRAM") : "");
   reportf(cfg, "COLORTERM=%s\n", getenv("COLORTERM") ? getenv("COLORTERM") : "");
   reportf(cfg, "cases=%s\n", cfg->cases_path ? cfg->cases_path : "built-in");
   reportf(cfg, "case_count=%zu\n", cfg->sample_count);
   reportf(cfg, "screen_rows=%d\n", rows);
   reportf(cfg, "screen_cols=%d\n", cols);
   reportf(cfg, "\n");
}

static void run_matrix_probe(ProbeConfig *cfg)
{
   size_t s;

   erase();
   cfg->row = 2;
   if (!cfg->no_visual)
   {
      mvprintw(0, 0, "UTF-8 terminal probe: width and write-method matrix");
      mvprintw(1, cfg->data_col, "0....5....10...15...20");
   }
   reportf(cfg, "section=matrix\n");
   reportf(cfg, "sample,class,method,mode,policy_width,wcswidth,curses_row,curses_col,dsr_row,dsr_col,write_rc,codepoints\n");

   for (s = 0; s < cfg->sample_count; s++)
   {
      ProbeMethod method;
      wchar_t wide[128];
      int wide_len = utf8_to_wide(cfg->samples[s].utf8, wide, sizeof(wide) / sizeof(wide[0]));
      int width = (wide_len >= 0) ? wcswidth(wide, (size_t)wide_len) : -1;
      char cps[256];

      utf8_codepoints(cfg->samples[s].utf8, cps, sizeof(cps));
      for (method = 0; method < METHOD_COUNT; method++)
      {
         ProbeMode mode;

         for (mode = 0; mode < MODE_COUNT; mode++)
         {
            int cy = -1;
            int cx = -1;
            int tr = -1;
            int tc = -1;
            int rc;
            int row;

            maybe_page(cfg);
            row = cfg->row++;
            clear_probe_row(row);
            if (!cfg->no_visual)
               mvprintw(row, 0, "%-13s %-18s %-22s", cfg->samples[s].name,
                        method_name(method), mode_name(mode));
            write_wrapped(method, mode, &cfg->samples[s], row, cfg->data_col, A_NORMAL);
            if (method != METHOD_RAW_UTF8)
               getyx(stdscr, cy, cx);
            rc = OK;
            refresh();
            if (query_terminal_cursor(cfg->timeout_ms, &tr, &tc) != 0)
            {
               tr = -1;
               tc = -1;
            }
            reportf(cfg, "%s,%s,%s,%s,%d,%d,%d,%d,%d,%d,%d,%s\n",
                    cfg->samples[s].name,
                    cfg->samples[s].klass,
                    method_name(method),
                    mode_name(mode),
                    cfg->samples[s].expected_policy_width,
                    width,
                    cy,
                    cx,
                    tr,
                    tc,
                    rc,
                    cps);
         }
      }
   }
   reportf(cfg, "\n");
}

static void write_motion_cluster(ProbeMethod method, int row, int col,
                                 const char *utf8, int display_width,
                                 attr_t attr)
{
   int i;

   if (method != METHOD_RAW_UTF8)
   {
      move(row, col);
      for (i = 0; i < display_width; i++)
         addch(' ' | attr);
      move(row, col);
   }
   else
   {
      terminal_move(row, col);
      for (i = 0; i < display_width; i++)
         terminal_write(" ");
      terminal_move(row, col);
   }
   write_utf8_with_method(method, utf8, attr);
   if (method == METHOD_RAW_UTF8)
      terminal_move(row, col + display_width);
   else
      move(row, col + display_width);
}

static void draw_motion_base(ProbeMethod method, int row, int col)
{
   write_motion_cluster(method, row, col + 0, "A", 1, A_NORMAL);
   write_motion_cluster(method, row, col + 1, U8_KEYCAP_1, 2, A_NORMAL);
   write_motion_cluster(method, row, col + 3, "B", 1, A_NORMAL);
   write_motion_cluster(method, row, col + 4, " ", 1, A_NORMAL);
   write_motion_cluster(method, row, col + 5, "A", 1, A_NORMAL);
}

static void motion_step(ProbeConfig *cfg, ProbeMethod method, int row, int col,
                        int old_index, int new_index)
{
   static const char *clusters[] = { "A", U8_KEYCAP_1, "B", " ", "A" };
   static const int offsets[] = { 0, 1, 3, 4, 5 };
   static const int widths[] = { 1, 2, 1, 1, 1 };
   int cy = -1;
   int cx = -1;
   int tr = -1;
   int tc = -1;

   if (old_index >= 0)
      write_motion_cluster(method, row, col + offsets[old_index],
                           clusters[old_index], widths[old_index], A_NORMAL);
   write_motion_cluster(method, row, col + offsets[new_index],
                        clusters[new_index], widths[new_index], A_REVERSE);
   if (method == METHOD_RAW_UTF8)
      terminal_move(row, col + offsets[new_index]);
   else
      move(row, col + offsets[new_index]);
   if (method != METHOD_RAW_UTF8)
      getyx(stdscr, cy, cx);
   refresh();
   if (cfg->no_visual
   &&  query_terminal_cursor(cfg->timeout_ms, &tr, &tc) != 0)
   {
      tr = -1;
      tc = -1;
   }
   reportf(cfg, "motion,%s,old=%d,new=%d,curses=%d:%d,dsr=%d:%d\n",
           method_name(method), old_index, new_index, cy, cx, tr, tc);
   if (cfg->pause)
      getch();
   else
      napms(350);
}

static void run_keycap_motion_probe(ProbeConfig *cfg)
{
   ProbeMethod methods[] = { METHOD_WADDWSTR, METHOD_CCHAR_CLUSTER, METHOD_RAW_UTF8 };
   size_t i;

   erase();
   reportf(cfg, "section=keycap_motion\n");
   if (!cfg->no_visual)
   {
      mvprintw(0, 0, "Keycap cursor-motion simulator. Watch the B and blank after A1-keycapB A.");
      mvprintw(1, cfg->data_col, "0123456789");
   }

   for (i = 0; i < sizeof(methods) / sizeof(methods[0]); i++)
   {
      int row = 3 + (int)i * 3;

      if (row >= LINES - 2)
      {
         finish_probe_page(cfg);
         erase();
         row = 3;
      }
      if (!cfg->no_visual)
      {
         mvprintw(row, 0, "%-18s", method_name(methods[i]));
         refresh();
      }
      draw_motion_base(methods[i], row, cfg->data_col);
      refresh();
      motion_step(cfg, methods[i], row, cfg->data_col, -1, 0);
      motion_step(cfg, methods[i], row, cfg->data_col, 0, 1);
      motion_step(cfg, methods[i], row, cfg->data_col, 1, 2);
      motion_step(cfg, methods[i], row, cfg->data_col, 2, 3);
      motion_step(cfg, methods[i], row, cfg->data_col, 3, 4);
      draw_motion_base(methods[i], row, cfg->data_col);
   }
   reportf(cfg, "\n");
}

static void show_poc_step(ProbeConfig *cfg, const char *scenario,
                          int old_index, int new_index, const char *target,
                          const char *mode, int guard_cells)
{
   if (cfg->no_visual)
      return;
   move(2, 0);
   clrtoeol();
   mvprintw(2, 0, "scenario=%s mode=%s guard=%d old=%d new=%d target=%s",
            scenario, mode, guard_cells, old_index, new_index, target);
   refresh();
}

static void draw_raw_target_marker(ProbeConfig *cfg, int row, int col,
                                   int target_offset)
{
   int i;

   if (cfg->no_visual)
      return;
   terminal_move(row + 1, col - 1);
   for (i = 0; i < 10; i++)
      terminal_write(" ");
   terminal_move(row + 1, col + target_offset);
   terminal_write("^");
   terminal_move(row, col + target_offset);
}

static void raw_absolute_motion_step(ProbeConfig *cfg, const char *scenario,
                                     int row, int col, int old_index,
                                     int new_index)
{
   static const char *clusters[] = { "A", U8_KEYCAP_1, "B", " ", "A" };
   static const char *names[] = { "A", "keycap", "B", "space", "next-A" };
   static const int offsets[] = { 0, 1, 3, 4, 5 };
   static const int widths[] = { 1, 2, 1, 1, 1 };
   int cy = -1;
   int cx = -1;
   int tr = -1;
   int tc = -1;
   int expected_dsr_row = row + 1;
   int expected_dsr_col = col + offsets[new_index] + 1;

   show_poc_step(cfg, scenario, old_index, new_index, names[new_index], "cell", 0);
   if (old_index >= 0)
      write_motion_cluster(METHOD_RAW_UTF8, row, col + offsets[old_index],
                           clusters[old_index], widths[old_index], A_NORMAL);
   write_motion_cluster(METHOD_RAW_UTF8, row, col + offsets[new_index],
                        clusters[new_index], widths[new_index], A_REVERSE);

   terminal_move(row, col + offsets[new_index]);
   draw_raw_target_marker(cfg, row, col, offsets[new_index]);
   cy = row;
   cx = col + offsets[new_index];
   if (cfg->no_visual
   &&  query_terminal_cursor(cfg->timeout_ms, &tr, &tc) != 0)
   {
      tr = -1;
      tc = -1;
   }

   reportf(cfg,
           "poc,%s,mode=cell,old=%d,new=%d,target=%s,expected_dsr=%d:%d,curses_expected=%d:%d,dsr=%d:%d\n",
           scenario, old_index, new_index, names[new_index],
           expected_dsr_row, expected_dsr_col, cy, cx, tr, tc);
   if (cfg->pause)
      getch();
   else
      napms(350);
}

static void write_raw_cluster_at(int row, int col, const char *utf8,
                                 int display_width, attr_t attr)
{
   terminal_move(row, col);
   terminal_write_attr(utf8, attr);
   terminal_move(row, col + display_width);
}

static void raw_span_repaint_step(ProbeConfig *cfg, const char *scenario,
                                  int row, int col, int old_index,
                                  int new_index, int guard_cells)
{
   static const char *clusters[] = { "A", U8_KEYCAP_1, "B", " ", "A" };
   static const char *names[] = { "A", "keycap", "B", "space", "next-A" };
   static const int offsets[] = { 0, 1, 3, 4, 5 };
   static const int widths[] = { 1, 2, 1, 1, 1 };
   int clear_start = col - guard_cells;
   int clear_width = 6 + (2 * guard_cells);
   int cy = row;
   int cx = col + offsets[new_index];
   int tr = -1;
   int tc = -1;
   int expected_dsr_row = row + 1;
   int expected_dsr_col = cx + 1;
   int i;

   if (clear_start < 0)
   {
      clear_width += clear_start;
      clear_start = 0;
   }
   if (clear_width < 1)
      clear_width = 1;

   show_poc_step(cfg, scenario, old_index, new_index, names[new_index],
                 "span", guard_cells);
   terminal_move(row, clear_start);
   for (i = 0; i < clear_width; i++)
      terminal_write(" ");
   for (i = 0; i < 5; i++)
      write_raw_cluster_at(row, col + offsets[i], clusters[i], widths[i],
                           i == new_index ? A_REVERSE : A_NORMAL);

   terminal_move(row, cx);
   draw_raw_target_marker(cfg, row, col, offsets[new_index]);
   if (query_terminal_cursor(cfg->timeout_ms, &tr, &tc) != 0)
   {
      tr = -1;
      tc = -1;
   }

   reportf(cfg,
           "poc,%s,mode=span,guard=%d,old=%d,new=%d,target=%s,expected_dsr=%d:%d,curses_expected=%d:%d,dsr=%d:%d\n",
           scenario, guard_cells, old_index, new_index, names[new_index],
           expected_dsr_row, expected_dsr_col, cy, cx, tr, tc);
   if (cfg->pause)
      getch();
   else
      napms(350);
}

static void raw_poc_status(ProbeConfig *cfg, const char *scenario,
                           const char *repaint, const char *style,
                           int paint_width, int cursor_width, int old_index,
                           int new_index, const char *target)
{
   char line[256];

   if (cfg->no_visual)
      return;
   snprintf(line, sizeof(line),
            "scenario=%s repaint=%s style=%s paint_width=%d cursor_width=%d old=%d new=%d target=%s",
            scenario, repaint, style, paint_width, cursor_width, old_index,
            new_index, target);
   terminal_move(2, 0);
   terminal_write("\033[2K");
   terminal_write(line);
}

static void raw_clear_cells(int row, int col, int width)
{
   int i;

   terminal_move(row, col);
   for (i = 0; i < width; i++)
      terminal_write(" ");
}

static void raw_fill_cells_attr(int row, int col, int width, attr_t attr)
{
   int i;

   terminal_move(row, col);
   for (i = 0; i < width; i++)
      terminal_write_attr(" ", attr);
}

static void raw_keycap_layout_offsets(int keycap_width, int offsets[5], int widths[5])
{
   offsets[0] = 0;
   offsets[1] = 1;
   offsets[2] = 1 + keycap_width;
   offsets[3] = 2 + keycap_width;
   offsets[4] = 3 + keycap_width;
   widths[0] = 1;
   widths[1] = keycap_width;
   widths[2] = 1;
   widths[3] = 1;
   widths[4] = 1;
}

static void raw_write_keycap_layout(int row, int col, int keycap_width,
                                    int highlight_index, int reverse_style)
{
   static const char *clusters[] = { "A", U8_KEYCAP_1, "B", " ", "A" };
   int offsets[5];
   int widths[5];
   int i;

   raw_keycap_layout_offsets(keycap_width, offsets, widths);
   for (i = 0; i < 5; i++)
      write_raw_cluster_at(row, col + offsets[i], clusters[i], widths[i],
                           reverse_style && i == highlight_index ? A_REVERSE : A_NORMAL);
}

static void raw_terminal_poc_step(ProbeConfig *cfg, const char *scenario,
                                  int row, int col, int paint_width,
                                  int cursor_width, int span_repaint,
                                  int reverse_style, int old_index,
                                  int new_index)
{
   static const char *clusters[] = { "A", U8_KEYCAP_1, "B", " ", "A" };
   static const char *names[] = { "A", "keycap", "B", "space", "next-A" };
   const char *repaint = span_repaint ? "span" : "cell";
   const char *style = reverse_style ? "reverse" : "marker";
   int paint_offsets[5];
   int paint_widths[5];
   int cursor_offsets[5];
   int cursor_widths[5];
   int row_cells;
   int cx;
   int tr = -1;
   int tc = -1;
   int expected_dsr_row;
   int expected_dsr_col;

   raw_keycap_layout_offsets(paint_width, paint_offsets, paint_widths);
   raw_keycap_layout_offsets(cursor_width, cursor_offsets, cursor_widths);
   row_cells = paint_offsets[4] + paint_widths[4];
   cx = col + cursor_offsets[new_index];
   expected_dsr_row = row + 1;
   expected_dsr_col = cx + 1;

   raw_poc_status(cfg, scenario, repaint, style, paint_width, cursor_width,
                  old_index, new_index, names[new_index]);
   if (span_repaint)
   {
      raw_clear_cells(row, col - 1, row_cells + 2);
      raw_write_keycap_layout(row, col, paint_width, new_index, reverse_style);
   }
   else
   {
      if (old_index >= 0)
      {
         raw_clear_cells(row, col + paint_offsets[old_index],
                         paint_widths[old_index]);
         write_raw_cluster_at(row, col + paint_offsets[old_index],
                              clusters[old_index], paint_widths[old_index],
                              A_NORMAL);
      }
      raw_clear_cells(row, col + paint_offsets[new_index],
                      paint_widths[new_index]);
      write_raw_cluster_at(row, col + paint_offsets[new_index],
                           clusters[new_index], paint_widths[new_index],
                           reverse_style ? A_REVERSE : A_NORMAL);
   }

   terminal_move(row, cx);
   draw_raw_target_marker(cfg, row, col, cursor_offsets[new_index]);
   if (query_terminal_cursor(cfg->timeout_ms, &tr, &tc) != 0)
   {
      tr = -1;
      tc = -1;
   }

   reportf(cfg,
           "raw_poc,%s,repaint=%s,style=%s,paint_width=%d,cursor_width=%d,old=%d,new=%d,target=%s,expected_dsr=%d:%d,dsr=%d:%d\n",
           scenario, repaint, style, paint_width, cursor_width, old_index,
           new_index, names[new_index], expected_dsr_row, expected_dsr_col,
           tr, tc);
   raw_pause_or_sleep(cfg);
}

static void run_raw_terminal_poc_probe(ProbeConfig *cfg)
{
   struct
   {
      const char *scenario;
      int paint_width;
      int cursor_width;
      int span_repaint;
      int reverse_style;
   } cases[] =
   {
      { "raw_width2_cell_reverse", 2, 2, 0, 1 },
      { "raw_width2_span_reverse", 2, 2, 1, 1 },
      { "raw_width3_span_reverse", 3, 3, 1, 1 },
      { "raw_width4_span_reverse", 4, 4, 1, 1 },
      { "raw_width2_span_marker", 2, 2, 1, 0 },
      { "raw_width3_span_marker", 3, 3, 1, 0 },
      { "raw_width4_span_marker", 4, 4, 1, 0 },
      { "raw_paint3_cursor2_marker", 3, 2, 1, 0 },
      { "raw_paint4_cursor2_marker", 4, 2, 1, 0 }
   };
   size_t i;

   terminal_clear_screen();
   terminal_show_cursor(0);
   reportf(cfg, "section=raw_terminal_poc\n");
   if (!cfg->no_visual)
   {
      terminal_move(0, 0);
      terminal_write("Raw ANSI-only keycap POC ");
      terminal_write(UTF8_TERMINAL_PROBE_VERSION);
      terminal_write(": no curses initialization, hardware cursor hidden");
      terminal_move(1, 0);
      terminal_write("Compare paint width, cursor width, and reverse-vs-marker styling");
      terminal_move(1, cfg->data_col);
      terminal_write("012345678901");
   }

   for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
   {
      int row = 4 + (int)i * 2;

      if (!cfg->no_visual)
      {
         terminal_move(row, 0);
         terminal_write(cases[i].scenario);
      }
      raw_write_keycap_layout(row, cfg->data_col, cases[i].paint_width, -1, 0);
      raw_terminal_poc_step(cfg, cases[i].scenario, row, cfg->data_col,
                            cases[i].paint_width, cases[i].cursor_width,
                            cases[i].span_repaint, cases[i].reverse_style,
                            -1, 0);
      raw_terminal_poc_step(cfg, cases[i].scenario, row, cfg->data_col,
                            cases[i].paint_width, cases[i].cursor_width,
                            cases[i].span_repaint, cases[i].reverse_style,
                            0, 1);
      raw_terminal_poc_step(cfg, cases[i].scenario, row, cfg->data_col,
                            cases[i].paint_width, cases[i].cursor_width,
                            cases[i].span_repaint, cases[i].reverse_style,
                            1, 2);
      raw_terminal_poc_step(cfg, cases[i].scenario, row, cfg->data_col,
                            cases[i].paint_width, cases[i].cursor_width,
                            cases[i].span_repaint, cases[i].reverse_style,
                            2, 3);
      raw_terminal_poc_step(cfg, cases[i].scenario, row, cfg->data_col,
                            cases[i].paint_width, cases[i].cursor_width,
                            cases[i].span_repaint, cases[i].reverse_style,
                            3, 4);
      raw_write_keycap_layout(row, cfg->data_col, cases[i].paint_width, -1, 0);
   }

   reportf(cfg, "\n");
   if (!cfg->no_visual)
   {
      terminal_move(25, 0);
      terminal_write(cfg->pause ? "press any key to continue" : "raw probe complete");
   }
   raw_pause_or_sleep(cfg);
   terminal_show_cursor(1);
}

typedef enum
{
   CURSES_CURSOR_REVERSE,
   CURSES_CURSOR_UNDERLINE,
   CURSES_CURSOR_MARKER,
   CURSES_CURSOR_HARDWARE,
   CURSES_CURSOR_CELL_BLOCK,
   CURSES_CURSOR_KEYCAP_BG2_BLOCK,
   CURSES_CURSOR_KEYCAP_BG3_BLOCK,
   CURSES_CURSOR_KEYCAP_BG3_UNDERLINE,
   CURSES_CURSOR_MASK2_BLOCK,
   CURSES_CURSOR_MASK3_BLOCK
} CursesCursorMode;

static const char *curses_cursor_mode_name(CursesCursorMode mode)
{
   switch (mode)
   {
      case CURSES_CURSOR_REVERSE:
         return "the-block";
      case CURSES_CURSOR_UNDERLINE:
         return "the-underline";
      case CURSES_CURSOR_MARKER:
         return "marker";
      case CURSES_CURSOR_HARDWARE:
         return "hardware";
      case CURSES_CURSOR_CELL_BLOCK:
         return "cell-block";
      case CURSES_CURSOR_KEYCAP_BG2_BLOCK:
         return "keycap-bg2-block";
      case CURSES_CURSOR_KEYCAP_BG3_BLOCK:
         return "keycap-bg3-block";
      case CURSES_CURSOR_KEYCAP_BG3_UNDERLINE:
         return "keycap-bg3-under";
      case CURSES_CURSOR_MASK2_BLOCK:
         return "mask2-block";
      case CURSES_CURSOR_MASK3_BLOCK:
         return "mask3-block";
      default:
         return "unknown";
   }
}

static attr_t curses_cursor_cluster_attr(CursesCursorMode mode)
{
   switch (mode)
   {
      case CURSES_CURSOR_REVERSE:
      case CURSES_CURSOR_KEYCAP_BG2_BLOCK:
      case CURSES_CURSOR_KEYCAP_BG3_BLOCK:
      case CURSES_CURSOR_MASK2_BLOCK:
      case CURSES_CURSOR_MASK3_BLOCK:
         return A_REVERSE;
      case CURSES_CURSOR_UNDERLINE:
      case CURSES_CURSOR_KEYCAP_BG3_UNDERLINE:
         return A_UNDERLINE;
      default:
         return A_NORMAL;
   }
}

static int curses_cursor_keycap_backdrop_width(CursesCursorMode mode)
{
   switch (mode)
   {
      case CURSES_CURSOR_KEYCAP_BG2_BLOCK:
         return 2;
      case CURSES_CURSOR_KEYCAP_BG3_BLOCK:
      case CURSES_CURSOR_KEYCAP_BG3_UNDERLINE:
         return 3;
      default:
         return 0;
   }
}

static int curses_cursor_mask_width(CursesCursorMode mode)
{
   switch (mode)
   {
      case CURSES_CURSOR_MASK2_BLOCK:
         return 2;
      case CURSES_CURSOR_MASK3_BLOCK:
         return 3;
      default:
         return 0;
   }
}

static void curses_clear_cells(int row, int col, int width, attr_t attr)
{
   int i;

   if (width <= 0 || row < 0 || row >= LINES || col >= COLS)
      return;
   if (col < 0)
   {
      width += col;
      col = 0;
   }
   if (width <= 0)
      return;
   if (col + width > COLS)
      width = COLS - col;
   move(row, col);
   for (i = 0; i < width; i++)
      addch(' ' | attr);
   wattrset(stdscr, A_NORMAL);
}

static int curses_write_cluster_at(ProbeMethod method, int row, int col,
                                   const char *utf8, int expected_width,
                                   attr_t attr)
{
   int rc;

   move(row, col);
   rc = write_utf8_with_method(method, utf8, attr);
   move(row, col + ((expected_width > 0) ? expected_width : 1));
   return rc;
}

static void curses_draw_cluster_layout(ProbeMethod method, int row, int col,
                                       const char *middle_cluster,
                                       int layout_width, int highlight_index,
                                       attr_t highlight_attr)
{
   const char *clusters[] = { "A", middle_cluster, "B", " ", "A" };
   int offsets[5];
   int widths[5];
   int i;

   raw_keycap_layout_offsets(layout_width, offsets, widths);
   for (i = 0; i < 5; i++)
      curses_write_cluster_at(method, row, col + offsets[i], clusters[i],
                              widths[i],
                              i == highlight_index ? highlight_attr : A_NORMAL);
}

static void curses_draw_target_marker(int row, int col, int target_offset)
{
   curses_clear_cells(row + 1, col - 1, 12, A_NORMAL);
   mvaddch(row + 1, col + target_offset, '^');
}

static int keycap_layout_clear_width(int layout_width, int repair_width)
{
   int offsets[5];
   int widths[5];
   int layout_cells;
   int repair_cells;

   raw_keycap_layout_offsets(layout_width, offsets, widths);
   layout_cells = offsets[4] + widths[4];
   repair_cells = offsets[1] + repair_width;
   return (layout_cells > repair_cells ? layout_cells : repair_cells) + 2;
}

static void curses_poc_clear_surface(int row, int col, int width)
{
   int clear_col = col - 2;
   int clear_width = width + 4;

   raw_clear_cells(row, clear_col, clear_width);
   raw_clear_cells(row + 1, clear_col, clear_width);
   curses_clear_cells(row, clear_col, clear_width, A_NORMAL);
   curses_clear_cells(row + 1, clear_col, clear_width, A_NORMAL);
}

static void raw_repair_cluster_layout(int row, int col,
                                      const char *middle_cluster,
                                      int layout_width, int repair_width,
                                      int highlight_index,
                                      attr_t highlight_attr)
{
   const char *clusters[] = { "A", middle_cluster, "B", " ", "A" };
   int offsets[5];
   int widths[5];
   int clear_width;
   int i;

   raw_keycap_layout_offsets(layout_width, offsets, widths);
   widths[1] = repair_width;
   clear_width = keycap_layout_clear_width(layout_width, repair_width);
   raw_clear_cells(row, col - 1, clear_width);
   for (i = 0; i < 5; i++)
      write_raw_cluster_at(row, col + offsets[i], clusters[i], widths[i],
                           i == highlight_index ? highlight_attr : A_NORMAL);
}

static void curses_poc_status(ProbeConfig *cfg, const char *scenario,
                              ProbeMethod method, int layout_width,
                              int repair_width, int cursor_width,
                              int span_repaint, CursesCursorMode cursor_mode,
                              int post_refresh_raw, int old_index,
                              int new_index, const char *target)
{
   char line[512];
   int limit;

   if (cfg->no_visual)
      return;
   move(2, 0);
   clrtoeol();
   if (LINES > 3)
   {
      move(3, 0);
      clrtoeol();
   }
   snprintf(line, sizeof(line),
            "%s old=%d new=%d target=%s layout=%d repair=%d cursor=%d %s postraw=%d",
            scenario, old_index, new_index, target, layout_width, repair_width,
            cursor_width, curses_cursor_mode_name(cursor_mode),
            post_refresh_raw);
   limit = COLS > 4 ? COLS - 4 : 0;
   if (limit > 118)
      limit = 118;
   mvaddnstr(2, 0, line, limit);
   clrtoeol();
   (void)method;
   (void)span_repaint;
}

static void curses_terminal_poc_step(ProbeConfig *cfg, const char *scenario,
                                     const char *middle_cluster,
                                     const char *middle_name,
                                     ProbeMethod method, int row, int col,
                                     int layout_width, int repair_width,
                                     int cursor_width, int span_repaint,
                                     CursesCursorMode cursor_mode,
                                     int post_refresh_raw, int old_index,
                                     int new_index)
{
   const char *clusters[] = { "A", middle_cluster, "B", " ", "A" };
   const char *names[] = { "A", middle_name, "B", "space", "next-A" };
   int layout_offsets[5];
   int layout_widths[5];
   int cursor_offsets[5];
   int cursor_widths[5];
   int target_col;
   int clear_width;
   int cy = -1;
   int cx = -1;
   int tr = -1;
   int tc = -1;
   attr_t cluster_cursor_attr = curses_cursor_cluster_attr(cursor_mode);
   int overlay_cell_block = cursor_mode == CURSES_CURSOR_CELL_BLOCK;
   int keycap_backdrop_width = curses_cursor_keycap_backdrop_width(cursor_mode);
   int mask_width = curses_cursor_mask_width(cursor_mode);

   raw_keycap_layout_offsets(layout_width, layout_offsets, layout_widths);
   raw_keycap_layout_offsets(cursor_width, cursor_offsets, cursor_widths);
   (void)cursor_widths;
   target_col = col + cursor_offsets[new_index];
   clear_width = keycap_layout_clear_width(layout_width, repair_width);
   if (new_index == 1 && (keycap_backdrop_width > 0 || mask_width > 0))
      cluster_cursor_attr = A_NORMAL;

   curses_poc_status(cfg, scenario, method, layout_width, repair_width,
                     cursor_width, span_repaint, cursor_mode, post_refresh_raw,
                     old_index, new_index, names[new_index]);
   if (span_repaint)
   {
      curses_poc_clear_surface(row, col, clear_width);
      curses_draw_cluster_layout(method, row, col, middle_cluster,
                                 layout_width, new_index,
                                 cluster_cursor_attr);
   }
   else
   {
      if (old_index >= 0)
      {
         int old_width = old_index == 1 ? repair_width : layout_widths[old_index];

         curses_clear_cells(row, col + layout_offsets[old_index], old_width,
                            A_NORMAL);
         curses_write_cluster_at(method, row, col + layout_offsets[old_index],
                                 clusters[old_index], layout_widths[old_index],
                                 A_NORMAL);
      }
      curses_clear_cells(row, col + layout_offsets[new_index],
                         new_index == 1 ? repair_width : layout_widths[new_index],
                         A_NORMAL);
      curses_write_cluster_at(method, row, col + layout_offsets[new_index],
                              clusters[new_index], layout_widths[new_index],
                              cluster_cursor_attr);
   }

   if (overlay_cell_block)
      curses_clear_cells(row, target_col, cursor_width, A_REVERSE);
   if (new_index == 1 && keycap_backdrop_width > 0)
   {
      curses_clear_cells(row, target_col, keycap_backdrop_width,
                         curses_cursor_cluster_attr(cursor_mode));
      curses_write_cluster_at(method, row, col + layout_offsets[1],
                              clusters[1], layout_widths[1], A_NORMAL);
   }
   if (new_index == 1 && mask_width > 0)
      curses_clear_cells(row, target_col, mask_width,
                         curses_cursor_cluster_attr(cursor_mode));
   if (cursor_mode == CURSES_CURSOR_MARKER)
      curses_draw_target_marker(row, col, cursor_offsets[new_index]);
   else
      curses_clear_cells(row + 1, col - 1, 12, A_NORMAL);

   if (cursor_mode == CURSES_CURSOR_HARDWARE)
      curs_set(1);
   else
      curs_set(0);
   move(row, target_col);
   getyx(stdscr, cy, cx);
   touchline(stdscr, row, 2);
   if (row + 1 < LINES)
      touchline(stdscr, row + 1, 1);
   refresh();

   if (post_refresh_raw)
   {
      raw_repair_cluster_layout(row, col, middle_cluster, layout_width,
                                repair_width, new_index, cluster_cursor_attr);
      if (overlay_cell_block)
         raw_fill_cells_attr(row, target_col, cursor_width, A_REVERSE);
      if (new_index == 1 && keycap_backdrop_width > 0)
      {
         raw_fill_cells_attr(row, target_col, keycap_backdrop_width,
                             curses_cursor_cluster_attr(cursor_mode));
         write_raw_cluster_at(row, col + layout_offsets[1], clusters[1],
                              layout_widths[1], A_NORMAL);
      }
      if (new_index == 1 && mask_width > 0)
         raw_fill_cells_attr(row, target_col, mask_width,
                             curses_cursor_cluster_attr(cursor_mode));
      if (cursor_mode == CURSES_CURSOR_MARKER)
         draw_raw_target_marker(cfg, row, col, cursor_offsets[new_index]);
      else
         terminal_move(row, target_col);
   }

   if (query_terminal_cursor(cfg->timeout_ms, &tr, &tc) != 0)
   {
      tr = -1;
      tc = -1;
   }
   reportf(cfg,
           "curses_poc,%s,method=%s,layout_width=%d,repair_width=%d,cursor_width=%d,repaint=%s,style=%s,post_refresh_raw=%d,old=%d,new=%d,target=%s,curses_expected=%d:%d,dsr=%d:%d\n",
           scenario, method_name(method), layout_width, repair_width,
           cursor_width, span_repaint ? "span" : "cell",
           curses_cursor_mode_name(cursor_mode), post_refresh_raw,
           old_index, new_index, names[new_index], cy, cx, tr, tc);
   if (cfg->pause)
      getch();
   else
      napms(350);
}

static const char *selector_alias_class(const char *selector)
{
   if (selector == NULL)
      return NULL;
   if (strcmp(selector, "flag") == 0)
      return "regional-flag";
   if (strcmp(selector, "keycap") == 0 || strcmp(selector, "keycaps") == 0)
      return "keycap";
   if (strcmp(selector, "cjk") == 0)
      return "wide";
   if (strcmp(selector, "presentation") == 0)
      return "emoji-variation";
   return NULL;
}

static int sample_matches_selector(const ProbeSample *sample, const char *selector)
{
   const char *klass;

   if (selector == NULL || strcmp(selector, "focus") == 0)
      return strcmp(sample->klass, "keycap") == 0
          || strcmp(sample->klass, "regional-flag") == 0;
   if (strcmp(selector, "all") == 0)
      return 1;
   if (strcmp(selector, "flags") == 0)
      return strcmp(sample->klass, "regional-flag") == 0
          || strcmp(sample->klass, "tag-flag") == 0;
   if (strcmp(selector, "variation") == 0)
      return strcmp(sample->klass, "text-variation") == 0
          || strcmp(sample->klass, "emoji-variation") == 0;
   if (strcmp(selector, "zwj") == 0 || strcmp(selector, "joiner") == 0)
      return strstr(sample->klass, "zwj") != NULL;
   if (strcmp(sample->name, selector) == 0 || strcmp(sample->klass, selector) == 0)
      return 1;
   klass = selector_alias_class(selector);
   return klass != NULL && strcmp(sample->klass, klass) == 0;
}

static const ProbeSample *find_first_sample(ProbeConfig *cfg, const char *selector)
{
   size_t i;

   for (i = 0; i < cfg->sample_count; i++)
      if (sample_matches_selector(&cfg->samples[i], selector))
         return &cfg->samples[i];
   return NULL;
}

static attr_t utfvis_cursor_attr(void)
{
   if (has_colors())
   {
      start_color();
      init_pair(1, COLOR_WHITE, COLOR_BLUE);
      return COLOR_PAIR(1);
   }
   return A_REVERSE;
}

static void utfvis_draw_header(const char *selector, int sample_col, int sample_stride)
{
   static const char *targets[] = { "A", "cluster", "B", "space", "last A" };
   size_t t;

   erase();
   mvprintw(0, 0, "utfvis %s %s: static THE-style background cursor",
            selector ? selector : "focus", UTF8_TERMINAL_PROBE_VERSION);
   mvprintw(1, 0, "Rows paint A-cluster-B-space-A. L=layout cells, C=cursor background cells.");
   for (t = 0; t < sizeof(targets) / sizeof(targets[0]); t++)
      mvprintw(3, sample_col + (int)t * sample_stride, "%-10s", targets[t]);
}

static void run_utfvis_probe(ProbeConfig *cfg, const char *selector)
{
   static const char *targets[] = { "A", "cluster", "B", "space", "last A" };
   static const int target_indexes[] = { 0, 1, 2, 3, 4 };
   size_t i;
   size_t t;
   int sample_col = cfg->data_col;
   int sample_stride = 14;
   int sample_width = 11;
   int row = 5;
   int rows = 0;
   attr_t cursor_attr = utfvis_cursor_attr();

   if (COLS > 0 && sample_col + (4 * sample_stride) + sample_width >= COLS)
      sample_col = 14;
   if (COLS > 0 && sample_col + (4 * sample_stride) + sample_width >= COLS)
      sample_stride = 13;
   if (sample_width >= sample_stride)
      sample_width = sample_stride - 1;

   curs_set(0);
   reportf(cfg, "section=utfvis selector=%s\n", selector ? selector : "focus");
   if (!cfg->no_visual)
      utfvis_draw_header(selector, sample_col, sample_stride);

   for (i = 0; i < cfg->sample_count; i++)
   {
      const ProbeSample *sample = &cfg->samples[i];
      int base_width;
      int layout_widths[3];
      int cursor_widths[3];
      int variant;

      if (!sample_matches_selector(sample, selector))
         continue;

      base_width = sample->expected_policy_width > 0
                 ? sample->expected_policy_width : 1;
      layout_widths[0] = base_width;
      cursor_widths[0] = base_width;
      layout_widths[1] = base_width + 1;
      cursor_widths[1] = base_width;
      layout_widths[2] = base_width + 1;
      cursor_widths[2] = base_width + 1;

      for (variant = 0; variant < 3; variant++)
      {
         int layout_width = layout_widths[variant];
         int cursor_width = cursor_widths[variant];

         if (!cfg->no_visual && row >= LINES - 2)
         {
            finish_probe_page(cfg);
            utfvis_draw_header(selector, sample_col, sample_stride);
            row = 5;
         }

         if (!cfg->no_visual)
         {
            int label_width = sample_col - 2;

            if (label_width < 12)
               label_width = 12;
            mvprintw(row, 0, "%-*.*s", label_width, label_width, "");
            mvprintw(row, 0, "%.*s L%d C%d", label_width - 6, sample->name,
                     layout_width, cursor_width);
         }

         for (t = 0; t < sizeof(target_indexes) / sizeof(target_indexes[0]); t++)
         {
            const char *clusters[] = { "A", sample->utf8, "B", " ", "A" };
            int col = sample_col + (int)t * sample_stride;
            int target = target_indexes[t];
            int offsets[5];
            int widths[5];
            int j;

            raw_keycap_layout_offsets(layout_width, offsets, widths);
            if (!cfg->no_visual)
            {
               int target_cursor_width = target == 1 ? cursor_width : widths[target];

               curses_clear_cells(row, col - 1, sample_width + 2, A_NORMAL);
               for (j = 0; j < 5; j++)
                  curses_write_cluster_at(METHOD_WADDWSTR, row, col + offsets[j],
                                          clusters[j], widths[j], A_NORMAL);
               curses_clear_cells(row, col + offsets[target], target_cursor_width,
                                  cursor_attr);
               curses_write_cluster_at(METHOD_WADDWSTR, row, col + offsets[target],
                                       clusters[target], widths[target], cursor_attr);
            }
            reportf(cfg,
                    "utfvis,%s,target=%s,layout_width=%d,cursor_width=%d,style=background\n",
                    sample->name, targets[t], layout_width, cursor_width);
         }
         row++;
         rows++;
      }
   }

   if (rows == 0 && !cfg->no_visual)
      mvprintw(row, 0, "No samples matched '%s'", selector ? selector : "");
   refresh();
   curs_set(1);
   reportf(cfg, "\n");
}

static void run_curses_terminal_poc_probe(ProbeConfig *cfg)
{
   run_utfvis_probe(cfg, "focus");
}

static void testcursor_offsets(int layout_width, int offsets[7], int widths[7])
{
   offsets[0] = 0;
   offsets[1] = 1;
   offsets[2] = 1 + layout_width;
   offsets[3] = 2 + layout_width;
   offsets[4] = 3 + layout_width;
   offsets[5] = 4 + layout_width;
   offsets[6] = 4 + (2 * layout_width);
   widths[0] = 1;
   widths[1] = layout_width;
   widths[2] = 1;
   widths[3] = 1;
   widths[4] = 1;
   widths[5] = layout_width;
   widths[6] = 1;
}

static int testcursor_mode_is(const ProbeConfig *cfg, const char *mode)
{
   const char *active = cfg->testcursor_mode ? cfg->testcursor_mode : "frame";

   return strcmp(active, mode) == 0;
}

static const char *testcursor_mode_name(const ProbeConfig *cfg)
{
   return cfg->testcursor_mode ? cfg->testcursor_mode : "frame";
}

static int testcursor_mode_flashfrom(const char *mode, int *first_target)
{
   const char *prefix = "flashfrom";
   size_t prefix_len = strlen(prefix);

   if (mode == NULL || strncmp(mode, prefix, prefix_len) != 0)
      return 0;
   if (mode[prefix_len] < '0' || mode[prefix_len] > '6'
   ||  mode[prefix_len + 1] != '\0')
      return 0;
   *first_target = mode[prefix_len] - '0';
   return 1;
}

static int testcursor_mode_needs_base(const ProbeConfig *cfg)
{
   int first_target = 0;

   return testcursor_mode_is(cfg, "cell")
       || testcursor_mode_is(cfg, "flashcell")
       || testcursor_mode_is(cfg, "flashpair")
       || testcursor_mode_flashfrom(testcursor_mode_name(cfg), &first_target);
}

static int testcursor_mode_valid(const char *mode)
{
   int first_target = 0;

   return mode == NULL
       || strcmp(mode, "frame") == 0
       || strcmp(mode, "cell") == 0
       || strcmp(mode, "line") == 0
       || strcmp(mode, "flashline") == 0
       || strcmp(mode, "flashcell") == 0
       || strcmp(mode, "flashpair") == 0
       || testcursor_mode_flashfrom(mode, &first_target);
}

static const char *testchain_mode_name(const ProbeConfig *cfg)
{
   return cfg->testchain_mode ? cfg->testchain_mode : "flashbackcluster1";
}

static int testchain_mode_is(const ProbeConfig *cfg, const char *mode)
{
   return strcmp(testchain_mode_name(cfg), mode) == 0;
}

static int testchain_mode_flashback(const char *mode, int *back_clusters)
{
   const char *prefix = "flashbackcluster";
   size_t prefix_len = strlen(prefix);

   if (mode == NULL || strncmp(mode, prefix, prefix_len) != 0)
      return 0;
   if (mode[prefix_len] < '0' || mode[prefix_len] > '6'
   ||  mode[prefix_len + 1] != '\0')
      return 0;
   *back_clusters = mode[prefix_len] - '0';
   return 1;
}

static int testchain_mode_valid(const char *mode)
{
   int back_clusters = 0;

   return mode == NULL
       || strcmp(mode, "line") == 0
       || strcmp(mode, "cell") == 0
       || strcmp(mode, "paintfirstcluster") == 0
       || strcmp(mode, "touchfirstcluster") == 0
       || strcmp(mode, "clearfirstcluster") == 0
       || strcmp(mode, "flashfirstfast") == 0
       || strcmp(mode, "flashfirstcluster") == 0
       || strcmp(mode, "flashwhole") == 0
       || testchain_mode_flashback(mode, &back_clusters);
}

static int testcursor_is_cluster_target(int target)
{
   return target == 1 || target == 5;
}

static int testcursor_cursor_footprint(int target, int layout_width,
                                       int cursor_width)
{
   return testcursor_is_cluster_target(target) ? cursor_width : layout_width;
}

static int testcursor_total_width(int layout_width)
{
   return 5 + (2 * layout_width);
}

static void draw_testcursor_base(const ProbeSample *sample, int row, int col,
                                 int layout_width)
{
   const char *clusters[] = { "A", sample->utf8, "B", " ", "A", sample->utf8, "B" };
   int offsets[7];
   int widths[7];
   int total_width;
   int i;

   testcursor_offsets(layout_width, offsets, widths);
   total_width = testcursor_total_width(layout_width);
   curses_clear_cells(row, col - 1, total_width + 2, A_NORMAL);
   for (i = 0; i < 7; i++)
      curses_write_cluster_at(METHOD_WADDWSTR, row, col + offsets[i],
                              clusters[i], widths[i], A_NORMAL);
}

static void draw_testcursor_target(const ProbeSample *sample, int row, int col,
                                   int layout_width, int cursor_width,
                                   int target, attr_t attr)
{
   const char *clusters[] = { "A", sample->utf8, "B", " ", "A", sample->utf8, "B" };
   int offsets[7];
   int widths[7];
   int target_cursor_width;

   testcursor_offsets(layout_width, offsets, widths);
   target_cursor_width = testcursor_is_cluster_target(target)
                       ? cursor_width : widths[target];
   curses_clear_cells(row, col + offsets[target], target_cursor_width,
                      attr);
   curses_write_cluster_at(METHOD_WADDWSTR, row, col + offsets[target],
                           clusters[target], widths[target], attr);
}

static void draw_testcursor_frame(const ProbeSample *sample, int row, int col,
                                  int layout_width, int cursor_width,
                                  int target, attr_t cursor_attr,
                                  int force_line)
{
   draw_testcursor_base(sample, row, col, layout_width);
   draw_testcursor_target(sample, row, col, layout_width, cursor_width,
                          target, cursor_attr);
   if (force_line)
      touchline(stdscr, row, 1);
}

static void draw_testcursor_step(const ProbeSample *sample, int row, int col,
                                 int layout_width, int cursor_width,
                                 int old_target, int new_target,
                                 attr_t cursor_attr)
{
   if (old_target >= 0)
      draw_testcursor_target(sample, row, col, layout_width, cursor_width,
                             old_target, A_NORMAL);
   draw_testcursor_target(sample, row, col, layout_width, cursor_width,
                          new_target, cursor_attr);
}

static void testcursor_target_range_cells(int layout_width, int cursor_width,
                                          int first_target, int last_target,
                                          int *start_cell, int *width_cells)
{
   int offsets[7];
   int widths[7];
   int first = first_target < last_target ? first_target : last_target;
   int last = first_target > last_target ? first_target : last_target;
   int start;
   int end;
   int i;

   if (first < 0)
      first = 0;
   if (last > 6)
      last = 6;
   testcursor_offsets(layout_width, offsets, widths);
   start = offsets[first];
   end = start + widths[first];
   for (i = first; i <= last; i++)
   {
      int footprint = widths[i];
      int cluster_footprint = testcursor_cursor_footprint(i, widths[i],
                                                          cursor_width);

      if (cluster_footprint > footprint)
         footprint = cluster_footprint;
      if (offsets[i] + footprint > end)
         end = offsets[i] + footprint;
   }
   *start_cell = start;
   *width_cells = end - start;
}

static void draw_testcursor_span_normal(const ProbeSample *sample, int row,
                                        int col, int layout_width,
                                        int first_target, int last_target)
{
   const char *clusters[] = { "A", sample->utf8, "B", " ", "A", sample->utf8, "B" };
   int offsets[7];
   int widths[7];
   int first = first_target < last_target ? first_target : last_target;
   int last = first_target > last_target ? first_target : last_target;
   int i;

   if (first < 0)
      first = 0;
   if (last > 6)
      last = 6;
   testcursor_offsets(layout_width, offsets, widths);
   for (i = first; i <= last; i++)
      curses_write_cluster_at(METHOD_WADDWSTR, row, col + offsets[i],
                              clusters[i], widths[i], A_NORMAL);
}

static void draw_testcursor_flash_frame(const ProbeSample *sample, int row,
                                        int col, int layout_width,
                                        int cursor_width, int target,
                                        attr_t cursor_attr)
{
   int total_width = testcursor_total_width(layout_width);

   curses_clear_cells(row, col - 1, total_width + 2, A_NORMAL);
   touchline(stdscr, row, 1);
   refresh();
   napms(25);
   draw_testcursor_frame(sample, row, col, layout_width, cursor_width,
                         target, cursor_attr, 1);
}

static void draw_testcursor_flash_span(const ProbeSample *sample, int row,
                                       int col, int layout_width,
                                       int cursor_width, int old_target,
                                       int new_target, attr_t cursor_attr,
                                       int trailing_targets)
{
   int first_target;
   int last_target;
   int start_cell;
   int width_cells;

   first_target = old_target >= 0 && old_target < new_target
                ? old_target : new_target;
   last_target = old_target > new_target ? old_target : new_target;
   last_target += trailing_targets;
   if (last_target > 6)
      last_target = 6;
   testcursor_target_range_cells(layout_width, cursor_width, first_target,
                                 last_target, &start_cell, &width_cells);
   curses_clear_cells(row, col + start_cell, width_cells, A_NORMAL);
   touchline(stdscr, row, 1);
   refresh();
   napms(25);
   draw_testcursor_span_normal(sample, row, col, layout_width,
                               first_target, last_target);
   draw_testcursor_target(sample, row, col, layout_width, cursor_width,
                          new_target, cursor_attr);
}

static void draw_testcursor_flash_suffix(const ProbeSample *sample, int row,
                                         int col, int layout_width,
                                         int cursor_width, int old_target,
                                         int first_target, int new_target,
                                         attr_t cursor_attr)
{
   int start_cell;
   int width_cells;

   if (old_target >= 0 && old_target < first_target)
      draw_testcursor_target(sample, row, col, layout_width, cursor_width,
                             old_target, A_NORMAL);
   testcursor_target_range_cells(layout_width, cursor_width, first_target, 6,
                                 &start_cell, &width_cells);
   curses_clear_cells(row, col + start_cell, width_cells, A_NORMAL);
   touchline(stdscr, row, 1);
   refresh();
   napms(25);
   draw_testcursor_span_normal(sample, row, col, layout_width, first_target, 6);
   draw_testcursor_target(sample, row, col, layout_width, cursor_width,
                          new_target, cursor_attr);
}

static void run_testcursor_probe(ProbeConfig *cfg)
{
   static const int path[] = { 0, 1, 2, 3, 4, 5, 6, 5, 4, 3, 2, 1 };
   static const char *names[] = { "A1", "cluster1", "B1", "space", "A2", "cluster2", "B2" };
   const ProbeSample *sample = find_first_sample(cfg, cfg->testcursor_selector);
   attr_t cursor_attr = utfvis_cursor_attr();
   int col = cfg->data_col;
   int row = 7;
   int delay = cfg->timeout_ms > 0 ? cfg->timeout_ms : 200;
   int step = 0;
   int old_target = -1;
   int flashfrom_target = -1;

   reportf(cfg, "section=testcursor selector=%s layout_width=%d cursor_width=%d mode=%s\n",
           cfg->testcursor_selector ? cfg->testcursor_selector : "",
           cfg->testcursor_layout_width, cfg->testcursor_cursor_width,
           testcursor_mode_name(cfg));
   if (sample == NULL)
   {
      reportf(cfg, "testcursor,error=no-sample\n\n");
      if (!cfg->no_visual)
         mvprintw(0, 0, "No sample matched '%s'", cfg->testcursor_selector);
      return;
   }

   curs_set(0);
   if (cfg->no_visual)
   {
      size_t i;

      for (i = 0; i < sizeof(path) / sizeof(path[0]); i++)
         reportf(cfg, "testcursor,%s,target=%s,layout_width=%d,cursor_width=%d,mode=%s\n",
                 sample->name, names[path[i]], cfg->testcursor_layout_width,
                 cfg->testcursor_cursor_width, testcursor_mode_name(cfg));
      reportf(cfg, "\n");
      curs_set(1);
      return;
   }

   erase();
   mvprintw(0, 0, "testcursor %s L%d C%d mode=%s %s: press any key to stop",
            cfg->testcursor_selector, cfg->testcursor_layout_width,
            cfg->testcursor_cursor_width, testcursor_mode_name(cfg),
            UTF8_TERMINAL_PROBE_VERSION);
   mvprintw(1, 0, "Pattern is A-cluster-B-space-A-cluster-B.");
   (void)testcursor_mode_flashfrom(testcursor_mode_name(cfg), &flashfrom_target);
   if (testcursor_mode_needs_base(cfg))
      draw_testcursor_base(sample, row, col, cfg->testcursor_layout_width);
   nodelay(stdscr, TRUE);
   for (;;)
   {
      int target = path[step % (int)(sizeof(path) / sizeof(path[0]))];

      mvprintw(3, 0, "sample=%s class=%s target=%-9s step=%d   ",
               sample->name, sample->klass, names[target], step);
      if (testcursor_mode_is(cfg, "cell"))
      {
         draw_testcursor_step(sample, row, col, cfg->testcursor_layout_width,
                              cfg->testcursor_cursor_width, old_target, target,
                              cursor_attr);
      }
      else if (testcursor_mode_is(cfg, "flashline"))
      {
         draw_testcursor_flash_frame(sample, row, col,
                                     cfg->testcursor_layout_width,
                                     cfg->testcursor_cursor_width, target,
                                     cursor_attr);
      }
      else if (testcursor_mode_is(cfg, "flashcell"))
      {
         draw_testcursor_flash_span(sample, row, col,
                                    cfg->testcursor_layout_width,
                                    cfg->testcursor_cursor_width, old_target,
                                    target, cursor_attr, 0);
      }
      else if (testcursor_mode_is(cfg, "flashpair"))
      {
         draw_testcursor_flash_span(sample, row, col,
                                    cfg->testcursor_layout_width,
                                    cfg->testcursor_cursor_width, old_target,
                                    target, cursor_attr, 1);
      }
      else if (flashfrom_target >= 0)
      {
         draw_testcursor_flash_suffix(sample, row, col,
                                      cfg->testcursor_layout_width,
                                      cfg->testcursor_cursor_width,
                                      old_target, flashfrom_target, target,
                                      cursor_attr);
      }
      else
      {
         draw_testcursor_frame(sample, row, col, cfg->testcursor_layout_width,
                               cfg->testcursor_cursor_width, target, cursor_attr,
                               testcursor_mode_is(cfg, "line"));
      }
      refresh();
      reportf(cfg, "testcursor,%s,target=%s,layout_width=%d,cursor_width=%d,mode=%s\n",
              sample->name, names[target], cfg->testcursor_layout_width,
              cfg->testcursor_cursor_width, testcursor_mode_name(cfg));
      if (getch() != ERR)
         break;
      napms(delay);
      old_target = target;
      step++;
   }
   nodelay(stdscr, FALSE);
   curs_set(1);
   reportf(cfg, "\n");
}

#define TESTCHAIN_TARGETS 21

static int testchain_is_cluster_target(int target)
{
   return target == 3
       || target == 6
       || target == 9
       || target == 10
       || target == 11
       || target == 14
       || target == 17;
}

static int testchain_first_cluster_target(void)
{
   int target;

   for (target = 0; target < TESTCHAIN_TARGETS; target++)
      if (testchain_is_cluster_target(target))
         return target;
   return 0;
}

static const char *testchain_item_text(const ProbeSample *sample, int target)
{
   static const char *ascii_targets[TESTCHAIN_TARGETS] =
   {
      "X", "X",
      "A", NULL, "B",
      "A", NULL, "B",
      "A", NULL, NULL, NULL, "B",
      "A", NULL, "B",
      "A", NULL, "B",
      "X", "X"
   };

   if (testchain_is_cluster_target(target))
      return sample->utf8;
   if (target < 0 || target >= TESTCHAIN_TARGETS)
      return "?";
   return ascii_targets[target] ? ascii_targets[target] : "?";
}

static const char *testchain_target_name(int target)
{
   static const char *names[TESTCHAIN_TARGETS] =
   {
      "leadX1", "leadX2",
      "A1", "cluster1", "B1",
      "A2", "cluster2", "B2",
      "A3", "cluster3", "cluster4", "cluster5", "B5",
      "A6", "cluster6", "B6",
      "A7", "cluster7", "B7",
      "trailX1", "trailX2"
   };

   if (target < 0 || target >= TESTCHAIN_TARGETS)
      return "?";
   return names[target];
}

static void testchain_offsets(int layout_width, int offsets[TESTCHAIN_TARGETS],
                              int widths[TESTCHAIN_TARGETS])
{
   int target;
   int offset = 0;

   for (target = 0; target < TESTCHAIN_TARGETS; target++)
   {
      offsets[target] = offset;
      widths[target] = testchain_is_cluster_target(target) ? layout_width : 1;
      offset += widths[target];
   }
}

static int testchain_total_width(int layout_width)
{
   return 14 + (7 * layout_width);
}

static void draw_testchain_base(const ProbeSample *sample, int row, int col,
                                int layout_width)
{
   int offsets[TESTCHAIN_TARGETS];
   int widths[TESTCHAIN_TARGETS];
   int target;

   testchain_offsets(layout_width, offsets, widths);
   curses_clear_cells(row, col - 1, testchain_total_width(layout_width) + 2,
                      A_NORMAL);
   for (target = 0; target < TESTCHAIN_TARGETS; target++)
      curses_write_cluster_at(METHOD_WADDWSTR, row, col + offsets[target],
                              testchain_item_text(sample, target),
                              widths[target], A_NORMAL);
}

static void draw_testchain_target(const ProbeSample *sample, int row, int col,
                                  int layout_width, int cursor_width,
                                  int target, attr_t attr)
{
   int offsets[TESTCHAIN_TARGETS];
   int widths[TESTCHAIN_TARGETS];
   int cursor_footprint;

   testchain_offsets(layout_width, offsets, widths);
   cursor_footprint = testchain_is_cluster_target(target)
                    ? cursor_width : widths[target];
   curses_clear_cells(row, col + offsets[target], cursor_footprint, attr);
   curses_write_cluster_at(METHOD_WADDWSTR, row, col + offsets[target],
                           testchain_item_text(sample, target),
                           widths[target], attr);
}

static void draw_testchain_frame(const ProbeSample *sample, int row, int col,
                                 int layout_width, int cursor_width,
                                 int target, attr_t cursor_attr,
                                 int force_line)
{
   draw_testchain_base(sample, row, col, layout_width);
   draw_testchain_target(sample, row, col, layout_width, cursor_width, target,
                         cursor_attr);
   if (force_line)
      touchline(stdscr, row, 1);
}

static void draw_testchain_step(const ProbeSample *sample, int row, int col,
                                int layout_width, int cursor_width,
                                int old_target, int new_target,
                                attr_t cursor_attr)
{
   if (old_target >= 0)
      draw_testchain_target(sample, row, col, layout_width, cursor_width,
                            old_target, A_NORMAL);
   draw_testchain_target(sample, row, col, layout_width, cursor_width,
                         new_target, cursor_attr);
}

static void testchain_target_range_cells(int layout_width, int cursor_width,
                                         int first_target, int last_target,
                                         int *start_cell, int *width_cells)
{
   int offsets[TESTCHAIN_TARGETS];
   int widths[TESTCHAIN_TARGETS];
   int first = first_target < last_target ? first_target : last_target;
   int last = first_target > last_target ? first_target : last_target;
   int start;
   int end;
   int target;

   if (first < 0)
      first = 0;
   if (last >= TESTCHAIN_TARGETS)
      last = TESTCHAIN_TARGETS - 1;
   testchain_offsets(layout_width, offsets, widths);
   start = offsets[first];
   end = start + widths[first];
   for (target = first; target <= last; target++)
   {
      int footprint = widths[target];

      if (testchain_is_cluster_target(target) && cursor_width > footprint)
         footprint = cursor_width;
      if (offsets[target] + footprint > end)
         end = offsets[target] + footprint;
   }
   *start_cell = start;
   *width_cells = end - start;
}

static void draw_testchain_span_normal(const ProbeSample *sample, int row,
                                       int col, int layout_width,
                                       int first_target, int last_target)
{
   int offsets[TESTCHAIN_TARGETS];
   int widths[TESTCHAIN_TARGETS];
   int first = first_target < last_target ? first_target : last_target;
   int last = first_target > last_target ? first_target : last_target;
   int target;

   if (first < 0)
      first = 0;
   if (last >= TESTCHAIN_TARGETS)
      last = TESTCHAIN_TARGETS - 1;
   testchain_offsets(layout_width, offsets, widths);
   for (target = first; target <= last; target++)
      curses_write_cluster_at(METHOD_WADDWSTR, row, col + offsets[target],
                              testchain_item_text(sample, target),
                              widths[target], A_NORMAL);
}

static int testchain_flash_start(int old_target, int new_target,
                                 int back_clusters)
{
   int start = old_target >= 0 && old_target < new_target
             ? old_target : new_target;
   int target;

   if (back_clusters <= 0)
      return start < 0 ? 0 : start;
   target = start;
   if (target >= 0 && testchain_is_cluster_target(target))
      back_clusters--;
   while (back_clusters > 0 && target > 0)
   {
      target--;
      if (testchain_is_cluster_target(target))
      {
         start = target;
         back_clusters--;
      }
   }
   if (back_clusters > 0)
      start = 0;
   if (start < 0)
      start = 0;
   return start;
}

static void draw_testchain_flashback(const ProbeSample *sample, int row,
                                     int col, int layout_width,
                                     int cursor_width, int old_target,
                                     int new_target, int back_clusters,
                                     attr_t cursor_attr)
{
   int start_target = testchain_flash_start(old_target, new_target,
                                            back_clusters);
   int start_cell;
   int width_cells;

   if (old_target >= 0 && old_target < start_target)
      draw_testchain_target(sample, row, col, layout_width, cursor_width,
                            old_target, A_NORMAL);
   testchain_target_range_cells(layout_width, cursor_width, start_target,
                                TESTCHAIN_TARGETS - 1, &start_cell,
                                &width_cells);
   curses_clear_cells(row, col + start_cell, width_cells, A_NORMAL);
   touchline(stdscr, row, 1);
   refresh();
   napms(25);
   draw_testchain_span_normal(sample, row, col, layout_width, start_target,
                              TESTCHAIN_TARGETS - 1);
   draw_testchain_target(sample, row, col, layout_width, cursor_width,
                         new_target, cursor_attr);
}

static void draw_testchain_flashfrom(const ProbeSample *sample, int row,
                                     int col, int layout_width,
                                     int cursor_width, int old_target,
                                     int new_target, int start_target,
                                     attr_t cursor_attr)
{
   int start_cell;
   int width_cells;

   if (start_target < 0)
      start_target = 0;
   if (start_target >= TESTCHAIN_TARGETS)
      start_target = TESTCHAIN_TARGETS - 1;
   if (old_target >= 0 && old_target < start_target)
      draw_testchain_target(sample, row, col, layout_width, cursor_width,
                            old_target, A_NORMAL);
   testchain_target_range_cells(layout_width, cursor_width, start_target,
                                TESTCHAIN_TARGETS - 1, &start_cell,
                                &width_cells);
   curses_clear_cells(row, col + start_cell, width_cells, A_NORMAL);
   touchline(stdscr, row, 1);
   refresh();
   napms(25);
   draw_testchain_span_normal(sample, row, col, layout_width, start_target,
                              TESTCHAIN_TARGETS - 1);
   draw_testchain_target(sample, row, col, layout_width, cursor_width,
                         new_target, cursor_attr);
}

static void draw_testchain_resetfrom(const ProbeSample *sample, int row,
                                     int col, int layout_width,
                                     int cursor_width, int old_target,
                                     int new_target, int start_target,
                                     attr_t cursor_attr, int clear_first,
                                     int separate_refresh, int pause_ms,
                                     int touch_after_repaint)
{
   int start_cell;
   int width_cells;

   if (start_target < 0)
      start_target = 0;
   if (start_target >= TESTCHAIN_TARGETS)
      start_target = TESTCHAIN_TARGETS - 1;
   if (old_target >= 0 && old_target < start_target)
      draw_testchain_target(sample, row, col, layout_width, cursor_width,
                            old_target, A_NORMAL);
   testchain_target_range_cells(layout_width, cursor_width, start_target,
                                TESTCHAIN_TARGETS - 1, &start_cell,
                                &width_cells);
   if (clear_first)
   {
      curses_clear_cells(row, col + start_cell, width_cells, A_NORMAL);
      touchline(stdscr, row, 1);
   }
   if (separate_refresh)
   {
      refresh();
      if (pause_ms > 0)
         napms(pause_ms);
   }
   draw_testchain_span_normal(sample, row, col, layout_width, start_target,
                              TESTCHAIN_TARGETS - 1);
   draw_testchain_target(sample, row, col, layout_width, cursor_width,
                         new_target, cursor_attr);
   if (touch_after_repaint)
      touchline(stdscr, row, 1);
}

static int testchain_path_length(void)
{
   return (TESTCHAIN_TARGETS * 2) - 2;
}

static int testchain_path_target(int step)
{
   int period = testchain_path_length();
   int index = step % period;

   return index < TESTCHAIN_TARGETS ? index : period - index;
}

static void run_testchain_probe(ProbeConfig *cfg)
{
   const ProbeSample *sample = find_first_sample(cfg, cfg->testchain_selector);
   attr_t cursor_attr = utfvis_cursor_attr();
   int col = cfg->data_col;
   int row = 7;
   int delay = cfg->timeout_ms > 0 ? cfg->timeout_ms : 200;
   int step = 0;
   int old_target = -1;
   int back_clusters = 1;
   int flashback = testchain_mode_flashback(testchain_mode_name(cfg),
                                            &back_clusters);
   int paintfirst = testchain_mode_is(cfg, "paintfirstcluster");
   int touchfirst = testchain_mode_is(cfg, "touchfirstcluster");
   int clearfirst = testchain_mode_is(cfg, "clearfirstcluster");
   int flashfirstfast = testchain_mode_is(cfg, "flashfirstfast");
   int flashfirst = testchain_mode_is(cfg, "flashfirstcluster");
   int flashwhole = testchain_mode_is(cfg, "flashwhole");
   int firststrategy = paintfirst || touchfirst || clearfirst
                    || flashfirstfast || flashfirst;

   reportf(cfg, "section=testchain selector=%s layout_width=%d cursor_width=%d mode=%s pattern=XX-A-cluster-B-A-cluster-B-A-cluster-cluster-cluster-B-A-cluster-B-A-cluster-B-XX\n",
           cfg->testchain_selector ? cfg->testchain_selector : "",
           cfg->testchain_layout_width, cfg->testchain_cursor_width,
           testchain_mode_name(cfg));
   if (sample == NULL)
   {
      reportf(cfg, "testchain,error=no-sample\n\n");
      if (!cfg->no_visual)
         mvprintw(0, 0, "No sample matched '%s'", cfg->testchain_selector);
      return;
   }

   curs_set(0);
   if (cfg->no_visual)
   {
      size_t i;
      int report_old_target = -1;
      int path_len = testchain_path_length();

      for (i = 0; i < (size_t)path_len; i++)
      {
         int target = testchain_path_target((int)i);
         int start_target = flashwhole ? 0
                          : firststrategy ? testchain_first_cluster_target()
                          : flashback
                          ? testchain_flash_start(report_old_target, target,
                                                  back_clusters)
                          : -1;

         reportf(cfg, "testchain,%s,target=%s,start=%s,layout_width=%d,cursor_width=%d,mode=%s\n",
                 sample->name, testchain_target_name(target),
                 start_target >= 0 ? testchain_target_name(start_target) : "-",
                 cfg->testchain_layout_width, cfg->testchain_cursor_width,
                 testchain_mode_name(cfg));
         report_old_target = target;
      }
      reportf(cfg, "\n");
      curs_set(1);
      return;
   }

   erase();
   mvprintw(0, 0, "testchain %s L%d C%d mode=%s %s: press any key to stop",
            cfg->testchain_selector, cfg->testchain_layout_width,
            cfg->testchain_cursor_width, testchain_mode_name(cfg),
            UTF8_TERMINAL_PROBE_VERSION);
   mvprintw(1, 0, "Pattern is XX-A-C-B-A-C-B-A-C-C-C-B-A-C-B-A-C-B-XX; flashback counts prior clusters.");
   if (testchain_mode_is(cfg, "cell") || flashback || firststrategy || flashwhole)
      draw_testchain_base(sample, row, col, cfg->testchain_layout_width);
   nodelay(stdscr, TRUE);
   for (;;)
   {
      int target = testchain_path_target(step);
      int start_target = flashwhole ? 0
                       : firststrategy ? testchain_first_cluster_target()
                       : flashback
                       ? testchain_flash_start(old_target, target,
                                               back_clusters)
                       : -1;

      mvprintw(3, 0, "sample=%s class=%s target=%-9s start=%-9s step=%d   ",
               sample->name, sample->klass, testchain_target_name(target),
               start_target >= 0 ? testchain_target_name(start_target) : "-",
               step);
      if (testchain_mode_is(cfg, "cell"))
      {
         draw_testchain_step(sample, row, col, cfg->testchain_layout_width,
                             cfg->testchain_cursor_width, old_target, target,
                             cursor_attr);
      }
      else if (flashback)
      {
         draw_testchain_flashback(sample, row, col,
                                  cfg->testchain_layout_width,
                                  cfg->testchain_cursor_width, old_target,
                                  target, back_clusters, cursor_attr);
      }
      else if (paintfirst || touchfirst || clearfirst || flashfirstfast)
      {
         draw_testchain_resetfrom(sample, row, col,
                                  cfg->testchain_layout_width,
                                  cfg->testchain_cursor_width, old_target,
                                  target, start_target, cursor_attr,
                                  clearfirst || flashfirstfast,
                                  flashfirstfast, 0, touchfirst);
      }
      else if (flashfirst || flashwhole)
      {
         draw_testchain_flashfrom(sample, row, col,
                                  cfg->testchain_layout_width,
                                  cfg->testchain_cursor_width, old_target,
                                  target, start_target, cursor_attr);
      }
      else
      {
         draw_testchain_frame(sample, row, col, cfg->testchain_layout_width,
                              cfg->testchain_cursor_width, target, cursor_attr,
                              testchain_mode_is(cfg, "line"));
      }
      refresh();
      reportf(cfg, "testchain,%s,target=%s,start=%s,layout_width=%d,cursor_width=%d,mode=%s\n",
              sample->name, testchain_target_name(target),
              start_target >= 0 ? testchain_target_name(start_target) : "-",
              cfg->testchain_layout_width, cfg->testchain_cursor_width,
              testchain_mode_name(cfg));
      if (getch() != ERR)
         break;
      napms(delay);
      old_target = target;
      step++;
   }
   nodelay(stdscr, FALSE);
   curs_set(1);
   reportf(cfg, "\n");
}

static int calibration_entry_seen(const CalibrationEntry *entries, size_t count,
                                  const char *klass, const char *intent)
{
   size_t i;

   for (i = 0; i < count; i++)
      if (strcmp(entries[i].feature_class, klass) == 0)
      {
         if (intent == NULL
         ||  strcmp(entries[i].display_intent, intent) == 0)
            return 1;
      }
   return 0;
}

static int calibration_entry_is_zwj(const CalibrationEntry *entry)
{
   return strstr(entry->feature_class, "zwj") != NULL;
}

static int calibration_entry_is_normal_intent(const CalibrationEntry *entry)
{
   return strcmp(entry->display_intent, "normal") == 0;
}

static CalibrationEntry *find_calibration_entry_for(CalibrationEntry *entries,
                                                    size_t count,
                                                    const char *feature_class,
                                                    const char *intent)
{
   size_t i;

   for (i = 0; i < count; i++)
      if (strcmp(entries[i].feature_class, feature_class) == 0
      &&  strcmp(entries[i].display_intent, intent) == 0)
         return &entries[i];
   return NULL;
}

static CalibrationEntry *find_calibration_entry_for_output(CalibrationEntry *entries,
                                                           size_t count,
                                                           const char *feature_class,
                                                           const char *output_method)
{
   size_t i;

   for (i = 0; i < count; i++)
      if (strcmp(entries[i].feature_class, feature_class) == 0
      &&  strcmp(entries[i].output_method, output_method) == 0)
         return &entries[i];
   return NULL;
}

static CalibrationEntry *find_calibration_entry(CalibrationEntry *entries,
                                                size_t count,
                                                const char *feature_class)
{
   size_t i;
   CalibrationEntry *group = NULL;

   for (i = 0; i < count; i++)
      if (strcmp(entries[i].feature_class, feature_class) == 0)
      {
         if (strcmp(entries[i].display_intent, "normal") == 0)
            return &entries[i];
         if (strcmp(entries[i].display_intent, "group") == 0)
            group = &entries[i];
      }
   if (group != NULL)
      return group;
   for (i = 0; i < count; i++)
      if (strcmp(entries[i].feature_class, feature_class) == 0)
         return &entries[i];
   return NULL;
}

static const ProbeSample *find_sample_for_class(ProbeConfig *cfg,
                                                const char *feature_class)
{
   size_t i;

   for (i = 0; i < cfg->sample_count; i++)
      if (strcmp(cfg->samples[i].klass, feature_class) == 0)
         return &cfg->samples[i];
   return NULL;
}

static int calibration_default_matches_selector(const CalibrationDefault *defaults,
                                                const ProbeSample *sample,
                                                const char *selector)
{
   if (selector == NULL || strcmp(selector, "all") == 0)
      return 1;
   if (strcmp(selector, "focus") == 0)
      return strcmp(defaults->feature_class, "keycap") == 0
          || strcmp(defaults->feature_class, "regional-flag") == 0;
   if (strcmp(selector, "zwj") == 0 || strcmp(selector, "joiner") == 0)
      return strstr(defaults->feature_class, "zwj") != NULL;
   if (strcmp(selector, "flags") == 0)
      return strcmp(defaults->feature_class, "regional-flag") == 0
          || strcmp(defaults->feature_class, "tag-flag") == 0;
   return sample != NULL && sample_matches_selector(sample, selector);
}

static size_t collect_calibration_entries(ProbeConfig *cfg,
                                          CalibrationEntry *entries,
                                          size_t entry_cap)
{
   const char *selector = cfg->calibrate_selector ? cfg->calibrate_selector : "all";
   size_t i;
   size_t count = 0;

   for (i = 0; i < sizeof(calibration_defaults) / sizeof(calibration_defaults[0])
   &&   count < entry_cap; i++)
   {
      const CalibrationDefault *defaults = &calibration_defaults[i];
      const ProbeSample *sample = find_sample_for_class(cfg,
                                                        defaults->feature_class);

      if (sample == NULL
      ||  !calibration_default_matches_selector(defaults, sample, selector))
         continue;
      if (calibration_entry_seen(entries, count, defaults->feature_class,
                                 defaults->display_intent))
         continue;
      entries[count].sample = sample;
      entries[count].feature_class = defaults->feature_class;
      entries[count].display_intent = defaults->display_intent;
      entries[count].output_method = defaults->output_method;
      entries[count].defaults = defaults;
      entries[count].layout_width = defaults->layout_width;
      entries[count].cursor_width = defaults->cursor_width;
      entries[count].cursor_strategy = defaults->cursor_strategy;
      entries[count].replacement_strategy = defaults->replacement_strategy;
      count++;
   }
   return count;
}

static void add_view_candidate(ViewCandidate *candidates, int *count,
                               const char *name, int layout_width,
                               int cursor_width)
{
   int i;

   if (layout_width < 1 || cursor_width < 1
   ||  *count >= MAX_VIEW_CANDIDATES)
      return;
   for (i = 0; i < *count; i++)
      if (candidates[i].layout_width == layout_width
      &&  candidates[i].cursor_width == cursor_width)
         return;
   candidates[*count].name = name;
   candidates[*count].layout_width = layout_width;
   candidates[*count].cursor_width = cursor_width;
   (*count)++;
}

static int build_view_candidates(const CalibrationEntry *entry,
                                 ViewCandidate *candidates)
{
   static const char *width_names[] =
   {
      "", "one", "two", "three", "four", "five", "six", "seven",
      "eight", "nine", "ten", "eleven", "twelve"
   };
   static const struct
   {
      const char *name;
      int layout_width;
      int cursor_width;
   } hybrids[] =
   {
      { "three-cursor-two", 3, 2 },
      { "four-cursor-two", 4, 2 },
      { "four-cursor-three", 4, 3 },
      { "five-cursor-two", 5, 2 },
      { "five-cursor-three", 5, 3 },
      { "six-cursor-two", 6, 2 },
      { "six-cursor-three", 6, 3 },
      { "six-cursor-four", 6, 4 },
      { "eight-cursor-two", 8, 2 },
      { "eight-cursor-four", 8, 4 },
      { "ten-cursor-two", 10, 2 },
      { "twelve-cursor-two", 12, 2 }
   };
   int count = 0;
   int base = entry->sample->expected_policy_width > 0
            ? entry->sample->expected_policy_width : 1;
   int width;
   size_t i;

   add_view_candidate(candidates, &count, "current", entry->layout_width,
                      entry->cursor_width);
   if (entry->defaults != NULL)
      add_view_candidate(candidates, &count, "default",
                         entry->defaults->layout_width,
                         entry->defaults->cursor_width);
   add_view_candidate(candidates, &count, "policy", base, base);
   for (width = 1; width <= 12; width++)
      add_view_candidate(candidates, &count, width_names[width],
                         width, width);
   for (i = 0; i < sizeof(hybrids) / sizeof(hybrids[0]); i++)
      add_view_candidate(candidates, &count, hybrids[i].name,
                         hybrids[i].layout_width,
                         hybrids[i].cursor_width);
   return count;
}

static void draw_sample5_at(const ProbeSample *sample, int row, int col,
                            int layout_width, int cursor_width, int target,
                            attr_t cursor_attr)
{
   const char *clusters[] = { "A", sample->utf8, "B", " ", "A" };
   int offsets[5];
   int widths[5];
   int clear_width;
   int i;

   raw_keycap_layout_offsets(layout_width, offsets, widths);
   clear_width = offsets[4] + widths[4] + 2;
   curses_clear_cells(row, col - 1, clear_width, A_NORMAL);
   for (i = 0; i < 5; i++)
      curses_write_cluster_at(METHOD_WADDWSTR, row, col + offsets[i],
                              clusters[i], widths[i], A_NORMAL);
   if (target >= 0 && target < 5)
   {
      int target_width = target == 1 ? cursor_width : widths[target];

      curses_clear_cells(row, col + offsets[target], target_width,
                         cursor_attr);
      curses_write_cluster_at(METHOD_WADDWSTR, row, col + offsets[target],
                              clusters[target], widths[target], cursor_attr);
   }
}

static int calibration_select_view(ProbeConfig *cfg, CalibrationEntry *entry)
{
   ViewCandidate candidates[MAX_VIEW_CANDIDATES];
   int candidate_count;
   int selected = 0;
   int top = 0;
   attr_t cursor_attr;

   candidate_count = build_view_candidates(entry, candidates);
   if (candidate_count <= 0)
      return -1;
   if (cfg->no_visual)
   {
      entry->layout_width = candidates[0].layout_width;
      entry->cursor_width = candidates[0].cursor_width;
      return 0;
   }

   cursor_attr = utfvis_cursor_attr();
   for (;;)
   {
      int i;
      int ch;
      int visible_rows = LINES > 9 ? LINES - 8 : 1;

      if (visible_rows > candidate_count)
         visible_rows = candidate_count;
      if (selected < top)
         top = selected;
      if (selected >= top + visible_rows)
         top = selected - visible_rows + 1;

      erase();
      mvprintw(0, 0, "calibrate view %s/%s intent=%s output=%s %s",
               entry->feature_class, entry->sample->name,
               entry->display_intent, entry->output_method,
               UTF8_TERMINAL_PROBE_VERSION);
      mvprintw(1, 0, "Current L%d C%d. Choose the row whose plain, cluster-cursor, and B-cursor cells look aligned.",
               entry->layout_width, entry->cursor_width);
      mvprintw(2, 0, "Keys: j/k move, 1-9 choose visible row, Enter accept, q quit. Showing %d-%d of %d.",
               top + 1, top + visible_rows, candidate_count);
      if (calibration_entry_is_zwj(entry))
      {
         char cps[256];

         utf8_codepoints(entry->sample->utf8, cps, sizeof(cps));
         mvprintw(3, 0, "emits: %s", cps);
      }
      mvprintw(4, cfg->data_col, "plain");
      mvprintw(4, cfg->data_col + 16, "cluster cursor");
      mvprintw(4, cfg->data_col + 34, "B cursor");
      for (i = 0; i < visible_rows; i++)
      {
         int row = 6 + i;
         int candidate_index = top + i;
         attr_t label_attr = candidate_index == selected ? A_REVERSE : A_NORMAL;

         attrset(label_attr);
         mvprintw(row, 0, "%d %2d %-18s L%d C%d",
                  i + 1, candidate_index + 1,
                  candidates[candidate_index].name,
                  candidates[candidate_index].layout_width,
                  candidates[candidate_index].cursor_width);
         attrset(A_NORMAL);
         draw_sample5_at(entry->sample, row, cfg->data_col,
                         candidates[candidate_index].layout_width,
                         candidates[candidate_index].cursor_width,
                         -1, cursor_attr);
         draw_sample5_at(entry->sample, row, cfg->data_col + 16,
                         candidates[candidate_index].layout_width,
                         candidates[candidate_index].cursor_width,
                         1, cursor_attr);
         draw_sample5_at(entry->sample, row, cfg->data_col + 34,
                         candidates[candidate_index].layout_width,
                         candidates[candidate_index].cursor_width,
                         2, cursor_attr);
      }
      refresh();
      ch = getch();
      if (ch == 'q' || ch == 'Q')
         return -1;
      if (ch == '\n' || ch == '\r' || ch == KEY_ENTER)
         break;
      if (ch >= '1' && ch <= '9')
      {
         int index = top + ch - '1';

         if (index < candidate_count)
         {
            selected = index;
            break;
         }
      }
      else if (ch == 'j' || ch == KEY_DOWN)
      {
         if (selected + 1 < candidate_count)
            selected++;
      }
      else if (ch == 'k' || ch == KEY_UP)
      {
         if (selected > 0)
            selected--;
      }
   }

   entry->layout_width = candidates[selected].layout_width;
   entry->cursor_width = candidates[selected].cursor_width;
   return 0;
}

static void draw_chain_strategy_frame(const ProbeSample *sample, int row,
                                      int col, int layout_width,
                                      int cursor_width, const char *strategy,
                                      int old_target, int target,
                                      attr_t cursor_attr)
{
   int back_clusters = 1;
   int flashback = testchain_mode_flashback(strategy, &back_clusters);
   int paintfirst = strcmp(strategy, "paintfirstcluster") == 0;
   int touchfirst = strcmp(strategy, "touchfirstcluster") == 0;
   int clearfirst = strcmp(strategy, "clearfirstcluster") == 0;
   int flashfirstfast = strcmp(strategy, "flashfirstfast") == 0;
   int flashfirst = strcmp(strategy, "flashfirstcluster") == 0;
   int flashwhole = strcmp(strategy, "flashwhole") == 0;
   int firststrategy = paintfirst || touchfirst || clearfirst
                    || flashfirstfast || flashfirst;
   int start_target = flashwhole ? 0
                    : firststrategy ? testchain_first_cluster_target()
                    : flashback
                    ? testchain_flash_start(old_target, target, back_clusters)
                    : -1;

   if (strcmp(strategy, "cell") == 0)
   {
      draw_testchain_step(sample, row, col, layout_width, cursor_width,
                          old_target, target, cursor_attr);
   }
   else if (flashback)
   {
      draw_testchain_flashback(sample, row, col, layout_width, cursor_width,
                               old_target, target, back_clusters, cursor_attr);
   }
   else if (paintfirst || touchfirst || clearfirst || flashfirstfast)
   {
      draw_testchain_resetfrom(sample, row, col, layout_width, cursor_width,
                               old_target, target, start_target, cursor_attr,
                               clearfirst || flashfirstfast,
                               flashfirstfast, 0, touchfirst);
   }
   else if (flashfirst || flashwhole)
   {
      draw_testchain_flashfrom(sample, row, col, layout_width, cursor_width,
                               old_target, target, start_target, cursor_attr);
   }
   else
   {
      draw_testchain_frame(sample, row, col, layout_width, cursor_width,
                           target, cursor_attr, strcmp(strategy, "line") == 0);
   }
}

#define REPLACE_TARGETS 7

static int replacement_cluster1_replaced(int state)
{
   return state == 1;
}

static int replacement_b1_replaced(int state)
{
   return state == 3;
}

static const char *replacement_item_text(const ProbeSample *sample, int target,
                                         int state)
{
   static const char *ascii_targets[REPLACE_TARGETS] =
   {
      "A", NULL, "B", " ", "A", NULL, "B"
   };

   if (target == 1)
      return replacement_cluster1_replaced(state) ? "Z" : sample->utf8;
   if (target == 2)
      return replacement_b1_replaced(state) ? "Z" : "B";
   if (target == 5)
      return sample->utf8;
   if (target < 0 || target >= REPLACE_TARGETS)
      return "?";
   return ascii_targets[target] ? ascii_targets[target] : "?";
}

static void replacement_offsets(int layout_width, int state,
                                int offsets[REPLACE_TARGETS],
                                int widths[REPLACE_TARGETS])
{
   int target;
   int offset = 0;

   for (target = 0; target < REPLACE_TARGETS; target++)
   {
      offsets[target] = offset;
      if (target == 1)
         widths[target] = replacement_cluster1_replaced(state) ? 1 : layout_width;
      else if (target == 5)
         widths[target] = layout_width;
      else
         widths[target] = 1;
      offset += widths[target];
   }
}

static int replacement_total_width(int layout_width, int state)
{
   return (replacement_cluster1_replaced(state) ? 1 : layout_width)
        + layout_width + 5;
}

static void draw_replacement_span(const ProbeSample *sample, int row, int col,
                                  int layout_width, int state,
                                  int first_target, int last_target)
{
   int offsets[REPLACE_TARGETS];
   int widths[REPLACE_TARGETS];
   int target;

   if (first_target < 0)
      first_target = 0;
   if (last_target >= REPLACE_TARGETS)
      last_target = REPLACE_TARGETS - 1;
   replacement_offsets(layout_width, state, offsets, widths);
   for (target = first_target; target <= last_target; target++)
      curses_write_cluster_at(METHOD_WADDWSTR, row, col + offsets[target],
                              replacement_item_text(sample, target, state),
                              widths[target], A_NORMAL);
}

static void replacement_range_cells(int layout_width, int state,
                                    int first_target, int last_target,
                                    int *start_cell, int *width_cells)
{
   int offsets[REPLACE_TARGETS];
   int widths[REPLACE_TARGETS];
   int first = first_target < last_target ? first_target : last_target;
   int last = first_target > last_target ? first_target : last_target;
   int start;
   int end;
   int target;

   if (first < 0)
      first = 0;
   if (last >= REPLACE_TARGETS)
      last = REPLACE_TARGETS - 1;
   replacement_offsets(layout_width, state, offsets, widths);
   start = offsets[first];
   end = start + widths[first];
   for (target = first; target <= last; target++)
      if (offsets[target] + widths[target] > end)
         end = offsets[target] + widths[target];
   *start_cell = start;
   *width_cells = end - start;
}

static int replacement_changed_target(int old_state, int new_state)
{
   if (replacement_cluster1_replaced(old_state)
   !=  replacement_cluster1_replaced(new_state))
      return 1;
   if (replacement_b1_replaced(old_state)
   !=  replacement_b1_replaced(new_state))
      return 2;
   return 0;
}

static void replacement_union_range(int layout_width, int old_state,
                                    int new_state, int first_target,
                                    int last_target, int *start_cell,
                                    int *width_cells)
{
   int old_start;
   int old_width;
   int new_start;
   int new_width;
   int start;
   int end;

   replacement_range_cells(layout_width, old_state, first_target, last_target,
                           &old_start, &old_width);
   replacement_range_cells(layout_width, new_state, first_target, last_target,
                           &new_start, &new_width);
   start = old_start < new_start ? old_start : new_start;
   end = old_start + old_width > new_start + new_width
       ? old_start + old_width : new_start + new_width;
   *start_cell = start;
   *width_cells = end - start;
}

static void draw_replacement_base(const ProbeSample *sample, int row, int col,
                                  int layout_width, int state)
{
   int total = replacement_total_width(layout_width, state);

   curses_clear_cells(row, col - 1, total + layout_width + 4, A_NORMAL);
   draw_replacement_span(sample, row, col, layout_width, state, 0,
                         REPLACE_TARGETS - 1);
}

static void draw_replacement_strategy_frame(const ProbeSample *sample, int row,
                                            int col, int layout_width,
                                            const char *strategy,
                                            int old_state, int new_state)
{
   int changed_target = replacement_changed_target(old_state, new_state);
   int start_target = changed_target;
   int start_cell;
   int width_cells;

   if (strcmp(strategy, "cell") == 0)
   {
      replacement_union_range(layout_width, old_state, new_state, changed_target,
                              changed_target, &start_cell, &width_cells);
      curses_clear_cells(row, col + start_cell, width_cells, A_NORMAL);
      draw_replacement_span(sample, row, col, layout_width, new_state,
                            changed_target, changed_target);
      return;
   }

   if (strcmp(strategy, "suffix") == 0)
      start_target = changed_target;
   else if (strcmp(strategy, "clearfirstfast") == 0)
      start_target = 1;
   else if (strcmp(strategy, "flashwhole") == 0)
      start_target = 0;
   else
   {
      draw_replacement_base(sample, row, col, layout_width, new_state);
      touchline(stdscr, row, 1);
      return;
   }

   replacement_union_range(layout_width, old_state, new_state, start_target,
                           REPLACE_TARGETS - 1, &start_cell, &width_cells);
   curses_clear_cells(row, col + start_cell, width_cells + layout_width + 2,
                      A_NORMAL);
   touchline(stdscr, row, 1);
   refresh();
   draw_replacement_span(sample, row, col, layout_width, new_state,
                         start_target, REPLACE_TARGETS - 1);
}

static int strategy_index_by_name(const StrategyCandidate *strategies,
                                  int strategy_count, const char *name)
{
   int i;

   if (name == NULL)
      return 0;
   for (i = 0; i < strategy_count; i++)
      if (strcmp(strategies[i].name, name) == 0)
         return i;
   return 0;
}

static int calibration_select_strategy(ProbeConfig *cfg,
                                       CalibrationEntry *entry,
                                       int replacement)
{
   static const StrategyCandidate cursor_strategies[] =
   {
      { "cell", "old/new cells only", 10 },
      { "line", "complete run repaint", 20 },
      { "flashbackcluster1", "clear from one prior cluster", 30 },
      { "flashfirstfast", "clear from first cluster, flush, repaint", 40 },
      { "flashfirstcluster", "same with a short pause", 50 },
      { "flashwhole", "clear whole run, flush, repaint", 60 }
   };
   static const StrategyCandidate replacement_strategies[] =
   {
      { "cell", "changed target only", 10 },
      { "line", "complete sample repaint", 20 },
      { "suffix", "clear changed suffix, flush, repaint", 30 },
      { "clearfirstfast", "clear from first cluster, flush, repaint", 40 },
      { "flashwhole", "clear whole sample, flush, repaint", 60 }
   };
   const StrategyCandidate *strategies = replacement
                                      ? replacement_strategies
                                      : cursor_strategies;
   int strategy_count = replacement
                      ? (int)(sizeof(replacement_strategies) / sizeof(replacement_strategies[0]))
                      : (int)(sizeof(cursor_strategies) / sizeof(cursor_strategies[0]));
   int selected = strategy_index_by_name(strategies, strategy_count,
                                         replacement
                                         ? entry->replacement_strategy
                                         : entry->cursor_strategy);
   int step = 0;
   int old_target = -1;
   int old_state = 0;
   int reset_sample = 1;
   int row = 14;
   int col = cfg->data_col;
   int delay = cfg->timeout_ms > 0 ? cfg->timeout_ms : 200;
   attr_t cursor_attr;

   if (cfg->no_visual)
   {
      if (replacement)
         entry->replacement_strategy = strategies[0].name;
      else
         entry->cursor_strategy = strategies[0].name;
      return 0;
   }

   cursor_attr = utfvis_cursor_attr();
   erase();
   nodelay(stdscr, TRUE);
   for (;;)
   {
      int ch;

      mvprintw(0, 0, "calibrate %s %s/%s %s/%s L%d C%d %s",
               replacement ? "replace" : "cursor",
               entry->feature_class, entry->sample->name,
               entry->display_intent, entry->output_method,
               entry->layout_width, entry->cursor_width,
               UTF8_TERMINAL_PROBE_VERSION);
      mvprintw(1, 0, "Current: %s. Keys: n/p choose strategy, g or Enter accept, q back",
               strategies[selected].name);
      mvprintw(2, 0, "strategy=%s score=%d - %s                         ",
               strategies[selected].name, strategies[selected].preference_score,
               strategies[selected].label);
      mvprintw(3, 0, "Lower score is preferred/faster when multiple strategies look correct.");
      {
         int option;

         for (option = 0; option < strategy_count; option++)
         {
            attr_t label_attr = option == selected ? A_REVERSE : A_NORMAL;

            attrset(label_attr);
            mvprintw(4 + option, 0, "%c %-22s score=%-2d %s",
                     option == selected ? '>' : ' ',
                     strategies[option].name,
                     strategies[option].preference_score,
                     strategies[option].label);
            attrset(A_NORMAL);
         }
      }

      if (reset_sample)
      {
         if (replacement)
            draw_replacement_base(entry->sample, row, col,
                                  entry->layout_width, 0);
         else
            draw_testchain_base(entry->sample, row, col,
                                entry->layout_width);
         reset_sample = 0;
      }

      if (replacement)
      {
         static const char *state_names[] =
         {
            "original", "cluster1->Z", "restore", "B1->Z", "restore"
         };
         int new_state = step % 5;

         mvprintw(11, 0, "replacement target=%s step=%d             ",
                  state_names[new_state], step);
         draw_replacement_strategy_frame(entry->sample, row, col,
                                         entry->layout_width,
                                         strategies[selected].name,
                                         old_state, new_state);
         old_state = new_state;
      }
      else
      {
         int target = testchain_path_target(step);

         mvprintw(11, 0, "cursor target=%s step=%d start policy is strategy-specific      ",
                  testchain_target_name(target), step);
         draw_chain_strategy_frame(entry->sample, row, col,
                                   entry->layout_width, entry->cursor_width,
                                   strategies[selected].name, old_target,
                                   target, cursor_attr);
         old_target = target;
      }
      refresh();
      reportf(cfg, "calibrate_preview,%s,class=%s,sample=%s,strategy=%s,step=%d\n",
              replacement ? "replace" : "cursor", entry->feature_class,
              entry->sample->name, strategies[selected].name, step);
      napms(delay);
      ch = getch();
      if (ch == 'q' || ch == 'Q')
      {
         nodelay(stdscr, FALSE);
         return -1;
      }
      if (ch == 'g' || ch == 'G' || ch == '\n' || ch == '\r'
      ||  ch == KEY_ENTER)
         break;
      if (ch == 'n' || ch == 'j' || ch == KEY_RIGHT || ch == KEY_DOWN)
      {
         selected = (selected + 1) % strategy_count;
         old_target = -1;
         old_state = 0;
         step = 0;
         reset_sample = 1;
         erase();
         continue;
      }
      else if (ch == 'p' || ch == 'k' || ch == KEY_LEFT || ch == KEY_UP)
      {
         selected = selected == 0 ? strategy_count - 1 : selected - 1;
         old_target = -1;
         old_state = 0;
         step = 0;
         reset_sample = 1;
         erase();
         continue;
      }
      step++;
   }
   nodelay(stdscr, FALSE);

   if (replacement)
      entry->replacement_strategy = strategies[selected].name;
   else
      entry->cursor_strategy = strategies[selected].name;
   return 0;
}

static const char *profile_strategy_name(const char *strategy)
{
   if (strategy == NULL)
      return "normal";
   if (strcmp(strategy, "cell") == 0)
      return "changed_cells";
   if (strcmp(strategy, "line") == 0)
      return "line";
   if (strcmp(strategy, "suffix") == 0)
      return "clear_changed_suffix_fast";
   if (strcmp(strategy, "clearfirstfast") == 0
   ||  strcmp(strategy, "flashfirstfast") == 0)
      return "clear_from_first_cluster_fast";
   if (strcmp(strategy, "flashfirstcluster") == 0)
      return "clear_from_first_cluster_pause";
   if (strcmp(strategy, "flashwhole") == 0)
      return "clear_whole_fast";
   if (strcmp(strategy, "flashbackcluster1") == 0)
      return "clear_from_one_prior_cluster";
   return strategy;
}

static const char *profile_to_cursor_strategy(const char *strategy)
{
   if (strcmp(strategy, "changed_cells") == 0)
      return "cell";
   if (strcmp(strategy, "line") == 0)
      return "line";
   if (strcmp(strategy, "clear_from_one_prior_cluster") == 0)
      return "flashbackcluster1";
   if (strcmp(strategy, "clear_from_first_cluster_fast") == 0)
      return "flashfirstfast";
   if (strcmp(strategy, "clear_from_first_cluster_pause") == 0)
      return "flashfirstcluster";
   if (strcmp(strategy, "clear_whole_fast") == 0)
      return "flashwhole";
   return "line";
}

static const char *profile_to_replacement_strategy(const char *strategy)
{
   if (strcmp(strategy, "changed_cells") == 0)
      return "cell";
   if (strcmp(strategy, "line") == 0)
      return "line";
   if (strcmp(strategy, "clear_changed_suffix_fast") == 0)
      return "suffix";
   if (strcmp(strategy, "clear_from_first_cluster_fast") == 0)
      return "clearfirstfast";
   if (strcmp(strategy, "clear_whole_fast") == 0)
      return "flashwhole";
   return "line";
}

static const char *known_output_method(const char *method)
{
   if (strcmp(method, "native") == 0 || strcmp(method, "literal") == 0)
      return "native";
   if (strcmp(method, "expanded") == 0)
      return "expanded";
   if (strcmp(method, "substitute") == 0 || strcmp(method, "placeholder") == 0)
      return "substitute";
   return "native";
}

static const char *intent_for_legacy_output_method(const char *method)
{
   if (strcmp(method, "expanded") == 0)
      return "components";
   return "group";
}

static int output_method_allowed_for_intent(const char *intent,
                                            const char *method)
{
   if (strcmp(intent, "normal") == 0)
      return strcmp(method, "native") == 0;
   if (strcmp(intent, "group") == 0)
      return strcmp(method, "native") == 0
          || strcmp(method, "substitute") == 0;
   if (strcmp(intent, "components") == 0)
      return strcmp(method, "native") == 0
          || strcmp(method, "expanded") == 0;
   return 0;
}

static const char *coerce_output_method_for_intent(const char *intent,
                                                   const char *method)
{
   method = known_output_method(method);
   if (output_method_allowed_for_intent(intent, method))
      return method;
   if (strcmp(intent, "components") == 0)
      return "expanded";
   if (strcmp(intent, "group") == 0)
      return "native";
   return "native";
}

static void apply_output_method_defaults(CalibrationEntry *entry)
{
   if (strcmp(entry->output_method, "substitute") == 0)
   {
      entry->layout_width = 1;
      entry->cursor_width = 1;
      entry->cursor_strategy = "cell";
      entry->replacement_strategy = "cell";
   }
}

static const CalibrationDefault *find_calibration_default_for_output(
   const char *feature_class, const char *intent, const char *output_method)
{
   size_t i;
   const CalibrationDefault *class_output = NULL;

   for (i = 0; i < sizeof(calibration_defaults) / sizeof(calibration_defaults[0]); i++)
   {
      const CalibrationDefault *defaults = &calibration_defaults[i];

      if (strcmp(defaults->feature_class, feature_class) != 0
      ||  strcmp(defaults->output_method, output_method) != 0)
         continue;
      if (strcmp(defaults->display_intent, intent) == 0)
         return defaults;
      if (class_output == NULL)
         class_output = defaults;
   }
   return class_output;
}

static void copy_calibration_physical_settings(CalibrationEntry *entry,
                                               const CalibrationEntry *source)
{
   entry->layout_width = source->layout_width;
   entry->cursor_width = source->cursor_width;
   entry->cursor_strategy = source->cursor_strategy;
   entry->replacement_strategy = source->replacement_strategy;
}

static void copy_calibration_default_physical_settings(
   CalibrationEntry *entry, const CalibrationDefault *defaults)
{
   entry->layout_width = defaults->layout_width;
   entry->cursor_width = defaults->cursor_width;
   entry->cursor_strategy = defaults->cursor_strategy;
   entry->replacement_strategy = defaults->replacement_strategy;
}

static void apply_output_method_physical_settings(CalibrationEntry *entry,
                                                  CalibrationEntry *entries,
                                                  size_t count,
                                                  const char *output_method)
{
   CalibrationEntry *source;
   const CalibrationDefault *defaults;
   const char *coerced_output;

   coerced_output = coerce_output_method_for_intent(entry->display_intent,
                                                    output_method);
   if (strcmp(entry->output_method, coerced_output) == 0)
   {
      if (strcmp(coerced_output, "substitute") == 0)
         apply_output_method_defaults(entry);
      return;
   }

   entry->output_method = coerced_output;
   apply_output_method_defaults(entry);
   if (strcmp(entry->output_method, "substitute") == 0)
      return;

   source = find_calibration_entry_for_output(entries, count,
                                             entry->feature_class,
                                             entry->output_method);
   if (source != NULL && source != entry)
   {
      copy_calibration_physical_settings(entry, source);
      return;
   }

   defaults = find_calibration_default_for_output(entry->feature_class,
                                                 entry->display_intent,
                                                 entry->output_method);
   if (defaults != NULL)
      copy_calibration_default_physical_settings(entry, defaults);
}

static int read_calibration_profile(ProbeConfig *cfg,
                                    CalibrationEntry *entries,
                                    size_t count)
{
   FILE *fp;
   char line[512];
   int loaded = 0;

   fp = fopen(cfg->profile_path, "r");
   if (fp == NULL)
   {
      reportf(cfg, "calibrate_profile_read,path=%s,status=not-found\n",
              cfg->profile_path);
      return 0;
   }

   while (fgets(line, sizeof(line), fp) != NULL)
   {
      char klass[96];
      char intent[96];
      char word[96];
      int layout_width;
      int cursor_width;
      CalibrationEntry *entry;

      if (sscanf(line, "SET UTF8 TERMINAL CLASS %95s INTENT %95s OUTPUT %95s",
                 klass, intent, word) == 3)
      {
         entry = find_calibration_entry_for(entries, count, klass, intent);
         if (entry != NULL)
         {
            entry->output_method = coerce_output_method_for_intent(
                                      entry->display_intent, word);
            apply_output_method_defaults(entry);
            loaded++;
         }
      }
      else if (sscanf(line, "SET UTF8 TERMINAL CLASS %95s INTENT %95s LAYOUT %d CURSOR %d",
                      klass, intent, &layout_width, &cursor_width) == 4)
      {
         entry = find_calibration_entry_for(entries, count, klass, intent);
         if (entry != NULL && layout_width > 0 && cursor_width > 0)
         {
            entry->layout_width = layout_width;
            entry->cursor_width = cursor_width;
            loaded++;
         }
      }
      else if (sscanf(line, "SET UTF8 TERMINAL CLASS %95s INTENT %95s CURSORSTRATEGY %95s",
                      klass, intent, word) == 3)
      {
         entry = find_calibration_entry_for(entries, count, klass, intent);
         if (entry != NULL)
         {
            entry->cursor_strategy = profile_to_cursor_strategy(word);
            loaded++;
         }
      }
      else if (sscanf(line, "SET UTF8 TERMINAL CLASS %95s INTENT %95s REPLACESTRATEGY %95s",
                      klass, intent, word) == 3)
      {
         entry = find_calibration_entry_for(entries, count, klass, intent);
         if (entry != NULL)
         {
            entry->replacement_strategy = profile_to_replacement_strategy(word);
            loaded++;
         }
      }
      else if (sscanf(line, "SET UTF8 TERMINAL CLASS %95s LAYOUT %d CURSOR %d",
                 klass, &layout_width, &cursor_width) == 3)
      {
         entry = find_calibration_entry(entries, count, klass);
         if (entry != NULL && layout_width > 0 && cursor_width > 0)
         {
            entry->layout_width = layout_width;
            entry->cursor_width = cursor_width;
            loaded++;
         }
      }
      else if (sscanf(line, "SET UTF8 TERMINAL CLASS %95s CURSORSTRATEGY %95s",
                      klass, word) == 2)
      {
         entry = find_calibration_entry(entries, count, klass);
         if (entry != NULL)
         {
            entry->cursor_strategy = profile_to_cursor_strategy(word);
            loaded++;
         }
      }
      else if (sscanf(line, "SET UTF8 TERMINAL CLASS %95s REPLACESTRATEGY %95s",
                      klass, word) == 2)
      {
         entry = find_calibration_entry(entries, count, klass);
         if (entry != NULL)
         {
            entry->replacement_strategy = profile_to_replacement_strategy(word);
            loaded++;
         }
      }
      else if (sscanf(line, "SET UTF8 TERMINAL CLASS %95s ZWJDISPLAY %95s",
                      klass, word) == 2)
      {
         const char *method = known_output_method(word);
         const char *legacy_intent = intent_for_legacy_output_method(method);

         entry = find_calibration_entry_for(entries, count, klass,
                                            legacy_intent);
         if (entry == NULL)
            entry = find_calibration_entry(entries, count, klass);
         if (entry != NULL)
         {
            entry->output_method = coerce_output_method_for_intent(
                                      entry->display_intent, method);
            apply_output_method_defaults(entry);
            loaded++;
         }
      }
   }
   fclose(fp);
   reportf(cfg, "calibrate_profile_read,path=%s,settings=%d\n",
           cfg->profile_path, loaded);
   return loaded;
}

static int calibration_entry_is_default(const CalibrationEntry *entry);

static int write_calibration_profile(ProbeConfig *cfg,
                                     const CalibrationEntry *entries,
                                     size_t count)
{
   FILE *fp;
   size_t i;
   const char *term = getenv("TERM") ? getenv("TERM") : "";
   const char *program = getenv("TERM_PROGRAM") ? getenv("TERM_PROGRAM") : "";
   size_t written = 0;

   fp = fopen(cfg->profile_path, "w");
   if (fp == NULL)
   {
      reportf(cfg, "calibrate_profile,error=%s,path=%s\n",
              strerror(errno), cfg->profile_path);
      return -1;
   }

   fprintf(fp, "* Proposed THE UTF-8 physical terminal profile\n");
   fprintf(fp, "* generated_by=utf8_terminal_probe %s\n",
           UTF8_TERMINAL_PROBE_VERSION);
   fprintf(fp, "* TERM=%s\n", term);
   fprintf(fp, "* TERM_PROGRAM=%s\n", program);
   fprintf(fp, "* These command names are the target THE instruction shape.\n");
   fprintf(fp, "* Until THE implements them, keep this file with the probe report.\n\n");
   for (i = 0; i < count; i++)
   {
      const CalibrationEntry *entry = &entries[i];

      if (calibration_entry_is_default(entry))
         continue;
      if (calibration_entry_is_normal_intent(entry))
      {
         fprintf(fp, "SET UTF8 TERMINAL CLASS %s LAYOUT %d CURSOR %d\n",
                 entry->feature_class, entry->layout_width,
                 entry->cursor_width);
         fprintf(fp, "SET UTF8 TERMINAL CLASS %s CURSORSTRATEGY %s\n",
                 entry->feature_class,
                 profile_strategy_name(entry->cursor_strategy));
         fprintf(fp, "SET UTF8 TERMINAL CLASS %s REPLACESTRATEGY %s\n\n",
                 entry->feature_class,
                 profile_strategy_name(entry->replacement_strategy));
      }
      else
      {
         fprintf(fp, "SET UTF8 TERMINAL CLASS %s INTENT %s OUTPUT %s\n",
                 entry->feature_class, entry->display_intent,
                 entry->output_method);
         if (strcmp(entry->output_method, "substitute") != 0)
         {
            fprintf(fp, "SET UTF8 TERMINAL CLASS %s INTENT %s LAYOUT %d CURSOR %d\n",
                    entry->feature_class, entry->display_intent,
                    entry->layout_width, entry->cursor_width);
            fprintf(fp, "SET UTF8 TERMINAL CLASS %s INTENT %s CURSORSTRATEGY %s\n",
                    entry->feature_class, entry->display_intent,
                    profile_strategy_name(entry->cursor_strategy));
            fprintf(fp, "SET UTF8 TERMINAL CLASS %s INTENT %s REPLACESTRATEGY %s\n",
                    entry->feature_class, entry->display_intent,
                    profile_strategy_name(entry->replacement_strategy));
         }
         fprintf(fp, "\n");
      }
      written++;
   }
   if (written == 0)
      fprintf(fp, "* No overrides. Built-in UTF-8 terminal defaults apply.\n");
   fclose(fp);
   reportf(cfg, "calibrate_profile,path=%s,count=%zu,overrides=%zu\n",
           cfg->profile_path, count, written);
   return 0;
}

static void make_effective_calibration_entry(const CalibrationEntry *entry,
                                             CalibrationEntry *effective,
                                             ProbeSample *sample,
                                             char *buffer,
                                             size_t buffer_size);

static int calibration_select_output_method(ProbeConfig *cfg,
                                            CalibrationEntry *entry,
                                            CalibrationEntry *entries,
                                            size_t count)
{
   static const char *group_methods[] =
   {
      "native", "substitute"
   };
   static const char *group_descriptions[] =
   {
      "emit stored sequence and accept terminal-native grouped shaping if it works",
      "use a configured replacement character/string; no ZWJ probe widths needed"
   };
   static const int group_scores[] =
   {
      10, 40
   };
   static const char *component_methods[] =
   {
      "native", "expanded"
   };
   static const char *component_descriptions[] =
   {
      "emit stored sequence and accept terminal-native component fallback if it works",
      "emit component code points with U+200D suppressed for file display"
   };
   static const int component_scores[] =
   {
      10, 20
   };
   const char **methods = group_methods;
   const char **descriptions = group_descriptions;
   const int *scores = group_scores;
   int method_count = (int)(sizeof(group_methods) / sizeof(group_methods[0]));
   int selected = 0;
   int i;

   if (!calibration_entry_is_zwj(entry))
      return 0;
   if (strcmp(entry->display_intent, "components") == 0)
   {
      methods = component_methods;
      descriptions = component_descriptions;
      scores = component_scores;
      method_count = (int)(sizeof(component_methods) / sizeof(component_methods[0]));
   }
   entry->output_method = coerce_output_method_for_intent(entry->display_intent,
                                                          entry->output_method);
   for (i = 0; i < method_count; i++)
      if (strcmp(entry->output_method, methods[i]) == 0)
         selected = i;
   if (cfg->no_visual)
      return 0;

   for (;;)
   {
      int ch;
      char logical_cps[256];

      erase();
      mvprintw(0, 0, "calibrate ZWJ output %s/%s intent=%s %s",
               entry->feature_class, entry->sample->name, entry->display_intent,
               UTF8_TERMINAL_PROBE_VERSION);
      mvprintw(1, 0, "Current: %s. This choice is used by the view/cursor/replace screens.",
               methods[selected]);
      mvprintw(2, 0, "Keys: n/p choose, Enter accept, q back");
      utf8_codepoints(entry->sample->utf8, logical_cps, sizeof(logical_cps));
      mvprintw(3, 0, "file/logical cluster: %s", logical_cps);
      mvprintw(5, 0, "Choose the row whose editor preview matches the intended display.");
      mvprintw(6, 0, "Lower score is preferred if more than one row passes view/cursor/replace.");
      for (i = 0; i < method_count; i++)
      {
         CalibrationEntry preview_entry;
         CalibrationEntry effective_entry;
         ProbeSample effective_sample;
         char display_buffer[512];
         char emit_cps[256];
         int row = 8 + i * 6;
         attr_t label_attr = i == selected ? A_REVERSE : A_NORMAL;

         preview_entry = *entry;
         if (strcmp(methods[i], entry->output_method) != 0)
            apply_output_method_physical_settings(&preview_entry, entries,
                                                  count, methods[i]);
         else
            preview_entry.output_method = methods[i];
         make_effective_calibration_entry(&preview_entry, &effective_entry,
                                          &effective_sample, display_buffer,
                                          sizeof(display_buffer));
         utf8_codepoints(effective_entry.sample->utf8, emit_cps,
                         sizeof(emit_cps));
         attrset(label_attr);
         mvprintw(row, 0, "%c %-12s score=%-2d L%d C%d cursor=%s replace=%s",
                  i == selected ? '>' : ' ', methods[i],
                  scores[i],
                  preview_entry.layout_width, preview_entry.cursor_width,
                  profile_strategy_name(preview_entry.cursor_strategy),
                  profile_strategy_name(preview_entry.replacement_strategy));
         attrset(A_NORMAL);
         mvprintw(row + 1, 2, "%s", descriptions[i]);
         mvprintw(row + 2, 2, "terminal emits: %s", emit_cps);
         mvprintw(row + 3, 2, "editor plain:");
         draw_sample5_at(effective_entry.sample, row + 3, cfg->data_col,
                         preview_entry.layout_width, preview_entry.cursor_width,
                         -1,
                         utfvis_cursor_attr());
         mvprintw(row + 4, 2, "editor cursor:");
         draw_sample5_at(effective_entry.sample, row + 4, cfg->data_col,
                         preview_entry.layout_width, preview_entry.cursor_width,
                         1,
                         utfvis_cursor_attr());
      }
      mvprintw(LINES - 2, 0, "The file bytes and logical cluster stay unchanged; only physical output changes.");
      refresh();
      ch = getch();
      if (ch == 'q' || ch == 'Q')
         return -1;
      if (ch == '\n' || ch == '\r' || ch == KEY_ENTER)
         break;
      if (ch == 'n' || ch == 'j' || ch == KEY_DOWN || ch == KEY_RIGHT)
         selected = (selected + 1) % method_count;
      else if (ch == 'p' || ch == 'k' || ch == KEY_UP || ch == KEY_LEFT)
         selected = selected == 0
                  ? method_count - 1
                  : selected - 1;
   }
   apply_output_method_physical_settings(entry, entries, count,
                                         methods[selected]);
   return 0;
}

static int estimated_codepoint_width(uint32_t codepoint)
{
   if (codepoint == 0x200Du
   ||  (codepoint >= 0xFE00u && codepoint <= 0xFE0Fu)
   ||  (codepoint >= 0xE0100u && codepoint <= 0xE01EFu)
   ||  (codepoint >= 0x0300u && codepoint <= 0x036Fu)
   ||  (codepoint >= 0x1AB0u && codepoint <= 0x1AFFu)
   ||  (codepoint >= 0x1DC0u && codepoint <= 0x1DFFu)
   ||  (codepoint >= 0x20D0u && codepoint <= 0x20FFu))
      return 0;
   if ((codepoint >= 0x1100u && codepoint <= 0x115Fu)
   ||  (codepoint >= 0x2E80u && codepoint <= 0xA4CFu)
   ||  (codepoint >= 0xAC00u && codepoint <= 0xD7A3u)
   ||  (codepoint >= 0xF900u && codepoint <= 0xFAFFu)
   ||  (codepoint >= 0xFE10u && codepoint <= 0xFE19u)
   ||  (codepoint >= 0xFE30u && codepoint <= 0xFE6Fu)
   ||  (codepoint >= 0xFF00u && codepoint <= 0xFF60u)
   ||  (codepoint >= 0xFFE0u && codepoint <= 0xFFE6u)
   ||  (codepoint >= 0x1F000u && codepoint <= 0x1FAFFu)
   ||  (codepoint >= 0x2600u && codepoint <= 0x27BFu))
      return 2;
   return 1;
}

static int estimated_expanded_width(const char *utf8)
{
   const unsigned char *cursor = (const unsigned char *)utf8;
   uint32_t codepoint;
   int width = 0;

   while (utf8_next_codepoint(&cursor, &codepoint))
      width += estimated_codepoint_width(codepoint);
   return width > 0 ? width : 1;
}

static const char *calibration_effective_utf8(const CalibrationEntry *entry,
                                              char *buffer,
                                              size_t buffer_size)
{
   const char *src = entry->sample->utf8;
   size_t zwj_len = strlen(ZWJ_UTF8);
   size_t used = 0;

   if (!calibration_entry_is_zwj(entry)
   ||  strcmp(entry->output_method, "native") == 0)
      return src;
   if (strcmp(entry->output_method, "substitute") == 0)
      return U8_ZWJ_SUBSTITUTE;
   if (strcmp(entry->output_method, "expanded") != 0)
      return src;

   if (buffer_size == 0)
      return src;
   while (*src != '\0' && used + 1 < buffer_size)
   {
      if (strncmp(src, ZWJ_UTF8, zwj_len) == 0)
      {
         src += zwj_len;
         continue;
      }
      buffer[used++] = *src++;
   }
   buffer[used] = '\0';
   return used > 0 ? buffer : entry->sample->utf8;
}

static void make_effective_calibration_entry(const CalibrationEntry *entry,
                                             CalibrationEntry *effective,
                                             ProbeSample *sample,
                                             char *buffer,
                                             size_t buffer_size)
{
   const char *display_utf8;

   *effective = *entry;
   *sample = *entry->sample;
   display_utf8 = calibration_effective_utf8(entry, buffer, buffer_size);
   sample->utf8 = (char *)display_utf8;
   if (strcmp(entry->output_method, "substitute") == 0)
      sample->expected_policy_width = 1;
   else if (strcmp(entry->output_method, "expanded") == 0)
      sample->expected_policy_width = estimated_expanded_width(display_utf8);
   effective->sample = sample;
}

static void copy_calibration_settings(CalibrationEntry *entry,
                                      const CalibrationEntry *source)
{
   entry->layout_width = source->layout_width;
   entry->cursor_width = source->cursor_width;
   entry->cursor_strategy = source->cursor_strategy;
   entry->replacement_strategy = source->replacement_strategy;
   entry->output_method = source->output_method;
}

static int calibration_entry_changed(const CalibrationEntry *left,
                                     const CalibrationEntry *right)
{
   return left->layout_width != right->layout_width
       || left->cursor_width != right->cursor_width
       || strcmp(left->cursor_strategy, right->cursor_strategy) != 0
       || strcmp(left->replacement_strategy, right->replacement_strategy) != 0
       || strcmp(left->output_method, right->output_method) != 0;
}

static int calibration_entry_is_default(const CalibrationEntry *entry)
{
   const CalibrationDefault *defaults = entry->defaults;

   if (defaults == NULL)
      return 0;
   return entry->layout_width == defaults->layout_width
       && entry->cursor_width == defaults->cursor_width
       && strcmp(entry->cursor_strategy, defaults->cursor_strategy) == 0
       && strcmp(entry->replacement_strategy,
                 defaults->replacement_strategy) == 0
       && strcmp(entry->display_intent, defaults->display_intent) == 0
       && strcmp(entry->output_method, defaults->output_method) == 0;
}

static int configure_calibration_entry(ProbeConfig *cfg,
                                       CalibrationEntry *entries,
                                       size_t count,
                                       CalibrationEntry *entry)
{
   CalibrationEntry before = *entry;
   CalibrationEntry effective;
   ProbeSample effective_sample;
   char display_buffer[512];

   if (calibration_select_output_method(cfg, entry, entries, count) != 0)
      return calibration_entry_changed(&before, entry);
   if (calibration_entry_is_zwj(entry)
   &&  strcmp(entry->output_method, "substitute") == 0)
   {
      apply_output_method_defaults(entry);
      return calibration_entry_changed(&before, entry);
   }

   make_effective_calibration_entry(entry, &effective, &effective_sample,
                                    display_buffer, sizeof(display_buffer));
   if (calibration_select_view(cfg, &effective) != 0)
      return calibration_entry_changed(&before, entry);
   copy_calibration_settings(entry, &effective);

   make_effective_calibration_entry(entry, &effective, &effective_sample,
                                    display_buffer, sizeof(display_buffer));
   if (calibration_select_strategy(cfg, &effective, 0) != 0)
   {
      copy_calibration_settings(entry, &effective);
      return calibration_entry_changed(&before, entry);
   }
   copy_calibration_settings(entry, &effective);

   make_effective_calibration_entry(entry, &effective, &effective_sample,
                                    display_buffer, sizeof(display_buffer));
   if (calibration_select_strategy(cfg, &effective, 1) != 0)
   {
      copy_calibration_settings(entry, &effective);
      return calibration_entry_changed(&before, entry);
   }
   copy_calibration_settings(entry, &effective);

   return calibration_entry_changed(&before, entry);
}

static void draw_calibration_menu(ProbeConfig *cfg,
                                  const CalibrationEntry *entries,
                                  size_t count, int selected,
                                  int top, int dirty)
{
   int visible_rows = LINES > 10 ? LINES - 8 : 5;
   int row;
   int i;

   erase();
   mvprintw(0, 0, "UTF-8 terminal calibration %s",
            UTF8_TERMINAL_PROBE_VERSION);
   mvprintw(1, 0, "Profile: %s %s", cfg->profile_path,
            dirty ? "(modified)" : "(clean)");
   mvprintw(2, 0, "Enter configure, j/k move, s save, q quit without saving");
   mvprintw(4, 0, "   class              intent     output     sample             src      L/C   cursor                 replace");
   for (row = 0; row < visible_rows; row++)
   {
      i = top + row;
      if (i >= (int)count)
         break;
      if (i == selected)
         attrset(A_REVERSE);
      mvprintw(5 + row, 0, "%c %-18s %-10s %-10s %-18s %-8s %2d/%-2d %-22s %-22s",
               i == selected ? '>' : ' ',
               entries[i].feature_class, entries[i].display_intent,
               entries[i].output_method, entries[i].sample->name,
               calibration_entry_is_default(&entries[i]) ? "default" : "override",
               entries[i].layout_width, entries[i].cursor_width,
               profile_strategy_name(entries[i].cursor_strategy),
               profile_strategy_name(entries[i].replacement_strategy));
      if (i == selected)
         attrset(A_NORMAL);
   }
   if (top > 0)
      mvprintw(5 + visible_rows, 0, "... more above ...");
   if (top + visible_rows < (int)count)
      mvprintw(6 + visible_rows, 0, "... more below ...");
   refresh();
}

static int calibration_main_menu(ProbeConfig *cfg,
                                 CalibrationEntry *entries,
                                 size_t count)
{
   int selected = 0;
   int top = 0;
   int dirty = 0;

   for (;;)
   {
      int ch;
      int visible_rows = LINES > 10 ? LINES - 8 : 5;

      if (selected < top)
         top = selected;
      if (selected >= top + visible_rows)
         top = selected - visible_rows + 1;
      draw_calibration_menu(cfg, entries, count, selected, top, dirty);
      ch = getch();
      if (ch == 'q' || ch == 'Q')
      {
         reportf(cfg, "calibrate,quit_without_save=1,dirty=%d\n", dirty);
         return 0;
      }
      if (ch == 's' || ch == 'S')
      {
         if (write_calibration_profile(cfg, entries, count) == 0)
         {
            dirty = 0;
            mvprintw(LINES - 1, 0, "saved to %s", cfg->profile_path);
            clrtoeol();
            refresh();
            napms(500);
         }
      }
      else if (ch == 'j' || ch == KEY_DOWN)
      {
         if (selected + 1 < (int)count)
            selected++;
      }
      else if (ch == 'k' || ch == KEY_UP)
      {
         if (selected > 0)
            selected--;
      }
      else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER)
      {
         if (configure_calibration_entry(cfg, entries, count,
                                         &entries[selected]))
            dirty = 1;
         reportf(cfg,
                 "calibrate,class=%s,intent=%s,output=%s,sample=%s,layout_width=%d,cursor_width=%d,cursor_strategy=%s,replacement_strategy=%s\n",
                 entries[selected].feature_class,
                 entries[selected].display_intent,
                 entries[selected].output_method,
                 entries[selected].sample->name,
                 entries[selected].layout_width, entries[selected].cursor_width,
                 entries[selected].cursor_strategy,
                 entries[selected].replacement_strategy);
      }
   }
}

static void run_calibration_probe(ProbeConfig *cfg)
{
   CalibrationEntry entries[32];
   size_t count;
   size_t i;

   count = collect_calibration_entries(cfg, entries,
                                       sizeof(entries) / sizeof(entries[0]));
   reportf(cfg, "section=calibrate selector=%s entries=%zu profile=%s\n",
           cfg->calibrate_selector ? cfg->calibrate_selector : "all",
           count, cfg->profile_path);
   if (count == 0)
   {
      reportf(cfg, "calibrate,error=no-matching-samples\n\n");
      if (!cfg->no_visual)
         mvprintw(0, 0, "No samples matched '%s'",
                  cfg->calibrate_selector ? cfg->calibrate_selector : "focus");
      return;
   }

   read_calibration_profile(cfg, entries, count);
   curs_set(0);
   if (cfg->no_visual)
   {
      for (i = 0; i < count; i++)
         reportf(cfg,
                 "calibrate,class=%s,intent=%s,output=%s,sample=%s,layout_width=%d,cursor_width=%d,cursor_strategy=%s,replacement_strategy=%s\n",
                 entries[i].feature_class, entries[i].display_intent,
                 entries[i].output_method, entries[i].sample->name,
                 entries[i].layout_width, entries[i].cursor_width,
                 entries[i].cursor_strategy, entries[i].replacement_strategy);
      reportf(cfg, "calibrate_profile_write_skipped,no_visual=1\n");
   }
   else
      calibration_main_menu(cfg, entries, count);
   reportf(cfg, "\n");
   curs_set(1);
}

static void run_terminal_absolute_poc_probe(ProbeConfig *cfg)
{
   struct
   {
      const char *scenario;
      ProbeMethod base_method;
      PocRepaintMode repaint_mode;
      int guard_cells;
   } cases[] =
   {
      { "absolute_raw_base_cell", METHOD_RAW_UTF8, POC_CELL_REPAINT, 0 },
      { "waddwstr_base_raw_cell", METHOD_WADDWSTR, POC_CELL_REPAINT, 0 },
      { "cchar_base_raw_cell", METHOD_CCHAR_CLUSTER, POC_CELL_REPAINT, 0 },
      { "absolute_raw_base_span", METHOD_RAW_UTF8, POC_SPAN_REPAINT, 0 },
      { "waddwstr_base_raw_span", METHOD_WADDWSTR, POC_SPAN_REPAINT, 0 },
      { "cchar_base_raw_span", METHOD_CCHAR_CLUSTER, POC_SPAN_REPAINT, 0 },
      { "cchar_base_raw_span_guard", METHOD_CCHAR_CLUSTER, POC_SPAN_REPAINT, 1 }
   };
   size_t i;

   erase();
   reportf(cfg, "section=terminal_absolute_poc\n");
   if (!cfg->no_visual)
   {
      mvprintw(0, 0, "Terminal-absolute keycap POC %s: watch raw '^' marker below target cell",
               UTF8_TERMINAL_PROBE_VERSION);
      mvprintw(1, 0, "Cell rows repaint old/new only; span rows clear and redraw A-keycap-B-space-A");
      mvprintw(1, cfg->data_col, "0123456789");
   }

   for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
   {
      int row = 4 + (int)i * 3;

      if (row >= LINES - 2)
      {
         finish_probe_page(cfg);
         erase();
         row = 4;
      }
      if (!cfg->no_visual)
      {
         mvprintw(row, 0, "%-28s", cases[i].scenario);
         refresh();
      }
      draw_motion_base(cases[i].base_method, row, cfg->data_col);
      refresh();
      if (cases[i].repaint_mode == POC_CELL_REPAINT)
      {
         raw_absolute_motion_step(cfg, cases[i].scenario, row, cfg->data_col, -1, 0);
         raw_absolute_motion_step(cfg, cases[i].scenario, row, cfg->data_col, 0, 1);
         raw_absolute_motion_step(cfg, cases[i].scenario, row, cfg->data_col, 1, 2);
         raw_absolute_motion_step(cfg, cases[i].scenario, row, cfg->data_col, 2, 3);
         raw_absolute_motion_step(cfg, cases[i].scenario, row, cfg->data_col, 3, 4);
      }
      else
      {
         raw_span_repaint_step(cfg, cases[i].scenario, row, cfg->data_col, -1, 0, cases[i].guard_cells);
         raw_span_repaint_step(cfg, cases[i].scenario, row, cfg->data_col, 0, 1, cases[i].guard_cells);
         raw_span_repaint_step(cfg, cases[i].scenario, row, cfg->data_col, 1, 2, cases[i].guard_cells);
         raw_span_repaint_step(cfg, cases[i].scenario, row, cfg->data_col, 2, 3, cases[i].guard_cells);
         raw_span_repaint_step(cfg, cases[i].scenario, row, cfg->data_col, 3, 4, cases[i].guard_cells);
      }
      draw_motion_base(METHOD_RAW_UTF8, row, cfg->data_col);
   }
   reportf(cfg, "\n");
}

static void usage(const char *argv0)
{
   printf("usage: %s [calibrate [selector]] [--profile path] [--report path] [--timeout-ms n]\n", argv0);
   printf("       %s list\n", argv0);
   printf("       %s view [selector] [--report path] [--pause]\n", argv0);
   printf("       %s cursor selector layout_width cursor_width [mode] [--report path] [--timeout-ms n]\n", argv0);
   printf("       %s chain selector layout_width cursor_width [mode] [--report path] [--timeout-ms n]\n", argv0);
   printf("\n");
   printf("common selectors: all, focus, keycap, flag, flags, zwj, or any listed class/sample\n");
   printf("calibrate defaults to selector=all and writes only overrides from coded defaults.\n");
   printf("\n");
   printf("diagnostic compatibility:\n");
   printf("       %s --raw-poc [--report path] [--pause] [--timeout-ms n]\n", argv0);
   printf("       %s --curses-poc [--report path] [--pause] [--timeout-ms n]\n", argv0);
   printf("       %s --utfvis selector [--report path] [--pause]\n", argv0);
   printf("       %s --testcursor selector layout_width cursor_width [mode] [--report path] [--timeout-ms n]\n", argv0);
   printf("          mode is frame, cell, line, flashline, flashcell, flashpair, or flashfrom0..6; default is frame\n");
   printf("       %s --testchain selector layout_width cursor_width [mode] [--report path] [--timeout-ms n]\n", argv0);
   printf("          mode is line, cell, paintfirstcluster, touchfirstcluster, clearfirstcluster,\n");
   printf("          flashfirstfast, flashfirstcluster, flashwhole, or flashbackcluster0..6; default is flashbackcluster1\n");
   printf("       %s --list\n", argv0);
   printf("       %s --version\n", argv0);
}

static void list_samples(const ProbeSample *list, size_t count)
{
   size_t i;

   for (i = 0; i < count; i++)
   {
      char cps[256];

      utf8_codepoints(list[i].utf8, cps, sizeof(cps));
      printf("%-18s %-14s policy=%d %s\n", list[i].name,
             list[i].klass, list[i].expected_policy_width, cps);
   }
}

int main(int argc, char **argv)
{
   ProbeConfig cfg;
   int i;
   int list_only = 0;

   memset(&cfg, 0, sizeof(cfg));
   cfg.report_path = "/tmp/the-utf8-terminal-probe.txt";
   cfg.profile_path = "/tmp/the-utf8-terminal-profile.the";
   cfg.run_matrix = 1;
   cfg.run_motion = 1;
   cfg.run_poc = 1;
   cfg.timeout_ms = 200;
   cfg.data_col = 38;

   if (argc == 1)
   {
      cfg.calibrate = 1;
      cfg.calibrate_selector = "all";
      cfg.run_matrix = 0;
      cfg.run_motion = 0;
      cfg.run_poc = 0;
   }

   for (i = 1; i < argc; i++)
   {
      if (strcmp(argv[i], "--help") == 0)
      {
         usage(argv[0]);
         return 0;
      }
      else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "version") == 0)
      {
         printf("utf8_terminal_probe %s\n", UTF8_TERMINAL_PROBE_VERSION);
         return 0;
      }
      else if (strcmp(argv[i], "--list") == 0 || strcmp(argv[i], "list") == 0)
      {
         list_only = 1;
      }
      else if (strcmp(argv[i], "--cases") == 0 && i + 1 < argc)
      {
         cfg.cases_path = argv[++i];
      }
      else if (strcmp(argv[i], "--report") == 0 && i + 1 < argc)
      {
         cfg.report_path = argv[++i];
      }
      else if (strcmp(argv[i], "--profile") == 0 && i + 1 < argc)
      {
         cfg.profile_path = argv[++i];
      }
      else if (strcmp(argv[i], "--pause") == 0)
      {
         cfg.pause = 1;
      }
      else if (strcmp(argv[i], "--no-visual") == 0)
      {
         cfg.no_visual = 1;
      }
      else if (strcmp(argv[i], "--no-motion") == 0)
      {
         cfg.run_motion = 0;
      }
      else if (strcmp(argv[i], "--no-poc") == 0)
      {
         cfg.run_poc = 0;
      }
      else if (strcmp(argv[i], "--raw-poc") == 0 || strcmp(argv[i], "raw") == 0)
      {
         cfg.raw_poc = 1;
      }
      else if (strcmp(argv[i], "--curses-poc") == 0 || strcmp(argv[i], "curses") == 0)
      {
         cfg.curses_poc = 1;
         cfg.run_matrix = 0;
         cfg.run_motion = 0;
         cfg.run_poc = 0;
      }
      else if ((strcmp(argv[i], "--utfvis") == 0
      ||        strcmp(argv[i], "utfvis") == 0
      ||        strcmp(argv[i], "view") == 0))
      {
         cfg.utfvis = 1;
         if (i + 1 < argc && argv[i + 1][0] != '-')
            cfg.utfvis_selector = argv[++i];
         else
            cfg.utfvis_selector = "focus";
         cfg.run_matrix = 0;
         cfg.run_motion = 0;
         cfg.run_poc = 0;
      }
      else if ((strcmp(argv[i], "--testcursor") == 0
      ||        strcmp(argv[i], "testcursor") == 0
      ||        strcmp(argv[i], "cursor") == 0)
      &&       i + 3 < argc)
      {
         cfg.testcursor = 1;
         cfg.testcursor_selector = argv[++i];
         cfg.testcursor_layout_width = atoi(argv[++i]);
         cfg.testcursor_cursor_width = atoi(argv[++i]);
         while (i + 1 < argc && argv[i + 1][0] != '-')
         {
            const char *arg = argv[++i];

            if (strcmp(arg, "frame") == 0
            ||  strcmp(arg, "cell") == 0
            ||  strcmp(arg, "line") == 0
            ||  strcmp(arg, "flashline") == 0
            ||  strcmp(arg, "flashcell") == 0
            ||  strcmp(arg, "flashpair") == 0
            ||  testcursor_mode_valid(arg))
            {
               cfg.testcursor_mode = arg;
            }
            else
            {
               cfg.testcursor_mode = arg;
            }
         }
         cfg.run_matrix = 0;
         cfg.run_motion = 0;
         cfg.run_poc = 0;
      }
      else if ((strcmp(argv[i], "--testchain") == 0
      ||        strcmp(argv[i], "testchain") == 0
      ||        strcmp(argv[i], "chain") == 0)
      &&       i + 3 < argc)
      {
         cfg.testchain = 1;
         cfg.testchain_selector = argv[++i];
         cfg.testchain_layout_width = atoi(argv[++i]);
         cfg.testchain_cursor_width = atoi(argv[++i]);
         while (i + 1 < argc && argv[i + 1][0] != '-')
         {
            const char *arg = argv[++i];

            cfg.testchain_mode = arg;
         }
         cfg.run_matrix = 0;
         cfg.run_motion = 0;
         cfg.run_poc = 0;
      }
      else if ((strcmp(argv[i], "--calibrate") == 0 || strcmp(argv[i], "calibrate") == 0))
      {
         cfg.calibrate = 1;
         if (i + 1 < argc && argv[i + 1][0] != '-')
            cfg.calibrate_selector = argv[++i];
         else
            cfg.calibrate_selector = "all";
         cfg.run_matrix = 0;
         cfg.run_motion = 0;
         cfg.run_poc = 0;
      }
      else if (strcmp(argv[i], "--testcursor-mode") == 0 && i + 1 < argc)
      {
         cfg.testcursor_mode = argv[++i];
      }
      else if (strcmp(argv[i], "--testchain-mode") == 0 && i + 1 < argc)
      {
         cfg.testchain_mode = argv[++i];
      }
      else if (strcmp(argv[i], "--no-matrix") == 0)
      {
         cfg.run_matrix = 0;
      }
      else if (strcmp(argv[i], "--timeout-ms") == 0 && i + 1 < argc)
      {
         cfg.timeout_ms = atoi(argv[++i]);
      }
      else
      {
         usage(argv[0]);
         return 2;
      }
   }

   if (load_probe_cases(&cfg, cfg.cases_path) != 0)
      return 2;
   if (cfg.utfvis && find_first_sample(&cfg, cfg.utfvis_selector) == NULL)
   {
      fprintf(stderr, "No UTF-8 probe sample matched '%s'\n", cfg.utfvis_selector);
      return 2;
   }
   if (cfg.testcursor)
   {
      if (find_first_sample(&cfg, cfg.testcursor_selector) == NULL)
      {
         fprintf(stderr, "No UTF-8 probe sample matched '%s'\n",
                 cfg.testcursor_selector);
         return 2;
      }
      if (cfg.testcursor_layout_width < 1 || cfg.testcursor_cursor_width < 1)
      {
         fprintf(stderr, "layout_width and cursor_width must be positive\n");
         return 2;
      }
      if (!testcursor_mode_valid(cfg.testcursor_mode))
      {
         fprintf(stderr, "testcursor mode must be frame, cell, line, flashline, flashcell, flashpair, or flashfrom0..6\n");
         return 2;
      }
   }
   if (cfg.testchain)
   {
      if (find_first_sample(&cfg, cfg.testchain_selector) == NULL)
      {
         fprintf(stderr, "No UTF-8 probe sample matched '%s'\n",
                 cfg.testchain_selector);
         return 2;
      }
      if (cfg.testchain_layout_width < 1 || cfg.testchain_cursor_width < 1)
      {
         fprintf(stderr, "layout_width and cursor_width must be positive\n");
         return 2;
      }
      if (!testchain_mode_valid(cfg.testchain_mode))
      {
         fprintf(stderr, "testchain mode must be line, cell, paintfirstcluster, touchfirstcluster, clearfirstcluster, flashfirstfast, flashfirstcluster, flashwhole, or flashbackcluster0..6\n");
         return 2;
      }
   }
   if (cfg.calibrate && find_first_sample(&cfg, cfg.calibrate_selector) == NULL)
   {
      fprintf(stderr, "No UTF-8 probe sample matched '%s'\n",
              cfg.calibrate_selector ? cfg.calibrate_selector : "focus");
      return 2;
   }
   if (list_only)
   {
      list_samples(cfg.samples, cfg.sample_count);
      return 0;
   }

   if (setlocale(LC_ALL, "") == NULL)
   {
      fprintf(stderr, "setlocale failed; UTF-8 conversion will not be reliable\n");
      return 2;
   }

   cfg.report = fopen(cfg.report_path, "w");
   if (cfg.report == NULL)
   {
      fprintf(stderr, "%s: %s\n", cfg.report_path, strerror(errno));
      return 2;
   }

   if (cfg.raw_poc)
   {
#if defined(_WIN32)
      log_raw_environment(&cfg);
      run_raw_terminal_poc_probe(&cfg);
#else
      struct termios saved_termios;
      int raw_enabled = enable_raw_input(&saved_termios) == 0;

      log_raw_environment(&cfg);
      run_raw_terminal_poc_probe(&cfg);
      if (raw_enabled)
         restore_raw_input(&saved_termios);
#endif
      fclose(cfg.report);
      printf("\nUTF-8 terminal probe %s report written to %s\n",
             UTF8_TERMINAL_PROBE_VERSION, cfg.report_path);
      return 0;
   }

   initscr();
   cbreak();
   noecho();
   keypad(stdscr, TRUE);
   leaveok(stdscr, FALSE);
   curs_set(1);
   if (COLS < 70)
      cfg.data_col = 26;

   log_environment(&cfg);
   if (cfg.curses_poc)
      run_curses_terminal_poc_probe(&cfg);
   if (cfg.utfvis)
      run_utfvis_probe(&cfg, cfg.utfvis_selector);
   if (cfg.testcursor)
      run_testcursor_probe(&cfg);
   if (cfg.testchain)
      run_testchain_probe(&cfg);
   if (cfg.calibrate)
      run_calibration_probe(&cfg);
   if (cfg.run_matrix)
      run_matrix_probe(&cfg);
   if (cfg.run_motion)
      run_keycap_motion_probe(&cfg);
   if (cfg.run_poc)
      run_terminal_absolute_poc_probe(&cfg);
   finish_probe_page(&cfg);
   endwin();
   fclose(cfg.report);
   printf("UTF-8 terminal probe %s report written to %s\n",
          UTF8_TERMINAL_PROBE_VERSION, cfg.report_path);
   return 0;
}
