#ifndef NET_DOWNLOAD_H
#define NET_DOWNLOAD_H

#include "vex.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DownloadOptions {
    u32 timeout_seconds;
    bool follow_redirects;
    bool silent;
    const char *user_agent;
} DownloadOptions;

static inline DownloadOptions DownloadOptions_0() {
    return (DownloadOptions) { .timeout_seconds = 30u, .follow_redirects = true,
        .silent = true, .user_agent = "vexgraph/1.0 (Homebrew/Tooling Client)" };
}
#define DownloadOptions(...) CONSTRUCTOR_DISPATCH(DownloadOptions, __VA_ARGS__)
static inline fn DownloadOptions_setTimeout(DownloadOptions *options, u32 seconds) {
    if (options) (*options).timeout_seconds = seconds;
}
static inline fn DownloadOptions_setFollowRedirects(DownloadOptions *options, bool enabled) {
    if (options) (*options).follow_redirects = enabled;
}
static inline fn DownloadOptions_setSilent(DownloadOptions *options, bool enabled) {
    if (options) (*options).silent = enabled;
}
static inline fn DownloadOptions_setUserAgent(DownloadOptions *options, const char *agent) {
    if (options) (*options).user_agent = agent;
}

typedef struct DownloadResponse {
    int exit_code;
    u32 http_status;
    char *data;
    bytes size;
} DownloadResponse;

static inline DownloadResponse DownloadResponse_0() { return (DownloadResponse) {0}; }
#define DownloadResponse(...) CONSTRUCTOR_DISPATCH(DownloadResponse, __VA_ARGS__)

// Download URL to an output file path on disk.
bool Download_to_file(const char *url, const char *output_file, const DownloadOptions *opts);

// Download URL directly to memory buffer (caller frees with DownloadResponse_free).
bool Download_to_memory(const char *url, const DownloadOptions *opts, DownloadResponse *response_out);

fn DownloadResponse_free(DownloadResponse *res);

// Fetch URL through the cache system: checks cache first, and if missing,
// downloads and caches it under <subsystem>. Populates cached_path_out.
bool Download_fetch_cached(
    const char *url,
    const char *subsystem,
    u64 ttl_sec,
    const DownloadOptions *opts,
    char *cached_path_out,
    bytes path_cap
);

#ifdef __cplusplus
}
#endif

#endif // NET_DOWNLOAD_H
