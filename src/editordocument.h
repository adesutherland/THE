#ifndef THE_EDITORDOCUMENT_H
#define THE_EDITORDOCUMENT_H

#include "workbenchdocument.h"

/* These adapters read the editor only on its execution owner. */
uint64_t the_editor_document_active(void);
void the_editor_document_record_bind(FILE_DETAILS *file, LINETYPE line, LINE *record);
int the_editor_document_inspection_command(const char *command);
int the_editor_document_inspection_key(int key);
int the_editor_document_command_record_key(int key);

#endif
