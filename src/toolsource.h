#ifndef THE_TOOLSOURCE_H
#define THE_TOOLSOURCE_H
#include <ctype.h>
#include <stddef.h>
#include <string.h>

/* A portable, relative snapshot basename. Logical identity remains in the
 * session; private job directory names never become the source argument. */
static void tool_source_name(const char *filename, char name[256]) {
   const char *base = filename ? filename : "", *p;
   size_t i, length, stem;
   for (p = base; *p; p++)
      if (*p == '/' || *p == '\\')
         base = p + 1;
   length = strlen(base);
   if (!length || length > 240 || !strcmp(base, ".") || !strcmp(base, ".."))
      base = "source";
   strcpy(name, base);
   length = strlen(name);
   for (i = 0; i < length; i++)
      if ((unsigned char)name[i] < 32 || strchr("<>:\"/\\|?*", name[i]))
         name[i] = '_';
   for (i = length; i && (name[i - 1] == ' ' || name[i - 1] == '.'); i--)
      name[i - 1] = '_';
   stem = strcspn(name, ".");
   if (stem == 3 || stem == 4) {
      char device[5] = {0};
      for (i = 0; i < stem; i++) device[i] = (char)toupper((unsigned char)name[i]);
      if (!strcmp(device, "CON") || !strcmp(device, "PRN") || !strcmp(device, "AUX") ||
          !strcmp(device, "NUL") || (stem == 4 && device[3] >= '1' && device[3] <= '9' &&
          (!strncmp(device, "COM", 3) || !strncmp(device, "LPT", 3)))) {
         memmove(name + 1, name, length + 1);
         name[0] = '_';
      }
   }
}
#endif
