#include "primitive/buffer.h"

#include <string.h>

#include "nio/mem.h"
#include "oop/vexspoke_type.h"

#include "util/annotation.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Buffer
 * ============================================================================
 * One contiguous byte block with length + capacity over engine Memory_alloc
 * bytes. The dumb contiguous memory every tier above R2 needs: graphics pixel
 * rows, audio frames, parse scratch, dump cells. Growable by cold doubling,
 * never shrinks. Free with Buffer_free. Transient scratch stays
 * Transient_alloc; Buffer is permanent.
 * Lives at R2 as a leaf primitive.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Buffer (primitive/buffer.c)
 * ============================================================================
 * the contiguous byte buffer.
 *
 * STRUCT FIELDS: none — procedural (operates on engine Memory blocks)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - Buffer_0()
 *   - Buffer_1(n)
 *   - Buffer_2(bytes, n)
 *   - Buffer_free(self)
 *   - Buffer_bytes(self)
 *   - Buffer_length(self)
 *   - Buffer_capacity(self)
 *   - Buffer_append(self, bytes, n)
 *   - Buffer_setLength(self, n)
 *   - Buffer_reserve(self, capacity)
 * ============================================================================
 */

Buffer *Buffer_0(void) {
    Buffer *self = (Buffer*) Memory_alloc(TYPE_BYTE_ARRAY, sizeof(Buffer));

    if (!self)
        return nullptr;

    (*self).bytes = nullptr;
    (*self).length = 0;
    (*self).capacity = 0;

    return self;
}

Buffer *Buffer_1(bytes n) {
    Buffer *self = Buffer_0();

    if (!self)
        return nullptr;

    if (n > 0) {
        (*self).bytes = (u8*) Memory_alloc(TYPE_BYTE_ARRAY, n);

        if (!(*self).bytes) {
            Memory_free(self);
            return nullptr;
        }

        memset((*self).bytes, 0, n);
        (*self).length = n;
        (*self).capacity = n;
    }

    return self;
}

Buffer *Buffer_2(const u8 *bytes, bytes n) {
    Buffer *self = Buffer_1(n);

    if (!self)
        return nullptr;

    if (n > 0 && bytes)
        memcpy((*self).bytes, bytes, n);

    return self;
}

void Buffer_free(Buffer *self) {
    if (!self)
        return;

    if ((*self).bytes)
        Memory_free((*self).bytes);

    Memory_free(self);
}

u8 *Buffer_bytes(const Buffer *self) {
    if (!self)
        return nullptr;

    return (*self).bytes;
}

bytes Buffer_length(const Buffer *self) {
    if (!self)
        return 0;

    return (*self).length;
}

bytes Buffer_capacity(const Buffer *self) {
    if (!self)
        return 0;

    return (*self).capacity;
}

bool Buffer_reserve(Buffer *self, bytes capacity) {
    if (!self)
        return false;

    if (capacity <= (*self).capacity)
        return true;

    bytes grown = (*self).capacity > 0 ? (*self).capacity : 64;

    while (grown < capacity) {
        if (grown > SIZE_MAX / 2)
            return false;

        grown *= 2;
    }

    u8 *next = (u8*) Memory_realloc((*self).bytes, grown);

    if (!next)
        return false;

    (*self).bytes = next;
    (*self).capacity = grown;

    return true;
}

bool Buffer_append(Buffer *self, const u8 *bytes, bytes n) {
    if (!self)
        return false;

    if (n == 0)
        return true;

    if (!bytes)
        return false;

    bytes need = (*self).length + n;

    if (need < (*self).length)
        return false;

    if (!Buffer_reserve(self, need))
        return false;

    memcpy((*self).bytes + (*self).length, bytes, n);
    (*self).length = need;

    return true;
}

bool Buffer_setLength(Buffer *self, bytes n) {
    if (!self)
        return false;

    if (n > (*self).capacity)
        return false;

    (*self).length = n;

    return true;
}
