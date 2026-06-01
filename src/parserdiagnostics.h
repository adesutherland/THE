#ifndef THE_PARSERDIAGNOSTICS_H
#define THE_PARSERDIAGNOSTICS_H

#include "thedefs.h"

#define THE_PARSER_DIAGNOSTIC_CODE_MAX 64
#define THE_PARSER_DIAGNOSTIC_MESSAGE_MAX 512
#define THE_PARSER_DIAGNOSTIC_SEVERITY_MAX 24

typedef struct
{
   LINETYPE line;
   LENGTHTYPE column;
   char severity[THE_PARSER_DIAGNOSTIC_SEVERITY_MAX];
   char code[THE_PARSER_DIAGNOSTIC_CODE_MAX];
   char message[THE_PARSER_DIAGNOSTIC_MESSAGE_MAX];
} TheParserDiagnostic;

short the_parser_diagnostics_collect(TheParserDiagnostic **diagnostics,
                                     int *count);
void the_parser_diagnostics_free(TheParserDiagnostic *diagnostics);

#endif
