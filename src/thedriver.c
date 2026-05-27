#include "thedriver.h"

#ifdef THE_DRIVER_ENABLE_CURSES
extern const TheDriverOps the_curses_driver_ops;
# define THE_DRIVER_DEFAULT_OPS (&the_curses_driver_ops)
#else
# define THE_DRIVER_DEFAULT_OPS NULL
#endif

#ifdef THE_DRIVER_ENABLE_HEADLESS
extern const TheDriverOps the_headless_driver_ops;
#endif

const TheDriverOps *the_driver = THE_DRIVER_DEFAULT_OPS;

void the_driver_select(const TheDriverOps *ops)
{
   the_driver = ops;
}

int the_driver_use_curses(void)
{
#ifdef THE_DRIVER_ENABLE_CURSES
   the_driver_select(&the_curses_driver_ops);
   return 1;
#else
   return 0;
#endif
}

int the_driver_use_headless(void)
{
#ifdef THE_DRIVER_ENABLE_HEADLESS
   the_driver_select(&the_headless_driver_ops);
   return 1;
#else
   return 0;
#endif
}
