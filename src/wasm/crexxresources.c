#include <stddef.h>

/* This static profile has no native command-resource owner. Decline that
 * optional service rather than expose ambient host directories. */
int rxcrexxcmd_active_resource_snapshot(char **out)
{
    if (out) *out = NULL;
    return -1;
}
