#include "vex.h"
#include "primitive/buffer.h"

#include <string.h>

#include "nio/mem.h"
#include "oop/vexspoke_type.h"

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
 *   - Buffer_2(data, n)
 *   - Buffer_free(self)
 *   - Buffer_bytes(self)
 *   - Buffer_length(self)
 *   - Buffer_capacity(self)
 *   - Buffer_append(self, data, n)
 *   - Buffer_setLength(self, n)
 *   - Buffer_reserve(self, capacity)
 * ============================================================================
 */


;;GETTER
static inline bytes Buffer_capacity(const Buffer *s) { return (*s).capacity; }
;;GETTER
static inline bytes *Buffer_capacityMut(Buffer *s) { return &(*s).capacity; }
;;SETTER
static inline fn Buffer_setCapacity(Buffer *s, bytes v) { (*s).capacity = v; }
;;GETTER
static inline u8 * Buffer_data(const Buffer *s) { return (*s).data; }
;;GETTER
static inline u8 * *Buffer_dataMut(Buffer *s) { return &(*s).data; }
;;SETTER
static inline fn Buffer_setData(Buffer *s, u8 * v) { (*s).data = v; }
;;GETTER
static inline bytes Buffer_length(const Buffer *s) { return (*s).length; }
;;GETTER
static inline bytes *Buffer_lengthMut(Buffer *s) { return &(*s).length; }
;;SETTER
static inline fn Buffer_setLength(Buffer *s, bytes v) { (*s).length = v; }

Buffer *Buffer_0() {
    Buffer *self = (Buffer*) Memory_alloc(TYPE_BYTE_ARRAY, sizeof(Buffer));

    if (!self)
        return nullptr;

    Buffer_setData(self, nullptr);
    Buffer_setLength(self, 0);
    Buffer_setCapacity(self, 0);

    return self;
}

Buffer *Buffer_1(bytes n) {
    Buffer *self = Buffer_0();

    if (!self)
        return nullptr;

    if (n > 0) {
        Buffer_setData(self, (u8*) Memory_alloc(TYPE_BYTE_ARRAY, n));

        if (!Buffer_data(self)) {
            Memory_free(self);
            return nullptr;
        }

        memset(Buffer_data(self), 0, n);
        Buffer_setLength(self, n);
        Buffer_setCapacity(self, n);
    }

    return self;
}

Buffer *Buffer_2(const u8 *data, bytes n) {
    Buffer *self = Buffer_1(n);

    if (!self)
        return nullptr;

    if (n > 0 && data)
        memcpy(Buffer_data(self), data, n);

    return self;
}

fn Buffer_free(Buffer *self) {
    if (!self)
        return;

    if (Buffer_data(self))
        Memory_free(Buffer_data(self));

    Memory_free(self);
}

u8 *Buffer_bytes(const Buffer *self) {
    if (!self)
        return nullptr;

    return Buffer_data(self);
}

bytes Buffer_length(const Buffer *self) {
    if (!self)
        return 0;

    return Buffer_length(self);
}

bytes Buffer_capacity(const Buffer *self) {
    if (!self)
        return 0;

    return Buffer_capacity(self);
}

bool Buffer_reserve(Buffer *self, bytes capacity) {
    if (!self)
        return false;

    if (capacity <= Buffer_capacity(self))
        return true;

    bytes grown = Buffer_capacity(self) > 0 ? Buffer_capacity(self) : 64;

    while (grown < capacity) {
        if (grown > SIZE_MAX / 2)
            return false;

        grown *= 2;
    }

    u8 *next = (u8*) Memory_realloc(Buffer_data(self), grown);

    if (!next)
        return false;

    Buffer_setData(self, next);
    Buffer_setCapacity(self, grown);

    return true;
}

bool Buffer_append(Buffer *self, const u8 *data, bytes n) {
    if (!self)
        return false;

    if (n == 0)
        return true;

    if (!data)
        return false;

    bytes need = Buffer_length(self) + n;

    if (need < Buffer_length(self))
        return false;

    if (!Buffer_reserve(self, need))
        return false;

    memcpy(Buffer_data(self) + Buffer_length(self), data, n);
    Buffer_setLength(self, need);

    return true;
}

bool Buffer_setLength(Buffer *self, bytes n) {
    if (!self)
        return false;

    if (n > Buffer_capacity(self))
        return false;

    Buffer_setLength(self, n);

    return true;
}
