#ifndef THE_FRONTENDPOLICY_H
#define THE_FRONTENDPOLICY_H

#include <stddef.h>

#include "frontendaction.h"

#define THE_FRONTEND_POLICY_PATH_MAX 4096
#define THE_FRONTEND_POLICY_ROOT_MAX 16

int the_frontend_policy_configure_from_environment(char *error,
                                                   size_t error_len);
void the_frontend_policy_disable(void);
int the_frontend_policy_enabled(void);
size_t the_frontend_policy_root_count(void);
const char *the_frontend_policy_root_at(size_t index, int *readonly);
int the_frontend_policy_resolve_path(const char *path, int allow_missing,
                                     char *out, size_t out_len,
                                     int *readonly, char *error,
                                     size_t error_len);
int the_frontend_policy_prepare_action(TheFrontendAction *action,
                                       char *error, size_t error_len);
int the_frontend_policy_command_allowed(const char *command);
int the_frontend_policy_current_file_writable(char *error,
                                              size_t error_len);
void the_frontend_policy_apply_current_file(void);

#endif
