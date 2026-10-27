/* A process-style, single-owner RXVML host. Programs and dependencies arrive
 * as RXBIN buffers. Native process/filesystem providers are not installed. */
#include <rxvml.h>
#include <rxbin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <emscripten.h>

#define VM_IMAGE_LIMIT (64u * 1024u * 1024u)
typedef struct VmImage { void *bytes; struct VmImage *next; } VmImage;
static rxvml_context *context;
static VmImage *images;
static size_t image_bytes;
static int running, program_rc;
static char error_text[1024];

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
    error_text[0] = '\0'; program_rc = 0;
    context = rxvml_create(NULL, 0);
    return context ? 0 : error("Unable to create RXVML context");
}

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
