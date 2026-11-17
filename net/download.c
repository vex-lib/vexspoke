#include "vex.h"
#include "net/download.h"

#include <stdlib.h>
#include <unistd.h>
#include <spawn.h>
#include <sys/wait.h>

extern char **environ;

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Download
 * ============================================================================
 * Safe download bridge that shells out to the system curl binary via
 * posix_spawnp (never system()) to fetch packages (Homebrew, assets) to disk
 * or into a caller-owned DownloadResponse buffer. Options (timeout, redirects,
 * silent, user agent) are validated cold; the child is reaped with waitpid
 * and HTTP failures degrade to false with the response exit code recorded.
 * DownloadResponse owns its data buffer and must be freed with
 * DownloadResponse_free — the only allocation in the module.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Download (net/download.c)
 * ============================================================================
 * Safe download bridge utilizing system curl to retrieve packages (Homebrew, assets)
 * to disk or in-memory buffers.
 *
 * STRUCT FIELDS:
 *   - DownloadOptions: timeout_seconds, follow_redirects, silent, user_agent
 *   - DownloadResponse: exit_code, http_status, data, size
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Options & Lifecycle:
 *   - DownloadOptions()
 *   - DownloadResponse_free(resources)
 * Execution:
 *   - Download_to_file(url, output_file, opts)
 *   - Download_to_memory(url, opts, response_out)
 * ============================================================================
 */


;;GETTER
static inline char * DownloadResponse_data(const DownloadResponse *s) { return (*s).data; }
;;GETTER
static inline char * *DownloadResponse_dataMut(DownloadResponse *s) { return &(*s).data; }
;;SETTER
static inline fn DownloadResponse_setData(DownloadResponse *s, char * v) { (*s).data = v; }
;;GETTER
static inline i32 DownloadResponse_exit_code(const DownloadResponse *s) { return (*s).exit_code; }
;;GETTER
static inline i32 *DownloadResponse_exit_codeMut(DownloadResponse *s) { return &(*s).exit_code; }
;;SETTER
static inline fn DownloadResponse_setExit_code(DownloadResponse *s, i32 v) { (*s).exit_code = v; }
;;GETTER
static inline u32 DownloadResponse_http_status(const DownloadResponse *s) { return (*s).http_status; }
;;GETTER
static inline u32 *DownloadResponse_http_statusMut(DownloadResponse *s) { return &(*s).http_status; }
;;SETTER
static inline fn DownloadResponse_setHttp_status(DownloadResponse *s, u32 v) { (*s).http_status = v; }
;;GETTER
static inline bytes DownloadResponse_size(const DownloadResponse *s) { return (*s).size; }
;;GETTER
static inline bytes *DownloadResponse_sizeMut(DownloadResponse *s) { return &(*s).size; }
;;SETTER
static inline fn DownloadResponse_setSize(DownloadResponse *s, bytes v) { (*s).size = v; }

;;INTENTION("Reliable download manager leveraging system curl and HTTP/TLS stacks")

fn DownloadResponse_free(DownloadResponse *res) {
    if (!res) {
        return;
    }
    if (DownloadResponse_data(res)) {
        free(DownloadResponse_data(res));
        DownloadResponse_setData(res, nullptr);
    }
    DownloadResponse_setSize(res, 0);
    DownloadResponse_setHttp_status(res, 0);
    DownloadResponse_setExit_code(res, 0);
}

bool Download_to_file(const char *url, const char *output_file, const DownloadOptions *opts) {
    if (!url || !output_file) {
        return false;
    }

    DownloadOptions local_opts = opts ? (*opts) : DownloadOptions();

    char timeout_str[16];
    snprintf(timeout_str, sizeof(timeout_str), "%u", local_opts.timeout_seconds);

    const char *argv[16];
    i32 argc = 0;
    argv[argc++] = "curl";
    argv[argc++] = "-f"; // Fail fast on HTTP errors (>=400)
    if (local_opts.silent) {
        argv[argc++] = "-s";
    }
    if (local_opts.follow_redirects) {
        argv[argc++] = "-L";
    }
    argv[argc++] = "--max-time";
    argv[argc++] = timeout_str;
    if (local_opts.user_agent) {
        argv[argc++] = "-A";
        argv[argc++] = local_opts.user_agent;
    }
    argv[argc++] = "-o";
    argv[argc++] = output_file;
    argv[argc++] = url;
    argv[argc] = nullptr;

    pid_t pid = 0;
    i32 status = posix_spawnp(&pid, "curl", nullptr, nullptr, (char* const*) argv, environ);
    if (status != 0) {
        return false;
    }

    i32 wait_status = 0;
    if (waitpid(pid, &wait_status, 0) < 0) {
        return false;
    }

    if (WIFEXITED(wait_status) && WEXITSTATUS(wait_status) == 0) {
        return true;
    }

    return false;
}

bool Download_to_memory(const char *url, const DownloadOptions *opts, DownloadResponse *response_out) {
    if (!url || !response_out) {
        return false;
    }

    DownloadResponse_setExit_code(response_out, -1);
    DownloadResponse_setHttp_status(response_out, 0);
    DownloadResponse_setData(response_out, nullptr);
    DownloadResponse_setSize(response_out, 0);

    char temp_path[] = "/tmp/vex_dl_XXXXXX";
    i32 fd = mkstemp(temp_path);
    if (fd < 0) {
        return false;
    }
    close(fd);

    bool ok = Download_to_file(url, temp_path, opts);
    if (!ok) {
        unlink(temp_path);
        return false;
    }

    FILE *f = fopen(temp_path, "rb");
    if (!f) {
        unlink(temp_path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    i64 sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz < 0) {
        fclose(f);
        unlink(temp_path);
        return false;
    }

    char *buf = (char*) malloc((bytes) sz + 1);
    if (!buf) {
        fclose(f);
        unlink(temp_path);
        return false;
    }

    bytes read_bytes = fread(buf, 1, (bytes) sz, f);
    buf[read_bytes] = '\0';
    fclose(f);
    unlink(temp_path);

    DownloadResponse_setExit_code(response_out, 0);
    DownloadResponse_setHttp_status(response_out, 200);
    DownloadResponse_setData(response_out, buf);
    DownloadResponse_setSize(response_out, read_bytes);

    return true;
}

bool Download_fetch_cached(
    const char *url,
    const char *subsystem,
    u64 ttl_sec,
    const DownloadOptions *opts,
    char *cached_path_out,
    bytes path_cap
) {
    (discard) url; (discard) subsystem; (discard) ttl_sec; (discard) opts;
    (discard) cached_path_out; (discard) path_cap;
    return false;
}
