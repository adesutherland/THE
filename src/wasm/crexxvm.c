/* A process-style, single-owner RXVML host. Programs and dependencies arrive
 * as RXBIN buffers. Native process/filesystem providers are not installed. */
#include <rxvml.h>
#include <rxbin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#define VM_IMAGE_LIMIT (64u * 1024u * 1024u)
typedef struct VmImage { void *bytes; struct VmImage *next; } VmImage;
static rxvml_context *context;
static VmImage *images;
static size_t image_bytes;
static int running, program_rc;
static char error_text[1024];
#ifdef THE_WASM_RUNNER
static char *control_result;
EMSCRIPTEN_KEEPALIVE const char *the_vm_result(void) { return control_result ? control_result : ""; }
#endif

static int error(const char *message)
{
    snprintf(error_text, sizeof(error_text), "%s", message ? message : "VM error");
    return -1;
}

EMSCRIPTEN_KEEPALIVE const char *the_vm_error(void) { return error_text; }
EMSCRIPTEN_KEEPALIVE int the_vm_program_rc(void) { return program_rc; }

EMSCRIPTEN_KEEPALIVE int the_vm_destroy(void)
{
    VmImage *image;
    if (running) return error("VM is running");
    if (context) rxvml_destroy(context);
    context = NULL;
    /* Pin the admitted input buffers for the entire context lifetime. */
    while ((image = images) != NULL) {
        images = image->next;
        free(image->bytes); free(image);
    }
    image_bytes = 0;
    return 0;
}

EMSCRIPTEN_KEEPALIVE int the_vm_create(void)
{
    if (running) return error("VM is running");
    the_vm_destroy();
#ifdef THE_WASM_RUNNER
    free(control_result); control_result = NULL;
#endif
    error_text[0] = '\0'; program_rc = 0;
    context = rxvml_create(NULL, 0);
    return context ? 0 : error("Unable to create RXVML context");
}

#ifdef THE_WASM_RUNNER
/* Source and arguments are values, never stdin or a command-line string.
 * One owner may suspend here while the private process waits for console input. */
EMSCRIPTEN_KEEPALIVE int the_vm_interpret(const char *source, size_t source_length,
        const char *filename, size_t filename_length, int argc, const char **argv)
{
    rxvml_value *values[3] = {NULL, NULL, NULL}, *response = NULL;
    const char *text = NULL, *message = NULL;
    size_t length = 0, argument_bytes = 0;
    int rc = -1;
    if (!context || running) return error("VM is not ready to interpret");
    if (!source || source_length > 2097152 || memchr(source, 0, source_length)
        || !filename || !filename_length || filename_length > 4096 || memchr(filename, 0, filename_length)
        || argc < 0 || argc > 128 || (argc && !argv)) return error("Invalid interpreter input");
    free(control_result); control_result = NULL;
    error_text[0] = 0; program_rc = 0;
    values[0] = rxvml_value_new(context); values[1] = rxvml_value_new(context);
    values[2] = rxvml_array_new(context, argc);
    if (!values[0] || !values[1] || !values[2]) { error("Unable to allocate interpreter values"); goto done; }
    if (rxvml_set_str(values[0], source, source_length) || rxvml_set_str(values[1], filename, filename_length)) {
        error("Interpreter source and filename require valid UTF-8"); goto done;
    }
    for (int i = 0; i < argc; i++) {
        rxvml_value *value;
        if (!argv[i] || (length = strlen(argv[i])) > 1048576 - argument_bytes) {
            error("Interpreter arguments exceed the byte limit"); goto done;
        }
        argument_bytes += length;
        value = rxvml_value_new(context);
        if (!value) { error("Unable to allocate argument"); goto done; }
        int invalid = rxvml_set_str(value, argv[i], length) || rxvml_array_set(context, values[2], i + 1, value);
        rxvml_value_free(value);
        if (invalid) { error("Invalid UTF-8 program argument"); goto done; }
    }
    clearerr(stdin); clearerr(stdout); clearerr(stderr);
    running = 1;
    rc = rxvml_call_procedure_descriptor(context,
        "rxsig1|llcbrowser.run|.string|source=.string,filename=.string,arguments=.string[*]",
        3, values, &response);
    running = 0;
    if (rc) {
        rxvml_last_error(context, &message); error(message ? message : "Classic interpreter invocation failed");
    } else if (!response || rxvml_to_str(context, response, &text, &length) || length > 3145728) {
        rc = error("Invalid or oversized interpreter result");
    } else {
        control_result = malloc(length + 1);
        if (!control_result) rc = error("Unable to copy interpreter result");
        else { memcpy(control_result, text, length); control_result[length] = 0; }
    }
    {
        int out = fflush(stdout), err = fflush(stderr);
        if ((out || err) && !rc) rc = error("Unable to flush interpreter output");
    }
done:
    if (response) rxvml_value_free(response);
    for (int i = 0; i < 3; i++) if (values[i]) rxvml_value_free(values[i]);
    return rc;
}
#endif

EMSCRIPTEN_KEEPALIVE int the_vm_load(const void *bytes, size_t length)
{
    VmImage *image;
    if (!context || running) return error("VM is not ready to load bytecode");
    if (!bytes || !length || length > VM_IMAGE_LIMIT - image_bytes)
        return error("RXBIN image exceeds the host image budget");
    image = calloc(1, sizeof(*image));
    if (!image) return error("Unable to allocate image record");
    image->bytes = malloc(length);
    if (!image->bytes) { free(image); return error("Unable to allocate RXBIN image"); }
    memcpy(image->bytes, bytes, length);
    image->next = images; images = image; image_bytes += length;
    if (rxvml_load_module_buffer(context, image->bytes, length) <= 0) {
        const char *message = rxbin_last_error();
        return error(message ? message : "Unable to load RXBIN image");
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE int the_vm_run(int argc, const char **argv)
{
    const char *message = NULL;
    int rc;
    if (!context || running) return error("VM is not ready to run");
    if (argc < 0 || argc > 128 || (argc && !argv)) return error("Invalid program arguments");
    error_text[0] = '\0'; program_rc = 0;
    clearerr(stdin); clearerr(stdout); clearerr(stderr);
    running = 1;
    rc = rxvml_run(context, argc, argv, &program_rc);
    if (rc != 0) {
        rxvml_last_error(context, &message);
        error(message ? message : "RXVML execution failed");
    }
    {
        int output_error = fflush(stdout), diagnostic_error = fflush(stderr);
        if ((output_error != 0 || diagnostic_error != 0) && rc == 0)
            rc = error("Unable to flush VM output");
    }
    running = 0;
    return rc;
}
