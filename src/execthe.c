/*
 * Used to run different UI versions of THE, either explicitly or set a default
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

//#define _WIN32_WINNT 0x0501
#if defined(_WIN32) || defined(_WIN64)
# include <windows.h>
# include <io.h>
# include <time.h>
# include <userenv.h>
# include <malloc.h>
# define setenv(a,b,c) SetEnvironmentVariable(a,b)
#else
# include <unistd.h>
# include <limits.h>
# include <time.h>
# include <utime.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef __APPLE__
# include <mach-o/dyld.h>
#endif
#include <stdio.h>
#include <sys/stat.h>

#ifndef F_OK
# define F_OK 0
#endif

#if defined(_WIN32) || defined(_WIN64) || defined(__OS2__) || defined(__MSDOS__) || defined(MSDOS)
# define THE_PATH_SEP '\\'
# define THE_DIR_SEP ';'
#else
# define THE_PATH_SEP '/'
# define THE_DIR_SEP ':'
#endif

#define THE_DEFAULT -1
#define THE_GUI     0
#define THE_SDL     1
#define THE_CONSOLE 2

#if defined(__MSDOS__) || defined(__OS2__)
# include "thever.h"
#endif

char *the_version = THE_VERSION;
char *the_release = THE_VERSION_DATE;
char *the_copyright = "Copyright 1991-2026 Mark Hessling";

struct the_variant_t
{
   char *variant_name;
   char *variant_desc;
   size_t marker_file_size;
   int check_x;
   int is_console;
   int call_fork;
   int does_binary_exist;
};
typedef struct the_variant_t THE_VARIANT_T;

/*
 * The following table lists all variants of THE executables
 * When using PDCursesMod or ncurses up to 3 variants of each UI is possible:
 * - variant[w[8]]
 *   where:
 *      variant is UI; eg con, sdl2, gui
 *      w; WIDE_CHAR, but NO UTF8, but includes extra modifiers like A_OVERWRITE
 *      w8; WIDE_CHAR and UTF8, UTF8 and WIDE_CHAR support and extra modifiers like A_OVERWRITE
 *          w8 is work in progress until THE supports UTF8
 */
#if defined(_WIN32) || defined(_WIN64)
#define NUM_VARIANTS 8
#define THE_VARIANT_EXT ".exe"
THE_VARIANT_T variants[NUM_VARIANTS] =
{
   { "guiw",  "GUI with WIDE_CHAR support NO UTF8",0,0,0,0,0 },
   { "gui",  "GUI NO WIDE_CHAR or UTF8 support",0,0,0,0,0 },
   { "sdl2w", "SDLv2 with WIDE_CHAR support NO UTF8",0,0,0,0,0 },
   { "sdl2", "SDLv2 NO WIDE_CHAR or UTF8 support",0,0,0,0,0 },
   { "sdl1w", "SDLv1 with WIDE_CHAR support NO UTF8",0,0,0,0,0 },
   { "sdl1", "SDLv1 NO WIDE_CHAR or UTF8 support",0,0,0,0,0 },
   { "conw",  "console with WIDE_CHAR support NO UTF8",0,0,1,0,0 },
   { "con",  "console NO WIDE_CHAR or UTF8 support",0,0,1,0,0 }
};
#elif defined(OS2) || defined(EMX)
#define NUM_VARIANTS 6
#define THE_VARIANT_EXT ".exe"
THE_VARIANT_T variants[NUM_VARIANTS] =
{
   { "sdl2w","SDLv2 with WIDE_CHAR support",0,0,0,0,0 },
   { "sdl2", "SDLv2 NO UTF8 support",0,0,0,0,0 },
   { "sdl1w","SDLv1 with WIDE_CHAR support",0,0,0,0,0 },
   { "sdl1", "SDLv1 NO UTF8 support",0,0,0,0,0 },
   { "conw",  "console with WIDE_CHAR support NO UTF8",0,0,1,0,0 },
   { "con",  "console NO UTF8 support",0,0,1,0,0 }
};
#else
#define NUM_VARIANTS 10
#define THE_VARIANT_EXT ""
THE_VARIANT_T variants[NUM_VARIANTS] =
{
   { "x11w",   "Native X11 with WIDE_CHAR support NO UTF8",0,1,0,1,0        },
   { "x11",    "Native X11 NO WIDE_CHAR or UTF8 support",0,1,0,1,0         },
   { "sdl2w","SDLv2 with WIDE_CHAR support NO UTF8",0,0,0,1,0       },
   { "sdl2", "SDLv2 NO WIDE_CHAR or UTF8 support",0,0,0,1,0        },
   { "sdl1w","SDLv1 X11 with WIDE_CHAR support NO UTF8",0,0,0,1,0       },
   { "sdl1", "SDLv1 NO WIDE_CHAR or UTF8 support",0,0,0,1,0        },
   { "conw", "ncurses(console) with WIDE_CHAR support NO UTF8",0,0,1,0,0        },
   { "con",  "ncurses(console) NO WIDE_CHAR or UTF8 support",0,0,1,0,0         },
   { "vt",  "Virtual Terminal NO WIDE_CHAR or UTF8 support",0,0,1,0,0         },
   { "vtw",  "Virtual Terminal with WIDE_CHAR support NO UTF8",0,0,1,0,0         }
};
#endif

#ifdef __APPLE__
char *terms[] =
{
   "Terminal.app",
   "iTerm.app"
};
/*
 * homebrew on MacOS installs each package in its own Cellar.
 * The "the" package which contains this binary is installed in $HOMEBREW_CELLAR/the/[ver]/bin
 * The "the-sdl2w" package is installed in $HOMEBREW_CELLAR/the-sdl2w/[ver]/bin
 * There are symbolic links created for "the" and "the-sdl2w" binaries in $HOMEBREW_CELLAR/bin to the
 * files in each Cellar.  When we get thi sbinary's argv[0] it points to the installed directory not the
 * directory containing the symbolic links, so we can't use the normal mechanism to find our variants.
 * Instead we need to break down argv[0] to $HOMEBREW_CELLAR...
 */
char base_prog[PATH_MAX+1];
int brew_check = 0;
#endif

#if !defined(PATH_MAX)
/* if you don't use PATH_MAX (by #including <limits.h>
 * you will get a crash in realpath!
 */
# define PATH_MAX 2048
#endif
char prog[PATH_MAX+1];

char **my_argv;
int my_argc;

int newConsole = 0;

#ifdef __APPLE__
char *termProgram = NULL;
#endif

#if defined(_WIN32) || defined(_WIN64)
void StartupConsole( void )
{
   /*
    * the.exe is a GUI process that can be called from a parent
    * process that has a Console (such as running the.exe from an
    * open command prompt), or from Explorer or Run... in which
    * case the parent does not have a Console.
    * Attempt to attach to the Console of the parent process. If that
    * fails, we already have a Console, so use it. Otherwise, create
    * a Console to use.
    * MHES - 22/2/2013 - we don't attempt this anymore. Firstly it doesn't
    * appear to work, and secondly it precludes THE from running on Win2000
   if ( AttachConsole( ATTACH_PARENT_PROCESS ) == 0 )
    */
   {
      /*
       * We failed to attach to the parent's console, so we need
       * to create our own.
       */
      AllocConsole();
      newConsole = 1;
      freopen( "conin$", "r", stdin );
      freopen( "conout$", "w", stdout );
      freopen( "conout$", "w", stderr );
   }
}

void ClosedownConsole( void )
{
   if ( newConsole )
   {
      fprintf( stderr, "\n==> Press ENTER key to close this window. <==" );
      getchar();
      fclose(stdin);
      fclose(stdout);
      fclose(stderr);
      FreeConsole();
   }
}

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
   int rc;
   rc = main( __argc, __argv );
   return rc;
}
#endif

#ifdef __APPLE__
int GetTerminal( void )
{
   if ( termProgram == NULL )
      return 0;
   if ( strcmp( termProgram, "iTerm.app" ) == 0 )
      return 1;
   /* default is Terminal.app */
   return 0;
}
#endif

void ShowMessage( char *msg )
{
#ifdef __APPLE__X
   char *buf,*app;
   if ( getenv( "THE_RUNNING_AS_APP" ) )
      app = "The Hessling Editor";
   else
      app = terms[GetTerminal()];
   buf = alloca( strlen(msg) + strlen(app) + 200 );
   sprintf( buf, "osascript -e 'tell app \"%s\" to display alert \"THE\" message \"%s\" as warning buttons {\"Ok\"}' > /dev/null 2>&1", app, msg );
   system( buf );
#else
# if defined(_WIN32) || defined(_WIN64)
   StartupConsole();
# endif
   fprintf( stderr, "%s", msg );
# if defined(_WIN32) || defined(_WIN64)
   ClosedownConsole();
# endif
#endif
}

int CheckX( int i )
{
   if ( getenv( "DISPLAY" ) == NULL && variants[i].check_x == 1 )
      return 1;
   return 0;
}

int CheckAVariant( char *prog, int i )
{
   char tmp[PATH_MAX+500];
#ifdef __APPLE__
   if ( brew_check )
   {
      sprintf( base_prog, "%s/../../../the-%s/%s/bin/the-%s", prog, variants[i].variant_name, the_version, variants[i].variant_name );
      if ( realpath( base_prog, tmp ) == NULL )
      {
         return -1;
      }
   }
   else
#endif
   {
      sprintf( tmp, "%sthe-%s%s", prog, variants[i].variant_name, THE_VARIANT_EXT );
   }
   if ( access( tmp, F_OK ) == 0 )
   {
      return i;
   }
   return -1;
}

int CheckVariant( char *prog, char *var )
{
   int i;
   char tmp[PATH_MAX+500];
   for ( i = 0; i < NUM_VARIANTS; i++ )
   {
      if ( strcmp( var, variants[i].variant_name ) == 0 )
      {
         if ( CheckAVariant( prog, i ) != -1 )
         {
            return i;
         }
         else
         {
            sprintf( tmp, "Specified THE variant: %s is not available\n", var );
            ShowMessage( tmp );
            return -1;
         }
      }
   }
   sprintf( tmp, "Unknown THE variant: %s\n", var );
   ShowMessage( tmp );
   return -1;
}

void usage(char *prog, char *home)
{
   int i;
   char tmp[2048];
   char var[1024];

   strcpy(var,"The following variants are available:\n");
   for ( i = 0; i < NUM_VARIANTS; i++ )
   {
      if ( CheckAVariant( prog, i ) != -1 )
      {
         sprintf( tmp, " - %s - %s\n", variants[i].variant_name, variants[i].variant_desc );
         strcat( var, tmp );
      }
   }
   sprintf(tmp,"\nTHE %s %s %s. All rights reserved.\n"
               "THE is distributed under the terms of the GNU General Public \n"
               "License and comes with NO WARRANTY.\nSee the file COPYING for details.\n"
               "\nUsage:\n\nthe [-h] [-Svar|--set-variant=var] [-Rvar|--run-variant=var] [THE arguments] [file [...]]]\n"
               "\nwhere:\n\n"
               "-h                        show this message\n"
               "-Rvar | --run-variant=var override the default mode and run the specified variant of THE\n"
               "-Svar | --set-variant=var set the variant of THE to run if no specific variant is set via -R or --run-variant switch\n\n%s\n"
               "\nTHE variant configuration files are located in: %s\n",
               the_version, the_release, the_copyright, var, home);
   ShowMessage( tmp );
}

/*
 * GetArgv0 tries to find the fully qualified filename of the current program.
 * It uses some ugly and system specific tricks and it may return NULL if
 * it can't find any useful value.
 * The argument should be argv[0] of main() and it may be returned.
 * This function must not be called from another as the sole one when starting
 * up.
 */
static char *GetArgv0(char *argv0)
{
#if defined(_WIN32)
   char buf[1024];

   if (GetModuleFileName(NULL, buf, sizeof(buf)) != 0)
      return(strdup(buf)); /* never freed up */
#elif defined(__QNX__) && defined(__WATCOMC__)
   char buffer[PATH_MAX];
   char *buf;
   if ( (buf = _cmdname(buffer) ) != NULL )
      return(strdup(buf)); /* never freed up */
#elif defined(OS2)
   char buf[512];
   PPIB ppib;

# ifdef __EMX__
   if (_osmode == OS2_MODE)
   {
# endif
      if (DosGetInfoBlocks(NULL, &ppib) == 0)
         if (DosQueryModuleName(ppib->pib_hmte, sizeof(buf), buf) == 0)
            return(strdup(buf));
# ifdef __EMX__
   }
# endif
#endif

#ifdef __APPLE__
   {
      /*
       * will work on MacOS X
       */
      char buf[1024];
      uint32_t result=sizeof(buf);
      if ( _NSGetExecutablePath( buf, &result ) == 0 )
      {
         return strdup( buf );
      }
   }
#else
# if defined(HAVE_READLINK)
   {
      /*
       * will work on Linux 2.1+
       */
      char buf[1024];
      int result;
      result = readlink("/proc/self/exe", buf, sizeof( buf ) );
      if ( ( result > 0 ) && ( result < (int) sizeof( buf ) ) && ( buf[0] != '[' ) )
      {
         buf[result] = '\0';
         return strdup( buf );
      }
   }
# endif
# if defined(HAVE_REALPATH)
   char *prog = realpath( argv0, NULL );
   if ( prog )
   {
      return prog;
   }
# endif
   /*
    * As a last resort we test if the argv0 has no directory delimiter. Then loop through
    * the PATH env variable and test if the file exists in that directory. We then assume if
    * the file exists, then that is where it was launched from.
    */
   if ( strchr( argv0, THE_PATH_SEP ) == NULL )
   {
      int argv0_len = strlen(argv0);
      char *env = getenv( "PATH" );
      if ( env )
      {
         char *myenv = strdup( env );
         if ( myenv )
         {
            char *ptr = myenv;
            char *segment;
            int envlen,i,len = strlen( myenv );
            struct stat sb;
            for ( i = 0; i <= len; i++ )
            {
               if ( myenv[i] == THE_DIR_SEP
               ||   myenv[i] == '\0' ) /* end of string */
               {
                  myenv[i] = '\0';
                  envlen = strlen(ptr);
                  segment = malloc( envlen + 2 + argv0_len );
                  if ( segment )
                  {
                     strcpy( segment, ptr );
                     if ( segment[envlen-1] != THE_PATH_SEP )
                     {
                        segment[envlen] = THE_PATH_SEP;
                        segment[envlen+1] = '\0';
                     }
                     strcat( segment, argv0 );
                     if ( stat( segment, &sb) == 0 )
                     {
                        if ( (sb.st_mode & S_IFMT) == S_IFREG)
                        {
                           /*
                            * We should do more here like checking if the file is exectable (and we are the owner)
                            * or if the file is group executable (and we have the same gid), or other bit is executable
                            * but this will do for now
                            */
                           return segment;
                        }
                     }
                  }
                  /* get the next segment */
                  if ( i != len )
                  {
                     ptr = myenv + i + 1;
                  }
               }
            }
            free( myenv );
         }
      }
   }
#endif
   /* No specific code has found the right file name. Maybe, it's coded
    * in argv0. Check it, if it is an absolute path. Be absolutely sure
    * to detect it safely!
    */
   if (argv0 == NULL)
      return(NULL);

   if (argv0[0] == '/') /* unix systems and some others */
      return(argv0);

   if ((argv0[0] == '\\') && (argv0[1] == '\\')) /* MS and OS/2 UNC names */
      return(argv0);

   if (isalpha(argv0[0]) && (argv0[1] == ':') && (argv0[2] == '\\'))
      return(argv0); /* MS and OS/2 drive letter with path */

   return(NULL); /* not a proven argv0 argument */
}

/*
 * Determines where the special marker files .thegui and .theconsole are read/written
 */
char *GetHomeDirectory()
{
   static char user_dir[1024] = "";
#if defined(_WIN32) || defined(_WIN64)
   HANDLE hToken = 0;

   /* Use the Windows API to get the user's profile directory */
   if ( OpenProcessToken( GetCurrentProcess(), TOKEN_QUERY, &hToken ) )
   {
      DWORD BufSize = 1024;

      GetUserProfileDirectory(hToken, user_dir, &BufSize);
      CloseHandle(hToken);
      strcat( user_dir, "\\" );
   }
   /* If it fails set it to the root directory */
   if(!user_dir[0])
   {
      strcpy(user_dir, "C:\\");
   }
#else
   strcpy( user_dir, getenv( "HOME" ) );
   strcat( user_dir, "/" );
#endif
   return user_dir;
}

int CreateVariantFile( char *homedir, char *var, int idx )
{
   char buf[PATH_MAX+1];
   int fd;

   sprintf( buf, "%s.the-variant", homedir );
   if ( access( buf, F_OK ) != 0 )
   {
      /* file doesn't exists; create it */
#if defined(_WIN32) || defined(_WIN64)
      fd = open( buf, O_CREAT, _S_IWRITE);
#else
      fd = open( buf, O_CREAT|O_NOCTTY, S_IRUSR|S_IWUSR ); /* read/write user */
#endif
      close( fd );
   }
   /* file exists; change mtime to variant index */
#if defined(_WIN32) || defined(_WIN64)
   HANDLE hFile = CreateFile(buf, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
   FILETIME ft;
   SYSTEMTIME st;
   BOOL f;

   GetSystemTime(&st);              // Gets the current system time
   st.wHour = st.wMinute = st.wSecond = idx; // Set time to variant index
   SystemTimeToFileTime(&st, &ft);  // Converts the current system time to file time format
   f = SetFileTime(hFile,              // Sets last-write time of the file
                   (LPFILETIME) NULL,  // to the converted current system time
                   (LPFILETIME) NULL,
                   &ft);
#else
   struct stat foo;
   struct utimbuf new_times;
   time_t secs = time( NULL );
   struct tm nowdata, *nowptr;

   nowptr = localtime( &secs );
   if ( nowptr == NULL )
   {
      sprintf( buf, "Failed to set default THE binary: the-%s (%s)\n", var, variants[idx].variant_desc );
      ShowMessage( buf );
      return 1;
   }
   nowdata = *nowptr;
   nowdata.tm_hour = nowdata.tm_min = nowdata.tm_sec = idx;
   secs = mktime( &nowdata );
   stat(buf, &foo);

   new_times.actime = foo.st_atime; /* keep atime unchanged */
   new_times.modtime = secs;    /* set mtime to today with time set to variant index */
   utime(buf, &new_times);
#endif

   sprintf( buf, "Successfully set default THE binary: the-%s (%s)\n", var, variants[idx].variant_desc );
   ShowMessage( buf );
   return 0;
}

int CheckVariantFile( char *homedir )
{
   char buf[PATH_MAX+1];
   struct stat attr;
   struct tm *nowptr;

   sprintf( buf, "%s.the-variant", homedir );
   if ( stat(buf, &attr) == 0 )
   {
      /* get hour from st_mtime */
      nowptr = localtime( &attr.st_mtime );
      if ( nowptr == NULL )
      {
         return -1;
      }

      return nowptr->tm_min;
   }
   return -1;
}

int ReadMarkerFileAndSetEnvVariables( char *homedir, int i )
{
   char buf[PATH_MAX+1];
   FILE *fp;
   char *contents = NULL;
   int j,len;
   struct stat attr;

   sprintf( buf, "%s.the-%s", homedir, variants[i].variant_name );
   fp = fopen( buf, "r" );
   if ( fp )
   {
      if ( stat(buf, &attr) == 0 )
      {
         variants[i].marker_file_size = attr.st_size;
      }
      contents = malloc( variants[i].marker_file_size + 1 );
      if ( contents
      &&   variants[i].marker_file_size > 0 )
      {
         char *line;
         fread( contents, 1, variants[i].marker_file_size, fp );
         line = contents;
         /* get each line and set the env variable */
         for ( j = 0; j < variants[i].marker_file_size; j++ )
         {
            if ( contents[j] == '\r'
            ||  contents[j] == '\n' )
            {
               contents[j] = '\0';
               if ( strlen( line ) > 0 )
               {
                  char *var = line;
                  char *value=NULL;
                  int k;
                  len = strlen( line );
                  for ( k = 0; k < len; k++ )
                  {
                     if ( line[k] == '=' )
                     {
                        line[k] = '\0';
                        value = line+k+1;
                        break;
                     }
                  }
                  /* ignore comment lines starting with # or * */
                  if ( value != NULL )
                  {
                     if ( value[0] != '*'
                     &&   value[0] != '#' )
                     {
                        setenv( var, value, 1 );
                     }
                  }
                  else
                  {
                     fprintf(stderr,"Invalid format for setting in %s: Line: %s\n",buf, contents );
                  }
               }
               line = contents + j + 1;
            }
         }
         if ( strlen( line ) > 0 )
         {
            char *var = line;
            char *value=NULL;
            int k;
            int len = strlen( line );
            for ( k = 0; k < len; k++ )
            {
               if ( line[k] == '=' )
               {
                  line[k] = '\0';
                  value = line+k+1;
                  break;
               }
            }
            if ( value[0] != '*'
            &&   value[0] != '#' )
            {
               setenv( var, value, 1 );
            }
         }
         free( contents );
      }
      fclose( fp );
   }
   return 0;
}

#ifdef __APPLE1__
int CheckX11Installed( int terminate, char *msg )
{
   char buf[1024];
   char *app;

   if ( access( "/Applications/Utilities/X11.app", F_OK ) == 0 )
      return 1;
   if ( terminate )
   {
      if ( getenv( "THE_RUNNING_AS_APP" ) )
         app = "The Hessling Editor";
      else
         app = "Terminal";
      sprintf( buf, "osascript -e 'tell app \"%s\" to display alert \"X11 is not installed\" message \"%s\" as warning buttons {\"Ok\"}' > /dev/null 2>&1", app, msg );
      system( buf );
      exit( 1 );
   }
   return 0;
}
#endif

#ifdef __APPLE__
int SetMacroPath( char *runtimedir )
{
   char buf[1024];
   int len = strlen( runtimedir );

   if ( getenv( "THE_RUNNING_AS_APP" ) )
   {
      strncpy( buf, runtimedir, len-6 );
      buf[len-6] = '\0';
      strcat( buf, "Extras" );
      setenv( "THE_MACRO_PATH", buf, 1 );
   }
   return 0;
}
#endif

int ExecuteBinary( char **argv, char *home, char *prog, int idx )
{
//   char program[PATH_MAX+1];
   int rc;
   /* we can now read our marker file and set specified env variables */
   ReadMarkerFileAndSetEnvVariables( home, idx );

   /* now run our actual executable if we have found one */
#ifdef HAVE_FORK
   if ( variants[idx].call_fork )
   {
      int pid = fork();
      if (pid < 0)
      {
         /* Could not fork */
         fprintf( stderr, "Unable to create child process\n" );
         exit(1);
      }
      else if (pid > 0)
      {
         /* Child created ok, so exit parent process */
         exit(0);
      }
      /* child continues */
   }
#endif
   if ( variants[idx].is_console )
   {
#ifdef __APPLE__
      char prog1[PATH_MAX+1];
#endif
//      sprintf(program, "%sthe-%s", prog, variants[idx].variant_name );
//      argv[0] = prog;
#ifdef __APPLE__
      /* if running as a bundled app, we need to startup nthe via Terminal */
      if ( getenv( "THE_RUNNING_AS_APP" ) )
      {
         strcpy( prog1, "open -n -W -b com.apple.Terminal \"" );
         strcat( prog1, prog );
         strcat( prog1, "\" &" );
         rc = system( prog1 );
         return rc;
      }
#endif
#if defined(_WIN32) || defined(_WIN64)
      StartupConsole();
#endif
   }
   rc = execv( prog, argv );
   return rc;
}

static void setVariant( char *var, char *prog, char *home )
{
   int len = strlen( var );
   int idx;
   char *tmp;
   if ( len == 0 )
   {
      ShowMessage( "No variant specified with -S or --set-variant= switch\n" );
      exit( 1 );
   }
   tmp = alloca( strlen(var) + 1 );
   strcpy( tmp, var );
   idx = CheckVariant( prog, tmp );
   if ( idx != -1 )
   {
      CreateVariantFile( home, tmp, idx );
   }
}

#if defined(_WIN32) || defined(_WIN64) || defined(__OS2__) || defined(__MSDOS__) || defined(MSDOS)
char *quoteArgWithSpaces( char *the_arg )
{
   char *tmp;
   if ( strchr( the_arg, ' ' ) != NULL )
   {
      /* known memory leak! */
      tmp = malloc( 2 + strlen( the_arg ) );
      if ( tmp )
      {
         sprintf( tmp, "\"%s\"", the_arg );
         return tmp;
      }
      else
         /* multiple args better than nothing */
         return the_arg;
   }
   return the_arg;
}
#endif

int main( int argc, char *argv[] )
{
   char *argv0 = NULL;
#ifdef __APPLE__
   char *brew_cellar = NULL;
#endif
   char *home = NULL;
   int i;
   int len;
   int rc=0;
   int path_len = 0;
   int ignore_arg1 = 0;
   int var_idx = -1;
   char the_binary[PATH_MAX+500];
   /* get our home directory */
   home = GetHomeDirectory();
#ifdef __APPLE__
   char *termProgram = getenv("TERM_PROGRAM");
   /* fudge the X11 requirements for sdl* variants */
   for ( i = 0; i < NUM_VARIANTS; i++ )
   {
      if ( strncmp( variants[i].variant_name, "sdl", 3 ) == 0
      &&   termProgram != NULL )
      {
         variants[i].check_x = 0;
      }
   }
#endif
   /* get our current runtime directory */
   argv0 = GetArgv0( argv[0] );
   if ( argv0 == NULL )
      exit(1);
#ifdef HAVE_REALPATH
   realpath( argv0, prog );
#else
   strcpy( prog, argv0 );
#endif
   /* remove the.exe from the full filename */
   len = strlen( prog );
   for ( i = len; i > -1; i--)
   {
      if ( prog[i] == THE_PATH_SEP )
         break;
   }
   path_len = i + 1;
   prog[path_len] = '\0'; /* this is actually the full path to me, excluding program name */
#ifdef __APPLE__
   /*
    * Are we running from a Brew install?
    */
    brew_cellar = getenv( "HOMEBREW_CELLAR" );
    if ( brew_cellar )
    {
       if ( strncmp( brew_cellar, prog, strlen( brew_cellar ) ) == 0 )
       {
          brew_check = 1;
       }
    }
    else
    {
       /* no HOMEBREW_CELLAR set; we still might be running from HOMEBREW_CELLAR...*/
       if ( strncmp( prog, "/usr/local/Cellar", 17 ) == 0 )
       {
          brew_check = 1;
       }
       else
       {
          if ( strncmp( prog, "/opt/homebrew/Cellar", 20 ) == 0 )
          {
             brew_check = 1;
          }
       }
    }
#endif

   /* if displaying version do it and exit; don't run a variant */
   if ( ( argc == 2 && strlen( argv[1] ) == 2 && strcmp( argv[1], "-v" ) == 0 ) )
   {
      printf( "THE %s %s %s. All rights reserved.\n", the_version, the_release, the_copyright );
      exit(0);
   }

   /* if setting a variant, do it and exit */
   if ( argc == 2 && strlen( argv[1] ) >= 14 && strncmp( argv[1], "--set-variant=", 14 ) == 0 )
   {
      setVariant( argv[1]+14, prog, home );
      exit(0);
   }
   if ( argc == 2 && strlen( argv[1] ) >= 2 && strncmp( argv[1], "-S", 2 ) == 0 )
   {
      setVariant( argv[1]+2, prog, home );
      exit(0);
   }
   /* we will now try and run a variant */
   /* see if we are explicitly requesting a variant first */
   if ( argc > 1 )
   {
      char tmp[1024];
      if ( ( strlen( argv[1] ) >= 2 && strncmp( argv[1], "-R", 2 ) == 0 )
        || ( strlen( argv[1] ) >= 14 && strncmp( argv[1], "--run-variant=", 14 ) == 0 ) )
      {
         if ( strlen( argv[1] ) >= 14 )
         {
            strcpy( tmp, argv[1]+14 );
         }
         else
         {
            strcpy( tmp, argv[1]+2 );
         }
         ignore_arg1 = 1;
         var_idx = CheckVariant( prog, tmp );
         if ( var_idx == -1 )
         {
            /* need to abort here */
            exit( 1 );
         }
         if ( CheckX( var_idx ) == 1 )
         {
            ShowMessage( "The specified variant requires the X11 environment; DISPLAY environment variable is NOT set\n");
            /* need to abort here */
            exit( 1 );
         }
//         CheckMarkerFile( home, var_idx );
      }
   }
   if ( var_idx == -1 )
   {
      /* see if we have a default variant set; .the-variant file exists */
      var_idx = CheckVariantFile( home );
      if ( var_idx != -1 )
      {
         /* we have specified a default, but do we have a binary for it? */
         if ( CheckAVariant( prog, var_idx ) == -1 )
         {
            char tmp[1024];
            sprintf( tmp, "The default variant binary; the-%s is not available\n", variants[var_idx].variant_name);
            ShowMessage( tmp );
            /* need to abort here */
            exit( 1 );
         }
      }
   }
   if ( var_idx == -1 )
   {
      /* find our best variant */
      int xenv_ok = 1;
      for ( i = 0; i < NUM_VARIANTS; i++ )
      {
         xenv_ok = 1;
         if ( CheckAVariant( prog, i ) != -1 )
         {
            variants[i].does_binary_exist = 1;
            if ( CheckX( i ) == 1 )
            {
               xenv_ok = 0;
            }
            if ( variants[i].does_binary_exist == 1 && xenv_ok == 1 )
            {
               var_idx = i;
               break;
            }
         }
      }
   }
   /* if help; show usage and call variant to display it's usage */
   if ( ( argc == 2 && strlen( argv[1] ) == 2 && strcmp( argv[1], "-h" ) == 0 ) )
   {
      usage(prog,home);
   }

   if ( var_idx == -1 )
   {
      ShowMessage( "Cannot find any suitable THE binary to run. Aborting!\n" );
      exit ( 1 );
      /* we have no installed variants; abort */
   }

   /* we have a binary to run; run it */
#ifdef __APPLE__
//   SetMacroPath( prog );
#endif

#ifdef __APPLE__
   if ( brew_check )
   {
      sprintf( base_prog, "%s/../../../the-%s/%s/bin/the-%s", prog, variants[var_idx].variant_name, the_version, variants[var_idx].variant_name );
      if ( realpath( base_prog, the_binary ) == NULL )
      {
         return -1;
      }
   }
   else
#endif
   {
      /* we have to set the full path to the binary so we can use this in argv[0] for Windows and we can quote it if it has embedded spaces */
      sprintf( the_binary, "%sthe-%s%s", prog, variants[var_idx].variant_name, THE_VARIANT_EXT );
   }

   my_argv = (char **)malloc( sizeof(char *) * (argc+1) );
   my_argc = 0;
   for ( i = 0; i < argc; i++ )
   {
      /*
       * We have an arg to pass to the spawned process. If the arg has a space in it, the
       * user MUST have quoted it originally.  Windows (of course) doesn't work like Unix
       * and execv() actually concats all the args into one string to pass to CreateProcess().
       * This results in multiple args for a single arg that has spaces.
       * We have to work around this by appending quotes to args that have spaces for Windows.
       * And crucially argv[0] so we can run from a directory with spaces in it
       */
      if ( !(i == 1 && ignore_arg1) )
      {
#if defined(_WIN32) || defined(_WIN64) || defined(__OS2__) || defined(__MSDOS__) || defined(MSDOS)
         if ( i == 0 )
            my_argv[my_argc++] = quoteArgWithSpaces( the_binary );
         else
            my_argv[my_argc++] = quoteArgWithSpaces( argv[i] );
#else
         my_argv[my_argc++] = argv[i];
#endif
      }
   }
   /* ensure the argv array is NULL terminated */
   my_argv[my_argc] = NULL;
   /* we can now read our marker file and set specified env variables */
   rc = ExecuteBinary( my_argv, home, the_binary, var_idx );
   exit ( rc );
}
