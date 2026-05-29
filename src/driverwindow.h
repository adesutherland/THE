#ifndef THE_DRIVERWINDOW_H
#define THE_DRIVERWINDOW_H

#include "thedriver.h"

static inline int driver_window_valid_screen(CHARTYPE scrno)
{
   return scrno < MAX_SCREENS;
}

static inline int driver_window_valid_role(short role)
{
   return role >= 0 && role < VIEW_WINDOWS;
}

static inline TheDriverWindow *driver_screen_role_window(CHARTYPE scrno,
                                                         short role)
{
   if (!driver_window_valid_screen(scrno)
   ||  !driver_window_valid_role(role))
      return NULL;
   return screen[scrno].win[role];
}

static inline TheDriverWindow **driver_screen_role_window_slot(
   CHARTYPE scrno, short role)
{
   if (!driver_window_valid_screen(scrno)
   ||  !driver_window_valid_role(role))
      return NULL;
   return &screen[scrno].win[role];
}

static inline TheDriverWindow *driver_current_role_window(short role)
{
   return driver_screen_role_window(current_screen, role);
}

static inline TheDriverWindow **driver_current_role_window_slot(short role)
{
   return driver_screen_role_window_slot(current_screen, role);
}

static inline TheDriverWindow *driver_screen_current_window(CHARTYPE scrno)
{
   VIEW_DETAILS *view;

   if (!driver_window_valid_screen(scrno))
      return NULL;
   view = screen[scrno].screen_view;
   if (view == NULL || !driver_window_valid_role(view->current_window))
      return NULL;
   return screen[scrno].win[view->current_window];
}

static inline TheDriverWindow *driver_current_window(void)
{
   return driver_screen_current_window(current_screen);
}

static inline TheDriverWindow *driver_current_previous_window(void)
{
   if (CURRENT_VIEW == NULL
   ||  !driver_window_valid_role(CURRENT_VIEW->previous_window))
      return NULL;
   return CURRENT_SCREEN.win[CURRENT_VIEW->previous_window];
}

static inline int driver_current_window_is_role(short role)
{
   return CURRENT_VIEW != NULL && CURRENT_VIEW->current_window == role;
}

static inline int driver_current_window_exists(void)
{
   return driver_current_window() != NULL;
}

static inline int driver_screen_window_is_role(CHARTYPE scrno, short role)
{
   VIEW_DETAILS *view;

   if (!driver_window_valid_screen(scrno))
      return 0;
   view = screen[scrno].screen_view;
   return view != NULL && view->current_window == role;
}

static inline int driver_current_role_exists(short role)
{
   return driver_current_role_window(role) != NULL;
}

static inline int driver_screen_role_exists(CHARTYPE scrno, short role)
{
   return driver_screen_role_window(scrno, role) != NULL;
}

static inline TheDriverWindow **driver_global_window_slot(
   TheDriverGlobalWindowRole role)
{
   switch(role)
   {
      case THE_DRIVER_GLOBAL_STATAREA:
         return &statarea;
      case THE_DRIVER_GLOBAL_ERROR:
         return &error_window;
      case THE_DRIVER_GLOBAL_DIVIDER:
         return &divider;
      case THE_DRIVER_GLOBAL_FILETABS:
         return &filetabs;
      default:
         return NULL;
   }
}

static inline TheDriverWindow *driver_global_window(
   TheDriverGlobalWindowRole role)
{
   TheDriverWindow **slot = driver_global_window_slot(role);

   return slot == NULL ? NULL : *slot;
}

static inline int driver_global_window_exists(TheDriverGlobalWindowRole role)
{
   return driver_global_window(role) != NULL;
}

static inline void driver_delete_global_window(TheDriverGlobalWindowRole role)
{
   TheDriverWindow **slot = driver_global_window_slot(role);

   if (slot == NULL || *slot == NULL)
      return;
   the_driver->delete_window(*slot);
   *slot = NULL;
}

static inline TheDriverWindowRoleSave driver_save_current_role_window(
   short role)
{
   TheDriverWindowRoleSave saved = { NULL, 0 };
   TheDriverWindow **slot = driver_current_role_window_slot(role);

   if (slot == NULL)
      return saved;
   saved.window = *slot;
   saved.slot_valid = 1;
   return saved;
}

static inline void driver_restore_current_role_window(
   short role, TheDriverWindowRoleSave saved)
{
   TheDriverWindow **slot;

   if (!saved.slot_valid)
      return;
   slot = driver_current_role_window_slot(role);
   if (slot != NULL)
      *slot = saved.window;
}

static inline void driver_delete_current_role_window(short role)
{
   TheDriverWindow **slot = driver_current_role_window_slot(role);

   if (slot == NULL || *slot == NULL)
      return;
   the_driver->delete_window(*slot);
   *slot = NULL;
}

static inline void driver_touch_and_refresh_window(TheDriverWindow *win)
{
   the_driver->touch_window(win);
   the_driver->refresh_window(win);
}

static inline TheDriverScreenPoint driver_window_cursor_screen_point(
   TheDriverWindow *win)
{
   TheDriverScreenPoint point = { -1, -1, 0 };
   TheDriverWindowCursor cursor;
   TheDriverWindowOrigin origin;

   cursor = the_driver->capture_window_cursor(win);
   origin = the_driver->window_origin(win);
   if (!cursor.valid || !origin.valid)
      return point;
   point.row = (short)(cursor.row + origin.row);
   point.col = (short)(cursor.col + origin.col);
   point.valid = 1;
   return point;
}

#endif
