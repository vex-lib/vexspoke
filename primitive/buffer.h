#ifndef PRIMITIVE_BUFFER_H
#define PRIMITIVE_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "util/constructor.h"

#include "util/annotation.h"

// primitive/buffer.h — one contiguous byte block with length + capacity.
//
// Single Class Per File Law: Buffer.
//
// The dumb contiguous memory every tier above R2 needs: graphics pixel rows,
// audio frames, parse scratch, dump cells. Pointer + length + capacity over
// engine Memory_alloc bytes — no Collection overhead, no stride, no per-
// element class. Growable (cold doubling), never shrinks. Free with
// Buffer_free. Transient scratch stays Transient_alloc; Buffer is permanent.

typedef struct Buffer {
    uint8_t *bytes;
    size_t length;
    size_t capacity;
} Buffer;

// Empty buffer, zero capacity.
Buffer *Buffer_0(void);
// Pre-sized buffer, zeroed, length == capacity == n.
Buffer *Buffer_1(size_t n);
// Copy of n bytes.
Buffer *Buffer_2(const uint8_t *bytes, size_t n);

#define BUFFER_CHOOSER(_0, _1, _2, NAME, ...) NAME

#define Buffer(...) BUFFER_CHOOSER( \
    dummy __VA_OPT__(,) __VA_ARGS__, \
    Buffer_2, Buffer_1, Buffer_0 \
)(__VA_ARGS__)

void Buffer_free(Buffer *self);

// Contiguous payload. Stable until the next grow.
uint8_t *Buffer_bytes(const Buffer *self);
size_t Buffer_length(const Buffer *self);
size_t Buffer_capacity(const Buffer *self);

// Append n bytes, growing cold (doubling) as needed. False on refusal.
bool Buffer_append(Buffer *self, const uint8_t *bytes, size_t n);

// Set length explicitly (clamped to capacity, never grows).
bool Buffer_setLength(Buffer *self, size_t n);

// Ensure capacity, growing if short. False on refusal.
bool Buffer_reserve(Buffer *self, size_t capacity);

#endif
