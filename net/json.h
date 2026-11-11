#ifndef NET_JSON_H
#define NET_JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "lang/constructor.h"

// net/json.h — off-heap JSON documents (Legacy: net/JSON.java).
//
// Parses into a CALLER-OWNED document: a fixed node pool plus two fixed text
// arenas (keys+strings share one, numbers none). No malloc anywhere — a
// document that does not fit simply reports failure, and the embedder sizes
// the pool for its payload class. Values are views INTO your input buffer;
// only escaped strings are copied (into the scratch arena) during parse.
//
// Writer builds into a caller byte buffer the same way.

// Recursive-descent depth bound. This is a C-STACK SAFETY limit, not a payload
// policy: parseValue checks depth at entry and FAILS CLOSED beyond it, so no input
// can recurse further and overflow the stack. 32 matches the host admission bound.
// Need deeper documents? add a separate NON-RECURSIVE parse entry rather than
// raising this constant.
#define JSON_MAX_DEPTH 32

typedef enum {
    JSON_NULL = 0,
    JSON_FALSE,
    JSON_TRUE,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT,
} JsonType;

typedef i32 JsonRef; // index into the doc's node pool; <0 = none

typedef struct JsonStringView {
    const char *data;
    u32 length;
} JsonStringView;

typedef struct JsonNode {
    JsonType type;
    JsonRef child;     // first element/member
    JsonRef next;      // next sibling
    u32 offset;   // string/key start (in src, or scratch when inScratch)
    u32 length;   // string/key length (decoded)
    bool inScratch;    // true when offset points into the decode arena
    d64 number;     // JSON_NUMBER payload
} JsonNode;

typedef struct JsonDoc {
    JsonNode *nodes;       // caller-owned pool
    u32 nodeCap;
    u32 nodeCount;

    const char *src;       // input text (must outlive all string views)
    char *scratch;         // caller-owned: decoded escape copies
    u32 scratchCap;
    u32 scratchUsed;

    JsonRef root;
    bool ok;               // parse result
} JsonDoc;

// Parse `text` (NUL-terminated) using caller storage. Returns (*doc).ok.
bool Json_parse(JsonDoc *doc, JsonNode *nodes, u32 nodeCap,
                char *scratch, u32 scratchCap, const char *text);

// --- Navigation ---
JsonRef Json_root(const JsonDoc *doc);
bool Json_ok(const JsonDoc *doc);

// Object member lookup by key (objects only). JSON_NONE-ref when missing.
JsonRef Json_member(const JsonDoc *doc, JsonRef obj, const char *key);
// Array/object element by ordinal; JSON_NONE-ref when _out of range.
JsonRef Json_at(const JsonDoc *doc, JsonRef node, u32 index);
u32 Json_count(const JsonDoc *doc, JsonRef node); // array/object arity

JsonType Json_type(const JsonDoc *doc, JsonRef ref);
bool Json_bool(const JsonDoc *doc, JsonRef ref);
d64 Json_number(const JsonDoc *doc, JsonRef ref);
// String view (NUL-terminated when it came through scratch; use the length).
const char *Json_string(const JsonDoc *doc, JsonRef ref, u32 *lengthOut);
// Object member key of a node.
const char *Json_key(const JsonDoc *doc, JsonRef ref, u32 *lengthOut);

// --- JS-flex helpers (defaults + paths, still zero-alloc) ---
// Member-exists check (objects only). False on NULL/missing/wrong type.
bool Json_has(const JsonDoc *doc, JsonRef obj, const char *key);
// Dotted path lookup: "a.b.sh[0].c" (~ _.get). Array suffixes repeat
// ("m[0][1]"). Returns value ref or -1 when any hop misses.
JsonRef Json_path(const JsonDoc *doc, JsonRef root, const char *path);
// Value getters with JS `??` defaults: wrong-type/missing ref -> def.
bool Json_getBool(const JsonDoc *doc, JsonRef ref, bool def);
d64 Json_getNumber(const JsonDoc *doc, JsonRef ref, d64 def);
// Borrow a JSON_STRING view. The data is length-delimited, not necessarily NUL
// terminated. Invalid refs and wrong types return {nullptr, 0}.
JsonStringView Json_getString(const JsonDoc *doc, JsonRef ref);

// --- Writer ---
// Appends a JSON rendering of value into _out (NUL-terminated). Strings are
// escaped. Returns Bytes written (excluding NUL) or -1 when _out ran _out.
int64_t Json_writeNumber(char *out, bytes cap, d64 v);
int64_t Json_writeString(char *out, bytes cap, const char *s);
int64_t Json_writeBool(char *out, bytes cap, bool v);

// Streaming builder (~ JSON.stringify): appends into caller storage,
// bounded, zero-alloc. Any overflow flips ok once and later calls no-op.
typedef struct JsonWriter {
    char *out;
    bytes cap;
    bytes used;
    bool ok;
    int depth;
    bool needComma[JSON_MAX_DEPTH];
} JsonWriter;

JsonWriter JsonWriter_2(char *out, bytes cap);
#define JsonWriter(...) CONSTRUCTOR_DISPATCH(JsonWriter, __VA_ARGS__)
bool JsonWriter_ok(const JsonWriter *w);
bytes JsonWriter_used(const JsonWriter *w);
bool Json_beginObject(JsonWriter *w);
bool Json_endObject(JsonWriter *w);
bool Json_beginArray(JsonWriter *w);
bool Json_endArray(JsonWriter *w);
bool Json_addKey(JsonWriter *w, const char *k);
bool Json_stringVal(JsonWriter *w, const char *s);
bool Json_numberVal(JsonWriter *w, d64 v);
bool Json_boolVal(JsonWriter *w, bool v);
bool Json_nullVal(JsonWriter *w);
bool Json_rawVal(JsonWriter *w, const char *raw);

#endif
