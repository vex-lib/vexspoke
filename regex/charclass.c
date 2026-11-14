// regex/charclass.c — the RegexCharClass 256-bit bitmap.

#include "vex.h"
#include "regex/charclass.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: RegexCharClass
 * ============================================================================
 * The byte-membership set used by bracket expressions and the \d \w \s
 * families. One 256-bit bitmap makes both compilation and matching trivial:
 * building a class is a bit set per member, and testing one at match time is a
 * shift, a mask and a load — no range list to walk and no ordering to keep, so
 * a class costs the same whether it names one byte or two hundred.
 *
 * Case folding happens where the bits are added: with an insensitive compile,
 * every letter sets both of its cases, so the matcher never folds the subject.
 * That keeps the hot path a plain bit read (the Cold-Strict, Hot-Minimal
 * Validation Law's split: decide once at compile, trust the bitmap at match).
 *
 * memory: none. A RegexCharClass is a 32-byte by-value record; the class ARRAY
 * a compiled pattern owns is grown by the parser (regex/parser.c). Every
 * function here is pure over the bitmap and allocates nothing.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: RegexCharClass (regex/charclass.c — defined in regex/charclass.h)
 * ============================================================================
 * A 256-bit byte-membership bitmap with compile-time case folding.
 *
 * STRUCT FIELDS (Mirroring regex/charclass.h):
 * ----------------------------------------------------------------------------
 *   RegexCharClass {
 *     u8 bits[REGEX_CLASS_BYTES];  // one bit per byte value
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Core Functions: (.h)
 *   - RegexCharClass_clear(cls)
 *   - RegexCharClass_addByte(cls, byte, insensitive)
 *   - RegexCharClass_addRange(cls, lo, hi, insensitive)
 *   - RegexCharClass_addPredefined(cls, letter)
 *   - RegexCharClass_isPredefined(letter)
 *   - RegexCharClass_negate(cls)
 *   - RegexCharClass_test(cls, byte)
 *
 * Private Core Functions: (.c static)
 *   - setBit(cls, byte)
 *   - addFoldedByte(cls, byte, insensitive)
 *
 * Admission, ownership, failure:
 *   - Pure bitmap operations: no allocation, no failure path, no lifetime.
 *   - clear leaves the set empty; addRange with lo > hi adds nothing (the
 *     parser rejects an inverted range before calling).
 * ============================================================================
 */

// Sets one byte's bit.
static fn setBit(RegexCharClass *cls, u8 byte) {
    (*cls).bits[byte >> 3] |= (u8)(1u << (byte & 7u));
}

// Sets a byte and, when folding, its opposite ASCII case (letters only).
static fn addFoldedByte(RegexCharClass *cls, u8 byte, bool insensitive) {
    setBit(cls, byte);
    if (!insensitive)
        return;
    if (byte >= (u8) 'A' && byte <= (u8) 'Z')
        setBit(cls, (u8)(byte + 32u));
    else if (byte >= (u8) 'a' && byte <= (u8) 'z')
        setBit(cls, (u8)(byte - 32u));
}

fn RegexCharClass_clear(RegexCharClass *cls) {
    for (u32 i = 0; i < REGEX_CLASS_BYTES; i++)
        (*cls).bits[i] = 0;
}

fn RegexCharClass_addByte(RegexCharClass *cls, u8 byte, bool insensitive) {
    addFoldedByte(cls, byte, insensitive);
}

fn RegexCharClass_addRange(RegexCharClass *cls, u8 lo, u8 hi, bool insensitive) {
    if (hi < lo)
        return;
    for (u32 c = (u32) lo; c <= (u32) hi; c++)
        addFoldedByte(cls, (u8) c, insensitive);
}

bool RegexCharClass_isPredefined(char letter) {
    return letter == 'd' || letter == 'D' || letter == 'w' || letter == 'W' ||
           letter == 's' || letter == 'S';
}

fn RegexCharClass_addPredefined(RegexCharClass *cls, char letter) {
    switch (letter) {
    case 'd':
        for (u32 c = '0'; c <= '9'; c++)
            setBit(cls, (u8) c);
        break;
    case 'D':
        for (u32 c = 0; c < 256u; c++)
            if (c < '0' || c > '9')
                setBit(cls, (u8) c);
        break;
    case 'w':
        for (u32 c = '0'; c <= '9'; c++) setBit(cls, (u8) c);
        for (u32 c = 'A'; c <= 'Z'; c++) setBit(cls, (u8) c);
        for (u32 c = 'a'; c <= 'z'; c++) setBit(cls, (u8) c);
        setBit(cls, (u8) '_');
        break;
    case 'W':
        for (u32 c = 0; c < 256u; c++) {
            bool word = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
                        (c >= 'a' && c <= 'z') || c == '_';
            if (!word)
                setBit(cls, (u8) c);
        }
        break;
    case 's':
        setBit(cls, (u8) ' ');
        setBit(cls, (u8) '\t');
        setBit(cls, (u8) '\n');
        setBit(cls, (u8) '\r');
        setBit(cls, (u8) '\f');
        setBit(cls, (u8) '\v');
        break;
    case 'S':
        for (u32 c = 0; c < 256u; c++) {
            bool space = c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
                         c == '\f' || c == '\v';
            if (!space)
                setBit(cls, (u8) c);
        }
        break;
    default:
        break;
    }
}

fn RegexCharClass_negate(RegexCharClass *cls) {
    for (u32 i = 0; i < REGEX_CLASS_BYTES; i++)
        (*cls).bits[i] = (u8) ~(*cls).bits[i];
}

bool RegexCharClass_test(const RegexCharClass *cls, u8 byte) {
    return ((*cls).bits[byte >> 3] & (u8)(1u << (byte & 7u))) != 0;
}
