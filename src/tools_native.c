/* Native resources. Only the editor owner updates THE's session model. */
#include "tools.h"
#ifndef __EMSCRIPTEN__
#include "the.h"
#include "proto.h"
#include "toolrequest.h"
#include <errno.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#define unlink _unlink
#define rmdir _rmdir
#else
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#ifndef THE_NATIVE_TOOL_ROOT
#define THE_NATIVE_TOOL_ROOT ""
#endif
#define INPUT_LIMIT 16777216u
typedef struct {
   unsigned id, source_id, revision;
   int active, compiler, exited, eof_requested;
   char directory[THE_TOOL_PATH];
   char *input;
   size_t length, offset;
#ifdef _WIN32
   HANDLE process, group, writer, in, out, err;
   CRITICAL_SECTION lock;
   CONDITION_VARIABLE wake;
   volatile LONG closing, write_error;
#else
   int in, out, err;
   pid_t pid;
#endif
} NativeJob;
static NativeJob job;
static int initialized;
static char root[THE_TOOL_PATH];
static void join(char *out, size_t cap, const char *base, const char *name) {
   snprintf(out, cap, "%s/%s", base, name);
}
static int exists(const char *p) {
   FILE *f = fopen(p, "rb");
   if (!f)
      return 0;
   fclose(f);
   return 1;
}
static void remove_job_directory(const char *directory, int depth) {
   char path[THE_TOOL_PATH + 256];
   if (depth > 64)
      return;
#ifdef _WIN32
   WIN32_FIND_DATAA data;
   HANDLE find;
   snprintf(path, sizeof(path), "%s/*", directory);
   find = FindFirstFileA(path, &data);
   if (find == INVALID_HANDLE_VALUE)
      return;
   do {
      if (!strcmp(data.cFileName, ".") || !strcmp(data.cFileName, ".."))
         continue;
      snprintf(path, sizeof(path), "%s/%s", directory, data.cFileName);
      if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
          !(data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
         remove_job_directory(path, depth + 1);
      else if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
         RemoveDirectoryA(path);
      else
         DeleteFileA(path);
   } while (FindNextFileA(find, &data));
   FindClose(find);
   RemoveDirectoryA(directory);
#else
   DIR *d = opendir(directory);
   struct dirent *entry;
   struct stat info;
   if (!d)
      return;
   while ((entry = readdir(d))) {
      if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
         continue;
      if (snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name) >= (int)sizeof(path))
         continue;
      if (lstat(path, &info))
         continue;
      if (S_ISDIR(info.st_mode))
         remove_job_directory(path, depth + 1);
      else
         unlink(path);
   }
   closedir(d);
   rmdir(directory);
#endif
}
static void cleanup_files(void) {
   if (*job.directory)
      remove_job_directory(job.directory, 0);
   job.directory[0] = 0;
}

#ifdef _WIN32
static wchar_t *wide(const char *s) {
   int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, NULL, 0);
   wchar_t *w = n ? malloc((size_t)n * sizeof(*w)) : NULL;
   if (w)
      MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, w, n);
   return w;
}
static void close_handle(HANDLE *h) {
   if (*h) {
      CloseHandle(*h);
      *h = NULL;
   }
}
static DWORD WINAPI writer(void *unused) {
   (void)unused;
   for (;;) {
      char *b;
      size_t n, offset = 0;
      int eof;
      EnterCriticalSection(&job.lock);
      while (!job.length && !job.closing && !job.eof_requested)
         SleepConditionVariableCS(&job.wake, &job.lock, INFINITE);
      if (job.closing) {
         LeaveCriticalSection(&job.lock);
         break;
      }
      b = job.input;
      n = job.length;
      eof = job.eof_requested;
      job.input = NULL;
      job.length = 0;
      LeaveCriticalSection(&job.lock);
      while (n && !job.closing) {
         DWORD got = 0, k = n > 65536 ? 65536 : (DWORD)n;
         if (!WriteFile(job.in, b + offset, k, &got, NULL) || !got) {
            InterlockedExchange(&job.write_error, 1);
            break;
         }
         offset += got;
         n -= got;
      }
      free(b);
      if (job.write_error || (!n && eof))
         break;
   }
   close_handle(&job.in);
   return 0;
}
#else
static void close_fd(int *f) {
   if (*f >= 0) {
      close(*f);
      *f = -1;
   }
}
#endif
static void stop(unsigned id) {
   if (!job.active || job.id != id)
      return;
   job.active = 0;
#ifdef _WIN32
   InterlockedExchange(&job.closing, 1);
   WakeConditionVariable(&job.wake);
   if (job.group)
      TerminateJobObject(job.group, 125);
   else if (job.process)
      TerminateProcess(job.process, 125);
   if (job.writer) {
      CancelSynchronousIo(job.writer);
      WaitForSingleObject(job.writer, INFINITE);
      close_handle(&job.writer);
      DeleteCriticalSection(&job.lock);
   }
   close_handle(&job.process);
   close_handle(&job.group);
   close_handle(&job.in);
   close_handle(&job.out);
   close_handle(&job.err);
#else
   if (!job.exited) {
      kill(-job.pid, SIGKILL);
      while (waitpid(job.pid, NULL, 0) < 0 && errno == EINTR) {
      }
   }
   close_fd(&job.in);
   close_fd(&job.out);
   close_fd(&job.err);
#endif
   free(job.input);
   job.input = NULL;
   job.length = job.offset = 0;
   cleanup_files();
}
static int input(unsigned id, const void *b, size_t n) {
   char *p;
   int ok = 0;
   if (!job.active || job.id != id || job.eof_requested)
      return 0;
#ifdef _WIN32
   EnterCriticalSection(&job.lock);
#endif
   if (n <= INPUT_LIMIT - job.length) {
      if (job.offset) {
         memmove(job.input, job.input + job.offset, job.length);
         job.offset = 0;
      }
      p = realloc(job.input, job.length + n);
      if (p || !n) {
         job.input = p;
         if (n)
            memcpy(p + job.length, b, n);
         job.length += n;
         ok = 1;
      }
   }
#ifdef _WIN32
   WakeConditionVariable(&job.wake);
   LeaveCriticalSection(&job.lock);
#endif
   return ok;
}
static int eof(unsigned id) {
   if (!job.active || job.id != id)
      return 0;
   job.eof_requested = 1;
#ifdef _WIN32
   WakeConditionVariable(&job.wake);
#endif
   return 1;
}
static int save_profile(char *out, size_t cap) {
   char source[THE_TOOL_PATH + 32], base[THE_TOOL_PATH], *slash;
   FILE *in, *f;
   unsigned char b[65536];
   size_t n;
   int good = 1;
   snprintf(base, sizeof(base), "%s", the_tools_profile_path());
   slash = strrchr(base, '/');
   if (!slash)
      return 0;
   *slash = 0;
#ifdef _WIN32
   _mkdir(base);
#else
   mkdir(base, 0700);
#endif
   snprintf(out, cap, "%s/build-%s-%u.rxbin", base,
            strrchr(job.directory, '/') ? strrchr(job.directory, '/') + 1 : job.directory, job.id);
   join(source, sizeof(source), job.directory, "profile.rxbin");
   in = fopen(source, "rb");
   if (!in)
      return 0;
   if (fread(b, 1, 8, in) != 8 || memcmp(b, "cReXx007", 8)) {
      fclose(in);
      return 0;
   }
   rewind(in);
#ifdef _WIN32
   {
      int fd = _open(out, _O_CREAT | _O_EXCL | _O_WRONLY | _O_BINARY, _S_IREAD | _S_IWRITE);
      f = fd < 0 ? NULL : _fdopen(fd, "wb");
   }
#else
   {
      int fd = open(out, O_CREAT | O_EXCL | O_WRONLY, 0600);
      f = fd < 0 ? NULL : fdopen(fd, "wb");
   }
#endif
   if (!f) {
      fclose(in);
      return 0;
   }
   while ((n = fread(b, 1, sizeof(b), in)))
      if (fwrite(b, 1, n, f) != n) {
         good = 0;
         break;
      }
   if (ferror(in))
      good = 0;
   fclose(in);
   if (fclose(f))
      good = 0;
   if (!good)
      unlink(out);
   return good;
}
static void finish(void) {
   char path[THE_TOOL_PATH + 32], magic[32], number[32], profile[THE_TOOL_PATH], *error = NULL,
                                                                                 *wire = NULL;
   int rc = -1, status = 0;
   FILE *f;
   size_t n;
   unsigned id = job.id;
   join(path, sizeof(path), job.directory, "result");
   f = fopen(path, "rb");
   if (f) {
      if (fgets(magic, sizeof(magic), f) && !strcmp(magic, "THEJOB1\n") &&
          fgets(number, sizeof(number), f) && (rc = atoi(number), 1) &&
          fgets(number, sizeof(number), f) && (status = atoi(number), 1)) {
         error = tool_read_bytes(f, &n);
         wire = tool_read_bytes(f, &n);
      }
      fclose(f);
   }
   if (!error || !wire) {
      free(error);
      free(wire);
      wire = NULL;
      rc = -1;
      error = malloc(80);
      if (error)
         strcpy(error, "VM process exited without a complete result");
   }
   if (job.compiler && !rc && !status) {
      if (save_profile(profile, sizeof(profile)))
         the_tools_profile_built(id, profile, job.source_id, job.revision);
      else {
         rc = -1;
         free(error);
         error = malloc(80);
         if (error)
            strcpy(error, "Compiler did not produce a valid RXBIN");
      }
   }
   if (job.compiler && !rc && status) {
      rc = -1;
      free(error);
      error = malloc(80);
      if (error)
         strcpy(error, "Profile compilation failed");
   }
   {
      char number[32];
      snprintf(number, sizeof(number), "%d", status);
      the_tool_finish(id, rc, number, error, wire);
   }
   free(error);
   free(wire);
   stop(id);
}
static void poll(void) {
   unsigned char b[65536];
   if (!job.active)
      return;
#ifdef _WIN32
   HANDLE *streams[2] = {&job.out, &job.err};
   int i;
   DWORD status;
   for (i = 0; i < 2; i++)
      while (job.active && *streams[i]) {
         DWORD n = 0, got = 0;
         if (!PeekNamedPipe(*streams[i], NULL, 0, NULL, &n, NULL)) {
            close_handle(streams[i]);
            break;
         }
         if (!n)
            break;
         if (n > sizeof(b))
            n = sizeof(b);
         if (!ReadFile(*streams[i], b, n, &got, NULL) || !got) {
            close_handle(streams[i]);
            break;
         }
         the_tool_output(job.id, i + 1, b, got);
      }
   if (!job.active)
      return;
   if (GetExitCodeProcess(job.process, &status) && status != STILL_ACTIVE)
      job.exited = 1;
   if (job.exited) {
      for (i = 0; i < 2; i++)
         if (*streams[i]) {
            DWORD n = 0;
            if (PeekNamedPipe(*streams[i], NULL, 0, NULL, &n, NULL) && !n)
               close_handle(streams[i]);
         }
      if (!job.out && !job.err)
         finish();
   }
#else
   int *streams[2] = {&job.out, &job.err};
   int i, status;
   ssize_t n;
   if (job.in >= 0 && job.length) {
#ifdef MSG_NOSIGNAL
      n = send(job.in, job.input + job.offset, job.length, MSG_NOSIGNAL);
#else
      n = send(job.in, job.input + job.offset, job.length, 0);
#endif
      if (n > 0) {
         job.offset += (size_t)n;
         job.length -= (size_t)n;
         if (!job.length) {
            free(job.input);
            job.input = NULL;
            job.offset = 0;
         }
      } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
         close_fd(&job.in);
   }
   if (job.eof_requested && !job.length)
      close_fd(&job.in);
   for (i = 0; i < 2; i++)
      while (job.active && *streams[i] >= 0) {
         n = read(*streams[i], b, sizeof(b));
         if (n > 0)
            the_tool_output(job.id, i + 1, b, (size_t)n);
         else {
            if (!n || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR))
               close_fd(streams[i]);
            break;
         }
      }
   if (!job.active)
      return;
   if (!job.exited && waitpid(job.pid, &status, WNOHANG) == job.pid)
      job.exited = 1;
   if (job.exited && job.out < 0 && job.err < 0)
      finish();
#endif
}
static int start(const TheToolRequest *r, char *error, size_t cap) {
   char helper[THE_TOOL_PATH], image[THE_TOOL_PATH], sdk[THE_TOOL_PATH], path[THE_TOOL_PATH + 32];
   FILE *f;
   int mode, i, ok;
#ifdef _WIN32
   char tmp[MAX_PATH];
   wchar_t *command = NULL, *exe = NULL;
   STARTUPINFOW si = {0};
   PROCESS_INFORMATION pi = {0};
   SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
   HANDLE child_in = NULL, child_out = NULL, child_err = NULL;
   JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {0};
#else
   int ins[2] = {-1, -1}, outs[2] = {-1, -1}, errs[2] = {-1, -1};
   const char *tmp = getenv("TMPDIR");
#endif
   memset(&job, 0, sizeof(job));
   job.id = r->id;
   job.source_id = r->source_id;
   job.revision = r->revision;
   job.compiler = !strcmp(r->tool.role, "compiler");
   mode = job.compiler                       ? 2
          : !strcmp(r->tool.role, "runner")  ? 1
          : !strcmp(r->tool.entry, "linked") ? 3
                                             : 0;
   join(sdk, sizeof(sdk), root, "sdk");
   if (r->tool.image[0] == '@')
      join(image, sizeof(image), root,
           !strcmp(r->tool.image, "@classic") ? "llcbrowser.rxbin" : "dslsh-classic.rxbin");
   else
      snprintf(image, sizeof(image), "%s", r->tool.image);
#ifdef _WIN32
   GetTempPathA(sizeof(tmp), tmp);
   if (!GetTempFileNameA(tmp, "the", 0, job.directory))
      goto failed;
   DeleteFileA(job.directory);
   if (!CreateDirectoryA(job.directory, NULL))
      goto failed;
   join(helper, sizeof(helper), root, "the_rxbin_host.exe");
#else
   job.in = job.out = job.err = -1;
   snprintf(job.directory, sizeof(job.directory), "%s/crxthe-job-XXXXXX",
            tmp && *tmp ? tmp : "/tmp");
   if (!mkdtemp(job.directory))
      goto failed;
   join(helper, sizeof(helper), root, "the_rxbin_host");
#endif
   join(path, sizeof(path), job.directory, "request");
   f = fopen(path, "wb");
   if (!f)
      goto failed;
   ok = fwrite("THEJOB1\n", 1, 8, f) == 8 && tool_write_uint(f, (uint32_t)mode) &&
        tool_write_uint(f, (uint32_t)r->argc) && tool_write_bytes(f, image, strlen(image)) &&
        tool_write_bytes(f, sdk, strlen(sdk)) &&
        tool_write_bytes(f, r->filename, strlen(r->filename)) &&
        tool_write_bytes(f, r->source, r->source_length);
   for (i = 0; i < r->argc && ok; i++)
      ok = tool_write_bytes(f, r->argv[i], strlen(r->argv[i]));
   if (fclose(f))
      ok = 0;
   if (!ok)
      goto failed;
#ifdef _WIN32
   if (!CreatePipe(&child_in, &job.in, &sa, 65536) ||
       !CreatePipe(&job.out, &child_out, &sa, 65536) ||
       !CreatePipe(&job.err, &child_err, &sa, 65536))
      goto failed;
   SetHandleInformation(job.in, HANDLE_FLAG_INHERIT, 0);
   SetHandleInformation(job.out, HANDLE_FLAG_INHERIT, 0);
   SetHandleInformation(job.err, HANDLE_FLAG_INHERIT, 0);
   snprintf(path, sizeof(path), "\"%s\" \"%s\"", helper, job.directory);
   command = wide(path);
   exe = wide(helper);
   si.cb = sizeof(si);
   si.dwFlags = STARTF_USESTDHANDLES;
   si.hStdInput = child_in;
   si.hStdOutput = child_out;
   si.hStdError = child_err;
   if (!command || !exe ||
       !CreateProcessW(exe, command, NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, NULL,
                       NULL, &si, &pi))
      goto failed;
   free(command);
   free(exe);
   command = exe = NULL;
   close_handle(&child_in);
   close_handle(&child_out);
   close_handle(&child_err);
   job.process = pi.hProcess;
   job.group = CreateJobObjectW(NULL, NULL);
   limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
   if (!job.group ||
       !SetInformationJobObject(job.group, JobObjectExtendedLimitInformation, &limits,
                                sizeof(limits)) ||
       !AssignProcessToJobObject(job.group, job.process)) {
      TerminateProcess(job.process, 125);
      CloseHandle(pi.hThread);
      goto failed;
   }
   InitializeCriticalSection(&job.lock);
   InitializeConditionVariable(&job.wake);
   job.active = 1;
   job.writer = CreateThread(NULL, 0, writer, NULL, 0, NULL);
   if (!job.writer) {
      TerminateProcess(job.process, 125);
      CloseHandle(pi.hThread);
      goto failed;
   }
   ResumeThread(pi.hThread);
   CloseHandle(pi.hThread);
#else
   if (socketpair(AF_UNIX, SOCK_STREAM, 0, ins) || pipe(outs) || pipe(errs))
      goto failed;
   job.pid = fork();
   if (job.pid < 0)
      goto failed;
   if (!job.pid) {
      setpgid(0, 0);
      dup2(ins[0], 0);
      dup2(outs[1], 1);
      dup2(errs[1], 2);
      close(ins[0]);
      close(ins[1]);
      close(outs[0]);
      close(outs[1]);
      close(errs[0]);
      close(errs[1]);
      execl(helper, helper, job.directory, (char *)NULL);
      _exit(127);
   }
   setpgid(job.pid, job.pid);
   close(ins[0]);
   close(outs[1]);
   close(errs[1]);
   job.in = ins[1];
   job.out = outs[0];
   job.err = errs[0];
   fcntl(job.in, F_SETFL, O_NONBLOCK);
   fcntl(job.out, F_SETFL, O_NONBLOCK);
   fcntl(job.err, F_SETFL, O_NONBLOCK);
   fcntl(job.in, F_SETFD, FD_CLOEXEC);
   fcntl(job.out, F_SETFD, FD_CLOEXEC);
   fcntl(job.err, F_SETFD, FD_CLOEXEC);
#ifdef SO_NOSIGPIPE
   {
      int on = 1;
      setsockopt(job.in, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
   }
#endif
   job.active = 1;
#endif
   the_tool_started(r->id);
   return 1;
failed:
   snprintf(error, cap, "Cannot start native tool: %s", strerror(errno));
#ifdef _WIN32
   free(command);
   free(exe);
   close_handle(&child_in);
   close_handle(&child_out);
   close_handle(&child_err);
   if (job.active)
      stop(job.id);
   else {
      close_handle(&job.in);
      close_handle(&job.out);
      close_handle(&job.err);
      close_handle(&job.process);
      close_handle(&job.group);
   }
#else
   for (i = 0; i < 2; i++) {
      if (ins[i] >= 0)
         close(ins[i]);
      if (outs[i] >= 0)
         close(outs[i]);
      if (errs[i] >= 0)
         close(errs[i]);
   }
#endif
   cleanup_files();
   return 0;
}
void the_tools_native_initialise(void) {
   TheToolAdapter a = {start, input, eof, stop, poll};
   const char *configured;
   char helper[THE_TOOL_PATH];
   int enabled;
   if (initialized)
      return;
   initialized = 1;
   configured = getenv("THE_TOOL_ROOT");
   snprintf(root, sizeof(root), "%s",
            configured && *configured ? configured : THE_NATIVE_TOOL_ROOT);

#ifdef _WIN32
   join(helper, sizeof(helper), root, "the_rxbin_host.exe");
#else
   join(helper, sizeof(helper), root, "the_rxbin_host");
#endif
#ifdef THE_NATIVE_TOOL_INSTALL_ROOT
   if (!configured && !exists(helper)) {
      snprintf(root, sizeof(root), "%s", THE_NATIVE_TOOL_INSTALL_ROOT);
#ifdef _WIN32
      join(helper, sizeof(helper), root, "the_rxbin_host.exe");
#else
      join(helper, sizeof(helper), root, "the_rxbin_host");
#endif
   }
#endif
   enabled = *root && exists(helper);
   the_tools_initialise(&a, enabled);
   join(helper, sizeof(helper), root, "dslsh-classic.rxbin");
   if (!exists(helper)) {
      TheToolDefinition t = *the_tool_find("classic-highlight");
      t.available = 0;
      the_tool_register(&t);
   }
}
int the_tools_native_language(const TheToolDefinition *t) {
#ifdef USE_SDSLH
   char image[THE_TOOL_PATH], command[THE_TOOL_PATH * 2 + 160], extensions[128], *p, *end;
   int n;
   the_tools_native_initialise();
   if (!initialized || !*root || !t)
      return 0;
   if (t->image[0] == '@')
      join(image, sizeof(image), root, "dslsh-classic.rxbin");
   else
      snprintf(image, sizeof(image), "%s", t->image);
   if (!exists(image) || strchr(image, '"') || strchr(root, '"'))
      return 0;
   n = snprintf(command, sizeof(command), "%s \"%s/the_rxbin_host%s\" --service \"%s\"", t->id,
                root,
#ifdef _WIN32
                ".exe",
#else
                "",
#endif
                image);
   if (n < 0 || n > MAX_FILE_NAME)
      return 0;
   if (Sdslh((CHARTYPE *)command) != RC_OK)
      return 0;
   snprintf(extensions, sizeof(extensions), "%s", t->extensions);
   p = extensions;
   while (*p) {
      end = strchr(p, ',');
      if (end)
         *end = 0;
      snprintf(command, sizeof(command), "*%s %s", p, t->id);
      if (Autocolour((CHARTYPE *)command) != RC_OK)
         return 0;
      if (!end)
         break;
      p = end + 1;
   }
   return 1;
#else
   (void)t;
   return 0;
#endif
}
void the_tools_native_poll(void) { the_tool_poll(); }
#endif

#ifdef __EMSCRIPTEN__
int the_tools_native_language(const TheToolDefinition *t) {
   (void)t;
   return 0;
}
void the_tools_native_initialise(void) {}
void the_tools_native_poll(void) { the_tool_poll(); }
#endif
