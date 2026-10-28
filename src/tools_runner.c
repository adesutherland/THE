/* Isolated native RXVML process. stdout/stderr belong exclusively to its job. */
#include "toolrequest.h"
#include <errno.h>
#include <limits.h>
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#include <windows.h>
#define chdir _chdir
#else
#include <sys/wait.h>
#include <unistd.h>
#endif
int the_vm_create(void);
int the_vm_load(const void *, size_t);
int the_vm_destroy(void);
int the_vm_run(int, const char **);
int the_vm_program_rc(void);
int the_vm_interpret(const char *, size_t, const char *, size_t, int, const char **);
const char *the_vm_error(void);
const char *the_vm_result(void);
static int load(const char *path) {
   FILE *f = fopen(path, "rb");
   long n;
   void *b;
   int rc;
   if (!f)
      return -1;
   fseek(f, 0, SEEK_END);
   n = ftell(f);
   rewind(f);
   if (n <= 0 || n > 67108864) {
      fclose(f);
      return -1;
   }
   b = malloc((size_t)n);
   if (!b) {
      fclose(f);
      return -1;
   }
   if (fread(b, 1, (size_t)n, f) != (size_t)n) {
      free(b);
      fclose(f);
      return -1;
   }
   fclose(f);
   rc = the_vm_load(b, (size_t)n);
   free(b);
   return rc;
}
static int spawn(char *const *args) {
#ifdef _WIN32
   int i, n;
   wchar_t **wide;
   intptr_t rc;
   for (n = 0; args[n]; n++)
      ;
   wide = calloc((size_t)n + 1, sizeof(*wide));
   if (!wide)
      return -1;
   for (i = 0; i < n; i++) {
      int k = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, args[i], -1, NULL, 0);
      wide[i] = k ? malloc((size_t)k * sizeof(wchar_t)) : NULL;
      if (!wide[i]) {
         while (i)
            free(wide[--i]);
         free(wide);
         return -1;
      }
      MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, args[i], -1, wide[i], k);
   }
   rc = _wspawnv(_P_WAIT, wide[0], (const wchar_t *const *)wide);
   for (i = 0; i < n; i++)
      free(wide[i]);
   free(wide);
   return (int)rc;
#else
   pid_t pid = fork();
   int status;
   if (pid < 0)
      return -1;
   if (!pid) {
      execv(args[0], args);
      _exit(127);
   }
   while (waitpid(pid, &status, 0) < 0) {
      if (errno != EINTR)
         return -1;
   }
   return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}
static int run(const char *directory) {
   char path[4096], magic[8], *image = NULL, *sdk = NULL, *filename = NULL, *source = NULL,
                              **args = NULL;
   size_t n, image_n, sdk_n, name_n, source_n;
   uint32_t mode, argc = 0, i;
   FILE *f;
   int rc = -1, status = 0;
   const char *error = "Invalid host request", *wire = "";
   snprintf(path, sizeof(path), "%s/request", directory);
   f = fopen(path, "rb");
   if (!f)
      return 125;
   if (fread(magic, 1, 8, f) != 8 || memcmp(magic, "THEJOB1\n", 8) || !tool_read_uint(f, &mode) ||
       mode > 3 || !tool_read_uint(f, &argc) || argc > 128)
      goto request_done;
   image = tool_read_bytes(f, &image_n);
   sdk = tool_read_bytes(f, &sdk_n);
   filename = tool_read_bytes(f, &name_n);
   source = tool_read_bytes(f, &source_n);
   if (!image || !sdk || !filename || !source || memchr(image, 0, image_n) ||
       memchr(sdk, 0, sdk_n) || memchr(filename, 0, name_n))
      goto request_done;
   args = calloc(argc ? argc : 1, sizeof(*args));
   if (!args)
      goto request_done;
   for (i = 0; i < argc; i++) {
      args[i] = tool_read_bytes(f, &n);
      if (!args[i] || memchr(args[i], 0, n))
         goto request_done;
   }
   if (fgetc(f) != EOF)
      goto request_done;
   if (chdir(directory)) {
      error = "Cannot enter private job directory";
      goto request_done;
   }
   if (mode == 2) {
      char compiler[4096], assembler[4096], src[4096], out[4096];
      char *compile_args[10], *assemble_args[5];
      FILE *input;
#ifdef _WIN32
      snprintf(compiler, sizeof(compiler), "%s/rxc.exe", sdk);
      snprintf(assembler, sizeof(assembler), "%s/rxas.exe", sdk);
#else
      snprintf(compiler, sizeof(compiler), "%s/rxc", sdk);
      snprintf(assembler, sizeof(assembler), "%s/rxas", sdk);
#endif
      snprintf(src, sizeof(src), "%s/profile.the", directory);
      snprintf(out, sizeof(out), "%s/profile", directory);
      input = fopen(src, "wb");
      if (!input) {
         error = "Cannot stage profile source";
         goto request_done;
      }
      if (fwrite(source, 1, source_n, input) != source_n) {
         fclose(input);
         goto request_done;
      }
      fclose(input);
      compile_args[0] = compiler;
      compile_args[1] = "--no-exe-import";
      compile_args[2] = "-i";
      compile_args[3] = sdk;
      compile_args[4] = "-o";
      compile_args[5] = out;
      compile_args[6] = src;
      compile_args[7] = NULL;
      status = spawn(compile_args);
      if (!status) {
         assemble_args[0] = assembler;
         assemble_args[1] = "-o";
         assemble_args[2] = out;
         assemble_args[3] = out;
         assemble_args[4] = NULL;
         status = spawn(assemble_args);
      }
      rc = 0;
      error = "";
   } else {
      if (the_vm_create()) {
         error = the_vm_error();
         goto request_done;
      }
      if (mode == 0) {
         snprintf(path, sizeof(path), "%s/library.rxbin", sdk);
         if (load(path)) {
            error = "Cannot load standard library";
            goto vm_done;
         }
      }
      if (load(image)) {
         error = the_vm_error();
         goto vm_done;
      }
      if (mode == 1) {
         rc = the_vm_interpret(source, source_n, filename, name_n, (int)argc, (const char **)args);
         wire = the_vm_result();
      } else {
         rc = the_vm_run((int)argc, (const char **)args);
         status = the_vm_program_rc();
      }
      error = rc ? the_vm_error() : "";
   vm_done:;
   }
request_done:
   fclose(f);
   snprintf(path, sizeof(path), "%s/result", directory);
   f = fopen(path, "wb");
   if (f) {
      fprintf(f, "THEJOB1\n%d\n%d\n", rc, status);
      tool_write_bytes(f, error, strlen(error));
      tool_write_bytes(f, wire, strlen(wire));
      fclose(f);
   }
   if (image)
      the_vm_destroy();
   for (i = 0; i < argc; i++)
      if (args)
         free(args[i]);
   free(args);
   free(image);
   free(sdk);
   free(filename);
   free(source);
   fflush(stdout);
   fflush(stderr);
   return 0;
}
static int service(const char *image) {
   const char *args[] = {"--stdio"};
   int rc;
   if (the_vm_create() || load(image))
      return 125;
   rc = the_vm_run(1, args);
   the_vm_destroy();
   return rc;
}
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
   int n, rc;
   char *s;
   if (argc != 2 && !(argc == 3 && !wcscmp(argv[1], L"--service")))
      return 2;
   n = WideCharToMultiByte(CP_UTF8, 0, argv[argc - 1], -1, NULL, 0, NULL, NULL);
   s = malloc(n);
   if (!s)
      return 125;
   WideCharToMultiByte(CP_UTF8, 0, argv[argc - 1], -1, s, n, NULL, NULL);
   rc = argc == 3 ? service(s) : run(s);
   free(s);
   return rc;
}
#else
int main(int argc, char **argv) {
   if (argc == 3 && !strcmp(argv[1], "--service"))
      return service(argv[2]);
   return argc == 2 ? run(argv[1]) : 2;
}
#endif
