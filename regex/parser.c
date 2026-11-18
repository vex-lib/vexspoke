// regex/parser.c — the RegexParser: pattern bytes to a flat AST plus bitmaps.

#include "vex.h"
#include "regex/parser.h"

#include "nio/mem.h"
#include "oop/vexspoke_type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: RegexParser
 * ============================================================================
 * A recursive-descent parser that turns pattern bytes into the flat AST in
 * regex/nodelist.h plus the character-class bitmaps in regex/charclass.h. It
 * exists as its own unit because parsing is where every hostile input arrives:
 * an unbalanced bracket, a malformed quantifier, an inverted range, an
 * unsupported group — all of them are decided here, once, before any program
 * is emitted. Each rejection is recorded as one reason string and the parse
 * stops producing anything usable (the Cold-Strict, Hot-Minimal Validation
 * Law's cold half: validate exhaustively where input enters).
 *
 * Precedence, lowest to highest: alternation, concatenation, repetition, atom.
 * Concatenation and alternation are built by LOOPS (left-nested), so only
 * group nesting recurses — and that depth is capped at REGEX_MAX_DEPTH, which
 * keeps the C stack bounded on a pathological pattern like '((((...'.
 *
 * memory: owns the node list and a doubling class-bitmap array, both in the
 * engine arena stamped TYPE_REGEX. Case folding is resolved here from
 * ignoreCase and baked into the nodes and bitmaps, so nothing downstream ever
 * folds. Lifetime: one parser per compile call; the caller takes the classes
 * and frees the rest.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: RegexParser (regex/parser.c — defined in regex/parser.h)
 * ============================================================================
 * Pattern bytes -> AST nodes + class bitmaps, with one recorded reason per
 * rejection.
 *
 * STRUCT FIELDS (Mirroring regex/parser.h):
 * ----------------------------------------------------------------------------
 *   RegexParser {
 *     const char     *pattern;       // borrowed pattern text (not owned)
 *     bytes          length;        // pattern byte length
 *     bytes          pos;           // parse cursor
 *     bool            ignoreCase;    // fold ASCII case while building the AST
 *     RegexNodeList   nodes;         // the AST being built (owned)
 *     RegexCharClass *classes;       // growable class bitmaps (owned)
 *     u32        classCount;    // classes in use
 *     u32        classCapacity; // classes allocated
 *     u32        groupCount;    // capturing groups seen
 *     u32        depth;         // current group nesting depth
 *     bool            failed;        // a rejection was recorded
 *     const char     *error;         // first rejection reason (static text)
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Core Functions: (.h)
 *   - RegexParser(pattern, length, ignoreCase)
 *   - RegexParser_parse(parser)
 *   - RegexParser_ok(parser)
 *   - RegexParser_error(parser)
 *   - RegexParser_nodes(parser)
 *   - RegexParser_groupCount(parser)
 *   - RegexParser_releaseClasses(parser, outCount)
 *   - RegexParser_free(parser)
 *
 * Private Core Functions: (.c static)
 *   - lowerByte(c) / isAsciiDigit(c) / parserFail(parser, message)
 *   - parserNode(parser, kind) / parserAddClass(parser)
 *   - parseEscapeByte(parser, outByte)
 *   - parseGroup / parseClass / parseAtom / parseBraces / parseRepeat /
 *     parseConcat / parseAlternation
 *
 * Admission, ownership, failure:
 *   - init zeroes the parser and allocates nothing; a null pattern is the
 *     caller's misuse and outside this unit's contract.
 *   - parse returns the root index or REGEX_NODE_NONE; ok/error report the
 *     single recorded reason. Unsupported alphabetic escapes and stacked
 *     quantifiers reject instead of changing meaning; punctuation stays literal.
 *     A failed parse leaves nodes/classes allocated
 *     for free() and nothing usable for the caller.
 *   - releaseClasses transfers ownership once; a second call yields nullptr.
 *   - free releases the AST and any un-transferred class array, then zeroes
 *     the parser; null-safe.
 * ============================================================================
 */


;;SETTER
static inline fn RegexNode_setA(RegexNode *s, u32 v) { (*s).a = v; }
;;SETTER
static inline fn RegexNode_setB(RegexNode *s, u32 v) { (*s).b = v; }
;;SETTER
static inline fn RegexNode_setByte(RegexNode *s, u8 v) { (*s).byte = v; }
;;SETTER
static inline fn RegexNode_setClsIndex(RegexNode *s, u32 v) { (*s).clsIndex = v; }
;;SETTER
static inline fn RegexNode_setGroup(RegexNode *s, u32 v) { (*s).group = v; }
;;SETTER
static inline fn RegexNode_setRepMax(RegexNode *s, u32 v) { (*s).repMax = v; }
;;SETTER
static inline fn RegexNode_setRepMin(RegexNode *s, u32 v) { (*s).repMin = v; }

;;GETTER
static inline u32 RegexParser_classCapacity(const RegexParser *s) { return (*s).classCapacity; }
;;SETTER
static inline fn RegexParser_setClassCapacity(RegexParser *s, u32 v) { (*s).classCapacity = v; }
;;GETTER
static inline u32 RegexParser_classCount(const RegexParser *s) { return (*s).classCount; }
;;GETTER
static inline u32 *RegexParser_classCountMut(RegexParser *s) { return &(*s).classCount; }
;;SETTER
static inline fn RegexParser_setClassCount(RegexParser *s, u32 v) { (*s).classCount = v; }
;;GETTER
static inline RegexCharClass * RegexParser_classes(const RegexParser *s) { return (*s).classes; }
;;SETTER
static inline fn RegexParser_setClasses(RegexParser *s, RegexCharClass * v) { (*s).classes = v; }
;;GETTER
static inline u32 RegexParser_depth(const RegexParser *s) { return (*s).depth; }
;;SETTER
static inline fn RegexParser_setDepth(RegexParser *s, u32 v) { (*s).depth = v; }
;;GETTER
static inline const char * RegexParser_errorRaw(const RegexParser *s) { return (*s).error; }
;;SETTER
static inline fn RegexParser_setError(RegexParser *s, const char * v) { (*s).error = v; }
;;GETTER
static inline bool RegexParser_failed(const RegexParser *s) { return (*s).failed; }
;;SETTER
static inline fn RegexParser_setFailed(RegexParser *s, bool v) { (*s).failed = v; }
;;GETTER
static inline u32 RegexParser_groupCountRaw(const RegexParser *s) { return (*s).groupCount; }
;;GETTER
static inline u32 *RegexParser_groupCountMut(RegexParser *s) { return &(*s).groupCount; }
;;GETTER
static inline bool RegexParser_ignoreCase(const RegexParser *s) { return (*s).ignoreCase; }
;;GETTER
static inline bytes RegexParser_length(const RegexParser *s) { return (*s).length; }
;;GETTER
static inline RegexNodeList RegexParser_nodesRaw(const RegexParser *s) { return (*s).nodes; }
;;GETTER
static inline RegexNodeList *RegexParser_nodesMut(RegexParser *s) { return &(*s).nodes; }
;;GETTER
static inline const char * RegexParser_pattern(const RegexParser *s) { return (*s).pattern; }
;;GETTER
static inline bytes RegexParser_pos(const RegexParser *s) { return (*s).pos; }
;;GETTER
static inline bytes *RegexParser_posMut(RegexParser *s) { return &(*s).pos; }
;;SETTER
static inline fn RegexParser_setPos(RegexParser *s, bytes v) { (*s).pos = v; }

// ASCII-only lowercase; deliberately not locale-sensitive (byte patterns).
static u8 lowerByte(u8 c) {
    if (c >= (u8) 'A' && c <= (u8) 'Z')
        return (u8)(c + 32u);
    return c;
}

static bool isAsciiDigit(char c) {
    return c >= '0' && c <= '9';
}

// Records the first rejection reason; later ones are ignored.
static fn parserFail(RegexParser *parser, const char *message) {
    if (!RegexParser_failed(parser)) {
        RegexParser_setFailed(parser, true);
        RegexParser_setError(parser, message);
    }
}

// Appends a zeroed AST node under the node ceiling.
static u32 parserNode(RegexParser *parser, u8 kind) {
    RegexNodeList nodes = RegexParser_nodesRaw(parser);
    if (nodes.count >= REGEX_MAX_NODES) {
        parserFail(parser, "pattern too complex (node ceiling)");
        return REGEX_NODE_NONE;
    }
    u32 index = RegexNodeList_add(RegexParser_nodesMut(parser), kind);
    if (index == REGEX_NODE_NONE)
        parserFail(parser, "out of memory");
    return index;
}

// Adds one cleared class bitmap under the class ceiling; UINT32_MAX on refusal.
static u32 parserAddClass(RegexParser *parser) {
    if (RegexParser_classCount(parser) >= REGEX_MAX_CLASSES) {
        parserFail(parser, "too many character classes");
        return UINT32_MAX;
    }
    if (RegexParser_classCount(parser) == RegexParser_classCapacity(parser)) {
        u32 next = RegexParser_classCapacity(parser) ? RegexParser_classCapacity(parser) * 2u : 8u;
        if (next > REGEX_MAX_CLASSES)
            next = REGEX_MAX_CLASSES;
        RegexCharClass *grown = (RegexCharClass*) Memory_realloc(
            RegexParser_classes(parser), (bytes) next * sizeof(RegexCharClass));
        if (!grown) {
            parserFail(parser, "out of memory");
            return UINT32_MAX;
        }
        RegexParser_setClasses(parser, grown);
        RegexParser_setClassCapacity(parser, next);
    }
    u32 index = (*RegexParser_classCountMut(parser))++;
    RegexCharClass_clear(&RegexParser_classes(parser)[index]);
    return index;
}

// Reads the escape body after a backslash and yields a literal byte. A
// predefined family is recognised by the caller before this runs.
static bool parseEscapeByte(RegexParser *parser, u8 *outByte) {
    if (RegexParser_pos(parser) >= RegexParser_length(parser)) {
        parserFail(parser, "trailing backslash");
        return false;
    }
    char c = RegexParser_pattern(parser)[(*RegexParser_posMut(parser))++];
    switch (c) {
    case 'n': *outByte = (u8) '\n'; return true;
    case 'r': *outByte = (u8) '\r'; return true;
    case 't': *outByte = (u8) '\t'; return true;
    case 'f': *outByte = (u8) '\f'; return true;
    case 'v': *outByte = (u8) '\v'; return true;
    case '0': *outByte = (u8) '\0'; return true;
    case 'x': {
        if (RegexParser_pos(parser) + 1u >= RegexParser_length(parser)) {
            parserFail(parser, "malformed \\x escape");
            return false;
        }
        char h = RegexParser_pattern(parser)[RegexParser_pos(parser)];
        char l = RegexParser_pattern(parser)[RegexParser_pos(parser) + 1u];
        u32 hi, lo;
        if (h >= '0' && h <= '9') hi = (u32)(h - '0');
        else if (h >= 'a' && h <= 'f') hi = (u32)(h - 'a' + 10);
        else if (h >= 'A' && h <= 'F') hi = (u32)(h - 'A' + 10);
        else { parserFail(parser, "malformed \\x escape"); return false; }
        if (l >= '0' && l <= '9') lo = (u32)(l - '0');
        else if (l >= 'a' && l <= 'f') lo = (u32)(l - 'a' + 10);
        else if (l >= 'A' && l <= 'F') lo = (u32)(l - 'A' + 10);
        else { parserFail(parser, "malformed \\x escape"); return false; }
        RegexParser_setPos(parser, RegexParser_pos(parser) + (2u));
        *outByte = (u8)((hi << 4) | lo);
        return true;
    }
    default:
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '1' && c <= '9')) {
            parserFail(parser, "unsupported escape");
            return false;
        }
        // Every other escaped byte is that literal byte (\\. \\* \\\\ ...).
        *outByte = (u8) c;
        return true;
    }
}

static u32 parseAlternation(RegexParser *parser);

// Parses "(...)" or "(?:...)" into a group node.
static u32 parseGroup(RegexParser *parser) {
    RegexParser_setPos(parser, RegexParser_pos(parser) + 1u); // consume '('
    bool capturing = true;
    if (RegexParser_pos(parser) + 1u < RegexParser_length(parser) && RegexParser_pattern(parser)[RegexParser_pos(parser)] == '?' &&
        RegexParser_pattern(parser)[RegexParser_pos(parser) + 1u] == ':') {
        capturing = false;
        RegexParser_setPos(parser, RegexParser_pos(parser) + (2u));
    } else if (RegexParser_pos(parser) < RegexParser_length(parser) && RegexParser_pattern(parser)[RegexParser_pos(parser)] == '?') {
        parserFail(parser, "unsupported group construct (only (?:...) is supported)");
        return REGEX_NODE_NONE;
    }
    u32 group = 0;
    if (capturing)
        group = ++(*RegexParser_groupCountMut(parser));
    if (RegexParser_depth(parser) >= REGEX_MAX_DEPTH) {
        parserFail(parser, "pattern nesting too deep");
        return REGEX_NODE_NONE;
    }
    RegexParser_setDepth(parser, RegexParser_depth(parser) + 1u);
    u32 body = parseAlternation(parser);
    RegexParser_setDepth(parser, RegexParser_depth(parser) - 1u);
    if (body == REGEX_NODE_NONE)
        return REGEX_NODE_NONE;
    if (RegexParser_pos(parser) >= RegexParser_length(parser) || RegexParser_pattern(parser)[RegexParser_pos(parser)] != ')') {
        parserFail(parser, "missing closing ')'");
        return REGEX_NODE_NONE;
    }
    RegexParser_setPos(parser, RegexParser_pos(parser) + 1u);
    if (!capturing)
        return body;
    u32 node = parserNode(parser, REGEX_NODE_GROUP);
    if (node == REGEX_NODE_NONE)
        return REGEX_NODE_NONE;
    RegexNode *group_node = RegexNodeList_get(RegexParser_nodesMut(parser), node);
    RegexNode_setA(group_node, body);
    RegexNode_setGroup(group_node, group);
    return node;
}

// Parses "[...]" into a class bitmap index; UINT32_MAX on failure.
static u32 parseClass(RegexParser *parser) {
    RegexParser_setPos(parser, RegexParser_pos(parser) + 1u); // consume '['
    bool negate = false;
    if (RegexParser_pos(parser) < RegexParser_length(parser) && RegexParser_pattern(parser)[RegexParser_pos(parser)] == '^') {
        negate = true;
        RegexParser_setPos(parser, RegexParser_pos(parser) + 1u);
    }
    u32 index = parserAddClass(parser);
    if (index == UINT32_MAX)
        return UINT32_MAX;
    RegexCharClass *cls = &RegexParser_classes(parser)[index];
    bool insensitive = RegexParser_ignoreCase(parser);
    bool first = true;
    bool closed = false;
    while (RegexParser_pos(parser) < RegexParser_length(parser)) {
        char c = RegexParser_pattern(parser)[RegexParser_pos(parser)];
        if (c == ']' && !first) {
            RegexParser_setPos(parser, RegexParser_pos(parser) + 1u);
            closed = true;
            break;
        }
        first = false;
        u8 lo = 0;
        bool predefined = false;
        if (c == '\\') {
            RegexParser_setPos(parser, RegexParser_pos(parser) + 1u);
            if (RegexParser_pos(parser) < RegexParser_length(parser) &&
                RegexCharClass_isPredefined(RegexParser_pattern(parser)[RegexParser_pos(parser)])) {
                RegexCharClass_addPredefined(cls, RegexParser_pattern(parser)[RegexParser_pos(parser)]);
                RegexParser_setPos(parser, RegexParser_pos(parser) + 1u);
                predefined = true;
            } else if (!parseEscapeByte(parser, &lo)) {
                return UINT32_MAX;
            }
        } else {
            lo = (u8) c;
            RegexParser_setPos(parser, RegexParser_pos(parser) + 1u);
        }
        if (predefined)
            continue;
        if (RegexParser_pos(parser) + 1u < RegexParser_length(parser) && RegexParser_pattern(parser)[RegexParser_pos(parser)] == '-' &&
            RegexParser_pattern(parser)[RegexParser_pos(parser) + 1u] != ']') {
            RegexParser_setPos(parser, RegexParser_pos(parser) + 1u); // consume '-'
            u8 hi = 0;
            char hc = RegexParser_pattern(parser)[RegexParser_pos(parser)];
            if (hc == '\\') {
                RegexParser_setPos(parser, RegexParser_pos(parser) + 1u);
                if (RegexParser_pos(parser) < RegexParser_length(parser) &&
                    RegexCharClass_isPredefined(RegexParser_pattern(parser)[RegexParser_pos(parser)])) {
                    parserFail(parser, "character class in a range endpoint");
                    return UINT32_MAX;
                }
                if (!parseEscapeByte(parser, &hi))
                    return UINT32_MAX;
            } else {
                hi = (u8) hc;
                RegexParser_setPos(parser, RegexParser_pos(parser) + 1u);
            }
            if (hi < lo) {
                parserFail(parser, "invalid character range (end before start)");
                return UINT32_MAX;
            }
            RegexCharClass_addRange(cls, lo, hi, insensitive);
        } else {
            RegexCharClass_addByte(cls, lo, insensitive);
        }
    }
    if (!closed) {
        parserFail(parser, "missing closing ']'");
        return UINT32_MAX;
    }
    if (negate)
        RegexCharClass_negate(cls);
    return index;
}

// Parses one atom: a literal, '.', an anchor, a group, a class or an escape.
static u32 parseAtom(RegexParser *parser) {
    char c = RegexParser_pattern(parser)[RegexParser_pos(parser)];
    switch (c) {
    case '(':
        return parseGroup(parser);
    case '[': {
        u32 index = parseClass(parser);
        if (index == UINT32_MAX)
            return REGEX_NODE_NONE;
        u32 node = parserNode(parser, REGEX_NODE_CLASS);
        if (node == REGEX_NODE_NONE)
            return REGEX_NODE_NONE;
        RegexNode *cls_node = RegexNodeList_get(RegexParser_nodesMut(parser), node);
        RegexNode_setClsIndex(cls_node, index);
        return node;
    }
    case '.':
        RegexParser_setPos(parser, RegexParser_pos(parser) + 1u);
        return parserNode(parser, REGEX_NODE_ANY);
    case '^':
        RegexParser_setPos(parser, RegexParser_pos(parser) + 1u);
        return parserNode(parser, REGEX_NODE_BOL);
    case '$':
        RegexParser_setPos(parser, RegexParser_pos(parser) + 1u);
        return parserNode(parser, REGEX_NODE_EOL);
    case '\\': {
        RegexParser_setPos(parser, RegexParser_pos(parser) + 1u);
        if (RegexParser_pos(parser) < RegexParser_length(parser) &&
            RegexCharClass_isPredefined(RegexParser_pattern(parser)[RegexParser_pos(parser)])) {
            u32 index = parserAddClass(parser);
            if (index == UINT32_MAX)
                return REGEX_NODE_NONE;
            RegexCharClass_addPredefined(&RegexParser_classes(parser)[index],
                                         RegexParser_pattern(parser)[RegexParser_pos(parser)]);
            RegexParser_setPos(parser, RegexParser_pos(parser) + 1u);
            u32 node = parserNode(parser, REGEX_NODE_CLASS);
            if (node == REGEX_NODE_NONE)
                return REGEX_NODE_NONE;
            RegexNode *cls_node = RegexNodeList_get(RegexParser_nodesMut(parser), node);
            RegexNode_setClsIndex(cls_node, index);
            return node;
        }
        u8 byte = 0;
        if (!parseEscapeByte(parser, &byte))
            return REGEX_NODE_NONE;
        u32 node = parserNode(parser, REGEX_NODE_CHAR);
        if (node == REGEX_NODE_NONE)
            return REGEX_NODE_NONE;
        RegexNode *char_node = RegexNodeList_get(RegexParser_nodesMut(parser), node);
        RegexNode_setByte(char_node, RegexParser_ignoreCase(parser) ? lowerByte(byte) : byte);
        return node;
    }
    case '*':
    case '+':
    case '?':
        parserFail(parser, "nothing to repeat");
        return REGEX_NODE_NONE;
    default: {
        RegexParser_setPos(parser, RegexParser_pos(parser) + 1u);
        u32 node = parserNode(parser, REGEX_NODE_CHAR);
        if (node == REGEX_NODE_NONE)
            return REGEX_NODE_NONE;
        RegexNode *char_node = RegexNodeList_get(RegexParser_nodesMut(parser), node);
        u8 byte = (u8) c;
        RegexNode_setByte(char_node, RegexParser_ignoreCase(parser) ? lowerByte(byte) : byte);
        return node;
    }
    }
}

// Parses "{m}", "{m,}" or "{m,n}". 1 = quantifier consumed, 0 = a literal
// '{', -1 = a malformed quantifier (already recorded).
static i32 parseBraces(RegexParser *parser, u32 *outMin, u32 *outMax, bool *outHasMax) {
    bytes i = RegexParser_pos(parser) + 1u;
    if (i >= RegexParser_length(parser) || !isAsciiDigit(RegexParser_pattern(parser)[i]))
        return 0;
    u32 lo = 0;
    while (i < RegexParser_length(parser) && isAsciiDigit(RegexParser_pattern(parser)[i])) {
        lo = lo * 10u + (u32)(RegexParser_pattern(parser)[i] - '0');
        if (lo > REGEX_MAX_REPEAT) {
            parserFail(parser, "repetition count too large");
            return -1;
        }
        i++;
    }
    bool hasMax = true;
    u32 hi = lo;
    if (i < RegexParser_length(parser) && RegexParser_pattern(parser)[i] == ',') {
        i++;
        if (i < RegexParser_length(parser) && isAsciiDigit(RegexParser_pattern(parser)[i])) {
            hi = 0;
            while (i < RegexParser_length(parser) && isAsciiDigit(RegexParser_pattern(parser)[i])) {
                hi = hi * 10u + (u32)(RegexParser_pattern(parser)[i] - '0');
                if (hi > REGEX_MAX_REPEAT) {
                    parserFail(parser, "repetition count too large");
                    return -1;
                }
                i++;
            }
        } else {
            hasMax = false;
        }
    }
    if (i >= RegexParser_length(parser) || RegexParser_pattern(parser)[i] != '}') {
        parserFail(parser, "malformed repetition");
        return -1;
    }
    if (hasMax && hi < lo) {
        parserFail(parser, "invalid repetition range (end before start)");
        return -1;
    }
    RegexParser_setPos(parser, i + 1u);
    *outMin = lo;
    *outMax = hasMax ? hi : REGEX_REP_INF;
    *outHasMax = hasMax;
    return 1;
}

// Parses an atom plus any trailing quantifiers into one node.
static u32 parseRepeat(RegexParser *parser) {
    u32 atom = parseAtom(parser);
    if (atom == REGEX_NODE_NONE)
        return REGEX_NODE_NONE;
    for (;;) {
        if (RegexParser_pos(parser) >= RegexParser_length(parser))
            break;
        char c = RegexParser_pattern(parser)[RegexParser_pos(parser)];
        u8 wrapKind = 0;
        u32 min = 0, max = 0;
        bool hasMax = false;
        if (c == '*') {
            wrapKind = REGEX_NODE_STAR;
            RegexParser_setPos(parser, RegexParser_pos(parser) + 1u);
        } else if (c == '+') {
            wrapKind = REGEX_NODE_PLUS;
            RegexParser_setPos(parser, RegexParser_pos(parser) + 1u);
        } else if (c == '?') {
            wrapKind = REGEX_NODE_QUEST;
            RegexParser_setPos(parser, RegexParser_pos(parser) + 1u);
        } else if (c == '{') {
            i32 braces = parseBraces(parser, &min, &max, &hasMax);
            if (braces < 0)
                return REGEX_NODE_NONE;
            if (braces == 0)
                break; // a literal '{' — leave it for the concatenation loop
            wrapKind = REGEX_NODE_REPEAT;
        } else {
            break;
        }
        u32 wrap = parserNode(parser, wrapKind);
        if (wrap == REGEX_NODE_NONE)
            return REGEX_NODE_NONE;
        RegexNode *n = RegexNodeList_get(RegexParser_nodesMut(parser), wrap);
        RegexNode_setA(n, atom);
        if (wrapKind == REGEX_NODE_REPEAT) {
            RegexNode_setRepMin(n, min);
            RegexNode_setRepMax(n, hasMax ? max : REGEX_REP_INF);
        }
        atom = wrap;
        if (RegexParser_pos(parser) < RegexParser_length(parser)) {
            char next = RegexParser_pattern(parser)[RegexParser_pos(parser)];
            if (next == '*' || next == '+' || next == '?' ||
                (next == '{' && RegexParser_pos(parser) + 1u < RegexParser_length(parser) &&
                 isAsciiDigit(RegexParser_pattern(parser)[RegexParser_pos(parser) + 1u]))) {
                parserFail(parser, "stacked, lazy or possessive quantifier unsupported");
                return REGEX_NODE_NONE;
            }
        }
        break;
    }
    return atom;
}

// Parses a run of repeat atoms into a left-nested concatenation.
static u32 parseConcat(RegexParser *parser) {
    u32 head = REGEX_NODE_NONE;
    for (;;) {
        if (RegexParser_pos(parser) >= RegexParser_length(parser))
            break;
        char c = RegexParser_pattern(parser)[RegexParser_pos(parser)];
        if (c == '|' || c == ')')
            break;
        u32 piece = parseRepeat(parser);
        if (piece == REGEX_NODE_NONE)
            return REGEX_NODE_NONE;
        if (head == REGEX_NODE_NONE) {
            head = piece;
            continue;
        }
        u32 cat = parserNode(parser, REGEX_NODE_CONCAT);
        if (cat == REGEX_NODE_NONE)
            return REGEX_NODE_NONE;
        RegexNode *n = RegexNodeList_get(RegexParser_nodesMut(parser), cat);
        RegexNode_setA(n, head);
        RegexNode_setB(n, piece);
        head = cat;
    }
    if (head == REGEX_NODE_NONE)
        return parserNode(parser, REGEX_NODE_EMPTY);
    return head;
}

// Parses a run of concatenations separated by '|' into a left-nested alternation.
static u32 parseAlternation(RegexParser *parser) {
    u32 left = parseConcat(parser);
    if (left == REGEX_NODE_NONE)
        return REGEX_NODE_NONE;
    while (RegexParser_pos(parser) < RegexParser_length(parser) && RegexParser_pattern(parser)[RegexParser_pos(parser)] == '|') {
        RegexParser_setPos(parser, RegexParser_pos(parser) + 1u);
        u32 right = parseConcat(parser);
        if (right == REGEX_NODE_NONE)
            return REGEX_NODE_NONE;
        u32 alt = parserNode(parser, REGEX_NODE_ALT);
        if (alt == REGEX_NODE_NONE)
            return REGEX_NODE_NONE;
        RegexNode *n = RegexNodeList_get(RegexParser_nodesMut(parser), alt);
        RegexNode_setA(n, left);
        RegexNode_setB(n, right);
        left = alt;
    }
    return left;
}

RegexParser RegexParser_3(const char *pattern, bytes length, bool ignoreCase) {
    RegexParser parser;
    parser.pattern = pattern;
    parser.length = length;
    parser.pos = 0u;
    parser.ignoreCase = ignoreCase;
    parser.nodes = RegexNodeList();
    parser.classes = nullptr;
    parser.classCount = 0u;
    parser.classCapacity = 0u;
    parser.groupCount = 0u;
    parser.depth = 0u;
    parser.failed = false;
    parser.error = nullptr;
    return parser;
}

u32 RegexParser_parse(RegexParser *parser) {
    if (RegexParser_length(parser) == 0)
        return parseAlternation(parser); // the empty pattern is the empty node
    u32 root = parseAlternation(parser);
    if (root == REGEX_NODE_NONE || RegexParser_failed(parser))
        return REGEX_NODE_NONE;
    if (RegexParser_pos(parser) != RegexParser_length(parser)) {
        parserFail(parser, "unexpected ')'");
        return REGEX_NODE_NONE;
    }
    return root;
}

bool RegexParser_ok(const RegexParser *parser) {
    return !RegexParser_failed(parser);
}

const char *RegexParser_error(const RegexParser *parser) {
    return RegexParser_errorRaw(parser);
}

RegexNodeList *RegexParser_nodes(RegexParser *parser) {
    return RegexParser_nodesMut(parser);
}

u32 RegexParser_groupCount(const RegexParser *parser) {
    return RegexParser_groupCountRaw(parser);
}

RegexCharClass *RegexParser_releaseClasses(RegexParser *parser, u32 *outCount) {
    RegexCharClass *classes = RegexParser_classes(parser);
    if (outCount)
        *outCount = RegexParser_classCount(parser);
    RegexParser_setClasses(parser, nullptr);
    RegexParser_setClassCount(parser, 0);
    RegexParser_setClassCapacity(parser, 0);
    return classes;
}

fn RegexParser_free(RegexParser *parser) {
    RegexNodeList_free(RegexParser_nodesMut(parser));
    if (RegexParser_classes(parser))
        Memory_free(RegexParser_classes(parser));
    *parser = RegexParser(nullptr, 0u, false);
}
