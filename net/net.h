#ifndef NET_NET_H
#define NET_NET_H

// net/net.h — THE networking surface (Legacy: net/* as one).
//
// Everything network lives behind this single include under the Net_
// namespace: JSON documents, URL/auth synthesis, the HTTP client, the WebSocket
// client, and the micro server. Sub-modules stay includable individually; this
// header is the door engine code uses.

#include "net/frame_inbox.h"
#include "net/http.h"
#include "net/json.h"
#include "net/url.h"
#include "net/vex_ws_source.h"
#include "net/websocket.h"

// --- Combined conveniences: HTTP + JSON in one call each ---

// GET text into bodyOut (NUL-terminated). Returns HTTP status or -1 on
// transport failure.
int64_t Net_get(const char *host, int port, const char *path,
                char *bodyOut, bytes cap);

// GET and parse a JSON document in one shot using caller storage.
bool Net_getJson(const char *host, int port, const char *path,
                 JsonDoc *doc, JsonNode *nodes, u32 nodeCap,
                 char *scratch, u32 scratchCap);

// POST a pre-rendered JSON body; response lands in respBody. Returns HTTP
// status or -1 on transport failure.
int64_t Net_postJson(const char *host, int port, const char *path,
                     const char *jsonBody, char *respBody, bytes cap);

#endif
