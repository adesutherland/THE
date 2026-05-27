#include "the.h"
#include "thedriver.h"
#include "cursesdriver.h"

const TheDriverOps *the_driver = &the_curses_driver_ops;
