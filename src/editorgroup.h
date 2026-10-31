#ifndef THE_EDITORGROUP_H
#define THE_EDITORGROUP_H
#include "editgroup.h"
void the_editor_edit_group_initialize(void);
short the_editor_edit_group_json(char *out,size_t capacity,uint64_t document_id);
/* Provisional lexical admission only; dispatch must resolve to Workbench. */
int the_editor_edit_group_command(const char *command);
#endif
