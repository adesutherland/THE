#include "thedriver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "thekeys.h"

#if defined(_WIN32)
# include <windows.h>
# define THE_DRIVER_PATH_SEPARATOR ';'
# define THE_DRIVER_DIR_SEPARATOR '\\'
#else
# include <dlfcn.h>
# include <unistd.h>
# define THE_DRIVER_PATH_SEPARATOR ':'
# define THE_DRIVER_DIR_SEPARATOR '/'
#endif

#ifndef THE_DRIVER_MODULE_SUFFIX
# if defined(_WIN32)
#  define THE_DRIVER_MODULE_SUFFIX ".dll"
# else
#  define THE_DRIVER_MODULE_SUFFIX ".so"
# endif
#endif

#ifndef THE_DRIVER_INSTALL_DIR
# define THE_DRIVER_INSTALL_DIR ""
#endif

#ifdef THE_DRIVER_ENABLE_CURSES
extern const TheDriverOps the_curses_driver_ops;
extern const TheDriverModuleLifecycle the_curses_driver_lifecycle;
#endif

#ifdef THE_DRIVER_ENABLE_HEADLESS
extern const TheDriverOps the_headless_driver_ops;
extern const TheDriverModuleLifecycle the_headless_driver_lifecycle;
#endif

typedef const TheDriverOps *(*TheDriverModuleOpsFn)(void);
typedef const TheDriverModuleLifecycle *(*TheDriverModuleLifecycleFn)(void);

const TheDriverOps *the_driver = NULL;

static const TheDriverModuleLifecycle *current_lifecycle = NULL;
static int current_lifecycle_started = 0;
static char current_driver_name[32];

#if defined(_WIN32)
static HMODULE current_module = NULL;
#else
static void *current_module = NULL;
#endif

static void copy_error(char *error, size_t error_len, const char *text)
{
   size_t len;

   if (error == NULL || error_len == 0)
      return;
   if (text == NULL)
      text = "";
   len = strlen(text);
   if (len >= error_len)
      len = error_len - 1;
   if (len > 0)
      memcpy(error, text, len);
   error[len] = '\0';
}

static void append_error(char *error, size_t error_len, const char *text)
{
   size_t used;
   size_t len;

   if (error == NULL || error_len == 0 || text == NULL)
      return;
   used = strlen(error);
   if (used + 1 >= error_len)
      return;
   len = strlen(text);
   if (len >= error_len - used)
      len = error_len - used - 1;
   if (len > 0)
      memcpy(error + used, text, len);
   error[used + len] = '\0';
}

static int path_has_separator(const char *path)
{
   if (path == NULL)
      return 0;
   return strchr(path, '/') != NULL || strchr(path, '\\') != NULL;
}

static void dirname_from_path(const char *path, char *out, size_t out_len)
{
   const char *slash;
   const char *backslash;
   const char *end;
   size_t len;

   if (out == NULL || out_len == 0)
      return;
   out[0] = '\0';
   if (path == NULL || *path == '\0')
   {
      copy_error(out, out_len, ".");
      return;
   }
   slash = strrchr(path, '/');
   backslash = strrchr(path, '\\');
   end = slash > backslash ? slash : backslash;
   if (end == NULL)
   {
      copy_error(out, out_len, ".");
      return;
   }
   len = (size_t)(end - path);
   if (len == 0)
      len = 1;
   if (len >= out_len)
      len = out_len - 1;
   memcpy(out, path, len);
   out[len] = '\0';
}

static void join_path(char *out, size_t out_len, const char *dir,
                      const char *leaf)
{
   size_t used;

   if (out == NULL || out_len == 0)
      return;
   if (dir == NULL || *dir == '\0')
      dir = ".";
   snprintf(out, out_len, "%s", dir);
   used = strlen(out);
   if (used > 0
   &&  out[used - 1] != '/'
   &&  out[used - 1] != '\\')
   {
      snprintf(out + used, out_len - used, "%c", THE_DRIVER_DIR_SEPARATOR);
      used = strlen(out);
   }
   snprintf(out + used, out_len - used, "%s", leaf == NULL ? "" : leaf);
}

static void module_filename(const char *name, char *out, size_t out_len)
{
   snprintf(out, out_len, "the_driver_%s%s", name,
            THE_DRIVER_MODULE_SUFFIX);
}

static int try_load_file(const char *path, const char *name,
                         char *error, size_t error_len)
{
   TheDriverModuleOpsFn ops_fn;
   TheDriverModuleLifecycleFn lifecycle_fn;
   const TheDriverOps *ops;
   const TheDriverModuleLifecycle *lifecycle = NULL;

#if defined(_WIN32)
   HMODULE module;

   module = LoadLibraryA(path);
   if (module == NULL)
   {
      char message[256];
      snprintf(message, sizeof(message), "load failed: %s\n", path);
      append_error(error, error_len, message);
      return 0;
   }
   ops_fn = (TheDriverModuleOpsFn)(void *)GetProcAddress(
      module, "the_driver_module_ops");
   lifecycle_fn = (TheDriverModuleLifecycleFn)(void *)GetProcAddress(
      module, "the_driver_module_lifecycle");
#else
   void *module;

   module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
   if (module == NULL)
   {
      char message[512];
      snprintf(message, sizeof(message), "load failed: %s: %s\n",
               path, dlerror());
      append_error(error, error_len, message);
      return 0;
   }
   ops_fn = (TheDriverModuleOpsFn)dlsym(module, "the_driver_module_ops");
   lifecycle_fn = (TheDriverModuleLifecycleFn)dlsym(
      module, "the_driver_module_lifecycle");
#endif

   if (ops_fn == NULL)
   {
      char message[256];
      snprintf(message, sizeof(message),
               "load failed: %s: missing the_driver_module_ops\n", path);
      append_error(error, error_len, message);
#if defined(_WIN32)
      FreeLibrary(module);
#else
      dlclose(module);
#endif
      return 0;
   }
   ops = ops_fn();
   if (ops == NULL)
   {
      char message[256];
      snprintf(message, sizeof(message),
               "load failed: %s: module returned no driver ops\n", path);
      append_error(error, error_len, message);
#if defined(_WIN32)
      FreeLibrary(module);
#else
      dlclose(module);
#endif
      return 0;
   }
   if (lifecycle_fn != NULL)
      lifecycle = lifecycle_fn();

   the_driver_close_module();
   current_module = module;
   current_lifecycle = lifecycle;
   the_driver = ops;
   snprintf(current_driver_name, sizeof(current_driver_name), "%s", name);
   return 1;
}

static int try_load_from_dir(const char *dir, const char *name,
                             char *error, size_t error_len)
{
   char filename[128];
   char path[2048];
   char driver_dir[2048];

   module_filename(name, filename, sizeof(filename));
   join_path(path, sizeof(path), dir, filename);
   if (try_load_file(path, name, error, error_len))
      return 1;
   join_path(driver_dir, sizeof(driver_dir), dir, "drivers");
   join_path(path, sizeof(path), driver_dir, filename);
   return try_load_file(path, name, error, error_len);
}

static int try_load_from_env(const char *name, char *error, size_t error_len)
{
   const char *env = getenv("THE_DRIVER_PATH");
   const char *start;

   if (env == NULL || *env == '\0')
      return 0;
   start = env;
   while (*start != '\0')
   {
      const char *end = strchr(start, THE_DRIVER_PATH_SEPARATOR);
      char component[2048];
      size_t len = end == NULL ? strlen(start) : (size_t)(end - start);

      if (len >= sizeof(component))
         len = sizeof(component) - 1;
      memcpy(component, start, len);
      component[len] = '\0';
      if (component[0] != '\0')
      {
         if (path_has_separator(component)
         &&  try_load_file(component, name, error, error_len))
            return 1;
         if (try_load_from_dir(component, name, error, error_len))
            return 1;
      }
      if (end == NULL)
         break;
      start = end + 1;
   }
   return 0;
}

static int select_static_driver(const char *name)
{
#ifdef THE_DRIVER_ENABLE_CURSES
   if (strcmp(name, "curses") == 0)
   {
      the_driver_select(&the_curses_driver_ops);
      current_lifecycle = &the_curses_driver_lifecycle;
      current_lifecycle_started = 0;
      snprintf(current_driver_name, sizeof(current_driver_name), "%s", name);
      return 1;
   }
#endif
#ifdef THE_DRIVER_ENABLE_HEADLESS
   if (strcmp(name, "llm") == 0 || strcmp(name, "headless") == 0)
   {
      the_driver_select(&the_headless_driver_ops);
      current_lifecycle = &the_headless_driver_lifecycle;
      current_lifecycle_started = 0;
      snprintf(current_driver_name, sizeof(current_driver_name), "%s", name);
      return 1;
   }
#endif
   return 0;
}

void the_driver_select(const TheDriverOps *ops)
{
   the_driver = ops;
   current_lifecycle = NULL;
   current_lifecycle_started = 0;
   current_driver_name[0] = '\0';
}

int the_driver_load(const char *name, const char *argv0,
                    char *error, size_t error_len)
{
   char exe_dir[2048];
   char release_dir[2048];

   if (name == NULL || *name == '\0')
   {
      copy_error(error, error_len, "missing driver name");
      return 0;
   }
   if (strcmp(name, "headless") == 0)
      name = "llm";
   if (strcmp(name, "curses") != 0 && strcmp(name, "llm") != 0)
   {
      copy_error(error, error_len, "unknown driver");
      return 0;
   }
   if (error != NULL && error_len > 0)
      error[0] = '\0';

   if (select_static_driver(name))
      return 1;
   if (try_load_from_env(name, error, error_len))
      return 1;

   dirname_from_path(argv0, exe_dir, sizeof(exe_dir));
   if (try_load_from_dir(exe_dir, name, error, error_len))
      return 1;
   join_path(release_dir, sizeof(release_dir), exe_dir, "release");
   if (try_load_from_dir(release_dir, name, error, error_len))
      return 1;
   if (THE_DRIVER_INSTALL_DIR[0] != '\0'
   &&  try_load_from_dir(THE_DRIVER_INSTALL_DIR, name, error, error_len))
      return 1;

   if (error != NULL && error_len > 0 && error[0] == '\0')
      snprintf(error, error_len, "driver module not found: %s", name);
   return 0;
}

int the_driver_use_curses(void)
{
   return select_static_driver("curses");
}

int the_driver_use_headless(void)
{
   return select_static_driver("llm");
}

int the_driver_is_curses(void)
{
   return strcmp(current_driver_name, "curses") == 0;
}

int the_driver_is_headless(void)
{
   return strcmp(current_driver_name, "llm") == 0
       || strcmp(current_driver_name, "headless") == 0;
}

int the_driver_read_legacy_key(void)
{
   TheInputEvent event;
   int key = THE_KEY_NONE;

   if (the_driver == NULL || the_driver->read_input_event == NULL)
      return THE_KEY_NONE;
   if (!the_driver->read_input_event(&event))
      return THE_KEY_NONE;
   if (!the_input_event_to_legacy_key(&event, &key))
      return THE_KEY_NONE;
   return key;
}

int the_driver_start(const TheDriverStartupOptions *options,
                     char *error, size_t error_len)
{
   current_lifecycle_started = 0;
   if (current_lifecycle != NULL && current_lifecycle->activate != NULL
   &&  !current_lifecycle->activate())
   {
      copy_error(error, error_len, "driver activation failed");
      return 0;
   }
   if (current_lifecycle != NULL && current_lifecycle->start != NULL)
   {
      int started = current_lifecycle->start(options, error, error_len);
      current_lifecycle_started = started ? 1 : 0;
      return started;
   }
   if (current_lifecycle != NULL)
      current_lifecycle_started = 1;
   return 1;
}

void the_driver_shutdown(int prompt_on_error)
{
   if (current_lifecycle != NULL && current_lifecycle->shutdown != NULL)
      current_lifecycle->shutdown(prompt_on_error);
   current_lifecycle_started = 0;
}

void the_driver_signal_shutdown(void)
{
   if (current_lifecycle != NULL && current_lifecycle->signal_shutdown != NULL)
      current_lifecycle->signal_shutdown();
}

void the_driver_close_module(void)
{
   the_driver = NULL;
   current_lifecycle = NULL;
   current_lifecycle_started = 0;
   current_driver_name[0] = '\0';
   if (current_module != NULL)
   {
#if defined(_WIN32)
      FreeLibrary(current_module);
#else
      dlclose(current_module);
#endif
      current_module = NULL;
   }
}

void the_driver_suspend_terminal(void)
{
   if (current_lifecycle != NULL && current_lifecycle->suspend_terminal != NULL)
      current_lifecycle->suspend_terminal();
}

void the_driver_resume_terminal(void)
{
   if (current_lifecycle != NULL && current_lifecycle->resume_terminal != NULL)
      current_lifecycle->resume_terminal();
}

void the_driver_resize_terminal(int rows, int cols)
{
   if (current_lifecycle != NULL && current_lifecycle->resize_terminal != NULL)
      current_lifecycle->resize_terminal(rows, cols);
}

void the_driver_refresh_terminal_size(void)
{
   if (current_lifecycle != NULL
   &&  current_lifecycle->refresh_terminal_size != NULL)
      current_lifecycle->refresh_terminal_size();
}

int the_driver_is_terminal_resized(void)
{
   if (current_lifecycle != NULL
   &&  current_lifecycle->is_terminal_resized != NULL)
      return current_lifecycle->is_terminal_resized();
   return 0;
}

int the_driver_read_terminal_legacy_key(void)
{
   if (current_lifecycle != NULL
   &&  current_lifecycle->read_terminal_legacy_key != NULL)
      return current_lifecycle->read_terminal_legacy_key();
   return the_driver_read_legacy_key();
}

int the_driver_read_raw_window_key(TheDriverWindow *win)
{
   if (current_lifecycle != NULL
   &&  current_lifecycle->read_raw_window_key != NULL)
      return current_lifecycle->read_raw_window_key(win);
   (void)win;
   return the_driver_read_legacy_key();
}

void the_driver_set_window_leaveok(TheDriverWindow *win, bool enabled)
{
   if (current_lifecycle != NULL
   &&  current_lifecycle->set_window_leaveok != NULL)
      current_lifecycle->set_window_leaveok(win, enabled);
}

void the_driver_slk_touch(void)
{
   if (current_lifecycle != NULL && current_lifecycle->slk_touch != NULL)
      current_lifecycle->slk_touch();
}

void the_driver_slk_noutrefresh(void)
{
   if (current_lifecycle != NULL && current_lifecycle->slk_noutrefresh != NULL)
      current_lifecycle->slk_noutrefresh();
}

void the_driver_slk_clear(void)
{
   if (current_lifecycle != NULL && current_lifecycle->slk_clear != NULL)
      current_lifecycle->slk_clear();
}

void the_driver_slk_restore(void)
{
   if (current_lifecycle != NULL && current_lifecycle->slk_restore != NULL)
      current_lifecycle->slk_restore();
}

void the_driver_slk_set(int key, const char *label, int format)
{
   if (current_lifecycle != NULL && current_lifecycle->slk_set != NULL)
      current_lifecycle->slk_set(key, label, format);
}

void the_driver_slk_attrset(TheDriverAttr attr)
{
   if (current_lifecycle != NULL && current_lifecycle->slk_attrset != NULL)
      current_lifecycle->slk_attrset(attr);
}

void the_driver_set_current_screen(CHARTYPE scrno)
{
   if (current_lifecycle != NULL
   &&  current_lifecycle->set_current_screen != NULL)
      current_lifecycle->set_current_screen(scrno);
}

void the_driver_set_screen_current_role(CHARTYPE scrno, short role)
{
   if (current_lifecycle != NULL
   &&  current_lifecycle->set_screen_current_role != NULL)
      current_lifecycle->set_screen_current_role(scrno, role);
}

TheDriverWindow *the_driver_create_screen_role(CHARTYPE scrno, short role,
                                               int rows, int cols,
                                               int row, int col)
{
   TheDriverWindow *win = NULL;

   if (current_lifecycle != NULL
   &&  current_lifecycle->create_screen_role != NULL)
      win = current_lifecycle->create_screen_role(scrno, role, rows, cols,
                                                  row, col);
   else if (the_driver != NULL && the_driver->create_window != NULL)
      win = the_driver->create_window(rows, cols, row, col);
   return win;
}

TheDriverWindow *the_driver_create_global_window(TheDriverGlobalWindowRole role,
                                                 int rows, int cols,
                                                 int row, int col)
{
   TheDriverWindow *win = NULL;

   if (current_lifecycle != NULL
   &&  current_lifecycle->create_global_window != NULL)
      win = current_lifecycle->create_global_window(role, rows, cols, row,
                                                    col);
   else if (the_driver != NULL && the_driver->create_window != NULL)
      win = the_driver->create_window(rows, cols, row, col);
   return win;
}

size_t the_driver_log_count(void)
{
   if (current_lifecycle != NULL && current_lifecycle->log_count != NULL)
      return current_lifecycle->log_count();
   return 0;
}

const char *the_driver_log_entry(size_t index)
{
   if (current_lifecycle != NULL && current_lifecycle->log_entry != NULL)
      return current_lifecycle->log_entry(index);
   return NULL;
}

void the_driver_current_mouse_screen_role_position(CHARTYPE scrno,
                                                   short role,
                                                   int *row, int *col)
{
   if (current_lifecycle != NULL
   &&  current_lifecycle->current_mouse_screen_role_position != NULL)
      current_lifecycle->current_mouse_screen_role_position(scrno, role,
                                                            row, col);
}

void the_driver_current_mouse_global_position(TheDriverGlobalWindowRole role,
                                              int *row, int *col)
{
   if (current_lifecycle != NULL
   &&  current_lifecycle->current_mouse_global_position != NULL)
      current_lifecycle->current_mouse_global_position(role, row, col);
}

void the_driver_current_mouse_screen_position(int *row, int *col)
{
   if (current_lifecycle != NULL
   &&  current_lifecycle->current_mouse_screen_position != NULL)
      current_lifecycle->current_mouse_screen_position(row, col);
}

void the_driver_clear_mouse_packet_position(void)
{
   if (current_lifecycle != NULL
   &&  current_lifecycle->clear_mouse_packet_position != NULL)
      current_lifecycle->clear_mouse_packet_position();
}

int the_driver_read_pending_mouse_button(int *button, int *action,
                                         int *modifier)
{
   if (current_lifecycle != NULL
   &&  current_lifecycle->read_pending_mouse_button != NULL)
      return current_lifecycle->read_pending_mouse_button(button, action,
                                                          modifier);
   return 0;
}

int the_driver_read_transient_mouse_event(TheDriverWindow *win,
                                          TheDriverMouseEvent *event)
{
   if (current_lifecycle != NULL
   &&  current_lifecycle->read_transient_mouse_event != NULL)
      return current_lifecycle->read_transient_mouse_event(win, event);
   return 0;
}

int the_driver_read_current_role_transient_mouse_event(
   short role, TheDriverMouseEvent *event)
{
   if (current_lifecycle != NULL
   &&  current_lifecycle->read_current_role_transient_mouse_event != NULL)
      return current_lifecycle->read_current_role_transient_mouse_event(
         role, event);
   return 0;
}

int the_driver_color_pair_count(void)
{
   if (current_lifecycle != NULL
   &&  current_lifecycle->color_pair_count != NULL)
      return current_lifecycle->color_pair_count();
   return 1;
}

int the_driver_color_count(void)
{
   if (current_lifecycle != NULL && current_lifecycle->color_count != NULL)
      return current_lifecycle->color_count();
   return 16;
}

int the_driver_can_change_color(void)
{
   if (current_lifecycle != NULL
   &&  current_lifecycle->can_change_color != NULL)
      return current_lifecycle->can_change_color();
   return 0;
}

void the_driver_init_pair(int pair, int fg, int bg)
{
   if (current_lifecycle != NULL && current_lifecycle->init_pair != NULL)
      current_lifecycle->init_pair(pair, fg, bg);
}

void the_driver_init_color(int color, int red, int green, int blue)
{
   if (current_lifecycle != NULL && current_lifecycle->init_color != NULL)
      current_lifecycle->init_color(color, red, green, blue);
}

const char *the_driver_ui_version(void)
{
   if (current_lifecycle != NULL && current_lifecycle->ui_version != NULL)
      return current_lifecycle->ui_version();
   if (current_driver_name[0] != '\0')
      return current_driver_name;
   return "unloaded";
}

int the_driver_mouse_interval(int interval)
{
   if (current_lifecycle != NULL && current_lifecycle->mouse_interval != NULL)
      return current_lifecycle->mouse_interval(interval);
   (void)interval;
   return -1;
}

void the_driver_mouse_mask(int enabled)
{
   if (current_lifecycle != NULL && current_lifecycle->mouse_mask != NULL)
      current_lifecycle->mouse_mask(enabled);
}

void the_driver_nap_ms(int milliseconds)
{
   if (milliseconds <= 0)
      return;
   if (current_lifecycle_started
   &&  current_lifecycle != NULL
   &&  current_lifecycle->nap_ms != NULL)
   {
      current_lifecycle->nap_ms(milliseconds);
      return;
   }
#if defined(_WIN32)
   Sleep((DWORD)milliseconds);
#else
   usleep((useconds_t)milliseconds * 1000);
#endif
}

TheDriverCell the_driver_alternate_cell(TheDriverAltCell cell)
{
   if (current_lifecycle != NULL
   &&  current_lifecycle->alternate_cell != NULL)
      return current_lifecycle->alternate_cell(cell);
   switch (cell)
   {
      case THE_DRIVER_ALT_UARROW:
         return the_driver_cell_make_alternate('^', THE_RENDER_ATTR_NORMAL);
      case THE_DRIVER_ALT_DARROW:
         return the_driver_cell_make_alternate('v', THE_RENDER_ATTR_NORMAL);
      case THE_DRIVER_ALT_LARROW:
         return the_driver_cell_make_alternate('<', THE_RENDER_ATTR_NORMAL);
      case THE_DRIVER_ALT_RARROW:
         return the_driver_cell_make_alternate('>', THE_RENDER_ATTR_NORMAL);
      case THE_DRIVER_ALT_VLINE:
      default:
         return the_driver_cell_make_alternate('|', THE_RENDER_ATTR_NORMAL);
   }
}

CursorShape current_cursor_shape(void)
{
   if (current_lifecycle != NULL
   &&  current_lifecycle->current_cursor_shape != NULL)
      return current_lifecycle->current_cursor_shape();
   return CURSOR_BLOCK;
}

CursorBlink current_cursor_blink(void)
{
   if (current_lifecycle != NULL
   &&  current_lifecycle->current_cursor_blink != NULL)
      return current_lifecycle->current_cursor_blink();
   return CURSOR_STEADY;
}

CursorPresentation current_cursor_presentation(void)
{
   if (current_lifecycle != NULL
   &&  current_lifecycle->current_cursor_presentation != NULL)
      return current_lifecycle->current_cursor_presentation();
   return CURSOR_PRESENTATION_HARDWARE;
}

bool current_cursor_uses_software(void)
{
   if (current_lifecycle != NULL
   &&  current_lifecycle->current_cursor_uses_software != NULL)
      return current_lifecycle->current_cursor_uses_software();
   return current_cursor_presentation() == CURSOR_PRESENTATION_SOFTWARE;
}
