#ifndef THE_UTFINPUT_H
#define THE_UTFINPUT_H

#include "textpos.h"

#define UTFINPUT_ERROR_TOKEN_MAX 80

typedef enum
{
   UTFINPUT_PARSE_OK = 0,
   UTFINPUT_PARSE_MISSING_CODE,
   UTFINPUT_PARSE_MALFORMED_CODE,
   UTFINPUT_PARSE_RANGE,
   UTFINPUT_PARSE_SURROGATE,
   UTFINPUT_PARSE_CONTROL,
   UTFINPUT_PARSE_TOO_LONG
} UtfInputParseStatus;

typedef struct
{
   UtfInputParseStatus status;
   char token[UTFINPUT_ERROR_TOKEN_MAX];
} UtfInputParseError;

void utfinput_error_init(UtfInputParseError *error);
const char *utfinput_parse_status_name(UtfInputParseStatus status);
int utfinput_parse_command(const CHARTYPE *params,
                           CHARTYPE *out,
                           LENGTHTYPE out_capacity,
                           LENGTHTYPE *out_len,
                           UtfInputParseError *error);

#endif
