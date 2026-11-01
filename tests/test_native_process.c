/* Exercise the real native subprocess owner without an editor build. The test
 * is also its own deterministic child executable. Optional ooRexx qualification
 * uses THE_TEST_OOREXX_EXECUTABLE, never PATH or a hardcoded developer install. */
#ifdef _WIN32
#include <stdio.h>
int main(void) {
   puts("Native process component probe currently requires POSIX");
   return 77;
}
#else
#include "../src/tools_native.c"
#include <assert.h>
#include <time.h>

static unsigned char output_bytes[2][65536];
static size_t output_lengths[2];
static int starts, finishes, completion_code;
static char completion_status[32], completion_error[256];
static unsigned next_id = 100;
static char temporary[THE_TOOL_PATH], executable[THE_TOOL_PATH];

void the_tool_started(unsigned id) { assert(id == job.id); starts++; }
void the_tool_output(unsigned id, int fd, const void *bytes, size_t length) {
   assert(id == job.id && fd >= 1 && fd <= 2 && !finishes);
   assert(length <= sizeof(output_bytes[0]) - output_lengths[fd - 1]);
   memcpy(output_bytes[fd - 1] + output_lengths[fd - 1], bytes, length);
   output_lengths[fd - 1] += length;
}
void the_tool_finish(unsigned id, int code, const char *status,
                     const char *error, const char *llb) {
   assert(id == job.id && !finishes && llb == NULL);
   completion_code = code;
   snprintf(completion_status, sizeof(completion_status), "%s", status ? status : "");
   snprintf(completion_error, sizeof(completion_error), "%s", error ? error : "");
   finishes++;
}
/* The component's unrelated RXBIN initialization/profile dependencies. */
const char *the_tools_profile_path(void) { return "/unqualified/profile.the"; }
const char *the_tools_source_role(const char *path) { (void)path; assert(0); return "other"; }
size_t the_tools_build_name(const char *path,char *out,size_t capacity) {
   (void)path; (void)out; (void)capacity; assert(0); return 0;
}
const TheToolConsole *the_tool_console(void) { assert(0); return NULL; }
void the_tools_profile_built(unsigned id, const char *path, unsigned source, unsigned revision) {
   (void)id; (void)path; (void)source; (void)revision; assert(0);
}
void the_tools_initialise(const TheToolAdapter *adapter, int available) {
   (void)adapter; (void)available; assert(0);
}
const TheToolDefinition *the_tool_find(const char *id) { (void)id; assert(0); return NULL; }
int the_tool_register(const TheToolDefinition *tool) { (void)tool; assert(0); return 0; }
void the_tool_poll(void) { assert(0); }
#ifdef USE_SDSLH
short Sdslh(CHARTYPE *params) { (void)params; assert(0); return RC_INVALID_ENVIRON; }
short Autocolour(CHARTYPE *params) { (void)params; assert(0); return RC_INVALID_ENVIRON; }
#endif

static int child(int argc, char **argv) {
   char source[64] = {0};
   FILE *file = fopen(argv[1], "rb");
   assert(file);
   assert(fread(source, 1, sizeof(source) - 1, file));
   fclose(file);
   if (!strcmp(source, "argv")) {
      int i;
      const unsigned char stderr_bytes[] = {'E', 0, 0xff, '\n'};
      for (i = 2; i < argc; i++) {
         printf("%lu:", (unsigned long)strlen(argv[i]));
         fwrite(argv[i], 1, strlen(argv[i]), stdout);
         putchar('\n');
      }
      fwrite(stderr_bytes, 1, sizeof(stderr_bytes), stderr);
      return 0;
   }
   if (!strcmp(source, "input")) {
      unsigned char bytes[1024];
      size_t n;
      while ((n = fread(bytes, 1, sizeof(bytes), stdin)))
         assert(fwrite(bytes, 1, n, stdout) == n);
      return 0;
   }
   if (!strcmp(source, "wait")) {
      unsigned char byte;
      while (read(0, &byte, 1) > 0) {}
      return 0;
   }
   if (!strcmp(source, "signal")) {
      raise(SIGTERM);
      return 99;
   }
   if (!strcmp(source, "exit7"))
      return 7;
   if (!strcmp(source, "exit0"))
      return 0;
   if (!strcmp(source, "descendant")) {
      pid_t descendant = fork();
      assert(descendant >= 0);
      if (!descendant) {
         for (;;) pause();
      }
      printf("%ld\n", (long)descendant);
      fflush(stdout);
      return 0;
   }
   return 98;
}

static void reset_capture(void) {
   assert(!job.active);
   memset(output_lengths, 0, sizeof(output_lengths));
   starts = finishes = completion_code = 0;
   completion_status[0] = completion_error[0] = 0;
}
static TheToolRequest request(const char *source, int argc, const char *const *argv) {
   TheToolRequest r = {0};
   r.id = ++next_id;
   r.source_id = 17;
   r.revision = 3;
   strcpy(r.tool.id, "native-test");
   strcpy(r.tool.label, "Native test");
   strcpy(r.tool.role, "runner");
   strcpy(r.tool.entry, "native-process");
   strcpy(r.tool.image, executable);
   r.tool.available = 1;
   r.filename = "/original file remains untouched.rex";
   r.source = source;
   r.source_length = strlen(source);
   r.argc = argc;
   r.argv = argv;
   return r;
}
static void nap(void) {
   struct timespec interval = {0, 1000000};
   nanosleep(&interval, NULL);
}
static void drain(void) {
   int i;
   char directory[THE_TOOL_PATH];
   strcpy(directory, job.directory);
   for (i = 0; i < 10000 && job.active; i++) {
      poll();
      nap();
   }
   assert(!job.active && starts == 1 && finishes == 1);
   assert(!*job.directory && access(directory, F_OK) < 0);
}
static void no_stages(void) {
   DIR *directory = opendir(temporary);
   struct dirent *entry;
   assert(directory);
   while ((entry = readdir(directory)))
      assert(strncmp(entry->d_name, "crxthe-job-", 11));
   closedir(directory);
}
static void run(TheToolRequest *r) {
   char error[256];
   reset_capture();
   assert(start(r, error, sizeof(error)));
   drain();
   no_stages();
}

/* ooRexx may otherwise detach rxapi on first use. Start the explicitly adjacent
 * provider ourselves in a private runtime directory so this test owns its PID
 * and cannot terminate another session's shared provider during cleanup. */
static pid_t start_provider(const char *oorexx, char *directory, size_t capacity) {
   char provider[THE_TOOL_PATH], *slash;
   pid_t pid;
   int i, ready = 0;
   snprintf(provider, sizeof(provider), "%s", oorexx);
   slash = strrchr(provider, '/');
   assert(slash && (size_t)(slash - provider) + 7 < sizeof(provider));
   strcpy(slash + 1, "rxapi");
   assert(!access(provider, X_OK));
   snprintf(directory, capacity, "%s/provider", temporary);
   assert(!mkdir(directory, 0700));
   assert(!setenv("XDG_RUNTIME_DIR", directory, 1));
   pid = fork();
   assert(pid >= 0);
   if (!pid) {
      int null = open("/dev/null", O_RDWR);
      assert(null >= 0);
      dup2(null, 0); dup2(null, 1); dup2(null, 2);
      if (null > 2) close(null);
      execl(provider, provider, (char *)NULL);
      _exit(127);
   }
   for (i = 0; i < 5000 && !ready; i++) {
      DIR *scan = opendir(directory);
      struct dirent *entry;
      assert(scan);
      while ((entry = readdir(scan))) {
         const char *suffix = strrchr(entry->d_name, '.');
         if (suffix && !strcmp(suffix, ".service")) ready = 1;
      }
      closedir(scan);
      nap();
   }
   assert(ready);
   return pid;
}
static void stop_provider(pid_t pid, const char *directory) {
   int status, i;
   pid_t waited = 0;
   assert(!kill(pid, SIGTERM));
   for (i = 0; i < 5000 && !waited; i++) {
      waited = waitpid(pid, &status, WNOHANG);
      assert(waited >= 0);
      if (!waited) nap();
   }
   if (!waited) {
      assert(!kill(pid, SIGKILL));
      assert(waitpid(pid, &status, 0) == pid);
   }
   remove_job_directory(directory, 0);
   assert(access(directory, F_OK) < 0);
}

int main(int argc, char **argv) {
   char error[256], helper[THE_TOOL_PATH], bad[THE_TOOL_PATH], stage[THE_TOOL_PATH];
   char *self;
   FILE *in, *out;
   unsigned char bytes[8192];
   size_t n;
   TheToolRequest r;
   const char *args[] = {"Mixed Case", "", "quoted\"back\\", "Zoë😀", "$(touch never); & |"};
   const char expected[] = "10:Mixed Case\n0:\n12:quoted\"back\\\n8:Zoë😀\n19:$(touch never); & |\n";
   const unsigned char expected_error[] = {'E', 0, 0xff, '\n'};
   if (argc > 1)
      return child(argc, argv);
   snprintf(temporary, sizeof(temporary), "/tmp/THE-native-process-XXXXXX");
   assert(mkdtemp(temporary));
   assert(!setenv("TMPDIR", temporary, 1));
   self = realpath(argv[0], NULL);
   assert(self && strlen(self) < sizeof(executable));
   /* The interpreter pathname itself also contains spaces and non-ASCII. */
   snprintf(helper, sizeof(helper), "%s/Native Program Zoë", temporary);
   in = fopen(self, "rb"); out = fopen(helper, "wb");
   assert(in && out);
   while ((n = fread(bytes, 1, sizeof(bytes), in)))
      assert(fwrite(bytes, 1, n, out) == n);
   assert(!fclose(in) && !fclose(out) && !chmod(helper, 0700));
   strcpy(executable, helper);
   free(self);

   r = request("argv", 5, args);
   run(&r);
   assert(!completion_code && !strcmp(completion_status, "0") && !*completion_error);
   assert(output_lengths[0] == sizeof(expected) - 1);
   assert(!memcmp(output_bytes[0], expected, sizeof(expected) - 1));
   assert(output_lengths[1] == sizeof(expected_error));
   assert(!memcmp(output_bytes[1], expected_error, sizeof(expected_error)));

   /* Snapshot bytes are already staged when the caller changes its source. */
   {
      char source[] = "exit7";
      r = request(source, 0, NULL);
      reset_capture();
      assert(start(&r, error, sizeof(error)));
      memset(source, 'x', strlen(source));
      drain();
      assert(completion_code && !strcmp(completion_status, "7"));
      assert(strstr(completion_error, "status 7"));
   }
   {
      const unsigned char input_bytes[] = {'a', 0, 0xff, 'b', '\n'};
      r = request("input", 0, NULL);
      r.finite_input = 1;
      reset_capture();
      assert(start(&r, error, sizeof(error)));
      assert(input(r.id, input_bytes, sizeof(input_bytes)));
      assert(native_input_eof(r.id));
      assert(!input(r.id, "late", 4));
      drain();
      assert(!completion_code && output_lengths[0] == sizeof(input_bytes));
      assert(!memcmp(output_bytes[0], input_bytes, sizeof(input_bytes)));
   }
   r = request("signal", 0, NULL);
   run(&r);
   assert(completion_code && !strcmp(completion_status, "signal:15"));
   assert(strstr(completion_error, "signal 15"));

   /* Exit 0 cannot publish a finite-input job whose queued bytes were lost. */
   {
      char *finite = malloc(1048576);
      int mode;
      assert(finite);
      memset(finite, 'x', 1048576);
      for (mode = 0; mode <= 3; mode++) {
         r = request("exit0", 0, NULL);
         r.finite_input = mode >= 2;
         reset_capture();
         assert(start(&r, error, sizeof(error)));
         assert(input(r.id, finite, 1048576));
         if (mode == 1 || mode == 2) assert(native_input_eof(r.id));
         drain();
         assert(!strcmp(completion_status, "0"));
         if (mode >= 2) {
            assert(completion_code && strstr(completion_error, "stdin delivery failed"));
         } else
            assert(!completion_code && !*completion_error);
      }
      free(finite);
   }

   /* Stop keeps singleton identity and removes the staged file. */
   r = request("wait", 0, NULL);
   reset_capture();
   assert(start(&r, error, sizeof(error)));
   strcpy(stage, job.directory);
   assert(!start(&r, error, sizeof(error)) && job.id == r.id && job.active);
   stop(r.id + 1);
   assert(job.active);
   stop(r.id);
   assert(!job.active && !finishes && access(stage, F_OK) < 0);
   no_stages();

   /* Leader exit must not stop Stop from killing its pipe-holding children. */
   r = request("descendant", 0, NULL);
   reset_capture();
   assert(start(&r, error, sizeof(error)));
   {
      int i, witness, ended = 0;
      for (i = 0; i < 5000 && (!job.exited || !output_lengths[0]); i++) { poll(); nap(); }
      assert(job.exited && job.active && output_lengths[0]);
      witness = dup(job.out);
      assert(witness >= 0);
      strcpy(stage, job.directory);
      stop(r.id);
      assert(!job.active && access(stage, F_OK) < 0);
      /* The descendant never closes stdout itself. EOF on this retained pipe
       * proves Stop also ended that descendant after the leader was reaped. */
      for (i = 0; i < 5000 && !ended; i++) {
         ssize_t got = read(witness, bytes, sizeof(bytes));
         if (!got) ended = 1;
         else { assert(got < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)); nap(); }
      }
      assert(ended && !close(witness));
   }
   no_stages();

   /* An executable text file without a shebang must refuse, never use a shell. */
   snprintf(bad, sizeof(bad), "%s/not-an-image", temporary);
   out = fopen(bad, "wb"); assert(out);
   fputs("exit 0\n", out); assert(!fclose(out) && !chmod(bad, 0700));
   r = request("argv", 0, NULL);
   strcpy(r.tool.image, bad);
   reset_capture();
   assert(!start(&r, error, sizeof(error)) && !starts && !job.active);
   no_stages();
   assert(!unlink(bad));
   r = request("argv", 0, NULL);
   strcpy(r.tool.image, "rexx");
   assert(!start(&r, error, sizeof(error)));
   strcpy(r.tool.image, "/missing/explicit/ooRexx");
   assert(!start(&r, error, sizeof(error)));
   no_stages();

   /* 8191 includes the spaces ooRexx joins into its fixed CLI buffer. */
   {
      char argument[8193];
      const char *large[] = {argument};
      memset(argument, 'a', sizeof(argument));
      argument[8191] = 0;
      r = request("argv", 1, large);
      run(&r);
      assert(!completion_code);
      argument[8191] = 'a'; argument[8192] = 0;
      reset_capture();
      assert(!start(&r, error, sizeof(error)) && strstr(error, "8191"));
      no_stages();
   }
   {
      const char invalid_source[] = {'s', 0, 'x'};
      r = request("argv", 0, NULL);
      r.source = invalid_source; r.source_length = sizeof(invalid_source);
      assert(!start(&r, error, sizeof(error)));
      no_stages();
   }

   {
      const char *oorexx = getenv("THE_TEST_OOREXX_EXECUTABLE");
      if (oorexx && *oorexx) {
         char provider_directory[THE_TOOL_PATH];
         pid_t provider = start_provider(oorexx, provider_directory, sizeof(provider_directory));
         const char *ooargs[] = {"Mixed Case", "Zoë😀"};
         const char *program = "say .Greeter~new~greet('ready')\nsay .SYSCARGS~items\n"
            "say .SYSCARGS[1]\nsay .SYSCARGS[2]\nexit 7\n::class Greeter\n"
            "::method greet\nuse arg text\nreturn 'object-smoke:' || text\n";
         const char *ooexpected = "object-smoke:ready\n2\nMixed Case\nZoë😀\n";
         assert(strlen(oorexx) < sizeof(executable)); strcpy(executable, oorexx);
         r = request(program, 2, ooargs);
         run(&r);
         assert(completion_code && !strcmp(completion_status, "7"));
         assert(output_lengths[0] == strlen(ooexpected));
         assert(!memcmp(output_bytes[0], ooexpected, strlen(ooexpected)));
         assert(!output_lengths[1]);
         {
            char argument[8192];
            const char *limit_args[] = {argument};
            memset(argument, 'a', 8191); argument[8191] = 0;
            r = request("say .SYSCARGS[1]~length\n", 1, limit_args);
            run(&r);
            assert(!completion_code && output_lengths[0] == 5);
            assert(!memcmp(output_bytes[0], "8191\n", 5));
         }
         r = request("say c2x(charin(,,5))\n", 0, NULL);
         r.finite_input = 1;
         reset_capture();
         assert(start(&r, error, sizeof(error)));
         {
            const unsigned char input_bytes[] = {'a', 0, 0xff, 'b', '\n'};
            assert(input(r.id, input_bytes, sizeof(input_bytes)));
         }
         assert(native_input_eof(r.id));
         drain();
         assert(!completion_code && output_lengths[0] == 11);
         assert(!memcmp(output_bytes[0], "6100FF620A\n", 11));
         r = request("say 1/0\n", 0, NULL);
         run(&r);
         assert(completion_code && output_lengths[1] && *completion_status);
         r = request("do forever\nnop\nend\n", 0, NULL);
         reset_capture();
         assert(start(&r, error, sizeof(error)));
         poll(); stop(r.id);
         assert(!job.active && !finishes);
         no_stages();
         stop_provider(provider, provider_directory);
         puts("Actual configured ooRexx: snapshot, object dispatch, argv, bytes/EOF, exit, condition and Stop passed");
      } else
         puts("Actual ooRexx not configured; dependency qualification omitted");
   }
   assert(!unlink(helper));
   assert(!rmdir(temporary));
   puts("Native process component: argv, bytes/EOF, status, signal, Stop, cleanup and refusals passed");
   return 0;
}
#endif
