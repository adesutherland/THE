#include "utfinput.h"

#include <ctype.h>
#include <stdint.h>
#include <string.h>

static const CHARTYPE *skip_spaces(const CHARTYPE *p)
{
   while (p != NULL && *p != '\0' && isspace((unsigned char)*p))
      p++;
   return p;
}

static int ascii_equal_word_ci(const CHARTYPE *p, size_t len,
                               const char *word)
{
   size_t i;

   if (strlen(word) != len)
      return 0;
   for (i = 0; i < len; i++)
   {
      if (tolower((unsigned char)p[i])
       != tolower((unsigned char)word[i]))
         return 0;
   }
   return 1;
}

static void copy_error_token(UtfInputParseError *error, const CHARTYPE *start)
{
   size_t len = 0;

   if (error == NULL)
      return;
   error->token[0] = '\0';
   if (start == NULL)
      return;
   while (start[len] != '\0'
   &&     !isspace((unsigned char)start[len])
   &&     len + 1 < sizeof(error->token))
   {
      error->token[len] = (char)start[len];
      len++;
   }
   error->token[len] = '\0';
}

static int set_error(UtfInputParseError *error, UtfInputParseStatus status,
                     const CHARTYPE *token)
{
   if (error != NULL)
   {
      error->status = status;
      copy_error_token(error, token);
   }
   return 0;
}

void utfinput_error_init(UtfInputParseError *error)
{
   if (error == NULL)
      return;
   error->status = UTFINPUT_PARSE_OK;
   error->token[0] = '\0';
}

const char *utfinput_parse_status_name(UtfInputParseStatus status)
{
   switch (status)
   {
      case UTFINPUT_PARSE_OK:
         return "ok";
      case UTFINPUT_PARSE_MISSING_CODE:
         return "missing code";
      case UTFINPUT_PARSE_MALFORMED_CODE:
         return "malformed code";
      case UTFINPUT_PARSE_RANGE:
         return "code point out of range";
      case UTFINPUT_PARSE_SURROGATE:
         return "surrogate code point";
      case UTFINPUT_PARSE_CONTROL:
         return "control code point";
      case UTFINPUT_PARSE_TOO_LONG:
         return "decoded text too long";
      default:
         return "invalid code";
   }
}

static int has_u_plus_prefix(const CHARTYPE *p)
{
   return p != NULL
       && (p[0] == 'u' || p[0] == 'U')
       && p[1] == '+';
}

static int hex_value(CHARTYPE ch)
{
   if (ch >= '0' && ch <= '9')
      return ch - '0';
   if (ch >= 'a' && ch <= 'f')
      return ch - 'a' + 10;
   if (ch >= 'A' && ch <= 'F')
      return ch - 'A' + 10;
   return -1;
}

static int codepoint_allowed(uint32_t codepoint,
                             UtfInputParseError *error,
                             const CHARTYPE *token)
{
   if (codepoint > 0x10FFFFu)
      return set_error(error, UTFINPUT_PARSE_RANGE, token);
   if (codepoint >= 0xD800u && codepoint <= 0xDFFFu)
      return set_error(error, UTFINPUT_PARSE_SURROGATE, token);
   if (codepoint < 0x20u || codepoint == 0x7Fu)
      return set_error(error, UTFINPUT_PARSE_CONTROL, token);
   return 1;
}

static int append_codepoint(uint32_t codepoint,
                            CHARTYPE *out,
                            LENGTHTYPE out_capacity,
                            LENGTHTYPE *out_len,
                            UtfInputParseError *error,
                            const CHARTYPE *token)
{
   CHARTYPE encoded[4];
   LENGTHTYPE encoded_len;

   encoded_len = (LENGTHTYPE)text_utf8_encode(codepoint, encoded);
   if (encoded_len <= 0)
      return set_error(error, UTFINPUT_PARSE_MALFORMED_CODE, token);
   if (out == NULL || out_len == NULL || *out_len + encoded_len > out_capacity)
      return set_error(error, UTFINPUT_PARSE_TOO_LONG, token);
   memcpy(out + *out_len, encoded, (size_t)encoded_len);
   *out_len += encoded_len;
   return 1;
}

static int parse_codepoint_component(const CHARTYPE **cursor,
                                     int require_u_prefix,
                                     uint32_t *codepoint,
                                     UtfInputParseError *error,
                                     const CHARTYPE *token)
{
   const CHARTYPE *p = *cursor;
   uint32_t value = 0;
   int digits = 0;

   if (require_u_prefix || has_u_plus_prefix(p))
   {
      if (!has_u_plus_prefix(p))
         return set_error(error, UTFINPUT_PARSE_MALFORMED_CODE, token);
      p += 2;
   }
   while (*p != '\0' && !isspace((unsigned char)*p) && *p != '+')
   {
      int digit = hex_value(*p);

      if (digit < 0)
         return set_error(error, UTFINPUT_PARSE_MALFORMED_CODE, token);
      if (digits >= 6)
         return set_error(error, UTFINPUT_PARSE_RANGE, token);
      value = (value << 4) | (uint32_t)digit;
      digits++;
      p++;
   }
   if (digits == 0)
      return set_error(error, UTFINPUT_PARSE_MALFORMED_CODE, token);
   if (!codepoint_allowed(value, error, token))
      return 0;
   *cursor = p;
   *codepoint = value;
   return 1;
}

static int parse_code_token(const CHARTYPE **cursor,
                            CHARTYPE *out,
                            LENGTHTYPE out_capacity,
                            LENGTHTYPE *out_len,
                            UtfInputParseError *error)
{
   const CHARTYPE *token = *cursor;
   const CHARTYPE *p = token;
   int component_count = 0;

   if (!has_u_plus_prefix(p))
      return set_error(error, UTFINPUT_PARSE_MALFORMED_CODE, token);
   while (*p != '\0' && !isspace((unsigned char)*p))
   {
      uint32_t codepoint = 0;

      if (!parse_codepoint_component(&p, component_count == 0,
                                     &codepoint, error, token))
         return 0;
      if (!append_codepoint(codepoint, out, out_capacity, out_len,
                            error, token))
         return 0;
      component_count++;
      if (*p == '+')
      {
         p++;
         if (*p == '\0' || isspace((unsigned char)*p) || *p == '+')
            return set_error(error, UTFINPUT_PARSE_MALFORMED_CODE, token);
      }
      else if (*p != '\0' && !isspace((unsigned char)*p))
         return set_error(error, UTFINPUT_PARSE_MALFORMED_CODE, token);
   }
   if (component_count == 0)
      return set_error(error, UTFINPUT_PARSE_MISSING_CODE, token);
   *cursor = p;
   return 1;
}

int utfinput_parse_command(const CHARTYPE *params,
                           CHARTYPE *out,
                           LENGTHTYPE out_capacity,
                           LENGTHTYPE *out_len,
                           UtfInputParseError *error)
{
   const CHARTYPE *p;
   const CHARTYPE *word_start;
   size_t word_len = 0;

   utfinput_error_init(error);
   if (out_len != NULL)
      *out_len = 0;
   if (out_capacity < 0)
      out_capacity = 0;
   p = skip_spaces(params);
   if (p == NULL || *p == '\0')
      return set_error(error, UTFINPUT_PARSE_MISSING_CODE, p);

   word_start = p;
   while (p[word_len] != '\0' && !isspace((unsigned char)p[word_len]))
      word_len++;
   if (ascii_equal_word_ci(word_start, word_len, "codes"))
   {
      p = skip_spaces(word_start + word_len);
      if (p == NULL || *p == '\0')
         return set_error(error, UTFINPUT_PARSE_MISSING_CODE, p);
   }

   while (*p != '\0')
   {
      if (!parse_code_token(&p, out, out_capacity, out_len, error))
         return 0;
      p = skip_spaces(p);
   }
   if (out != NULL && out_len != NULL && *out_len <= out_capacity)
      out[*out_len] = '\0';
   return 1;
}
