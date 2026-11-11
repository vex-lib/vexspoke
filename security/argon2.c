#include "security/argon2.h"

#include <stdlib.h>
#include <string.h>

#include "util/annotation.h"
#include "util/throw.h"
#include "security/blake2b.h"

;;DEFINITION
/** Argon2id is the password hash that costs memory as well as time. PBKDF2 asks
 * an attacker to run HMAC many times; Argon2 asks them to hold tens of megabytes
 * per guess, which is what a GPU cannot do in parallel. It is specified in terms
 * of BLAKE2b (RFC 9106), so it stands on security/blake2b.c.
 *
 * This is a MODULE: no object is kept, only a cold derivation. The m' blocks of
 * work memory are allocated for the call, wiped, and released. Argon2id means
 * data-independent addressing for the first half of the first pass (resisting
 * side channels) and data-dependent addressing after that (resisting tradeoff
 * attacks); both are implemented exactly as the reference does, because a
 * "close enough" Argon2 produces a plausible 32-byte tag that is simply wrong.
 * The owner test proves the RFC 9106 vector; nothing here is a claim of
 * production readiness on any platform. */
;;OVERVIEW
/** ========================================================================
 * MODULE: Argon2 (security/argon2.c)
 * STRUCT FIELDS (private; Mirroring security/argon2.h's Argon2Params)
 *   Argon2Params { memoryKiB, passes, lanes, tagBytes }  caller's m, t, p, T
 * PRIVATE HELPERS
 *   Argon2Instance { lanes, passes, memoryBlocks, segmentLength, laneLength,
 *                    type, version, memory }  one derivation's live state
 *   storeLE32(p, v)              little-endian 32-bit store
 *   rotr64(v, bits)              right rotation
 *   mixArgon(v, a, b.sh, c, d)      the BlaMka G (a+b.sh+2*lo(a)*lo(b.sh))
 *   roundP(v)                    the 16-word BLAKE2b permutation P
 *   fillBlock(prev, ref, next, withXor)   the compression G(X, Y)
 *   nextAddresses(address, input, zero)   Argon2i address block
 *   indexAlpha(inst, pass, slice, index, pseudoRand, sameLane)
 *   fillSegment(inst, pass, lane, slice)
 *   hPrime(out, outlen, in, inlen)   the variable-length hash H'
 *   secureWipe(p, n)             volatile zeroing of work memory
 * FUNCTION REGISTRY (exports)
 *   Argon2id_derive(params, password, passwordLen, salt, saltLen, secret,
 *                   secretLen, associated, associatedLen, tag, tagCap)
 * CONTRACT: invalid params/password/salt/tag reject with one THROW and write
 * nothing; work memory is wiped and freed on every path. No logging on success.
 * ======================================================================== */

#define ARGON2_ADDRESSES_IN_BLOCK 128u
#define ARGON2_BLOCK_WORDS        128u

typedef struct Argon2Instance {
    u32 lanes;
    u32 passes;
    u32 memoryBlocks;  // m'
    u32 segmentLength; // q
    u32 laneLength;    // 4q
    u32 type;
    u32 version;
    u64 *memory;       // memoryBlocks * 128 words
} Argon2Instance;

static void storeLE32(u8 *p, u32 v) {
    p[0] = (u8) v;
    p[1] = (u8) (v >> 8);
    p[2] = (u8) (v >> 16);
    p[3] = (u8) (v >> 24);
}

static u64 rotr64(u64 v, unsigned bits) {
    return (v >> bits) | (v << (64u - bits));
}

static void secureWipe(void *p, bytes n) {
    volatile u8 *v = (volatile u8*) p;
    while (n > 0u) {
        *v = 0u;
        ++v;
        --n;
    }
}

static void mixArgon(u64 *v, unsigned a, unsigned b, unsigned c, unsigned d) {
    v[a] = v[a] + v[b] + 2u * (v[a] & 0xFFFFFFFFu) * (v[b] & 0xFFFFFFFFu);
    v[d] = rotr64(v[d] ^ v[a], 32u);
    v[c] = v[c] + v[d] + 2u * (v[c] & 0xFFFFFFFFu) * (v[d] & 0xFFFFFFFFu);
    v[b] = rotr64(v[b] ^ v[c], 24u);
    v[a] = v[a] + v[b] + 2u * (v[a] & 0xFFFFFFFFu) * (v[b] & 0xFFFFFFFFu);
    v[d] = rotr64(v[d] ^ v[a], 16u);
    v[c] = v[c] + v[d] + 2u * (v[c] & 0xFFFFFFFFu) * (v[d] & 0xFFFFFFFFu);
    v[b] = rotr64(v[b] ^ v[c], 63u);
}

static void roundP(u64 *v) {
    mixArgon(v, 0u, 4u, 8u, 12u);
    mixArgon(v, 1u, 5u, 9u, 13u);
    mixArgon(v, 2u, 6u, 10u, 14u);
    mixArgon(v, 3u, 7u, 11u, 15u);
    mixArgon(v, 0u, 5u, 10u, 15u);
    mixArgon(v, 1u, 6u, 11u, 12u);
    mixArgon(v, 2u, 7u, 8u, 13u);
    mixArgon(v, 3u, 4u, 9u, 14u);
}

static void fillBlock(const u64 *prev, const u64 *ref, u64 *next,
                      bool withXor) {
    u64 r[ARGON2_BLOCK_WORDS];
    u64 tmp[ARGON2_BLOCK_WORDS];
    for (unsigned i = 0; i < ARGON2_BLOCK_WORDS; ++i)
        r[i] = prev[i] ^ ref[i];
    memcpy(tmp, r, sizeof r);
    if (withXor)
        for (unsigned i = 0; i < ARGON2_BLOCK_WORDS; ++i)
            tmp[i] ^= next[i];
    // The rounds are applied to R (the pre-XOR value); `tmp` is the value that
    // gets XORed back over the permutation's result. Applying the rounds to
    // `tmp` instead is the bug that only shows up from pass 1 onward.
    for (unsigned i = 0; i < 8u; ++i)
        roundP(&r[16u * i]);
    for (unsigned j = 0; j < 8u; ++j) {
        u64 column[16];
        for (unsigned g = 0; g < 8u; ++g) {
            column[2u * g] = r[16u * g + 2u * j];
            column[2u * g + 1u] = r[16u * g + 2u * j + 1u];
        }
        roundP(column);
        for (unsigned g = 0; g < 8u; ++g) {
            r[16u * g + 2u * j] = column[2u * g];
            r[16u * g + 2u * j + 1u] = column[2u * g + 1u];
        }
    }
    for (unsigned i = 0; i < ARGON2_BLOCK_WORDS; ++i)
        next[i] = tmp[i] ^ r[i];
}

static void nextAddresses(u64 *addressBlock, u64 *inputBlock,
                          const u64 *zeroBlock) {
    inputBlock[6]++;
    fillBlock(zeroBlock, inputBlock, addressBlock, false);
    fillBlock(zeroBlock, addressBlock, addressBlock, false);
}

static u32 indexAlpha(const Argon2Instance *inst, u32 pass, u32 slice,
                           u32 index, u32 pseudoRand, bool sameLane) {
    u32 referenceAreaSize;
    if (pass == 0u) {
        if (slice == 0u)
            referenceAreaSize = index - 1u;
        else if (sameLane)
            referenceAreaSize = slice * (*inst).segmentLength + index - 1u;
        else
            referenceAreaSize = slice * (*inst).segmentLength +
                                (index == 0u ? 0xFFFFFFFFu : 0u);
    } else {
        if (sameLane)
            referenceAreaSize = (*inst).laneLength - (*inst).segmentLength + index - 1u;
        else
            referenceAreaSize = (*inst).laneLength - (*inst).segmentLength +
                                (index == 0u ? 0xFFFFFFFFu : 0u);
    }

    u64 relativePosition = pseudoRand;
    relativePosition = (relativePosition * relativePosition) >> 32;
    relativePosition = (u64) referenceAreaSize - 1u -
                       (((u64) referenceAreaSize * relativePosition) >> 32);

    u32 startPosition = 0u;
    if (pass != 0u)
        startPosition = (slice == ARGON2_SYNC_POINTS - 1u)
            ? 0u
            : (slice + 1u) * (*inst).segmentLength;

    return (u32) (((u64) startPosition + relativePosition) % (*inst).laneLength);
}

static void fillSegment(Argon2Instance *inst, u32 pass, u32 lane, u32 slice) {
    bool dataIndependent = ((*inst).type == ARGON2_TYPE_ID && pass == 0u &&
                            slice < ARGON2_SYNC_POINTS / 2u);
    u64 addressBlock[ARGON2_BLOCK_WORDS] = {0};
    u64 inputBlock[ARGON2_BLOCK_WORDS] = {0};
    u64 zeroBlock[ARGON2_BLOCK_WORDS] = {0};

    if (dataIndependent) {
        inputBlock[0] = pass;
        inputBlock[1] = lane;
        inputBlock[2] = slice;
        inputBlock[3] = (*inst).memoryBlocks;
        inputBlock[4] = (*inst).passes;
        inputBlock[5] = (*inst).type;
    }

    u32 startingIndex = 0u;
    if (pass == 0u && slice == 0u) {
        startingIndex = 2u;
        if (dataIndependent)
            nextAddresses(addressBlock, inputBlock, zeroBlock);
    }

    u32 currOffset = lane * (*inst).laneLength + slice * (*inst).segmentLength +
                          startingIndex;
    u32 prevOffset = (currOffset % (*inst).laneLength == 0u)
        ? currOffset + (*inst).laneLength - 1u
        : currOffset - 1u;

    for (u32 i = startingIndex; i < (*inst).segmentLength; ++i, ++currOffset, ++prevOffset) {
        if (currOffset % (*inst).laneLength == 1u)
            prevOffset = currOffset - 1u;

        u64 pseudoRand;
        if (dataIndependent) {
            if (i % ARGON2_ADDRESSES_IN_BLOCK == 0u)
                nextAddresses(addressBlock, inputBlock, zeroBlock);
            pseudoRand = addressBlock[i % ARGON2_ADDRESSES_IN_BLOCK];
        } else {
            pseudoRand = (*inst).memory[(bytes) prevOffset * ARGON2_BLOCK_WORDS];
        }

        u32 refLane = (u32) ((pseudoRand >> 32) % (*inst).lanes);
        if (pass == 0u && slice == 0u)
            refLane = lane;

        u32 refIndex = indexAlpha(inst, pass, slice, i,
                                       (u32) (pseudoRand & 0xFFFFFFFFu), refLane == lane);

        const u64 *refBlock = (*inst).memory +
            (bytes) ((*inst).laneLength * refLane + refIndex) * ARGON2_BLOCK_WORDS;
        u64 *currBlock = (*inst).memory + (bytes) currOffset * ARGON2_BLOCK_WORDS;
        const u64 *prevBlock = (*inst).memory + (bytes) prevOffset * ARGON2_BLOCK_WORDS;
        fillBlock(prevBlock, refBlock, currBlock, pass != 0u);
    }
}

static bool hPrime(u8 *out, u32 outlen, const u8 *in, bytes inlen) {
    u8 le[4];
    storeLE32(le, outlen);
    if (outlen <= 64u) {
        Blake2b ctx = Blake2b_0();
        if (!Blake2b_init(&ctx, outlen))
            return false;
        Blake2b_update(&ctx, le, sizeof le);
        Blake2b_update(&ctx, in, inlen);
        return Blake2b_final(&ctx, out, outlen);
    }
    u32 r = (outlen + 31u) / 32u - 2u;
    u8 v[64];
    Blake2b first = Blake2b_0();
    if (!Blake2b_init(&first, 64u))
        return false;
    Blake2b_update(&first, le, sizeof le);
    Blake2b_update(&first, in, inlen);
    if (!Blake2b_final(&first, v, sizeof v))
        return false;
    memcpy(out, v, 32u);
    out += 32u;
    for (u32 i = 1u; i < r; ++i) {
        Blake2b next = Blake2b_0();
        if (!Blake2b_init(&next, 64u))
            return false;
        Blake2b_update(&next, v, sizeof v);
        if (!Blake2b_final(&next, v, sizeof v))
            return false;
        memcpy(out, v, 32u);
        out += 32u;
    }
    u32 last = outlen - 32u * r;
    Blake2b tail = Blake2b_0();
    if (!Blake2b_init(&tail, last))
        return false;
    Blake2b_update(&tail, v, sizeof v);
    return Blake2b_final(&tail, out, last);
}

bool Argon2id_derive(const Argon2Params *params,
                     const u8 *password, bytes passwordLen,
                     const u8 *salt, bytes saltLen,
                     const u8 *secret, bytes secretLen,
                     const u8 *associated, bytes associatedLen,
                     u8 *tag, bytes tagCap) {
    if (params == nullptr || password == nullptr || passwordLen == 0u ||
        passwordLen > 0xFFFFFFFFu || salt == nullptr || saltLen < ARGON2_SALT_MIN ||
        saltLen > 0xFFFFFFFFu || tag == nullptr || tagCap < (*params).tagBytes ||
        (*params).lanes == 0u || (*params).passes == 0u ||
        (*params).tagBytes < ARGON2_TAG_MIN || (*params).tagBytes > ARGON2_TAG_MAX) {
        THROW("Argon2id_derive: invalid parameter, password, salt or tag");
        return false;
    }
    if ((secret == nullptr && secretLen != 0u) ||
        (associated == nullptr && associatedLen != 0u) ||
        secretLen > 0xFFFFFFFFu || associatedLen > 0xFFFFFFFFu) {
        THROW("Argon2id_derive: null optional input with a nonzero length");
        return false;
    }

    u32 lanes = (*params).lanes;
    u32 memoryBlocks = 4u * lanes * ((*params).memoryKiB / (4u * lanes));
    if (memoryBlocks < 8u * lanes) {
        THROW("Argon2id_derive: memory is too small for the lane count");
        return false;
    }
    u32 segmentLength = memoryBlocks / (lanes * ARGON2_SYNC_POINTS);
    u32 laneLength = segmentLength * ARGON2_SYNC_POINTS;

    bytes memoryBytes = (bytes) memoryBlocks * ARGON2_BLOCK_BYTES;
    u64 *memory = (u64*) malloc(memoryBytes);
    if (memory == nullptr) {
        THROW("Argon2id_derive: work memory of %zu bytes was refused", memoryBytes);
        return false;
    }

    Argon2Instance inst = {
        .lanes = lanes,
        .passes = (*params).passes,
        .memoryBlocks = memoryBlocks,
        .segmentLength = segmentLength,
        .laneLength = laneLength,
        .type = ARGON2_TYPE_ID,
        .version = ARGON2_VERSION_13,
        .memory = memory,
    };

    u8 h0[64];
    {
        Blake2b ctx = Blake2b_0();
        u8 le[4];
        if (!Blake2b_init(&ctx, sizeof h0)) {
            secureWipe(memory, memoryBytes);
            free(memory);
            return false;
        }
        storeLE32(le, lanes);              Blake2b_update(&ctx, le, sizeof le);
        storeLE32(le, (*params).tagBytes); Blake2b_update(&ctx, le, sizeof le);
        storeLE32(le, (*params).memoryKiB);Blake2b_update(&ctx, le, sizeof le);
        storeLE32(le, (*params).passes);   Blake2b_update(&ctx, le, sizeof le);
        storeLE32(le, ARGON2_VERSION_13);  Blake2b_update(&ctx, le, sizeof le);
        storeLE32(le, ARGON2_TYPE_ID);     Blake2b_update(&ctx, le, sizeof le);
        storeLE32(le, (u32) passwordLen);
        Blake2b_update(&ctx, le, sizeof le);
        Blake2b_update(&ctx, password, passwordLen);
        storeLE32(le, (u32) saltLen);
        Blake2b_update(&ctx, le, sizeof le);
        Blake2b_update(&ctx, salt, saltLen);
        storeLE32(le, (u32) secretLen);
        Blake2b_update(&ctx, le, sizeof le);
        if (secretLen > 0u)
            Blake2b_update(&ctx, secret, secretLen);
        storeLE32(le, (u32) associatedLen);
        Blake2b_update(&ctx, le, sizeof le);
        if (associatedLen > 0u)
            Blake2b_update(&ctx, associated, associatedLen);
        if (!Blake2b_final(&ctx, h0, sizeof h0)) {
            secureWipe(memory, memoryBytes);
            free(memory);
            return false;
        }
    }

    u8 blockBytes[ARGON2_BLOCK_BYTES];
    u8 seed[64u + 8u];
    memcpy(seed, h0, sizeof h0);
    for (u32 lane = 0; lane < lanes; ++lane) {
        storeLE32(seed + 64u, 0u);
        storeLE32(seed + 68u, lane);
        if (!hPrime(blockBytes, ARGON2_BLOCK_BYTES, seed, sizeof seed)) {
            secureWipe(memory, memoryBytes);
            free(memory);
            return false;
        }
        memcpy(memory + (bytes) (lane * laneLength) * ARGON2_BLOCK_WORDS,
               blockBytes, ARGON2_BLOCK_BYTES);
        storeLE32(seed + 64u, 1u);
        if (!hPrime(blockBytes, ARGON2_BLOCK_BYTES, seed, sizeof seed)) {
            secureWipe(memory, memoryBytes);
            free(memory);
            return false;
        }
        memcpy(memory + (bytes) (lane * laneLength + 1u) * ARGON2_BLOCK_WORDS,
               blockBytes, ARGON2_BLOCK_BYTES);
    }

    for (u32 pass = 0; pass < (*params).passes; ++pass)
        for (u32 slice = 0; slice < ARGON2_SYNC_POINTS; ++slice)
            for (u32 lane = 0; lane < lanes; ++lane)
                fillSegment(&inst, pass, lane, slice);

    u64 finalBlock[ARGON2_BLOCK_WORDS];
    memcpy(finalBlock, memory + (bytes) (laneLength - 1u) * ARGON2_BLOCK_WORDS,
           ARGON2_BLOCK_BYTES);
    for (u32 lane = 1; lane < lanes; ++lane) {
        const u64 *last = memory +
            (bytes) (lane * laneLength + laneLength - 1u) * ARGON2_BLOCK_WORDS;
        for (unsigned i = 0; i < ARGON2_BLOCK_WORDS; ++i)
            finalBlock[i] ^= last[i];
    }

    bool ok = hPrime(tag, (*params).tagBytes, (const u8*) finalBlock,
                     ARGON2_BLOCK_BYTES);
    secureWipe(finalBlock, sizeof finalBlock);
    secureWipe(blockBytes, sizeof blockBytes);
    secureWipe(seed, sizeof seed);
    secureWipe(h0, sizeof h0);
    secureWipe(memory, memoryBytes);
    free(memory);
    return ok;
}
