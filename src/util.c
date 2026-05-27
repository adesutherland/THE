/***********************************************************************/
/* UTIL.C - Utility routines                                           */
/***********************************************************************/
/*
 * THE - The Hessling Editor. A text editor similar to VM/CMS xedit.
 * Copyright (C) 1991-2026 Mark Hessling
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 *
 *    The Free Software Foundation, Inc.
 *    675 Mass Ave,
 *    Cambridge, MA 02139 USA.
 *
 *
 * If you make modifications to this software that you feel increases
 * it usefulness for the rest of the community, please email the
 * changes, enhancements, bug fixes as well as any and all ideas to me.
 * This software is going to be maintained and enhanced as deemed
 * necessary by the community.
 *
 * Mark Hessling, mark@rexx.org  http://www.rexx.org/
 */


#include "the.h"
#include "proto.h"
#include "cursesdriver.h"

#ifdef my_stricmp
# undef my_stricmp
#endif
/*--------------------------- common data -----------------------------*/
 static CHARTYPE *rcvry[MAX_RECV];
 static LENGTHTYPE rcvry_len[MAX_RECV];
 static short add_rcvry=(-1);
 static short retr_rcvry=(-1);
 static short num_rcvry=0;

 static int CompareLen=0;
 static bool CompareExact;

#ifdef USE_EXTCURSES
 chtype color_pair[COLOR_PAIRS];
 static chtype fore_color[8];
 static chtype back_color[8];
#endif

/*
 * ASCII to EBCDIC
 */
static unsigned char _THE_FAR asc2ebc_table[256] = {
 0x00 ,0x01 ,0x02 ,0x03 ,0x37 ,0x2D ,0x2E ,0x2F,   /* 00 - 07 */
 0x16 ,0x05 ,0x25 ,0x0B ,0x0C ,0x0D ,0x0E ,0x0F,   /* 08 - 0f */
 0x10 ,0x11 ,0x12 ,0x13 ,0x3C ,0x3D ,0x32 ,0x26,   /* 10 - 17 */
 0x18 ,0x19 ,0x3F ,0x27 ,0x22 ,0x1D ,0x35 ,0x1F,   /* 18 - 1f */
 0x40 ,0x5A ,0x7F ,0x7B ,0x5B ,0x6C ,0x50 ,0x7D,   /* 20 - 27 */
 0x4D ,0x5D ,0x5C ,0x4E ,0x6B ,0x60 ,0x4B ,0x61,   /* 28 - 2f */
 0xF0 ,0xF1 ,0xF2 ,0xF3 ,0xF4 ,0xF5 ,0xF6 ,0xF7,   /* 30 - 37 */
 0xF8 ,0xF9 ,0x7A ,0x5E ,0x4C ,0x7E ,0x6E ,0x6F,   /* 38 - 3f */
 0x7C ,0xC1 ,0xC2 ,0xC3 ,0xC4 ,0xC5 ,0xC6 ,0xC7,   /* 40 - 47 */
 0xC8 ,0xC9 ,0xD1 ,0xD2 ,0xD3 ,0xD4 ,0xD5 ,0xD6,   /* 48 - 4f */
 0xD7 ,0xD8 ,0xD9 ,0xE2 ,0xE3 ,0xE4 ,0xE5 ,0xE6,   /* 50 - 57 */
 0xE7 ,0xE8 ,0xE9 ,0xAD ,0xE0 ,0xBD ,0x5F ,0x6D,   /* 58 - 5f */
 0x79 ,0x81 ,0x82 ,0x83 ,0x84 ,0x85 ,0x86 ,0x87,   /* 60 - 67 */
 0x88 ,0x89 ,0x91 ,0x92 ,0x93 ,0x94 ,0x95 ,0x96,   /* 68 - 6f */
 0x97 ,0x98 ,0x99 ,0xA2 ,0xA3 ,0xA4 ,0xA5 ,0xA6,   /* 70 - 77 */
 0xA7 ,0xA8 ,0xA9 ,0xC0 ,0x4F ,0xD0 ,0xA1 ,0x07,   /* 78 - 7f */
 0x43 ,0x20 ,0x21 ,0x1C ,0x23 ,0xEB ,0x24 ,0x9B,   /* 80 - 87 */
 0x71 ,0x28 ,0x38 ,0x49 ,0x90 ,0xBA ,0xEC ,0xDF,   /* 88 - 8f */
 0x45 ,0x29 ,0x2A ,0x9D ,0x72 ,0x2B ,0x8A ,0x9A,   /* 90 - 97 */
 0x67 ,0x56 ,0x64 ,0x4A ,0x53 ,0x68 ,0x59 ,0x46,   /* 98 - 9f */
 0xEA ,0xDA ,0x2C ,0xDE ,0x8B ,0x55 ,0x41 ,0xFE,   /* a0 - a7 */
 0x58 ,0x51 ,0x52 ,0x48 ,0x69 ,0xDB ,0x8E ,0x8D,   /* a8 - af */
 0x73 ,0x74 ,0x75 ,0xFA ,0x15 ,0xB0 ,0xB1 ,0xB3,   /* b0 - b7 */
 0xB4 ,0xB5 ,0x6A ,0xB7 ,0xB8 ,0xB9 ,0xCC ,0xBC,   /* b8 - bf */
 0xAB ,0x3E ,0x3B ,0x0A ,0xBF ,0x8F ,0x3A ,0x14,   /* c0 - c7 */
 0xA0 ,0x17 ,0xCB ,0xCA ,0x1A ,0x1B ,0x9C ,0x04,   /* c8 - cf */
 0x34 ,0xEF ,0x1E ,0x06 ,0x08 ,0x09 ,0x77 ,0x70,   /* d0 - d7 */
 0xBE ,0xBB ,0xAC ,0x54 ,0x63 ,0x65 ,0x66 ,0x62,   /* d8 - df */
 0x30 ,0x42 ,0x47 ,0x57 ,0xEE ,0x33 ,0xB6 ,0xE1,   /* e0 - e7 */
 0xCD ,0xED ,0x36 ,0x44 ,0xCE ,0xCF ,0x31 ,0xAA,   /* e8 - ef */
 0xFC ,0x9E ,0xAE ,0x8C ,0xDD ,0xDC ,0x39 ,0xFB,   /* f0 - f7 */
 0x80 ,0xAF ,0xFD ,0x78 ,0x76 ,0xB2 ,0x9F ,0xFF    /* f8 - ff */
};

/*
 * EBCDIC to ASCII
 */
static unsigned char _THE_FAR ebc2asc_table[256] = {
 0x00 ,0x01 ,0x02 ,0x03 ,0xCF ,0x09 ,0xD3 ,0x7F,   /* 00 - 07 */
 0xD4 ,0xD5 ,0xC3 ,0x0B ,0x0C ,0x0D ,0x0E ,0x0F,   /* 08 - 0f */
 0x10 ,0x11 ,0x12 ,0x13 ,0xC7 ,0xB4 ,0x08 ,0xC9,   /* 10 - 17 */
 0x18 ,0x19 ,0xCC ,0xCD ,0x83 ,0x1D ,0xD2 ,0x1F,   /* 18 - 1f */
 0x81 ,0x82 ,0x1C ,0x84 ,0x86 ,0x0A ,0x17 ,0x1B,   /* 20 - 27 */
 0x89 ,0x91 ,0x92 ,0x95 ,0xA2 ,0x05 ,0x06 ,0x07,   /* 28 - 2f */
 0xE0 ,0xEE ,0x16 ,0xE5 ,0xD0 ,0x1E ,0xEA ,0x04,   /* 30 - 37 */
 0x8A ,0xF6 ,0xC6 ,0xC2 ,0x14 ,0x15 ,0xC1 ,0x1A,   /* 38 - 3f */
 0x20 ,0xA6 ,0xE1 ,0x80 ,0xEB ,0x90 ,0x9F ,0xE2,   /* 40 - 47 */
 0xAB ,0x8B ,0x9B ,0x2E ,0x3C ,0x28 ,0x2B ,0x7C,   /* 48 - 4f */
 0x26 ,0xA9 ,0xAA ,0x9C ,0xDB ,0xA5 ,0x99 ,0xE3,   /* 50 - 57 */
 0xA8 ,0x9E ,0x21 ,0x24 ,0x2A ,0x29 ,0x3B ,0x5E,   /* 58 - 5f */
 0x2D ,0x2F ,0xDF ,0xDC ,0x9A ,0xDD ,0xDE ,0x98,   /* 60 - 67 */
 0x9D ,0xAC ,0xBA ,0x2C ,0x25 ,0x5F ,0x3E ,0x3F,   /* 68 - 6f */
 0xD7 ,0x88 ,0x94 ,0xB0 ,0xB1 ,0xB2 ,0xFC ,0xD6,   /* 70 - 77 */
 0xFB ,0x60 ,0x3A ,0x23 ,0x40 ,0x27 ,0x3D ,0x22,   /* 78 - 7f */
 0xF8 ,0x61 ,0x62 ,0x63 ,0x64 ,0x65 ,0x66 ,0x67,   /* 80 - 87 */
 0x68 ,0x69 ,0x96 ,0xA4 ,0xF3 ,0xAF ,0xAE ,0xC5,   /* 88 - 8f */
 0x8C ,0x6A ,0x6B ,0x6C ,0x6D ,0x6E ,0x6F ,0x70,   /* 90 - 97 */
 0x71 ,0x72 ,0x97 ,0x87 ,0xCE ,0x93 ,0xF1 ,0xFE,   /* 98 - 9f */
 0xC8 ,0x7E ,0x73 ,0x74 ,0x75 ,0x76 ,0x77 ,0x78,   /* a0 - a7 */
 0x79 ,0x7A ,0xEF ,0xC0 ,0xDA ,0x5B ,0xF2 ,0xF9,   /* a8 - af */
 0xB5 ,0xB6 ,0xFD ,0xB7 ,0xB8 ,0xB9 ,0xE6 ,0xBB,   /* b0 - b7 */
 0xBC ,0xBD ,0x8D ,0xD9 ,0xBF ,0x5D ,0xD8 ,0xC4,   /* b8 - bf */
 0x7B ,0x41 ,0x42 ,0x43 ,0x44 ,0x45 ,0x46 ,0x47,   /* c0 - c7 */
 0x48 ,0x49 ,0xCB ,0xCA ,0xBE ,0xE8 ,0xEC ,0xED,   /* c8 - cf */
 0x7D ,0x4A ,0x4B ,0x4C ,0x4D ,0x4E ,0x4F ,0x50,   /* d0 - d7 */
 0x51 ,0x52 ,0xA1 ,0xAD ,0xF5 ,0xF4 ,0xA3 ,0x8F,   /* d8 - df */
 0x5C ,0xE7 ,0x53 ,0x54 ,0x55 ,0x56 ,0x57 ,0x58,   /* e0 - e7 */
 0x59 ,0x5A ,0xA0 ,0x85 ,0x8E ,0xE9 ,0xE4 ,0xD1,   /* e8 - ef */
 0x30 ,0x31 ,0x32 ,0x33 ,0x34 ,0x35 ,0x36 ,0x37,   /* f0 - f7 */
 0x38 ,0x39 ,0xB3 ,0xF7 ,0xF0 ,0xFA ,0xA7 ,0xFF
};

/***********************************************************************/
CHARTYPE *asc2ebc(CHARTYPE *str,int len,int start,int end)
/***********************************************************************/
/* Function  : Converts an ASCII string to an EBCDIC string.           */
/* Parameters: str      - ASCII string                                 */
/*             len      - length of string to convert                  */
/* Return    : *str     - the same string converted                    */
/***********************************************************************/
{
   register int i = 0;

   for (i=0; i<len; i++)
      str[i] = asc2ebc_table[str[i]];
   return(str);
}

/***********************************************************************/
CHARTYPE *ebc2asc(CHARTYPE *str,int len,int start,int end)
/***********************************************************************/
/* Function  : Converts an EBCDIC string to an ASCII string.           */
/* Parameters: str      - EBCDIC string                                */
/*             len      - length of string to convert                  */
/* Return    : *str     - the same string converted                    */
/***********************************************************************/
{
   register int i = 0;

   for (i=start; i<min(len,end+1); i++)
      str[i] = ebc2asc_table[str[i]];
   return(str);
}

/*man***************************************************************************
NAME
     memreveq - search buffer reversed for character

SYNOPSIS
     LENGTHTYPE memreveq(buffer,chr,max_length)
     CHARTYPE *buffer;
     CHARTYPE ch;
     LENGTHTYPE max_length;

DESCRIPTION
     The memreveq function searches the buffer from the right for the
     first character equal to the supplied character.

RETURN VALUE
     If successful, returns the position of first matching character
     or (-1) if unsuccessful.

SEE ALSO
     strzreveq, memrevne
*******************************************************************************/
LENGTHTYPE memreveq( CHARTYPE *buffer, CHARTYPE ch, LENGTHTYPE max_len)
/***********************************************************************/
{
   LENGTHTYPE len=max_len;

   for (--len; len>=0 && buffer[len]!=ch; len--);
   return(len);
}
/*man***************************************************************************
NAME
     memrevne - search buffer reversed for NOT character

SYNOPSIS
     LENGTHTYPE memrevne(buffer,known_char,max_len)
     CHARTYPE *buffer;
     CHARTYPE known_char;
     LENGTHTYPE max_len;

DESCRIPTION
     The memrevne function searches the buffer from the right for first
     character NOT equal to the supplied character.

RETURN VALUE
     If successful, returns the position of first NON-matching character
     or (-1) if unsuccessful.

SEE ALSO
     strzrevne, strzne
*******************************************************************************/
LENGTHTYPE memrevne( CHARTYPE *buffer, CHARTYPE known_char, LENGTHTYPE max_len )
{
   LENGTHTYPE len=max_len;

   for (--len; len>=0 && buffer[len]==known_char; len--);
   return(len);
}
/*man***************************************************************************
NAME
     meminschr - insert character into buffer

SYNOPSIS
     CHARTYPE *meminschr(buffer,chr,location,max_length,curr_length)
     CHARTYPE *buffer;
     CHARTYPE chr;
     LENGTHTYPE location,max_length,curr_length;

DESCRIPTION
     The meminschr inserts the supplied 'chr' into the buffer 'buffer'
     before the 'location' specified. 'location' is an offset (0 based)
     from the start of 'buffer'.
     The 'buffer' will not be allowed to have more than 'max_length'
     characters, so if the insertion of the character causes the
     'max_length' to be exceeded, the last character of 'buffer' will
     be lost.

RETURN VALUE
     A pointer to the same 'buffer' as was supplied.

SEE ALSO
    meminsstr, memdeln

*******************************************************************************/
CHARTYPE *meminschr( CHARTYPE *buffer, CHARTYPE chr, LENGTHTYPE location, LENGTHTYPE max_length, LENGTHTYPE curr_length )
{
   LENGTHTYPE i=0;

   for (i=curr_length;i > location;i--)
   {
      if (i < max_length)
         buffer[i] = buffer[i-1];
   }
   if (location < max_length)
      buffer[location] = chr;
   return(buffer);
}
/*man***************************************************************************
NAME
     meminsmem - insert memory into buffer

SYNOPSIS
     #include "the.h"

     CHARTYPE *meminsmem(buffer,str,len,location,max_length,curr_length)
     CHARTYPE *buffer;
     CHARTYPE *str;
     LENGTHTYPE len,location,max_length,curr_length;

DESCRIPTION
     The meminsmem function inserts the supplied 'str' into the buffer 'buffer'
     before the 'location' specified. 'location' is an offset (0 based)
     from the start of 'buffer'.
     The 'buffer' will not be allowed to have more than 'max_length'
     characters, so if the insertion of the string causes the
     'max_length' to be exceeded, the last character(s) of 'buffer' will
     be lost.

RETURN VALUE
     A pointer to the same 'buffer' as was supplied.

SEE ALSO
    meminschr

*******************************************************************************/
CHARTYPE *meminsmem( CHARTYPE *buffer, CHARTYPE *str, LENGTHTYPE len, LENGTHTYPE location, LENGTHTYPE max_length, LENGTHTYPE curr_length )
{
   LENGTHTYPE i=0;

   for (i=curr_length;i > location;i--)
   {
      if (i+len-1 < max_length)
         buffer[i+len-1] = buffer[i-1];
   }
   for (i=0;i<len;i++)
   {
      if (location+i < max_length)
         buffer[location+i] = str[i];
   }
   return(buffer);
}
/*man***************************************************************************
NAME
     memdeln - delete a number of character(s) from buffer

SYNOPSIS
     CHARTYPE *memdeln(buffer,location,curr_length,num_chars)
     CHARTYPE *buffer;
     LENGTHTYPE location,curr_length,num_chars;

DESCRIPTION
     The memdeln deletes the supplied number of characters from the
     buffer starting at the 'location' specified. 'location' is an offset (0 based)
     from the start of 'buffer'.
     For each character deleted, what was the last character in buffer;
     based on 'curr_length' will be replaced with a space.

RETURN VALUE
     A pointer to the same 'buffer' as was supplied.

SEE ALSO
    meminschr, strdelchr

*******************************************************************************/
CHARTYPE *memdeln( CHARTYPE *buffer, LENGTHTYPE location, LENGTHTYPE curr_length, LENGTHTYPE num_chars )
{
   LENGTHTYPE i=0;

   for (i=location;i <curr_length;i++)
   {
      if (i+num_chars >= curr_length)
         buffer[i] = ' ';
      else
         buffer[i] = buffer[i+num_chars];
   }
   return(buffer);
}
/*man***************************************************************************
NAME
     strdelchr - delete all supplied character from buffer

SYNOPSIS
     CHARTYPE *memdeln(buffer,chr)
     CHARTYPE *buffer;
     CHARTYPE chr;

DESCRIPTION
     The memdeln deletes all occurrences of chr from the ASCIIZ buffer.

RETURN VALUE
     A pointer to the same 'buffer' as was supplied.

SEE ALSO
    meminschr, memdeln

*******************************************************************************/
CHARTYPE *strdelchr(CHARTYPE *buffer,CHARTYPE chr)
{
   LENGTHTYPE i=0,j=0;
   LENGTHTYPE len=strlen( (DEFCHAR *)buffer );

   for( i = 0; i < len; i++ )
   {
      if (buffer[i] != chr)
         buffer[j++] = buffer[i];
   }
   buffer[j] = (CHARTYPE)'\0';
   return(buffer);
}
/*man***************************************************************************
NAME
     memrmdup - remove duplicate, contiguous characters

SYNOPSIS
     CHARTYPE *memrmdup(buf,len,chr)
     CHARTYPE *buf;
     LENGTHTYPE *len;
     CHARTYPE ch;

DESCRIPTION
     The memrmdup function removes all duplicate, contiguous characters
     from the supplied buffer.
     e.g. memrmdup("abc$$$def$$ghi$",15,'$')
     will return pointer to buf equal to "abc$def$ghi$" and new length
     in len.

RETURN VALUE
     Returns the new buf.
*******************************************************************************/
CHARTYPE *memrmdup( CHARTYPE *buf, LENGTHTYPE *len, CHARTYPE ch )
{
   LENGTHTYPE i=0,num_dups=0,newlen=*len;
   CHARTYPE *src=buf,*dst=buf;
   bool dup=FALSE;

   for (; i<newlen; i++,src++)
   {
      if (*src == ch)
      {
         if (dup)
         {
            num_dups++;
            continue;
         }
         else
         {
            dup = TRUE;
         }
      }
      else
         dup = FALSE;
      *dst++ = *src;
   }
   *len = newlen-num_dups;
   return(buf);
}
/*man***************************************************************************
NAME
     strrmdup - remove duplicate, contiguous characters

SYNOPSIS
     CHARTYPE *strrmdup(buf,chr)
     CHARTYPE *buf;
     CHARTYPE ch;
     bool exclude_leading;

DESCRIPTION
     The strrmdup function removes all duplicate, contiguous characters
     from the supplied string.  if exclude_leading is TRUE, no removal
     is done of leading characters.
     e.g. strrmdup("abc$$$def$$ghi$",'$')
     will return pointer to buf equal to "abc$def$ghi$".

RETURN VALUE
     Returns the new buf.
*******************************************************************************/
CHARTYPE *strrmdup(CHARTYPE *buf,CHARTYPE ch,bool exclude_leading)
{
   CHARTYPE *src=buf,*dst=buf;
   bool dup=FALSE;

   if (exclude_leading)
   {
      while(*src == ch)
         *dst++ = *src++;
   }
   while(*src)
   {
      if (*src == ch)
      {
         if (dup)
         {
            src++;
            continue;
         }
         else
            dup = TRUE;
      }
      else
         dup = FALSE;
      *dst++ = *src++;
   }
   *dst = '\0';
   return(buf);
}
/*man***************************************************************************
NAME
     strzne - search string for NOT character

SYNOPSIS
     LENGTHTYPE strzne(str,chr)
     CHARTYPE *str;
     CHARTYPE ch;

DESCRIPTION
     The strzne function searches the string from the left for the first
     character NOT equal to the supplied character.

RETURN VALUE
     If successful, returns the position of first NON-matching character
     or (-1) if unsuccessful.

SEE ALSO
     strzrevne, memrevne
*******************************************************************************/
LENGTHTYPE strzne(CHARTYPE *str,CHARTYPE ch)
{
   LENGTHTYPE len=0;
   LENGTHTYPE i = 0;

   len = strlen( (DEFCHAR *)str );
   for (; i<len && str[i]==ch; i++);
   if (i>=len)
      i = (-1);
   return(i);
}
/*man***************************************************************************
NAME
     my_strdup - equivalent to strdup

SYNOPSIS
     CHARTYPE *my_strdup(str)
     CHARTYPE *str;

DESCRIPTION
     The my_strdup function duplicates the supplied string.

RETURN VALUE
     If successful, returns a pointer to the copy of the supplied string
     or NULL if unsuccessful.
*******************************************************************************/
CHARTYPE *my_strdup(CHARTYPE *str)
{
   LENGTHTYPE len=0;
   CHARTYPE *tmp=NULL;

   len = strlen( (DEFCHAR *)str );
   if ((tmp = (CHARTYPE *)(*the_malloc)((len+1)*sizeof(CHARTYPE))) == (CHARTYPE *)NULL)
      return((CHARTYPE *)NULL);
   strcpy( (DEFCHAR *)tmp, (DEFCHAR *)str );
   return(tmp);
}
/*man***************************************************************************
NAME
     memne - search buffer for NOT character

SYNOPSIS
     #include "the.h"

     LENGTHTYPE memne(buffer,chr,length)
     CHARTYPE *buffer;
     CHARTYPE chr;
     LENGTHTYPE length;

DESCRIPTION
     The memne function searches the buffer from the left for the first
     character NOT equal to the supplied character.

RETURN VALUE
     If successful, returns the position of first NON-matching character
     or (-1) if unsuccessful.

SEE ALSO
     strzrevne, memrevne, strzne
*******************************************************************************/
LENGTHTYPE memne( CHARTYPE *buffer, CHARTYPE chr, LENGTHTYPE length )
{
   LENGTHTYPE i = 0;

   for (; i<length && buffer[i]==chr; i++);
   if (i>=length)
      i = (-1);
   return(i);
}
/*man***************************************************************************
NAME
     strzrevne - search string reversed for NOT character

SYNOPSIS
     #include "the.h"

     LENGTHTYPE strzrevne(str,chr)
     CHARTYPE *str;
     CHARTYPE ch;

DESCRIPTION
     The strzrevne function searches the string from the right for the
     first character NOT equal to the supplied character.

RETURN VALUE
     If successful, returns the position of first NON-matching character
     or (-1) if unsuccessful.

SEE ALSO
     strzne, memrevne
*******************************************************************************/
LENGTHTYPE strzrevne( CHARTYPE *str, CHARTYPE ch )
{
   LENGTHTYPE len=0;

   len = strlen( (DEFCHAR *)str );
   for (--len; len>=0 && str[len]==ch; len--);
   return(len);
}
/*man***************************************************************************
NAME
     strzreveq - search string reversed for character

SYNOPSIS
     LENGTHTYPE strzreveq(str,chr)
     CHARTYPE *str;
     CHARTYPE ch;

DESCRIPTION
     The strzreveq function searches the string from the right for the
     first character equal to the supplied character.

RETURN VALUE
     If successful, returns the position of first matching character
     or (-1) if unsuccessful.

SEE ALSO
     strzrevne
*******************************************************************************/
LENGTHTYPE strzreveq(CHARTYPE *str,CHARTYPE ch)
/***********************************************************************/
{
   LENGTHTYPE len=0;

   len = strlen( (DEFCHAR *)str );
   for (--len; len>=0 && str[len]!=ch; len--);
   return(len);
}
/*man***************************************************************************
NAME
     strtrunc - truncate leading and trailing spaces from string

SYNOPSIS
     #include "the.h"

     CHARTYPE *strtrunc(string)
     CHARTYPE *string;

DESCRIPTION
     The strtrunc function truncates all leading and trailing spaces
     from the supplied string.

RETURN VALUE
     A pointer to the original string, now truncated.

SEE ALSO

*******************************************************************************/
CHARTYPE *strtrunc(CHARTYPE *string)
{
   return( MyStrip( string, STRIP_BOTH, ' ' ) );
}
/*man***************************************************************************
NAME
     MyStrip - truncate leading and/or trailing spaces from string

SYNOPSIS
     #include "the.h"

     CHARTYPE *MyStrip(string,option,ch)
     CHARTYPE *string;
     char option;
     char ch;

DESCRIPTION
     The MyStrip function truncates all leading and/or trailing ch
     from the supplied string.
     The value of the "option" argument can be one of:
        STRIP_LEADING
        STRIP_TRAILING
        STRIP_BOTH
        STRIP_ALL
        STRIP_NONE
     These are defined elsewhere with the values:
        STRIP_LEADING    1
        STRIP_TRAILING   2
        STRIP_INTERNAL   4
        STRIP_BOTH       STRIP_LEADING|STRIP_TRAILING
        STRIP_ALL        STRIP_LEADING|STRIP_TRAILING|STRIP_INTERNAL
        STRIP_NONE       0

RETURN VALUE
     A pointer to the original string, now truncated.

SEE ALSO

*******************************************************************************/
CHARTYPE *MyStrip(CHARTYPE *string,char option,char ch)
{
   LENGTHTYPE i=0;
   LENGTHTYPE pos=0;

   if ( strlen( (DEFCHAR *)string ) == 0 )
      return(string);
   if (option & STRIP_TRAILING)
   {
      pos = strzrevne( string, ch );
      if (pos == (-1))
         *(string) = '\0';
      else
         *(string+pos+1) = '\0';
   }
   if (option & STRIP_LEADING)
   {
      pos = strzne( string, ch );
      if (pos == (-1))
         *(string) = '\0';
      else
      {
/*       for (i=0;*(string+i)!='\0';i++) */
         for (i=0;*(string+i+pos)!='\0';i++) /* fixed by FGC */
            *(string+i) = *(string+i+pos);
         *(string+i) = '\0';
      }
   }
   if (option == STRIP_ALL)
      string = strdelchr(string,' ');
   return(string);
}
/*man***************************************************************************
NAME
     memfind - finds a needle in a haystack respecting case and arbitrary
               characters if set.

SYNOPSIS
     LENGTHTYPE memfind(haystack,needle,hay_len,nee_len,case_ignore,arbsts,arb)
     CHARTYPE *haystack;                            string to be searched
     CHARTYPE *needle;        string to search for - may contain arbchars
     LENGTHTYPE hay_len;                               length of haystack
     LENGTHTYPE nee_len;                                 length of needle
     bool case_ignore;                      TRUE if search to ignore case
     bool arbsts;          TRUE if need to check for arbitrary characters
     CHARTYPE single                       the single arbitrary character
     CHARTYPE multiple                   the multiple arbitrary character
     LENGTHTYPE *target_len       return the length of the matched string

DESCRIPTION
     The memfind function locates a needle in a haystack. Both the needle
     and haystack may contain null characters. If case_ignore is TRUE,
     then upper and lower case characters are treated equal. If arbsts
     is ON, any arbitrary character, specified by arb, in needle, will
     match ANY character in the haystack.

RETURN VALUE
     The first occurrence (0 based) of needle in haystack, or (-1) if
     the needle does not appear in the haystack. The length of the matched
     string is returned in target_len
*******************************************************************************/
LENGTHTYPE memfind( CHARTYPE *haystack, CHARTYPE *needle, LENGTHTYPE hay_len, LENGTHTYPE nee_len,
                    bool case_ignore, bool arbsts, CHARTYPE arb_single, CHARTYPE arb_multiple,
                    LENGTHTYPE *target_len )
{
   register CHARTYPE c1=0,c2=0;
   register CHARTYPE *buf1=NULL,*buf2=NULL;
   LENGTHTYPE i=0,j=0;
   LENGTHTYPE matches=0;
   CHARTYPE *new_needle=needle;
   bool need_free=FALSE;

   /*
    * Strip any duplicate, contiguous occurrences of arb_multiple if
    * we are handling arbchars.
    */
   if ( arbsts
   &&   strzeq( needle, arb_multiple ) != (-1) )
   {
      if ( ( new_needle = (CHARTYPE *)my_strdup( needle ) ) == NULL )
      {
         display_error( 30, (CHARTYPE *)"", FALSE );
         return(-1);
      }
      need_free = TRUE;
      memrmdup( new_needle, &nee_len, arb_multiple );
   }

   for ( i = 0; i < (hay_len-nee_len+1); i++ )
   {
      buf1 = haystack+i;
      buf2 = new_needle;
      matches=0;
      for ( j = 0; j < nee_len; j++ )
      {
         if ( case_ignore )
         {
            if ( isupper( *buf1 ) )
               c1 = tolower( *buf1 );
            else
               c1 = *buf1;
            if ( isupper( *buf2 ) )
               c2 = tolower( *buf2 );
            else
               c2 = *buf2;
         }
         else
         {
            c1 = *buf1;
            c2 = *buf2;
         }
         if ( arbsts )
         {
/* Next lines added by R.BOSSUT */
            if ( c2 == arb_multiple )
            {
               /*
                *
                */
               LENGTHTYPE new_hay_len = hay_len-(buf1-haystack);
               LENGTHTYPE new_nee_len = nee_len-(buf2+1-new_needle);
               LENGTHTYPE new_tar=0;
               LENGTHTYPE new_start;

               *target_len = *target_len + matches;

               new_start = memfind( buf1, buf2+1, new_hay_len, new_nee_len,
                                    case_ignore, arbsts, arb_single, arb_multiple, &new_tar );
               if ( new_start != (-1) )
               {
                  if ( need_free )
                     (*the_free)( new_needle );
                  *target_len = *target_len + new_tar + new_start;
                  if ( new_needle[nee_len-1] == arb_multiple
                  && new_start == 0 )
                  {
                    *target_len = hay_len - *target_len - 1;
                  }
                  return(i);
               }
            }
            else
            {
/* Up to here... */
               if ( c1 != c2 && c2 != arb_single )
                  break;
               else
                  matches++;
            }
/* Next lines added by R.BOSSUT */
         }
/* Up to here... */
         else
         {
            if ( c1 != c2 )
               break;
            else
               matches++;
         }
         ++buf1;
         ++buf2;
      }
      if ( matches == nee_len )
      {
         if ( need_free )
            (*the_free)( new_needle );
         *target_len = *target_len + matches;
         return(i);
      }
   }
   if ( need_free )
      (*the_free)( new_needle );
   return(-1);
}
/*man***************************************************************************
NAME
     memrev - reverses a buffer's contents

SYNOPSIS
     void memrev(dest,source,len)
     CHARTYPE *dest;
     CHARTYPE *source;
     LENGTHTYPE length;

DESCRIPTION
     The memrev function reverses the contents of the source buffer
     and places the result in dest.

RETURN VALUE
     None
*******************************************************************************/
void memrev( CHARTYPE *dest, CHARTYPE *src, LENGTHTYPE length )
{
   LENGTHTYPE i,j;

   for ( i = 0, j = length-1; i < length; i++, j-- )
   {
      dest[j] = src[i];
   }
}
/***********************************************************************/
LENGTHTYPE memcmpi( CHARTYPE *buf1, CHARTYPE *buf2, LENGTHTYPE len )
/***********************************************************************/
/* Function  : Compares two memory buffers for equality;               */
/*             case insensitive. Same as memicmp() Microsoft C.        */
/* Parameters: buf1     - first buffer                                 */
/*             buf2     - second buffer                                */
/*             len      - number of characters to compare.             */
/* Return    : <0 if buf1 < buf2,                                      */
/*             =0 if buf1 = buf2,                                      */
/*             >0 if buf1 > buf2,                                      */
/***********************************************************************/
{
   LENGTHTYPE i=0;
   CHARTYPE c1,c2;

   for( i = 0; i < len; i++ )
   {
      if ( isupper( *buf1 ) )
         c1 = tolower( *buf1 );
    else
         c1 = *buf1;
      if ( isupper( *buf2 ) )
         c2 = tolower( *buf2 );
      else
         c2 = *buf2;
      if ( c1 != c2 )
         return( c1 - c2 );
      ++buf1;
      ++buf2;
   }
   return(0);
}
/***********************************************************************/
LENGTHTYPE my_stricmp( DEFCHAR *str1, DEFCHAR *str2 )
/***********************************************************************/
/* Function  : Compares two string buffers for equality;               */
/*             case insensitive. Same as stricmp(), strcasecmp() etc.  */
/* Parameters: str1     - first string                                 */
/*             str2     - second string                                */
/* Return    : <0 if str1 < str2,                                      */
/*             =0 if str1 = str2,                                      */
/*             >0 if str1 > str2,                                      */
/***********************************************************************/
{
   LENGTHTYPE len1=strlen((DEFCHAR *)str1),len2=strlen((DEFCHAR *)str2);
   LENGTHTYPE i,len;
   DEFCHAR c1,c2;

   len = min( len1, len2 );
   for( i = 0; i < len; i++ )
   {
      if ( isupper( *str1 ) )
         c1 = tolower( *str1 );
      else
         c1 = *str1;
      if ( isupper( *str2 ) )
         c2 = tolower( *str2 );
      else
         c2 = *str2;
      if ( c1 != c2 )
         return( c1 - c2 );
      ++str1;
      ++str2;
   }
   if ( len1 > len2 )
      return(1);
   if ( len1 < len2 )
      return(-1);
   return(0);
}
/***********************************************************************/
CHARTYPE *make_upper(CHARTYPE *str)
/***********************************************************************/
/* Function  : Makes the supplied string uppercase.                    */
/*             Equivalent to strupr() on some platforms.               */
/* Parameters: str      - string to uppercase                          */
/* Return    : str uppercased                                          */
/***********************************************************************/
{
   CHARTYPE *save_str=str;

   while(*str)
   {
      if ( islower( *str ) )
         *str = toupper( *str );
      ++str;
   }
   return(save_str);
}
/*man***************************************************************************
NAME
     equal - determine if strings are equal up to specified length

SYNOPSIS
     bool equal(con,str,min_len)
     CHARTYPE *con,*str;
     LENGTHTYPE min_len;

DESCRIPTION
     The equal function determines if a two strings are equal, irrespective
     of case, up to the length of the second string. The length of the
     second string must be greater than or equal to the specified minimum
     length for the strings to be considered equal.

RETURN VALUE
     If 'equal' TRUE else FALSE.
*******************************************************************************/
bool equal( CHARTYPE *con, CHARTYPE *str, LENGTHTYPE min_len )
{
   LENGTHTYPE i=0,lenstr=0;
   CHARTYPE c1,c2;

   if (min_len == 0)
   {
      return(FALSE);
   }
   if (strlen((DEFCHAR *)str) < min_len
   ||  strlen((DEFCHAR *)con) < strlen((DEFCHAR *)str))
   {
      return(FALSE);
   }
   lenstr = strlen( (DEFCHAR*)str );
   for ( i = 0; i < lenstr; i++ )
   {
      if ( isupper( *con ) )
         c1 = tolower( *con );
      else
         c1 = *con;
      if ( isupper( *str ) )
         c2 = tolower( *str );
      else
         c2 = *str;
      if ( c1 != c2 )
      {
         return(FALSE);
      }
      ++con;
      ++str;
   }
   return(TRUE);

}
/***********************************************************************/
bool valid_integer( CHARTYPE *str )
/***********************************************************************/
/* Function  : Checks that string contains only 0-9,- or +.            */
/* Parameters: *str     - string to be checked                         */
/* Return    : TRUE or FALSE                                           */
/***********************************************************************/
{
   LENGTHTYPE i=0;
   LENGTHTYPE num_signs=0;

   TRACE_FUNCTION("util.c:    valid_integer");
   for ( i = 0; i < strlen( (DEFCHAR *)str ); i++ )
   {
      if ( *(str+i) == '-' || *(str+i) == '+' )
         num_signs++;
      else
      {
         if ( !isdigit( *(str+i) ) )
         {
            TRACE_RETURN();
            return(FALSE);
         }
      }
   }
   if ( num_signs > 1 )
   {
      TRACE_RETURN();
      return(FALSE);
   }
   TRACE_RETURN();
   return(TRUE);
}
/***********************************************************************/
bool valid_positive_integer( CHARTYPE *str )
/***********************************************************************/
/* Function  : Checks that string contains only 0-9, or +.             */
/* Parameters: *str     - string to be checked                         */
/* Return    : TRUE or FALSE                                           */
/***********************************************************************/
{
   LENGTHTYPE i=0;

   TRACE_FUNCTION("util.c:    valid_positive_integer");
   if ( *str == '+' )
      str++;
   for ( i = 0; i < strlen( (DEFCHAR *)str ); i++ )
   {
      if ( !isdigit( *(str+i) ) )
      {
         TRACE_RETURN();
         return(FALSE);
      }
   }
   TRACE_RETURN();
   return(TRUE);
}
/***********************************************************************/
short valid_positive_integer_against_maximum( CHARTYPE *str, LENGTHTYPE maximum )
/***********************************************************************/
/* Function  : Checks that string contains only 0-9, or +              */
/*             and is less than supplied string maximum                */
/* Parameters: *str     - string to be checked                         */
/* Return    : TRUE or FALSE                                           */
/***********************************************************************/
{
   LENGTHTYPE i,len_str,len_max;
   CHARTYPE _THE_FAR buffer[50];
   CHARTYPE *buf;
   short rc=0;

   TRACE_FUNCTION("util.c:    valid_positive_integer_against_maximum");
   if ( !valid_positive_integer( str ) )
   {
      TRACE_RETURN();
      return(4); /* invlaid number */
   }
   /*
    * Now check against the maximum
    */
   buf = buffer;
   sprintf( (DEFCHAR *)buf, "%ld", maximum );
   if ( *str == '+' )
      str++;
   len_max = strlen( (DEFCHAR *)buf );
   len_str = strlen( (DEFCHAR *)str );
   if ( len_str > len_max )
   {
      TRACE_RETURN();
      return(6);
   }
   if ( len_str < len_max )
   {
      TRACE_RETURN();
      return(0);
   }
   for ( i = 0; i < len_str; i++, str++, buf++ )
   {
      if ( *str > *buf )
      {
         rc = 6;
         break;
      }
      if ( *str < *buf )
      {
         rc = 0;
         break;
      }
   }
   TRACE_RETURN();
   return(rc);
}
/***********************************************************************/
LENGTHTYPE strzeq( CHARTYPE *str, CHARTYPE ch )
/***********************************************************************/
/* Function  : Locate in ASCIIZ string, character                      */
/* Parameters: *str     - string to be searched                        */
/*             ch       - character to be searched for                 */
/* Return    : position in string of character - (-1) if not found     */
/***********************************************************************/
{
   LENGTHTYPE len=0;
   LENGTHTYPE i = 0;

   len = strlen( (DEFCHAR *)str );
   for ( ; i < len && str[i] != ch; i++ );
   if ( i >= len )
      i = (-1L);
   return(i);
}
/***********************************************************************/
CHARTYPE *strtrans( CHARTYPE *str, CHARTYPE oldch, CHARTYPE newch )
/***********************************************************************/
/* Function  : Translate all occurrences of oldch to newch in str      */
/* Parameters: *str     - string to be amendedd                        */
/*             oldch    - character to be replaced                     */
/*             newch    - character to replace oldch                   */
/* Return    : same string but with characters translated              */
/***********************************************************************/
{
   LENGTHTYPE i=0;

   for ( i = 0; i < strlen( (DEFCHAR *)str ); i++ )
   {
      if ( *(str+i) == oldch )
         *(str+i) = newch;
   }
   return(str);
   }

   #ifdef USE_SDSLH
   void sdslh_update_current_line(unsigned short y) {
       if (CURRENT_FILE && CURRENT_FILE->sdslh_comm && CURRENT_FILE->cb && CURRENT_VIEW->current_window == WINDOW_FILEAREA) {
           LINETYPE line_number = CURRENT_SCREEN.sl[y].line_number;
           if (line_number > 0 && line_number <= (LINETYPE)CURRENT_FILE->cb->line_count) {
               rec[rec_len] = '\0';
               cb_sync_line(CURRENT_FILE->cb, line_number - 1, (const char *)rec);
           }
       }
   }

   void ui_log(const char *fmt, ...) {
       FILE *f = fopen("ui_debug.log", "a");
       if (!f) return;
       va_list args;
       va_start(args, fmt);
       vfprintf(f, fmt, args);
       va_end(args);
       fprintf(f, "\n");
       fclose(f);
   }
   #endif   /***********************************************************************/
   LINE *add_LINE( LINE *first, LINE *curr, CHARTYPE *line, LENGTHTYPE len, SELECTTYPE select, bool new_flag )/***********************************************************************/
/* Adds a member of the linked list for the specified file containing  */
/* the line contents and length.                                       */
/* PARAMETERS:                                                         */
/* first      - pointer to first line for the file                     */
/* curr       - pointer to current line for the file                   */
/* line       - contents of line to be added                           */
/* len        - length of line to be added                             */
/* select     - select level of new line                               */
/* RETURN:    - pointer to current item in linked list or NULL if error*/
/***********************************************************************/
{
   TRACE_FUNCTION("util.c:    add_LINE");
   /*
    * Validate that the line being added is shorter than the maximum line length
    */
   if ( len > max_line_length )
   {
      display_error( 0, (CHARTYPE*)"Truncated", FALSE );
      len = max_line_length;
   }
   next_line = lll_add( first, curr, sizeof(LINE) );
   if ( next_line == NULL )
   {
      TRACE_RETURN();
      return(NULL);
   }
   curr_line = next_line;

   curr_line->line = (CHARTYPE *)(*the_malloc)( (len+1)*sizeof(CHARTYPE) );
   if ( curr_line->line == NULL )
   {
      TRACE_RETURN();
      return(NULL);
   }
   memcpy( curr_line->line, line, len );
   *(curr_line->line+len) = '\0'; /* for functions that expect ASCIIZ string */
   curr_line->length = len;
   curr_line->select = select;
   curr_line->save_select = select;
   curr_line->pre = NULL;
   curr_line->first_name = NULL;
   curr_line->name = NULL; /* TODO - get rid of this when all uses of this structure for
                              other purposes are changed */
   curr_line->flags.new_flag = new_flag;
   curr_line->flags.changed_flag = FALSE;
   curr_line->flags.tag_flag = FALSE;
   curr_line->flags.save_tag_flag = FALSE;



#ifdef USE_SDSLH
   if (CURRENT_VIEW && CURRENT_FILE && CURRENT_FILE->sdslh_comm && CURRENT_FILE->cb && new_flag) {
       if (first == CURRENT_FILE->first_line) {
           int idx = -2;
           LINE *p = curr_line;
           while (p) { idx++; p = p->prev; }
           if (idx >= 0) {
               Transaction txn = { TRANSACTION_ADDLINE, idx, 0, NULL, 1 };
               editor_apply_transaction(CURRENT_FILE->cb, txn);
               if (curr_line->length > 0) {
                   Transaction add_txn = { TRANSACTION_ADDCHARS, idx, 0, (char *)curr_line->line, 0 };
                   editor_apply_transaction(CURRENT_FILE->cb, add_txn);
               }
           }
       }
   }
#endif

   /*
    * If this is the first line of the file, and the current parser for the
    * file is NULL, see if we can use one of the magic string parsers...
    */
   if (curr_line->prev
   &&  curr_line->prev->prev == NULL
   &&  CURRENT_VIEW
   &&  CURRENT_FILE
   &&  CURRENT_FILE->parser == NULL)
   {
      find_auto_parser( CURRENT_FILE );
   }
   TRACE_RETURN();
   return(curr_line);
}
/***********************************************************************/
LINE *append_LINE( LINE *curr, CHARTYPE *line, LENGTHTYPE len )
/***********************************************************************/
{
   TRACE_FUNCTION("util.c:    append_LINE");
   curr->line = (CHARTYPE *)(*the_realloc)( curr->line, (curr->length+len+1)*sizeof(CHARTYPE) );
   if ( curr->line == NULL )
   {
      TRACE_RETURN();
      return(NULL);
   }
   memcpy( curr->line+curr->length, line, len );
   curr->length += len;
   *(curr->line+curr->length) = '\0'; /* for functions that expect ASCIIZ string */
   TRACE_RETURN();
   return(curr);
}
/***********************************************************************/
LINE *delete_LINE( LINE **first, LINE **last, LINE *curr, short direction, bool delete_names )
/***********************************************************************/
/* Deletes a member of the linked list for the specified file.         */
/* PARAMETERS:                                                         */
/* first      - pointer to first line for the file                     */
/* first      - pointer to last  line for the file                     */
/* curr       - pointer to current line for the file                   */
/* direction  - direction in which to delete.                          */
/* delete_names - if 1 delete the names linked list                    */
/* RETURN:    - pointer to current item in linked list or NULL if error*/
/***********************************************************************/
{
   TRACE_FUNCTION("util.c:    delete_LINE");

#ifdef USE_SDSLH
   if (CURRENT_VIEW && CURRENT_FILE && CURRENT_FILE->sdslh_comm && CURRENT_FILE->cb && (*first == CURRENT_FILE->first_line)) {
       int idx = -2;
       LINE *p = curr;
       while (p) { idx++; p = p->prev; }
       if (idx >= 0) {
           Transaction txn = { TRANSACTION_DELETELINE, idx, 0, NULL, 1 };
           editor_apply_transaction(CURRENT_FILE->cb, txn);
       }
   }
#endif

   if ( delete_names )
   {
      if ( curr->first_name != (THELIST *)NULL )
      {
         ll_free( curr->first_name, the_free );
         curr->first_name = NULL;
      }
      if ( curr->name )
      {
         (*the_free)( curr->name );
         curr->name = NULL;
      }
   }
   if ( curr->line )
   {
      (*the_free)(curr->line);
      curr->line = NULL;
   }
   curr = lll_del( first, last, curr, direction );
   TRACE_RETURN();
   return( curr );
}

/***********************************************************************/
void put_string( WINDOW *win, ROWTYPE row, COLTYPE col, CHARTYPE *string, LENGTHTYPE len )
/***********************************************************************/
{
   LENGTHTYPE i=0;

   TRACE_FUNCTION("util.c:    put_string");
   curses_driver_move_window_cursor(win, row, col);
   for ( i = 0; i < len; i++ )
   {
      curses_driver_add_chtype(win, etmode_table[*(string+i)]);
   }
   TRACE_RETURN();
   return;
}
/***********************************************************************/
void put_char(WINDOW *win,chtype ch,CHARTYPE add_ins)
/***********************************************************************/
{
#ifdef VMS
   chtype chr=0;
#endif

   TRACE_FUNCTION("util.c:    put_char");
#ifdef VMS
   chr = ch & A_CHARTEXT;
   if (etmode_flag[chr])  /* etmode character has attributes, use them */
      ch = etmode_table[chr];
   else
      ch = etmode_table[chr] | (ch & A_ATTRIBUTES);
#endif

   if (add_ins == ADDCHAR)
      curses_driver_add_chtype(win, ch);
   else
      winsch( win, ch );
   TRACE_RETURN();
   return;
}
/***********************************************************************/
short set_up_windows(short scrn)
/***********************************************************************/
{
   register short i=0;
   short y=0,x=0;
   FILE_DETAILS fp;
   short my_prefix_width=0;
   CursesDriverWindowCursor cursor;

   TRACE_FUNCTION("util.c:    set_up_windows");
   /*
    * If curses has not started exit gracefully...
    */
   if ( !curses_started )
   {
      TRACE_RETURN();
      return(RC_OK);
   }
   /*
    * Allocate space for a file descriptor colour attributes...
    */
   if ( ( fp.attr = (COLOUR_ATTR *)(*the_malloc)( ATTR_MAX*sizeof(COLOUR_ATTR) ) ) == NULL )
   {
      display_error( 30, (CHARTYPE *)"", FALSE );
      TRACE_RETURN();
      return(RC_OUT_OF_MEMORY);
   }
   if ( screen[scrn].screen_view )
   {
      memcpy( fp.attr, screen[scrn].screen_view->file_for_view->attr, ATTR_MAX*sizeof(COLOUR_ATTR) );
      memcpy( fp.attr+ATTR_DIVIDER, CURRENT_FILE->attr+ATTR_DIVIDER, sizeof(COLOUR_ATTR) );
      my_prefix_width = screen[scrn].screen_view->prefix_width;
   }
   else
   {
      set_up_default_colours( &fp, (COLOUR_ATTR *)NULL, ATTR_MAX );
      my_prefix_width = prefix_width;
   }
   /*
    * Save the position of the cursor in each window, and then delete the
    * window. Recreate each window, that has a valid size and move the
    * cursor back to the position it had in each window.
    */
   for ( i = 0; i < VIEW_WINDOWS; i++ )
   {
      y = x = 0;
      if ( screen[scrn].win[i] != (WINDOW *)NULL )
      {
         cursor = curses_driver_capture_window_cursor(screen[scrn].win[i]);
         if (cursor.valid)
         {
            y = cursor.row;
            x = cursor.col;
         }
         curses_driver_delete_window(screen[scrn].win[i]);
         screen[scrn].win[i] = (WINDOW *)NULL;
      }
      if ( screen[scrn].rows[i] != 0
      &&   screen[scrn].cols[i] != 0 )
      {
         screen[scrn].win[i] = curses_driver_create_window(
            screen[scrn].rows[i], screen[scrn].cols[i],
            screen[scrn].start_row[i], screen[scrn].start_col[i]);
         if ( screen[scrn].win[i] == (WINDOW *)NULL )
         {
            display_error( 30, (CHARTYPE *)"creating window", FALSE );
            TRACE_RETURN();
            return(RC_OUT_OF_MEMORY);
         }
#ifdef HAVE_KEYPAD
         curses_driver_enable_keypad(screen[scrn].win[i], true);
#endif
#if !defined(PDCURSES)
         curses_driver_touch_window(screen[scrn].win[i]);
#endif
         curses_driver_move_window_cursor(screen[scrn].win[i], y, x);
      }
   }
   curses_driver_set_window_attr( screen[scrn].win[WINDOW_FILEAREA], set_colour( fp.attr+ATTR_FILEAREA ) );

   if ( screen[scrn].win[WINDOW_ARROW] != (WINDOW *)NULL )
   {
      curses_driver_set_window_attr( screen[scrn].win[WINDOW_ARROW], set_colour( fp.attr+ATTR_ARROW ) );
      for ( i = 0; i < my_prefix_width-2; i++ )
         curses_driver_add_chtype_at(screen[scrn].win[WINDOW_ARROW], 0, i, '=');
      curses_driver_add_string_at(screen[scrn].win[WINDOW_ARROW], 0,
                                  my_prefix_width - 2, "> ");
      curses_driver_refresh_window(screen[scrn].win[WINDOW_ARROW]);
   }

   if ( screen[scrn].win[WINDOW_IDLINE] != (WINDOW *)NULL )
   {
      curses_driver_set_window_attr( screen[scrn].win[WINDOW_IDLINE], set_colour( fp.attr+ATTR_IDLINE ) );
      curses_driver_move_window_cursor(screen[scrn].win[WINDOW_IDLINE], 0, 0);
      my_wclrtoeol( screen[scrn].win[WINDOW_IDLINE] );
   }

   if ( screen[scrn].win[WINDOW_PREFIX] != (WINDOW *)NULL )
      curses_driver_set_window_attr( screen[scrn].win[WINDOW_PREFIX], set_colour( fp.attr+ATTR_PENDING ) );

   if ( screen[scrn].win[WINDOW_GAP] != (WINDOW *)NULL )
      curses_driver_set_window_attr( screen[scrn].win[WINDOW_GAP], set_colour( fp.attr+ATTR_GAP ) );

   if ( screen[scrn].win[WINDOW_COMMAND] != (WINDOW *)NULL )
   {
      curses_driver_set_window_attr( screen[scrn].win[WINDOW_COMMAND], set_colour( fp.attr+ATTR_CMDLINE ) );
      cursor = curses_driver_capture_window_cursor(screen[scrn].win[WINDOW_COMMAND]);
      if (cursor.valid)
      {
         y = cursor.row;
         x = cursor.col;
      }
      curses_driver_move_window_cursor(screen[scrn].win[WINDOW_COMMAND], 0, 0);
      my_wclrtoeol( screen[scrn].win[WINDOW_COMMAND] );
      curses_driver_refresh_window(screen[scrn].win[WINDOW_COMMAND]);
      curses_driver_move_window_cursor(screen[scrn].win[WINDOW_COMMAND], y, x);
   }
   /*
    * Delete divider window.
    */
   if ( divider != (WINDOW *)NULL )
   {
      curses_driver_delete_window(divider);
      divider = NULL;
   }
   /*
    * Set up divider window...
    */
   if ( display_screens > 1
   &&   !horizontal)
   {
      divider = curses_driver_create_window(
         screen[1].screen_rows, 2, screen[1].screen_start_row,
         screen[1].screen_start_col - 2);
      if ( divider == (WINDOW *)NULL )
      {
         display_error( 30, (CHARTYPE *)"creating window", FALSE );
         TRACE_RETURN();
         return(RC_OUT_OF_MEMORY);
      }
#ifdef HAVE_KEYPAD
      curses_driver_enable_keypad(divider, true);
#endif

#if 0
# if defined(A_ALTCHARSET) && !defined(USE_NCURSES)
      curses_driver_set_window_attr( divider, A_ALTCHARSET|set_colour( fp.attr+ATTR_DIVIDER ) );
# else
      curses_driver_set_window_attr( divider, set_colour( fp.attr+ATTR_DIVIDER ) );
# endif
#else
      curses_driver_set_window_attr( divider, set_colour( fp.attr+ATTR_DIVIDER ) );
#endif

      draw_divider();
   }
   if ( max_slk_labels )
   {
#if defined(HAVE_SLK_INIT)
# if defined(HAVE_SLK_ATTRSET)
      slk_attrset( set_colour( fp.attr+ATTR_SLK ) );
# endif
      slk_noutrefresh();
#endif
   }
   /*
    * Free up  space for a file descriptor colour attributes...
    */
   (*the_free)( fp.attr );
   TRACE_RETURN();
   return(RC_OK);
}
/***********************************************************************/
short draw_divider(void)
/***********************************************************************/
{
#ifndef HAVE_WVLINE
   register int i=0;
#endif

   TRACE_FUNCTION("util.c:    draw_divider");

#ifdef HAVE_WVLINE
   curses_driver_move_window_cursor(divider, 0, 0);
   curses_driver_draw_vertical_line(divider,0,screen[1].screen_rows);
   curses_driver_move_window_cursor(divider, 0, 1);
   curses_driver_draw_vertical_line(divider,0,screen[1].screen_rows);
#else
   for (i=0;i<screen[1].screen_rows;i++)
   {
      curses_driver_move_window_cursor(divider, i, 0);
      curses_driver_add_chtype(divider, '|');
      curses_driver_add_chtype(divider, '|');
   }
#endif
   TRACE_RETURN();
   return(RC_OK);
}
/***********************************************************************/
short create_statusline_window(void)
/***********************************************************************/
{
   COLOUR_ATTR attr;

   TRACE_FUNCTION( "util.c:    create_statusline_window" );
   if ( !curses_started )
   {
      TRACE_RETURN();
      return(RC_OK);
   }
   if ( CURRENT_VIEW == NULL
   ||   CURRENT_FILE == NULL )
      set_up_default_colours( (FILE_DETAILS *)NULL, &attr, ATTR_STATAREA );
   else
      memcpy( &attr, CURRENT_FILE->attr+ATTR_STATAREA, sizeof(COLOUR_ATTR) );
   if ( statarea != (WINDOW *)NULL )
   {
      curses_driver_delete_window(statarea);
      statarea = (WINDOW *)NULL;
   }
   switch( STATUSLINEx )
   {
      case 'B':
         statarea = curses_driver_create_window(1, COLS, terminal_lines - 1, 0);
#ifdef HAVE_KEYPAD
         curses_driver_enable_keypad(statarea, true);
#endif
         curses_driver_set_window_attr( statarea, set_colour( &attr ) );
         clear_statarea();
         break;
      case 'T':
         statarea = curses_driver_create_window(1, COLS, (FILETABSx) ? 1 : 0, 0);
#ifdef HAVE_KEYPAD
         curses_driver_enable_keypad(statarea, true);
#endif
         curses_driver_set_window_attr( statarea, set_colour( &attr ) );
         clear_statarea();
         break;
      default:
         break;
   }
   TRACE_RETURN();
   return(RC_OK);
}
/***********************************************************************/
short create_filetabs_window(void)
/***********************************************************************/
{
   TRACE_FUNCTION( "util.c:    create_filetabs_window" );
   if ( !curses_started )
   {
      TRACE_RETURN();
      return(RC_OK);
   }
   if ( filetabs != (WINDOW *)NULL )
   {
      curses_driver_delete_window(filetabs);
      filetabs = (WINDOW *)NULL;
   }
   if ( FILETABSx )
   {
      filetabs = curses_driver_create_window(1, COLS, 0, 0);
#ifdef HAVE_KEYPAD
      curses_driver_enable_keypad(filetabs, true);
#endif
      display_filetabs( NULL );
      /*
       * If STATUSLINE is TOP, then we need to recreate it
       */
      create_statusline_window();
   }
   TRACE_RETURN();
   return(RC_OK);
}
/***********************************************************************/
void pre_process_line(VIEW_DETAILS *the_view,LINETYPE line_number,LINE *known_curr)
/***********************************************************************/
{
   LINE *curr=known_curr;

   TRACE_FUNCTION("util.c:    pre_process_line");
   /*
    * If we haven't been passed a valid LINE*, go and get one for the
    * supplied line_number.
    */
   if (curr == (LINE *)NULL)
      curr = lll_find( the_view->file_for_view->first_line, the_view->file_for_view->last_line, line_number, the_view->file_for_view->number_lines );
   memset( rec, ' ', max_line_length );
   memcpy( rec, curr->line, curr->length );
   rec_len = curr->length;
   /*
    * Now set up the prefix command from the linked list...
    */
   if (curr->pre == NULL)
   {
      memset( pre_rec, ' ', MAX_PREFIX_WIDTH );
      pre_rec_len = 0;
   }
   else
   {
      memset( pre_rec, ' ', MAX_PREFIX_WIDTH );
      strcpy( (DEFCHAR *)pre_rec, (DEFCHAR *)curr->pre->ppc_orig_command );
      pre_rec_len = strlen( (DEFCHAR *)pre_rec );
      pre_rec[pre_rec_len] = ' ';
      pre_rec[MAX_PREFIX_WIDTH] = '\0';
   }
   TRACE_RETURN();
   return;
}
/***********************************************************************/
short post_process_line(VIEW_DETAILS *the_view,LINETYPE line_number,LINE *known_curr,bool set_alt)
/***********************************************************************/
{
   LINE *curr=known_curr;
   short rc=RC_OK;

   TRACE_FUNCTION("util.c:    post_process_line");
   /*
    * If there are no lines in the file associated with the view, exit...
    */
   if (the_view->file_for_view->first_line == NULL)
   {
      TRACE_RETURN();
      return(RC_OK);
   }
   /*
    * If we haven't been passed a valid LINE*, go and get one for the
    * supplied line_number.
    */
   if (curr == (LINE *)NULL)
      curr = lll_find( the_view->file_for_view->first_line, the_view->file_for_view->last_line, line_number, the_view->file_for_view->number_lines );
   /*
    * First copy the pending prefix command to the linked list.
    * Only do it if the prefix command has a value or there is already a
    * pending prefix command for that line.
    */
   if ( prefix_changed )
      add_prefix_command( current_screen, CURRENT_VIEW, curr, line_number, FALSE, FALSE );
   /*
    * If the line hasn't changed, return.
    */
   if ( rec_len == curr->length
   && ( memcmp( rec, curr->line, curr->length) == 0 ) )
   {
      TRACE_RETURN();
      return(RC_NO_LINES_CHANGED);
   }
   /*
    * If it has set the changed_flag.
    */
   curr->flags.changed_flag = TRUE;
   /*
    * Increment the alteration counters, if requested to do so...
    */
   if (set_alt)
      increment_alt(the_view->file_for_view);
   /*
    * Add the old line contents to the line recovery list.
    */
   if (the_view->file_for_view->undoing)
      add_to_recovery_list(curr->line,curr->length);
   /*
    * Realloc the dynamic memory for the line if the line is now longer.
    */
   if (rec_len > curr->length)
   {
      curr->line = (CHARTYPE *)(*the_realloc)((void *)curr->line,(rec_len+1)*sizeof(CHARTYPE));
      if (curr->line == NULL)
      {
         display_error(30,(CHARTYPE *)"",FALSE);
         TRACE_RETURN();
         return(RC_OUT_OF_MEMORY);
      }
   }
   /*
    * Copy the contents of rec into the line.
    */
   LENGTHTYPE old_length = curr->length;
   memcpy( curr->line, rec, rec_len );
   curr->length = rec_len;
   *(curr->line+rec_len) = '\0';

#ifdef USE_SDSLH
   if (the_view && the_view->file_for_view && the_view->file_for_view->sdslh_comm && the_view->file_for_view->cb && curr->flags.changed_flag) {
       cb_sync_line(the_view->file_for_view->cb, line_number - 1, (const char *)curr->line);
   }
#endif

   /*
    * If this is the first line of the file, and the current parser for the
    * file is NULL, see if we can use one of the magic string parsers...
    */
   if (line_number == 1
   &&  CURRENT_FILE->parser == NULL)
   {
      find_auto_parser(CURRENT_FILE);
   }
   TRACE_RETURN();
   return(rc);
}
/***********************************************************************/
bool blank_field(CHARTYPE *field)
/***********************************************************************/
{
   TRACE_FUNCTION("util.c:    blank_field");
   if ( field == NULL )
   {
      TRACE_RETURN();
      return(TRUE);                /* field is NULL */
   }
   if ( strzne( field, ' ' ) == (-1) )
   {
      TRACE_RETURN();
      return(TRUE);                /* field just contains spaces */
   }
   TRACE_RETURN();
   return(FALSE);
}
/***********************************************************************/
void adjust_marked_lines(bool binsert_line,LINETYPE base_line,LINETYPE num_lines)
/***********************************************************************/
{
   int iinsert_line=binsert_line;
/*
 * When lines are deleted, the base line is the first line in the file
 * irrespective of the direction that the delete is done.
 */
   TRACE_FUNCTION("util.c:    adjust_marked_lines");
   /*
    * If there are no marked lines in the current view, return.
    */
   if (MARK_VIEW != CURRENT_VIEW)
   {
      TRACE_RETURN();
      return;
   }
   switch(iinsert_line)
   {
      case TRUE:/* INSERT */
         if (base_line < CURRENT_VIEW->mark_start_line)
         {
            CURRENT_VIEW->mark_start_line += num_lines;
            CURRENT_VIEW->mark_end_line += num_lines;
            break;
         }
         if (base_line >= CURRENT_VIEW->mark_start_line
         &&  base_line < CURRENT_VIEW->mark_end_line)
         {
            CURRENT_VIEW->mark_end_line += num_lines;
            break;
         }
         break;
      case FALSE:  /* DELETE */
         if (base_line <= CURRENT_VIEW->mark_start_line
         &&  base_line+num_lines-1L >= CURRENT_VIEW->mark_end_line)
         {
            CURRENT_VIEW->marked_line = FALSE;
            MARK_VIEW = (VIEW_DETAILS *)NULL;
            break;
         }
         if (base_line+num_lines-1L < CURRENT_VIEW->mark_start_line)
         {
            CURRENT_VIEW->mark_start_line -= num_lines;
            CURRENT_VIEW->mark_end_line -= num_lines;
            break;
         }
         if (base_line > CURRENT_VIEW->mark_end_line)
         {
            break;
         }
         if (base_line+num_lines-1L > CURRENT_VIEW->mark_end_line)
         {
            CURRENT_VIEW->mark_end_line = base_line - 1L;
            break;
         }
         if (base_line < CURRENT_VIEW->mark_start_line)
         {
            CURRENT_VIEW->mark_start_line = base_line;
            CURRENT_VIEW->mark_end_line = base_line + (CURRENT_VIEW->mark_end_line - (base_line + num_lines));
            break;
         }
         CURRENT_VIEW->mark_end_line -= num_lines;
         break;
   }
   TRACE_RETURN();
   return;
}
/***********************************************************************/
void adjust_pending_prefix(VIEW_DETAILS *view,bool binsert_line,LINETYPE base_line,LINETYPE num_lines)
/***********************************************************************/
{
   int iinsert_line=binsert_line;
   /*
    * When lines are deleted, the base line is the first line in the file
    * irrespective of the direction that the delete is done.
    */
   THE_PPC *curr_ppc=NULL;

   TRACE_FUNCTION("util.c:    adjust_pending_prefix");
   /*
    * If there are no pending prefix commands in the view, return.
    */
   if (view->file_for_view->first_ppc == NULL)
   {
      TRACE_RETURN();
      return;
   }
   curr_ppc = view->file_for_view->first_ppc;
   while (curr_ppc != NULL)
   {
      switch( iinsert_line )
      {
         case TRUE:/* INSERT */
            if (base_line < curr_ppc->ppc_line_number)
            {
               curr_ppc->ppc_line_number += num_lines;
               break;
            }
            break;
         case FALSE:  /* DELETE */
            if (base_line+num_lines-1L < curr_ppc->ppc_line_number)
            {
               curr_ppc->ppc_line_number -= num_lines;
               break;
            }
            if (base_line > curr_ppc->ppc_line_number)
               break;
#if OLD_CLEAR
            (void)delete_pending_prefix_command(curr_ppc,view->file_for_view,(LINE *)NULL);
#else
            clear_pending_prefix_command( curr_ppc, view->file_for_view, (LINE *)NULL );
#endif
            break;
      }
      curr_ppc = curr_ppc->next;
   }
   TRACE_RETURN();
   return;
}
/***********************************************************************/
CHARTYPE case_translate(CHARTYPE key)
/***********************************************************************/
{
   CHARTYPE case_type = CURRENT_VIEW->case_enter;

   TRACE_FUNCTION("util.c:    case_translate");
   switch( CURRENT_VIEW->current_window )
   {
      case WINDOW_COMMAND:
         case_type = CURRENT_VIEW->case_enter_cmdline;
         break;
      case WINDOW_PREFIX:
         case_type = CURRENT_VIEW->case_enter_prefix;
         break;
      default:
         break;
   }
   if ( case_type == CASE_UPPER
   && islower( key ) )
   {
      TRACE_RETURN();
      return( toupper( key ) );
   }
   if ( case_type == CASE_LOWER
   && isupper( key ) )
   {
      TRACE_RETURN();
      return( tolower( key ) );
   }
   TRACE_RETURN();
   return( key );
}
/***********************************************************************/
void add_to_recovery_list(CHARTYPE *line,LENGTHTYPE len)
/***********************************************************************/
{
   register short i=0;

   TRACE_FUNCTION("util.c:    add_to_recovery_list");
   /*
    * Ignore if running in batch.
    */
   if (batch_only)
   {
      TRACE_RETURN();
      return;
   }
   /*
    * First time through, set line array to NULL,  to indicated unused.
    * This setup MUST occur before the freeing up code.
    */
   if (add_rcvry == (-1))
   {
      for ( i = 0; i < MAX_RECV; i++ )
         rcvry[i] = NULL;
      add_rcvry = 0;               /* set to point to next available slot */
   }
   /*
    * Now we are here, lets add to the array.
    */
   if (rcvry[add_rcvry] == NULL)  /* haven't malloced yet */
   {
      if ((rcvry[add_rcvry] = (CHARTYPE *)(*the_malloc)((len+1)*sizeof(CHARTYPE))) == NULL)
      {
         display_error(30,(CHARTYPE *)"",FALSE);
         TRACE_RETURN();
         return;
      }
   }
   else
   {
      if ((rcvry[add_rcvry] = (CHARTYPE *)(*the_realloc)(rcvry[add_rcvry],(len+1)*sizeof(CHARTYPE))) == NULL)
      {
         display_error(30,(CHARTYPE *)"",FALSE);
         TRACE_RETURN();
         return;
      }
   }
   memcpy(rcvry[add_rcvry],line,len);
   rcvry_len[add_rcvry] = len;
   retr_rcvry = add_rcvry;
   add_rcvry = (++add_rcvry >= MAX_RECV) ? 0 : add_rcvry;
   num_rcvry = (++num_rcvry > MAX_RECV) ? MAX_RECV : num_rcvry;

   TRACE_RETURN();
   return;
}
/***********************************************************************/
void get_from_recovery_list(short num)
/***********************************************************************/
{
   register short i=0;
   short num_retr = min(num,num_rcvry);

   TRACE_FUNCTION("util.c:    get_from_recovery_list");
   /*
    * Return error if nothing to recover.
    */
   if (retr_rcvry == (-1))
   {
      display_error( 0, (CHARTYPE *)"0 line(s) recovered", TRUE );
      TRACE_RETURN();
      return;
   }
   /*
    * Retrieve each allocated recovery line and put back into the body.
    */
   post_process_line(CURRENT_VIEW,CURRENT_VIEW->focus_line,(LINE *)NULL,TRUE);
   for ( i = 0; i < num_retr; i++ )
   {
      if (rcvry[retr_rcvry] != NULL)
      {
         insert_new_line( current_screen, CURRENT_VIEW, rcvry[retr_rcvry], rcvry_len[retr_rcvry], 1L, get_true_line(TRUE), TRUE, FALSE, FALSE, CURRENT_VIEW->display_low, TRUE, FALSE );
         retr_rcvry = (--retr_rcvry < 0) ? num_rcvry-1 : retr_rcvry;
      }
   }
   /*
    * If one or more lines were retrieved, increment the alteration counts
    */
   if (num_retr)
      increment_alt(CURRENT_FILE);

   sprintf((DEFCHAR *)temp_cmd,"%d line(s) recovered",num_retr);
   display_error(0,temp_cmd,TRUE);
   TRACE_RETURN();
   return;
}
/***********************************************************************/
void free_recovery_list(void)
/***********************************************************************/
{
   register short i=0;

   TRACE_FUNCTION("util.c:    free_recovery_list");
   for ( i = 0; i < MAX_RECV; i++ )
   {
      if (rcvry[i] != NULL)
      {
         (*the_free)(rcvry[i]);
         rcvry[i] = NULL;
      }
   }
   add_rcvry  = (-1);
   retr_rcvry = (-1);
   num_rcvry  = 0;
   TRACE_RETURN();
   return;
}

#if THIS_APPEARS_TO_NOT_BE_USED
/***********************************************************************/
WINDOW *adjust_window(WINDOW *win,short tr,short tc,short lines,short cols)
/***********************************************************************/
{
   WINDOW *neww=NULL;
   short begy=0,begx=0,maxy=0,maxx=0,y=0,x=0;
   short rc=RC_OK;
   CursesDriverWindowOrigin origin;
   CursesDriverWindowSize size;
   CursesDriverWindowCursor cursor;

   TRACE_FUNCTION("util.c:    adjust_window");
   /*
    * Get existing details about the current window.
    */
   origin = curses_driver_window_origin(win);
   size = curses_driver_window_size(win);
   if (origin.valid)
   {
      begy = origin.row;
      begx = origin.col;
   }
   if (size.valid)
   {
      maxy = size.rows;
      maxx = size.cols;
   }
   if (maxy == lines && maxx == cols)  /* same size */
   {
      if (begy == tr && begx == tc)   /* same position */
      {
         TRACE_RETURN();
         return(win); /* nothing to do, return same window */
      }
      else /* need to move window */
      {
         rc = mvwin(win,tr,tc);
         TRACE_RETURN();
         return(win);
      }
   }
   /*
    * To get here the window needs to be resized.
    */
   cursor = curses_driver_capture_window_cursor(win);
   if (cursor.valid)
   {
      y = cursor.row;
      x = cursor.col;
   }
   curses_driver_delete_window(win);
   neww = curses_driver_create_window(lines, cols, tr, tc);
   if (neww != (WINDOW *)NULL)
   {
      curses_driver_move_window_cursor(neww, y, x);
#ifdef HAVE_KEYPAD
      curses_driver_enable_keypad(neww, true);
#endif
   }
   TRACE_RETURN();
   return(neww);
}
#endif

/***********************************************************************/
short my_wclrtoeol(WINDOW *win)
/***********************************************************************/
{
   register short i=0;
   short x=0,y=0,maxx=0,maxy=0;
   CursesDriverWindowCursor cursor;
   CursesDriverWindowSize size;

   TRACE_FUNCTION("util.c:    my_wclrtoeol");
#if defined(USE_NCURSES_IGNORED)
   /*
    * This extra code here to get around an ncurses bug that
    * does not overwrite existing characters displayed when a
    * clrtoeol() is called.
    * Try COMPAT X#PREFIX OFF#PREFIX ON
    */
   if (win != (WINDOW *)NULL)
   {
      cursor = curses_driver_capture_window_cursor(win);
      size = curses_driver_window_size(win);
      if (cursor.valid)
      {
         y = cursor.row;
         x = cursor.col;
      }
      if (size.valid)
      {
         maxy = size.rows;
         maxx = size.cols;
      }
      for ( i = x; i < maxx; i++ )
         curses_driver_add_chtype(win, '@');
      curses_driver_move_window_cursor(win, y, x);
   }
#endif
   if (win != (WINDOW *)NULL)
   {
      cursor = curses_driver_capture_window_cursor(win);
      size = curses_driver_window_size(win);
      if (cursor.valid)
      {
         y = cursor.row;
         x = cursor.col;
      }
      if (size.valid)
      {
         maxy = size.rows;
         maxx = size.cols;
      }
      for ( i = x; i < maxx; i++ )
         curses_driver_add_chtype(win, ' ');
      curses_driver_move_window_cursor(win, y, x);
   }
   INTENTIONALLY_UNUSED_VARIABLE(maxy);
   TRACE_RETURN();
   return(0);
}
/***********************************************************************/
short my_wdelch(WINDOW *win)
/***********************************************************************/
{
   short x=0,y=0,maxx=0,maxy=0;
   CursesDriverWindowCursor cursor;
   CursesDriverWindowSize size;

   TRACE_FUNCTION("util.c:    my_wdelch");

   cursor = curses_driver_capture_window_cursor(win);
   size = curses_driver_window_size(win);
   if (cursor.valid)
   {
      y = cursor.row;
      x = cursor.col;
   }
   if (size.valid)
   {
      maxy = size.rows;
      maxx = size.cols;
   }
   wdelch(win);
   curses_driver_add_chtype_at(win, y, maxx - 1, ' ');
   curses_driver_move_window_cursor(win, y, x);
   INTENTIONALLY_UNUSED_VARIABLE(maxy);
   TRACE_RETURN();
   return(0);
}
/***********************************************************************/
short get_word(CHARTYPE *string,LENGTHTYPE length,LENGTHTYPE curr_pos,
               LENGTHTYPE *first_col,LENGTHTYPE *last_col)
/***********************************************************************/
/*
 * A "word" is based on the SET WORD settings
 * Returns the portion of the string containing a "word" to the right
 * of the current position. Used in SOS DELWORD and MARK WORD
 * If the current position is a blank, the the "word" is all blanks
 * the left or right of the current position.
 * If in a "word", all characters in the word and any following
 * blanks (to the next "word") are included
 */
{
   short state=0;
   LENGTHTYPE i=0;

   TRACE_FUNCTION("util.c:    get_word");
   /*
    * If we are after the last column of the line, then just ignore the
    * command and leave the cursor where it is.
    */
   if (curr_pos >= length)
   {
      TRACE_RETURN();
      return(0);
   }
   /*
    * Determine the end of the next word, or go to the end of the line
    * if already at or past beginning of last word.
    */
   /*
    * If the current character is a space, mark all spaces as the word.
    * The beahiour is the same for this situation regardless of WORD
    * setting.
    */
   if (*(string+curr_pos) == ' ')
   {
      for (i=curr_pos;i<length;i++)
      {
         if (*(string+i) != ' ')
         {
            *last_col = i-1;
            break;
         }
      }
      if (i == length)
         *last_col = length - 1;
      for (i=curr_pos;i>(-1);i--)
      {
         if (*(string+i) != ' ')
         {
            *first_col = i+1;
            break;
         }
      }
      if (i < 0)
         *first_col = 0;
      TRACE_RETURN();
      return(1);
   }
   /*
    * To get here the current character is non-blank.
    */
   state = my_isalphanum(*(string+curr_pos));
   if (CURRENT_VIEW->word == 'N')
   {
      /*
       * Get first column
       */
      for (i=curr_pos;i>(-1);i--)
      {
         if (*(string+i) == ' ')
         {
            *first_col = i+1;
            break;
         }
      }
      if (i < 0)
         *first_col = 0;
      /*
       * Get last column
       */
      for (i=curr_pos;i<length;i++)
      {
         if (*(string+i) == ' ')
         {
            *last_col = i-1;
            break;
         }
      }
      if (i < length)
      {
         for (;i<length;i++)
         {
            if (*(string+i) != ' ')
            {
               *last_col = i-1;
               break;
            }
         }
      }
      if (i == length)
         *last_col = length - 1;
   }
   else
   {
      /*
       * Get first column
       */
      for (i=curr_pos;i>(-1);i--)
      {
         if (my_isalphanum(*(string+i)) != state)
         {
            *first_col = i+1;
            break;
         }
      }
      if (i < 0)
         *first_col = 0;
      /*
       * Get last column
       */
      for (i=curr_pos;i<length;i++)
      {
         if (my_isalphanum(*(string+i)) != state)
         {
            *last_col = i-1;
            break;
         }
      }
      if (i < length
      && *(string+i) == ' ')
      {
         for (;i<length;i++)
         {
            if (*(string+i) != ' ')
            {
               *last_col = i-1;
               break;
            }
         }
      }
      if (i == length)
         *last_col = length - 1;
   }

   TRACE_RETURN();
   return(1);
}
/***********************************************************************/
short get_fieldword( CHARTYPE *string, LENGTHTYPE length, LENGTHTYPE curr_pos,
                     LENGTHTYPE *first_col, LENGTHTYPE *last_col )
/***********************************************************************/
/*
 * A "word" is based on the SET WORD settings
 * Returns the portion of the string containing a "word" nearest
 * the current position. Used in EXTRACT FIELDWORD
 * If the current position is a blank, the the "word" is looked for starting
 * at the left of the current position, and then to the right.
 *
 * eg:
 *    abc def
 *       ^      cursor location
 *   "abc"      "word" obtained
 *
 *    abc   def
 *        ^     cursor location
 *   "abc"      "word" obtained
 *
 *    abc   def
 *         ^    cursor location
 *   "def"      "word" obtained
 *
 *    abc def
 *        ^     cursor location
 *   "def"      "word" obtained
 *
 *    abc def
 *           ^  cursor location
 *   "def"      "word" obtained
 */
{
#define LOOK_LEFT 0
#define LOOK_BOTH 1
   short look;
   short state=0;
   LENGTHTYPE i=0,j=0;

   TRACE_FUNCTION("util.c:    get_fieldword");
   /*
    * If we are after the last column of the line, then just look left.
    */
   if (curr_pos >= length)
   {
      look = LOOK_LEFT;
      curr_pos = length-1;
   }
   else
      look = LOOK_BOTH;
   /*
    * If the current character is a space, then look for the closest word
    * starting at the left, and possibly to the right.
    */
   if ( *(string+curr_pos) == ' ' )
   {
      if ( curr_pos != 0 )
         i = curr_pos - 1;
      if ( curr_pos < length )
         j = curr_pos + 1;
      for ( ; ; i--,j++ )
/*      for ( ; i >= 0, j < length; i--,j++ )*/
      {
         /*
          * Look left
          */
         if ( *(string+i) != ' ' )
         {
            curr_pos = i;
            break;
         }
         /*
          * Look right if allowed
          */
         if ( look == LOOK_BOTH )
         {
            if ( *(string+j) != ' ' )
            {
               curr_pos = j;
               break;
            }
         }
         if ( i > 0 )
            i--;
         if ( j < length )
            j++;
      }
   }
   /*
    * To get here the current character (curr_pos) is non-blank.
    */
   state = my_isalphanum(*(string+curr_pos));
   if (CURRENT_VIEW->word == 'N')
   {
      /*
       * Get first column
       * Character to the right of the first blank found
       */
      for (i=curr_pos;i>(-1);i--)
      {
         if (*(string+i) == ' ')
         {
            *first_col = i+1;
            break;
         }
      }
      if (i < 0)
         *first_col = 0;
      /*
       * Get last column
       * Character to the left of the first blank found
       */
      for (i=curr_pos;i<length;i++)
      {
         if (*(string+i) == ' ')
         {
            *last_col = i-1;
            break;
         }
      }
#if 0
      if (i < length)
      {
         for (;i<length;i++)
         {
            if (*(string+i) != ' ')
            {
               *last_col = i-1;
               break;
            }
         }
      }
#endif
      if (i == length)
         *last_col = length - 1;
   }
   else
   {
      /*
       * Get first column
       * Character to the right of the first blank found
       */
      for (i=curr_pos;i>(-1);i--)
      {
         if (my_isalphanum(*(string+i)) != state)
         {
            *first_col = i+1;
            break;
         }
      }
      if (i < 0)
         *first_col = 0;
      /*
       * Get last column
       * Character to the left of the first blank found
       */
      for (i=curr_pos;i<length;i++)
      {
         if (my_isalphanum(*(string+i)) != state)
         {
            *last_col = i-1;
            break;
         }
      }
#if 0
      if (i < length
      && *(string+i) == ' ')
      {
         for (;i<length;i++)
         {
            if (*(string+i) != ' ')
            {
               *last_col = i-1;
               break;
            }
         }
      }
#endif
      if (i == length)
         *last_col = length - 1;
   }

   TRACE_RETURN();
   return(1);
}

/***********************************************************************/
short my_isalphanum(CHARTYPE chr)
/***********************************************************************/
{
   short char_type=CHAR_OTHER;

   TRACE_FUNCTION("util.c:    my_isalphanum");

   if (chr == ' ')
      char_type = CHAR_SPACE;
   else
   {
      if (isalpha(chr)
      ||  isdigit(chr)
      ||  chr == '_'
      ||  chr > 128)
         char_type = CHAR_ALPHANUM;
   }
   TRACE_RETURN();
   return(char_type);
}

/***********************************************************************/
short my_wmove(WINDOW *win,short scridx,short winidx,short y,short x)
/***********************************************************************/
{
   short rc=RC_OK;

   TRACE_FUNCTION("util.c:    my_wmove");
   /*
    * If the scridx or winidx are -1, do not try to save the x/y position.
    */
   if (scridx != (-1)
   &&  winidx != (-1))
   {
      screen[scridx].screen_view->x[winidx] = x;
      screen[scridx].screen_view->y[winidx] = y;
   }
   if (curses_started)
      curses_driver_move_window_cursor(win, y, x);
   TRACE_RETURN();
   return(rc);
}

/***********************************************************************/
short get_row_for_tof_eof(short row,CHARTYPE scridx)
/***********************************************************************/
{
   TRACE_FUNCTION("util.c:    get_row_for_tof_eof");
   if (screen[scridx].sl[row].line_type == LINE_OUT_OF_BOUNDS_ABOVE)
   {
      for(;screen[scridx].sl[row].line_type != LINE_TOF;row++)
/*    for(;screen[scridx].sl[row].line_type != LINE_TOF_EOF;row++) MH12 */
       ;
   }
   if (screen[scridx].sl[row].line_type == LINE_OUT_OF_BOUNDS_BELOW)
   {
/*    for(;screen[scridx].sl[row].line_type != LINE_TOF_EOF;row--) MH12 */
      for(;screen[scridx].sl[row].line_type != LINE_EOF;row--)
       ;
   }
   TRACE_RETURN();
   return(row);
}

/***********************************************************************/
void set_compare_exact( bool exact )
/***********************************************************************/
{
   CompareExact = exact;
}

/***********************************************************************/
static int query_item_compare(const void *inkey, const void *intpl)
/***********************************************************************/
{
   const char *key = (char *)inkey;
   const QUERY_ITEM *tpl = (QUERY_ITEM *)intpl;
   int rc=0,m=CompareLen;

   if (m > tpl->name_length)
      m = tpl->name_length;
   rc = memcmp(key, (DEFCHAR*)tpl->name, m);
   if (rc != 0)
      return(rc);
     if ( CompareExact )
     {
        if (CompareLen > tpl->name_length)
           return(1);
        if (CompareLen < tpl->name_length)
           return(-1);
     }
     else
     {
        if ( equal( tpl->name, (CHARTYPE *)key, tpl->min_len ) )
           return 0;
        if ( CompareLen > tpl->name_length )
           return(1);
        if ( CompareLen < tpl->name_length )
           return(-1);
     }
   return(0);
}

/***********************************************************************/
int search_query_item_array(void *base, size_t num, size_t width, const char *needle, int len)
/***********************************************************************/
{
   char *buf=NULL;
   void *result=NULL;
   int i=0;
#ifdef __CHECKER__
   /* checker has a buggy bsearch stub */
   size_t alloclen = width;
#endif

   TRACE_FUNCTION("util.c:    search_query_item_array");

#ifdef __CHECKER__
   if (len > alloclen)
      alloclen = len;
   if ((buf = (char*)(*the_malloc)(alloclen+1)) == NULL)
#else
   if ((buf = (char*)(*the_malloc)(len+1)) == NULL)
#endif
   {
      TRACE_RETURN();
      return(-1);
   }

   for (i=0; i<len; i++)
   {
      buf[i] = (char)tolower(needle[i]);
   }
   buf[i] = '\0';
   CompareLen = len;
   result = (char *)bsearch(buf, base, num, width, query_item_compare);
   (*the_free)(buf);

   if (result == NULL)
   {
      TRACE_RETURN();
      return(-1);
   }
   TRACE_RETURN();
//   return((int) (((unsigned long) result - (unsigned long) base) / width ));
   return((int) (( (size_t)result - (size_t)base) / width ));
}

/***********************************************************************/
int split_function_name(CHARTYPE *funcname, int *funcname_length)
/***********************************************************************/
{
   int functionname_length = strlen((DEFCHAR*)funcname);
   int itemno=0,pos=0;

   pos = memreveq((CHARTYPE *)funcname,(CHARTYPE)'.',functionname_length);
   if (pos == (-1)
   ||  functionname_length == pos-1)
   {
      /*
       * Not a valid implied extract function; could be a boolean
       */
      itemno = -1;
   }
   else
   {
      if (!valid_positive_integer((CHARTYPE *)funcname+pos+1))
      {
         /*
          * Not a valid implied extract function; could be a boolean
          */
         itemno = -1;
      }
      else
      {
         itemno = atoi((DEFCHAR *)funcname+pos+1);
         /*
          * If the tail is > maximum number of variables that we can
          * handle, exit with error.
          */
         functionname_length = pos;
      }
   }
   *funcname_length = functionname_length;
   return itemno;
}

/***********************************************************************/
char *gettempdir(void)
/***********************************************************************/
{
   char *path=NULL;

   if ((path = getenv("THE_TMPDIR")) != NULL)
      return path;
   if ((path = getenv("TMP")) != NULL)
      return path;
   if ((path = getenv("TEMP")) != NULL)
      return path;
   if ((path = getenv("TMPDIR")) != NULL)
      return path;
   return ".";
}

/***********************************************************************/
char *thetmpnam(char *prefix)
/***********************************************************************/
{
  /*
   * This function is not thread safe.
   */
   char *filename=NULL;
   char *path=gettempdir();
#if defined HAVE_MKSTEMP
   int fileno;
   filename = (*the_malloc)( strlen(prefix) + strlen( path ) + 15 );
   if ( filename == NULL )
   {
      return NULL;
   }
   sprintf( filename, "%s%s%sXXXXXX", path, ISTR_SLASH, prefix );
   fileno = mkstemp( filename );
   if ( fileno == -1 )
   {
      (*the_free)( filename );
      return NULL;
   }
   close( fileno );
   return filename;
#elif HAVE__MKTEMP_S
/* win32 */
   int len;
   filename = (*the_malloc)( strlen(prefix) + strlen( path ) + 10 );
   if ( filename == NULL )
   {
      return NULL;
   }
   len = sprintf( filename, "%s%s%sXXXXXX", path, ISTR_SLASH, prefix );
   if ( _mktemp_s( filename, len + 1) != 0 )
   {
      (*the_free)( filename );
      return NULL;
   }
   return filename;
#elif defined HAVE_BROKEN_TMPNAM
# define PATH_DELIMS ":\\/"
   static char *buffer = NULL;
   static size_t buffersize = 0;
   size_t needed;
   unsigned long i;
   FILE *fp=NULL;

   needed = strlen(path) + 1 /* ISTR_SLASH */ + sizeof("TMP12345.TMP");
   if (needed > buffersize)
   {
      if ((buffer = realloc(buffer,needed)) == NULL)
         return(NULL);
      buffersize = needed;
   }

   strcpy(buffer,path);
   if (strchr(PATH_DELIMS,buffer[strlen(buffer)-1]) == NULL)
      strcat(buffer,ISTR_SLASH);
   filename = buffer + strlen(buffer);
   for (i = 0;i <= 99999;i++)
   {
      sprintf(filename,"%s%05lu.TMP",prefix,i);
      if (access(filename,0) != 0)
      {
         /*
          * Open the file to ensure it is created
          */
         if ( ( fp = fopen( buffer, "w" ) ) == NULL )
            return( NULL );
         fclose( fp );
         return(buffer);
      }
   }

   return(NULL);
#else
   filename = (*the_malloc)( L_tmpnam + 1 );
   if ( filename == NULL )
   {
      return NULL;
   }
   return tmpnam( filename );
#endif
}

/***********************************************************************/
VIEW_DETAILS *find_filetab(int x)
/***********************************************************************/
{
   /*
    * Now we know where the mouse was clicked, determine which tab
    * or scroll arrow was clicked.
    */
   VIEW_DETAILS *curr;
   FILE_DETAILS *first_view_file=NULL;
   bool process_view=FALSE;
   register int j=0;
   int fname_len, fname_start = 0;
   bool first = TRUE;
   chtype filetab_cell=0;

   TRACE_FUNCTION("util.c:    find_filetab");
   /*
    * If filetabs is not displayed, don't do anything.
    */
   if ( FILETABSx )
   {
      curses_driver_move_window_cursor(filetabs, 0, COLS - 1);
      filetab_cell = curses_driver_read_window_cell(filetabs);
#ifdef VMS
      if ( filetab_cell == '>'
#else
      if ( ( filetab_cell & A_CHARTEXT ) == '>'
#endif
      &&  x == COLS-1 )
      {
         Tabfile( (CHARTYPE *)"+" );
         TRACE_RETURN();
         return NULL;
      }
      curses_driver_move_window_cursor(filetabs, 0, COLS - 2);
      filetab_cell = curses_driver_read_window_cell(filetabs);
#ifdef VMS
      if ( filetab_cell == '<'
#else
      if ( ( filetab_cell & A_CHARTEXT ) == '<'
#endif
      &&  x == COLS-2 )
      {
         Tabfile( (CHARTYPE *)"-" );
         TRACE_RETURN();
         return NULL;
      }
      if ( filetabs_start_view == NULL )
         curr = vd_current;
      else
         curr = filetabs_start_view;
      for ( j = 0; j < number_of_files; )
      {
         process_view = TRUE;
         if ( curr->file_for_view->file_views > 1 )
         {
            if ( first_view_file == curr->file_for_view )
               process_view = FALSE;
            else
               first_view_file = curr->file_for_view;
         }
         if ( process_view )
         {
            j++;
            if ( curr != CURRENT_VIEW )
            {
               fname_len = strlen( (DEFCHAR *)curr->file_for_view->fname );
               if ( first )
               {
                  /*
                   * If run from command line, return the VIEW_DETAILS
                   * pointer to the first view.
                   */
                  if ( x == -1 )
                  {
                     TRACE_RETURN();
                     return curr;
                  }
                  first = FALSE;
               }
               else
               {
                  fname_start += DEFAULT_FILETABS_GAP_WIDTH;
               }
               if ( fname_start + fname_len > COLS-2 )
                  break;
               if ( x >= fname_start
               &&   x <= fname_start + fname_len )
               {
                  TRACE_RETURN();
                  return curr;
               }
               fname_start += fname_len;
            }
         }
         curr = curr->next;
         if (curr == NULL)
            curr = vd_first;
      }
   }
   TRACE_RETURN();
   return NULL;
}
/***********************************************************************/
VIEW_DETAILS *find_next_file( VIEW_DETAILS *curr, short direction )
/***********************************************************************/
{
   /*
    * Starts in the ring at the specified location and finds the next
    * file that isn't the file in the current view or the starting view
    */
   VIEW_DETAILS *save_current_view=curr;
   int i;

   TRACE_FUNCTION("util.c:    find_next_file");

   for ( i = 0; i < number_of_files; i++ )
   {
      if ( direction == DIRECTION_FORWARD )
      {
         if ( curr->next == (VIEW_DETAILS *)NULL )
            curr = vd_first;
         else
            curr = curr->next;
      }
      else
      {
         if ( curr->prev == (VIEW_DETAILS *)NULL )
            curr = vd_last;
         else
            curr = curr->prev;
      }
      if ( curr != save_current_view
      &&   curr != vd_current )
         break;
   }
   return curr;
}

#ifndef HAVE_DOUPDATE
/***********************************************************************/
int doupdate(void)
/***********************************************************************/
{
   unsigned short y=0,x=0;
   CursesDriverWindowCursor cursor;

   TRACE_FUNCTION("util.c:    doupdate");
   cursor = curses_driver_capture_window_cursor(CURRENT_WINDOW);
   if (cursor.valid)
   {
      y = cursor.row;
      x = cursor.col;
   }
   curses_driver_refresh_standard_screen();
   curses_driver_move_window_cursor(CURRENT_WINDOW, y, x);
   curses_driver_refresh_window_now(CURRENT_WINDOW);
   TRACE_RETURN();
   return(0);
}
#endif

#ifdef USE_EXTCURSES
/***********************************************************************/
int has_colors(void)
/***********************************************************************/
{
   return(TRUE);
}
/***********************************************************************/
int start_color(void)
/***********************************************************************/
{
   register int i=0;

   for (i=0;i<COLOR_PAIRS;i++)
     color_pair[i] = NORMAL;
   fore_color[COLOR_BLACK  ] = F_BLACK  ;
   fore_color[COLOR_BLUE   ] = F_BLUE   ;
   fore_color[COLOR_GREEN  ] = F_GREEN  ;
   fore_color[COLOR_CYAN   ] = F_CYAN   ;
   fore_color[COLOR_RED    ] = F_RED    ;
   fore_color[COLOR_MAGENTA] = F_MAGENTA;
   fore_color[COLOR_YELLOW ] = F_BROWN  ;
   fore_color[COLOR_WHITE  ] = F_WHITE  ;
   back_color[COLOR_BLACK  ] = B_BLACK  ;
   back_color[COLOR_BLUE   ] = B_BLUE   ;
   back_color[COLOR_GREEN  ] = B_GREEN  ;
   back_color[COLOR_CYAN   ] = B_CYAN   ;
   back_color[COLOR_RED    ] = B_RED    ;
   back_color[COLOR_MAGENTA] = B_MAGENTA;
   back_color[COLOR_YELLOW ] = B_BROWN  ;
   back_color[COLOR_WHITE  ] = B_WHITE  ;
   return(0);
}
/***********************************************************************/
int init_pair(int pairnum,chtype fore,chtype back)
/***********************************************************************/
{
  register int i=0;

  color_pair[pairnum] = fore_color[fore] | back_color[back];
  return(0);
}
#endif
