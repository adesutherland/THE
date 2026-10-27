/* Browser host byte streams for cREXX. Keep libc buffering and binary bytes;
 * suspend only stdin reads, on the VM's single execution owner. */
#include <emscripten.h>
#include <wasi/api.h>
#include <stddef.h>
#include <stdint.h>

__wasi_errno_t __real___wasi_fd_read(__wasi_fd_t, const __wasi_iovec_t *, size_t, __wasi_size_t *);
__wasi_errno_t __real___wasi_fd_write(__wasi_fd_t, const __wasi_ciovec_t *, size_t, __wasi_size_t *);

EM_ASYNC_JS(int, the_vm_stream_read, (void *out, size_t capacity), {
    try {
        const chunk = await Module.vmStdinRead(capacity);
        if (chunk === null) return 0;
        if (!(chunk instanceof Uint8Array) || !chunk.length || chunk.length > capacity)
            throw new Error('Invalid stdin chunk');
        HEAPU8.set(chunk, out);
        return chunk.length;
    } catch (error) {
        Module.vmStreamError = String(error);
        return -1;
    }
});

EM_JS(int, the_vm_stream_write, (int fd, const void *vectors, size_t count), {
    let total = 0;
    for (let i = 0; i < count; i++) total += HEAPU32[(vectors + i * 8 + 4) >> 2];
    if (total > 16777216) return -1;
    const bytes = new Uint8Array(total);
    let offset = 0;
    for (let i = 0; i < count; i++) {
        const ptr = HEAPU32[(vectors + i * 8) >> 2];
        const len = HEAPU32[(vectors + i * 8 + 4) >> 2];
        bytes.set(HEAPU8.subarray(ptr, ptr + len), offset);
        offset += len;
    }
    try {
        if (total) Module.vmStreamWrite(fd, bytes);
        return total;
    } catch (error) {
        Module.vmStreamError = String(error);
        return -1;
    }
});

__wasi_errno_t __wrap___wasi_fd_read(__wasi_fd_t fd, const __wasi_iovec_t *iov,
                                    size_t count, __wasi_size_t *received)
{
    if (fd != 0) return __real___wasi_fd_read(fd, iov, count, received);
    *received = 0;
    for (size_t i = 0; i < count; i++) {
        if (iov[i].buf_len) {
            int bytes = the_vm_stream_read(iov[i].buf, iov[i].buf_len);
            if (bytes < 0) return __WASI_ERRNO_IO;
            *received = (__wasi_size_t)bytes;
            break; /* A short read is valid; libc owns any remaining buffering. */
        }
    }
    return 0;
}

__wasi_errno_t __wrap___wasi_fd_write(__wasi_fd_t fd, const __wasi_ciovec_t *iov,
                                     size_t count, __wasi_size_t *written)
{
    int bytes;
    if (fd != 1 && fd != 2) return __real___wasi_fd_write(fd, iov, count, written);
    *written = 0;
    bytes = the_vm_stream_write(fd, iov, count);
    if (bytes < 0) return __WASI_ERRNO_IO;
    *written = (__wasi_size_t)bytes;
    return 0;
}
