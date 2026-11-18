// net/json.c — off-heap JSON documents (Legacy: net/JSON.java port).
//
// A single-pass recursive-descent parser over the caller's text. Depth is
// bounded by JSON_MAX_DEPTH; every pool exhaustion or syntax error flips
// JsonDoc_ok(doc) once and the parse unwinds. Escaped strings decode forward into
// the scratch arena so value views stay pointer-stable.

#include "vex.h"
#include "net/json.h"

#include <stdlib.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Json
 * ============================================================================
 * Off-heap JSON documents (Legacy: net/JSON.java): a single-pass
 * recursive-descent parser over the caller's text that writes into a
 * caller-owned node pool and scratch arena — zero allocation of its own.
 * Depth is bounded by JSON_MAX_DEPTH; every pool exhaustion or syntax error
 * flips JsonDoc_ok(doc) once and the parse unwinds. Escaped strings decode forward
 * into the scratch arena so value views stay pointer-stable and contiguous,
 * while unescaped strings view the source text in place (the source must
 * outlive all string views). A streaming writer (JsonWriter) emits
 * JSON.stringify-style output into a caller buffer with the same zero-alloc
 * discipline.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Json (net/json.c)
 * ============================================================================
 * off-heap JSON documents (Legacy: net/JSON.java).
 *
 * STRUCT FIELDS (Mirroring net/json.h + local to this file):
 * ----------------------------------------------------------------------------
 *   JsonNode {
 *     JsonType type; // node type tag
 *     JsonRef child; // first element/member
 *     JsonRef next; // next sibling
 *     u32 offset; // string/key start (in src, or scratch when inScratch)
 *     u32 length; // string/key length (decoded)
 *     bool inScratch; // true when offset points into the decode arena
 *     d64 number; // JSON_NUMBER payload
 *   }
 *   JsonDoc {
 *     JsonNode *nodes; // caller-owned pool
 *     u32 nodeCap; // node pool state
 *     u32 nodeCount; // live entry count
 *     const char *src; // input text (must outlive all string views)
 *     char *scratch; // caller-owned: decoded escape copies
 *     u32 scratchCap; // input/decode buffer state
 *     u32 scratchUsed; // input/decode buffer state
 *     JsonRef root; // root node ref
 *     bool ok; // parse result
 *   }
 *   Parser {
 *     JsonDoc *doc;              // destination document being built
 *     const char *p;             // input cursor
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - Json_parse(doc, nodes, nodeCap, scratch, scratchCap, text)
 *   - Json_root(doc)
 *   - Json_ok(doc)
 *   - Json_member(doc, obj, key)
 *   - Json_at(doc, node, index)
 *   - Json_count(doc, node)
 *   - Json_type(doc, ref)
 *   - Json_bool(doc, ref)
 *   - Json_number(doc, ref)
 *   - Json_string(doc, ref, lengthOut)
 *   - Json_key(doc, ref, lengthOut)
 *   - Json_has(doc, obj, key)
 *   - Json_path(doc, root, path)
 *   - Json_getBool(doc, ref, def)
 *   - Json_getNumber(doc, ref, def)
 *   - Json_getString(doc, ref)
 *   - Json_writeNumber(out, cap, v)
 *   - Json_writeString(out, cap, s)
 *   - Json_writeBool(out, cap, v)
 *   - JsonWriter(out, cap) + JsonWriter_ok/used
 *   - Json_beginObject/endObject, Json_beginArray/endArray
 *   - Json_addKey(w, k) + Json_stringVal/numberVal/boolVal/nullVal/rawVal
 * ============================================================================
 */



;;GETTER
static inline u32 JsonDoc_nodeCap(const JsonDoc *s) { return (*s).nodeCap; }
;;SETTER
static inline fn JsonDoc_setNodeCap(JsonDoc *s, u32 v) { (*s).nodeCap = v; }
;;GETTER
static inline u32 JsonDoc_nodeCount(const JsonDoc *s) { return (*s).nodeCount; }
;;GETTER
static inline u32 *JsonDoc_nodeCountMut(JsonDoc *s) { return &(*s).nodeCount; }
;;SETTER
static inline fn JsonDoc_setNodeCount(JsonDoc *s, u32 v) { (*s).nodeCount = v; }
;;GETTER
static inline JsonNode * JsonDoc_nodes(const JsonDoc *s) { return (*s).nodes; }
;;SETTER
static inline fn JsonDoc_setNodes(JsonDoc *s, JsonNode * v) { (*s).nodes = v; }
;;GETTER
static inline bool JsonDoc_ok(const JsonDoc *s) { return (*s).ok; }
;;SETTER
static inline fn JsonDoc_setOk(JsonDoc *s, bool v) { (*s).ok = v; }
;;GETTER
static inline JsonRef JsonDoc_root(const JsonDoc *s) { return (*s).root; }
;;SETTER
static inline fn JsonDoc_setRoot(JsonDoc *s, JsonRef v) { (*s).root = v; }
;;GETTER
static inline char * JsonDoc_scratch(const JsonDoc *s) { return (*s).scratch; }
;;SETTER
static inline fn JsonDoc_setScratch(JsonDoc *s, char * v) { (*s).scratch = v; }
;;GETTER
static inline u32 JsonDoc_scratchCap(const JsonDoc *s) { return (*s).scratchCap; }
;;SETTER
static inline fn JsonDoc_setScratchCap(JsonDoc *s, u32 v) { (*s).scratchCap = v; }
;;GETTER
static inline u32 JsonDoc_scratchUsed(const JsonDoc *s) { return (*s).scratchUsed; }
;;SETTER
static inline fn JsonDoc_setScratchUsed(JsonDoc *s, u32 v) { (*s).scratchUsed = v; }
;;GETTER
static inline const char * JsonDoc_src(const JsonDoc *s) { return (*s).src; }
;;SETTER
static inline fn JsonDoc_setSrc(JsonDoc *s, const char * v) { (*s).src = v; }

;;SETTER
static inline fn JsonNode_setChild(JsonNode *s, JsonRef v) { (*s).child = v; }
;;SETTER
static inline fn JsonNode_setNext(JsonNode *s, JsonRef v) { (*s).next = v; }

;;GETTER
static inline bytes JsonWriter_cap(const JsonWriter *s) { return (*s).cap; }
;;GETTER
static inline i32 JsonWriter_depth(const JsonWriter *s) { return (*s).depth; }
;;SETTER
static inline fn JsonWriter_setDepth(JsonWriter *s, i32 v) { (*s).depth = v; }
;;GETTER
static inline bool *JsonWriter_needCommaMut(JsonWriter *s) { return (*s).needComma; }
;;GETTER
static inline bool JsonWriter_okRaw(const JsonWriter *s) { return (*s).ok; }
;;SETTER
static inline fn JsonWriter_setOk(JsonWriter *s, bool v) { (*s).ok = v; }
;;GETTER
static inline char * JsonWriter_out(const JsonWriter *s) { return (*s).out; }
;;GETTER
static inline bytes JsonWriter_usedRaw(const JsonWriter *s) { return (*s).used; }
;;GETTER
static inline bytes *JsonWriter_usedMut(JsonWriter *s) { return &(*s).used; }
;;SETTER
static inline fn JsonWriter_setUsed(JsonWriter *s, bytes v) { (*s).used = v; }

typedef struct Parser {
    JsonDoc *doc;
    const char *p;   // cursor
} Parser;

;;GETTER
static inline JsonDoc * Parser_doc(const Parser *s) { return (*s).doc; }
;;GETTER
static inline const char * Parser_p(const Parser *s) { return (*s).p; }
;;SETTER
static inline fn Parser_setP(Parser *s, const char * v) { (*s).p = v; }


static JsonRef allocNode(Parser *ps) {
    JsonDoc *d = Parser_doc(ps);
    if (JsonDoc_nodeCount(d) >= JsonDoc_nodeCap(d)) return -1;
    JsonNode *n = &JsonDoc_nodes(d)[JsonDoc_nodeCount(d)];
    memset(n, 0, sizeof(*n));
    // Zero is a VALID ref (the root); terminals must be none-refs.
    JsonNode_setChild(n, -1);
    JsonNode_setNext(n, -1);
    return (JsonRef)(*JsonDoc_nodeCountMut(d))++;
}

static fn skipWs(Parser *ps) {
    while (*Parser_p(ps) == ' ' || *Parser_p(ps) == '\t' || *Parser_p(ps) == '\n' || *Parser_p(ps) == '\r')
        Parser_setP(ps, Parser_p(ps) + 1u);
}

static bool fail(JsonDoc *d) {
    JsonDoc_setOk(d, false);
    return false;
}

// --- Strings ---

// Decode a JSON string body starting after the opening quote. Unescaped
// input is viewed in place; any escape forces a scratch copy of the whole
// string so the returned view is always contiguous.
static bool parseStringBody(Parser *ps, u32 *offOut, u32 *lenOut, bool *scratchFlagOut) {
    JsonDoc *d = Parser_doc(ps);
    const char *start = Parser_p(ps);

    // Fast path: scan for the closing quote with no escapes.
    const char *q = start;
    while (*q && *q != '"' && *q != '\\') q++;
    if (*q == '"') {
        // No escapes: the view points straight into the caller's text.
        *offOut = (u32)(start - JsonDoc_src(d));
        *lenOut = (u32)(q - start);
        *scratchFlagOut = false;
        Parser_setP(ps, q + 1);
        return true;
    }
    if (!*q) return fail(d);

    // Escape present: copy-decode into scratch.
    char *dst = JsonDoc_scratch(d) + JsonDoc_scratchUsed(d);
    bytes capLeft = JsonDoc_scratchCap(d) - JsonDoc_scratchUsed(d);
    char *out = dst;
    Parser_setP(ps, start);
    while (*Parser_p(ps) && *Parser_p(ps) != '"') {
        char c = *Parser_p(ps);
        if (c == '\\') {
            Parser_setP(ps, Parser_p(ps) + 1u); // step onto the escape designator
            switch (*Parser_p(ps)) {
                case '"': c = '"'; Parser_setP(ps, Parser_p(ps) + 1); break;
                case '\\': c = '\\'; Parser_setP(ps, Parser_p(ps) + 1); break;
                case '/': c = '/'; Parser_setP(ps, Parser_p(ps) + 1); break;
                case 'b': c = '\b'; Parser_setP(ps, Parser_p(ps) + 1); break;
                case 'f': c = '\f'; Parser_setP(ps, Parser_p(ps) + 1); break;
                case 'n': c = '\n'; Parser_setP(ps, Parser_p(ps) + 1); break;
                case 'r': c = '\r'; Parser_setP(ps, Parser_p(ps) + 1); break;
                case 't': c = '\t'; Parser_setP(ps, Parser_p(ps) + 1); break;
                case 'u': {
                    // \uXXXX => UTF-8 (BMP only)
                    u32 cp = 0;
                    for (i32 i = 0; i < 4; i++) {
                        Parser_setP(ps, Parser_p(ps) + 1u);
                        char h = *Parser_p(ps);
                        cp <<= 4;
                        if (h >= '0' && h <= '9') cp |= (u32)(h - '0');
                        else if (h >= 'a' && h <= 'f') cp |= (u32)(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') cp |= (u32)(h - 'A' + 10);
                        else return fail(d);
                    }
                    Parser_setP(ps, Parser_p(ps) + 1u); // past the last hex digit
                    if (cp < 0x80) {
                        if ((bytes)(out - dst) + 1 > capLeft) return fail(d);
                        *out++ = (char)cp;
                    } else if (cp < 0x800) {
                        if ((bytes)(out - dst) + 2 > capLeft) return fail(d);
                        *out++ = (char)(0xC0 | (cp >> 6));
                        *out++ = (char)(0x80 | (cp & 0x3F));
                    } else {
                        if ((bytes)(out - dst) + 3 > capLeft) return fail(d);
                        *out++ = (char)(0xE0 | (cp >> 12));
                        *out++ = (char)(0x80 | ((cp >> 6) & 0x3F));
                        *out++ = (char)(0x80 | (cp & 0x3F));
                    }
                    continue;
                }
                default: return fail(d);
            }
        } else {
            Parser_setP(ps, Parser_p(ps) + 1u); // consume the plain character
        }
        if ((bytes)(out - dst) + 1 > capLeft) return fail(d);
        *out++ = c;
    }
    if (*Parser_p(ps) != '"') return fail(d);
    Parser_setP(ps, Parser_p(ps) + 1u); // closing quote

    *offOut = (u32)(dst - JsonDoc_scratch(d));
    *lenOut = (u32)(out - dst);
    *scratchFlagOut = true;
    JsonDoc_setScratchUsed(d, JsonDoc_scratchUsed(d) + ((u32)(out - dst) + 1)); // keep NUL space per string
    if (JsonDoc_scratchUsed(d) < JsonDoc_scratchCap(d)) JsonDoc_scratch(d)[JsonDoc_scratchUsed(d) - 1] = '\0';
    return true;
}

// --- Values ---

static bool parseValue(Parser *ps, JsonRef *out, i32 depth);

static bool parseObject(Parser *ps, JsonRef *out, i32 depth) {
    JsonDoc *d = Parser_doc(ps);
    JsonRef obj = allocNode(ps);
    if (obj < 0) return fail(d);
    JsonDoc_nodes(d)[obj].type = JSON_OBJECT;
    Parser_setP(ps, Parser_p(ps) + 1u); // '{'
    skipWs(ps);

    JsonRef tail = -1;
    if (*Parser_p(ps) == '}') { Parser_setP(ps, Parser_p(ps) + 1); *out = obj; return true; }

    while (true) {
        skipWs(ps);
        if (*Parser_p(ps) != '"') return fail(d);
        Parser_setP(ps, Parser_p(ps) + 1u);
        u32 kOff, kLen;
        bool kScratch;
        if (!parseStringBody(ps, &kOff, &kLen, &kScratch)) return fail(d);

        skipWs(ps);
        if (*Parser_p(ps) != ':') return fail(d);
        Parser_setP(ps, Parser_p(ps) + 1u);

        JsonRef member = allocNode(ps);
        if (member < 0) return fail(d);
        JsonDoc_nodes(d)[member].offset = kOff;
        JsonDoc_nodes(d)[member].length = kLen;
        JsonDoc_nodes(d)[member].inScratch = kScratch;
        if (!parseValue(ps, &JsonDoc_nodes(d)[member].child, depth + 1)) return fail(d);

        if (tail < 0) JsonDoc_nodes(d)[obj].child = member;
        else JsonDoc_nodes(d)[tail].next = member;
        tail = member;

        skipWs(ps);
        if (*Parser_p(ps) == ',') { Parser_setP(ps, Parser_p(ps) + 1); continue; }
        if (*Parser_p(ps) == '}') { Parser_setP(ps, Parser_p(ps) + 1); break; }
        return fail(d);
    }
    *out = obj;
    return true;
}

static bool parseArray(Parser *ps, JsonRef *out, i32 depth) {
    JsonDoc *d = Parser_doc(ps);
    JsonRef arr = allocNode(ps);
    if (arr < 0) return fail(d);
    JsonDoc_nodes(d)[arr].type = JSON_ARRAY;
    Parser_setP(ps, Parser_p(ps) + 1u); // '['
    skipWs(ps);

    JsonRef tail = -1;
    if (*Parser_p(ps) == ']') { Parser_setP(ps, Parser_p(ps) + 1); *out = arr; return true; }

    while (true) {
        JsonRef element = allocNode(ps);
        if (element < 0) return fail(d);
        if (!parseValue(ps, &JsonDoc_nodes(d)[element].child, depth + 1)) return fail(d);

        if (tail < 0) JsonDoc_nodes(d)[arr].child = element;
        else JsonDoc_nodes(d)[tail].next = element;
        tail = element;

        skipWs(ps);
        if (*Parser_p(ps) == ',') { Parser_setP(ps, Parser_p(ps) + 1); continue; }
        if (*Parser_p(ps) == ']') { Parser_setP(ps, Parser_p(ps) + 1); break; }
        return fail(d);
    }
    *out = arr;
    return true;
}

static bool parseValue(Parser *ps, JsonRef *out, i32 depth) {
    JsonDoc *d = Parser_doc(ps);
    if (depth > JSON_MAX_DEPTH) return fail(d);
    skipWs(ps);

    char c = *Parser_p(ps);
    if (c == '{') return parseObject(ps, out, depth);
    if (c == '[') return parseArray(ps, out, depth);

    if (c == '"') {
        Parser_setP(ps, Parser_p(ps) + 1u);
        u32 off, len;
        bool scratch;
        if (!parseStringBody(ps, &off, &len, &scratch)) return fail(d);
        JsonRef s = allocNode(ps);
        if (s < 0) return fail(d);
        JsonDoc_nodes(d)[s].type = JSON_STRING;
        JsonDoc_nodes(d)[s].offset = off;
        JsonDoc_nodes(d)[s].length = len;
        JsonDoc_nodes(d)[s].inScratch = scratch;
        *out = s;
        return true;
    }

    if (c == 't') {
        if (strncmp(Parser_p(ps), "true", 4) != 0) return fail(d);
        Parser_setP(ps, Parser_p(ps) + (4));
        JsonRef v = allocNode(ps);
        if (v < 0) return fail(d);
        JsonDoc_nodes(d)[v].type = JSON_TRUE;
        *out = v;
        return true;
    }
    if (c == 'f') {
        if (strncmp(Parser_p(ps), "false", 5) != 0) return fail(d);
        Parser_setP(ps, Parser_p(ps) + (5));
        JsonRef v = allocNode(ps);
        if (v < 0) return fail(d);
        JsonDoc_nodes(d)[v].type = JSON_FALSE;
        *out = v;
        return true;
    }
    if (c == 'n') {
        if (strncmp(Parser_p(ps), "null", 4) != 0) return fail(d);
        Parser_setP(ps, Parser_p(ps) + (4));
        JsonRef v = allocNode(ps);
        if (v < 0) return fail(d);
        JsonDoc_nodes(d)[v].type = JSON_NULL;
        *out = v;
        return true;
    }

    // Number: strtod handles sign/exponent/fraction in one bite.
    char *end = nullptr;
    d64 num = strtod(Parser_p(ps), &end);
    if (end == Parser_p(ps)) return fail(d);
    JsonRef v = allocNode(ps);
    if (v < 0) return fail(d);
    JsonDoc_nodes(d)[v].type = JSON_NUMBER;
    JsonDoc_nodes(d)[v].number = num;
    Parser_setP(ps, end);
    *out = v;
    return true;
}

bool Json_parse(JsonDoc *doc, JsonNode *nodes, u32 nodeCap,
                char *scratch, u32 scratchCap, const char *text) {
    JsonDoc_setNodes(doc, nodes);
    JsonDoc_setNodeCap(doc, nodeCap);
    JsonDoc_setNodeCount(doc, 0);
    JsonDoc_setSrc(doc, text);
    JsonDoc_setScratch(doc, scratch);
    JsonDoc_setScratchCap(doc, scratchCap);
    JsonDoc_setScratchUsed(doc, 0);
    JsonDoc_setRoot(doc, -1);
    JsonDoc_setOk(doc, true);

    Parser ps = { .doc = doc, .p = text };
    JsonRef root = -1;
    if (!parseValue(&ps, &root, 0)) {
        JsonDoc_setOk(doc, false);
        return false;
    }
    skipWs(&ps);
    if (*ps.p != '\0')
        JsonDoc_setOk(doc, false); // trailing garbage
    JsonDoc_setRoot(doc, root);
    return JsonDoc_ok(doc);
}

JsonRef Json_root(const JsonDoc *doc) {
    return JsonDoc_root(doc);
}

bool Json_ok(const JsonDoc *doc) {
    return JsonDoc_ok(doc);
}

JsonRef Json_member(const JsonDoc *doc, JsonRef obj, const char *key) {
    if (obj < 0 || JsonDoc_nodes(doc)[obj].type != JSON_OBJECT) return -1;
    bytes klen = strlen(key);
    for (JsonRef m = JsonDoc_nodes(doc)[obj].child; m >= 0; m = JsonDoc_nodes(doc)[m].next) {
        u32 len = 0;
        const char *base = Json_key(doc, m, &len);
        if (len == klen && memcmp(base, key, klen) == 0)
            return JsonDoc_nodes(doc)[m].child;
    }
    return -1;
}

JsonRef Json_at(const JsonDoc *doc, JsonRef node, u32 index) {
    if (node < 0) return -1;
    JsonType t = JsonDoc_nodes(doc)[node].type;
    if (t != JSON_ARRAY && t != JSON_OBJECT) return -1;
    u32 i = 0;
    for (JsonRef e = JsonDoc_nodes(doc)[node].child; e >= 0; e = JsonDoc_nodes(doc)[e].next, i++)
        if (i == index) return JsonDoc_nodes(doc)[e].child;
    return -1;
}

u32 Json_count(const JsonDoc *doc, JsonRef node) {
    if (node < 0) return 0;
    JsonType t = JsonDoc_nodes(doc)[node].type;
    if (t != JSON_ARRAY && t != JSON_OBJECT) return 0;
    u32 n = 0;
    for (JsonRef e = JsonDoc_nodes(doc)[node].child; e >= 0; e = JsonDoc_nodes(doc)[e].next) n++;
    return n;
}

JsonType Json_type(const JsonDoc *doc, JsonRef ref) {
    if (ref < 0) return JSON_NULL;
    return JsonDoc_nodes(doc)[ref].type;
}

bool Json_bool(const JsonDoc *doc, JsonRef ref) {
    if (ref < 0) return false;
    return JsonDoc_nodes(doc)[ref].type == JSON_TRUE;
}

d64 Json_number(const JsonDoc *doc, JsonRef ref) {
    if (ref < 0) return 0.0;
    return JsonDoc_nodes(doc)[ref].number;
}

const char *Json_string(const JsonDoc *doc, JsonRef ref, u32 *lengthOut) {
    if (lengthOut) *lengthOut = 0;
    if (ref < 0 || JsonDoc_nodes(doc)[ref].type != JSON_STRING) return "";
    if (lengthOut) *lengthOut = JsonDoc_nodes(doc)[ref].length;
    if (JsonDoc_nodes(doc)[ref].inScratch)
        return JsonDoc_scratch(doc) + JsonDoc_nodes(doc)[ref].offset;
    return JsonDoc_src(doc) + JsonDoc_nodes(doc)[ref].offset;
}

const char *Json_key(const JsonDoc *doc, JsonRef ref, u32 *lengthOut) {
    if (lengthOut) *lengthOut = 0;
    if (ref < 0) return "";
    if (lengthOut) *lengthOut = JsonDoc_nodes(doc)[ref].length;
    if (JsonDoc_nodes(doc)[ref].inScratch)
        return JsonDoc_scratch(doc) + JsonDoc_nodes(doc)[ref].offset;
    return JsonDoc_src(doc) + JsonDoc_nodes(doc)[ref].offset;
}

// --- Writer ---

i64 Json_writeNumber(char *out, bytes cap, d64 v) {
    i64 asInt = (i64)v;
    if ((d64)asInt == v)
        return (i64)snprintf(out, cap, "%lld", asInt);
    return (i64)snprintf(out, cap, "%.17g", v);
}

i64 Json_writeString(char *out, bytes cap, const char *s) {
    bytes used = 0;
    if (cap < 2) return -1;
    out[used++] = '"';
    for (const char *p = s; *p; p++) {
        char c = *p;
        const char *esc = nullptr;
        switch (c) {
            case '"': esc = "\\\""; break;
            case '\\': esc = "\\\\"; break;
            case '\n': esc = "\\n"; break;
            case '\r': esc = "\\r"; break;
            case '\t': esc = "\\t"; break;
            case '\b': esc = "\\b.sh"; break;
            case '\f': esc = "\\f"; break;
            default: break;
        }
        if (esc) {
            bytes l = strlen(esc);
            if (used + l >= cap) return -1;
            memcpy(out + used, esc, l);
            used += l;
        } else {
            if (used + 1 >= cap) return -1;
            out[used++] = c;
        }
    }
    if (used + 2 >= cap) return -1;
    out[used++] = '"';
    out[used] = '\0';
    return (i64)used;
}

i64 Json_writeBool(char *out, bytes cap, bool v) {
    return (i64)snprintf(out, cap, "%s", v ? "true" : "false");
}

// --- JS-flex helpers (defaults + paths, still zero-alloc) ---

bool Json_has(const JsonDoc *doc, JsonRef obj, const char *key) {
    if (!doc || obj < 0 || !key)
        return false;
    return Json_member(doc, obj, key) >= 0;
}

JsonRef Json_path(const JsonDoc *doc, JsonRef root, const char *path) {
    if (!doc || root < 0 || !path || (*path) == '\0')
        return -1;
    JsonRef cur = root;
    const char *p = path;
    char keyBuf[128];
    while (*p) {
        if ((*p) == '.') {
            p++;
            continue;
        }
        if ((*p) != '[') {
            bytes klen = 0;
            while (*p && (*p) != '.' && (*p) != '[') {
                if (klen + 1 < sizeof(keyBuf))
                    keyBuf[klen++] = *p;
                else
                    return -1;
                p++;
            }
            keyBuf[klen] = '\0';
            if (klen > 0) {
                if (cur < 0 || JsonDoc_nodes(doc)[cur].type != JSON_OBJECT)
                    return -1;
                cur = Json_member(doc, cur, keyBuf);
                if (cur < 0)
                    return -1;
            }
            continue;
        }
        p++;
        u32 idx = 0;
        bool anyDigit = false;
        while (*p >= '0' && (*p) <= '9') {
            anyDigit = true;
            idx = idx * 10 + (u32)((*p) - '0');
            p++;
        }
        if (!anyDigit || (*p) != ']')
            return -1;
        p++;
        cur = Json_at(doc, cur, idx);
        if (cur < 0)
            return -1;
    }
    return cur;
}

bool Json_getBool(const JsonDoc *doc, JsonRef ref, bool def) {
    if (!doc || ref < 0)
        return def;
    JsonType t = JsonDoc_nodes(doc)[ref].type;
    if (t == JSON_TRUE)
        return true;
    if (t == JSON_FALSE)
        return false;
    return def;
}

d64 Json_getNumber(const JsonDoc *doc, JsonRef ref, d64 def) {
    if (!doc || ref < 0)
        return def;
    if (JsonDoc_nodes(doc)[ref].type != JSON_NUMBER)
        return def;
    return JsonDoc_nodes(doc)[ref].number;
}

JsonStringView Json_getString(const JsonDoc *doc, JsonRef ref) {
    JsonStringView empty = { nullptr, 0u };
    if (!doc)
        return empty;
    if (ref < 0 || JsonDoc_nodes(doc)[ref].type != JSON_STRING)
        return empty;
    JsonStringView view;
    view.data = Json_string(doc, ref, &view.length);
    return view;
}

// --- Streaming builder (~ JSON.stringify) ---

static bool writerFail(JsonWriter *w) {
    JsonWriter_setOk(w, false);
    return false;
}

static bool writerReserve(JsonWriter *w, bytes n) {
    if (!JsonWriter_ok(w))
        return false;
    if (!JsonWriter_out(w) || JsonWriter_used(w) + n >= JsonWriter_cap(w))
        return writerFail(w);
    return true;
}

static bool writerPut(JsonWriter *w, char c) {
    if (!writerReserve(w, 1))
        return false;
    JsonWriter_out(w)[(*JsonWriter_usedMut(w))++] = c;
    JsonWriter_out(w)[JsonWriter_used(w)] = '\0';
    return true;
}

static bool writerAppend(JsonWriter *w, const char *s, bytes n) {
    if (n == 0)
        return true;
    if (!writerReserve(w, n))
        return false;
    memcpy(JsonWriter_out(w) + JsonWriter_used(w), s, n);
    JsonWriter_setUsed(w, JsonWriter_used(w) + (n));
    JsonWriter_out(w)[JsonWriter_used(w)] = '\0';
    return true;
}

static bool writerStringBody(JsonWriter *w, const char *s) {
    for (const char *p = s; *p; p++) {
        char c = *p;
        const char *esc = nullptr;
        switch (c) {
            case '"': esc = "\\\""; break;
            case '\\': esc = "\\\\"; break;
            case '\n': esc = "\\n"; break;
            case '\r': esc = "\\r"; break;
            case '\t': esc = "\\t"; break;
            case '\b': esc = "\\b.sh"; break;
            case '\f': esc = "\\f"; break;
            default: break;
        }
        if (esc) {
            if (!writerAppend(w, esc, strlen(esc)))
                return false;
        } else {
            if (!writerPut(w, c))
                return false;
        }
    }
    return true;
}

JsonWriter JsonWriter_2(char *out, bytes cap) {
    JsonWriter result;
    memset(&result, 0, sizeof result);
    result.out = out;
    result.cap = cap;
    result.used = 0u;
    result.ok = (out != nullptr && cap > 0u);
    for (i32 i = 0; i < JSON_MAX_DEPTH; i++)
        result.needComma[i] = false;
    if (result.ok)
        out[0] = '\0';
    return result;
}

bool JsonWriter_ok(const JsonWriter *w) {
    if (!w)
        return false;
    return JsonWriter_okRaw(w);
}

bytes JsonWriter_used(const JsonWriter *w) {
    if (!w)
        return 0;
    return JsonWriter_usedRaw(w);
}

static bool writerValueOpen(JsonWriter *w) {
    if (!JsonWriter_ok(w))
        return false;
    if (JsonWriter_depth(w) == 0)
        return true;
    if (JsonWriter_depth(w) < 0 || JsonWriter_depth(w) > JSON_MAX_DEPTH)
        return writerFail(w);
    i32 parent = JsonWriter_depth(w) - 1;
    if (JsonWriter_needCommaMut(w)[parent]) {
        if (!writerPut(w, ','))
            return false;
        JsonWriter_needCommaMut(w)[parent] = false;
    }
    return true;
}

static bool writerValueClose(JsonWriter *w) {
    if (JsonWriter_depth(w) == 0)
        return JsonWriter_ok(w);
    i32 parent = JsonWriter_depth(w) - 1;
    JsonWriter_needCommaMut(w)[parent] = true;
    return JsonWriter_ok(w);
}

bool Json_beginObject(JsonWriter *w) {
    if (!w || !writerValueOpen(w))
        return false;
    if (!writerPut(w, '{'))
        return false;
    if (JsonWriter_depth(w) < 0 || JsonWriter_depth(w) >= JSON_MAX_DEPTH)
        return writerFail(w);
    JsonWriter_needCommaMut(w)[JsonWriter_depth(w)] = false;
    JsonWriter_setDepth(w, JsonWriter_depth(w) + 1u);
    return true;
}

bool Json_endObject(JsonWriter *w) {
    if (!w || !JsonWriter_ok(w))
        return false;
    if (JsonWriter_depth(w) <= 0)
        return writerFail(w);
    if (!writerPut(w, '}'))
        return false;
    JsonWriter_setDepth(w, JsonWriter_depth(w) - 1u);
    return writerValueClose(w);
}

bool Json_beginArray(JsonWriter *w) {
    if (!w || !writerValueOpen(w))
        return false;
    if (!writerPut(w, '['))
        return false;
    if (JsonWriter_depth(w) < 0 || JsonWriter_depth(w) >= JSON_MAX_DEPTH)
        return writerFail(w);
    JsonWriter_needCommaMut(w)[JsonWriter_depth(w)] = false;
    JsonWriter_setDepth(w, JsonWriter_depth(w) + 1u);
    return true;
}

bool Json_endArray(JsonWriter *w) {
    if (!w || !JsonWriter_ok(w))
        return false;
    if (JsonWriter_depth(w) <= 0)
        return writerFail(w);
    if (!writerPut(w, ']'))
        return false;
    JsonWriter_setDepth(w, JsonWriter_depth(w) - 1u);
    return writerValueClose(w);
}

bool Json_addKey(JsonWriter *w, const char *k) {
    if (!w || !k || !JsonWriter_ok(w))
        return false;
    if (JsonWriter_depth(w) <= 0)
        return writerFail(w);
    i32 idx = JsonWriter_depth(w) - 1;
    if (JsonWriter_needCommaMut(w)[idx]) {
        if (!writerPut(w, ','))
            return false;
    }
    if (!writerPut(w, '"'))
        return false;
    if (!writerStringBody(w, k))
        return false;
    if (!writerAppend(w, "\":", 2))
        return false;
    JsonWriter_needCommaMut(w)[idx] = false;
    return true;
}

bool Json_stringVal(JsonWriter *w, const char *s) {
    if (!w || !s || !writerValueOpen(w))
        return false;
    if (!writerPut(w, '"'))
        return false;
    if (!writerStringBody(w, s))
        return false;
    if (!writerPut(w, '"'))
        return false;
    return writerValueClose(w);
}

bool Json_numberVal(JsonWriter *w, d64 v) {
    if (!w || !writerValueOpen(w))
        return false;
    char tmp[32];
    i64 asInt = (i64)v;
    i32 n = 0;
    if ((d64)asInt == v)
        n = snprintf(tmp, sizeof(tmp), "%lld", asInt);
    else
        n = snprintf(tmp, sizeof(tmp), "%.17g", v);
    if (n <= 0 || (bytes)n >= sizeof(tmp))
        return writerFail(w);
    if (!writerAppend(w, tmp, (bytes)n))
        return false;
    return writerValueClose(w);
}

bool Json_boolVal(JsonWriter *w, bool v) {
    if (!w || !writerValueOpen(w))
        return false;
    if (!writerAppend(w, v ? "true" : "false", v ? 4 : 5))
        return false;
    return writerValueClose(w);
}

bool Json_nullVal(JsonWriter *w) {
    if (!w || !writerValueOpen(w))
        return false;
    if (!writerAppend(w, "null", 4))
        return false;
    return writerValueClose(w);
}

bool Json_rawVal(JsonWriter *w, const char *raw) {
    if (!w || !raw || !writerValueOpen(w))
        return false;
    if (!writerAppend(w, raw, strlen(raw)))
        return false;
    return writerValueClose(w);
}
