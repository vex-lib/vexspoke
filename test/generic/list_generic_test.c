// vexspoke/test/generic/list_generic_test.c — the generic/ List surface _test.
//
// Overkill on purpose (the Dynamic Scalability & Anti-Hardcoding Law):
//   - List(u32)(23) constructs a typed handle over the type-erased engine;
//   - the stamp's element class id and explicit stride reach the engine
//     metadata, including the floor shapes that have no registered class id
//     (ld128, b16, mat2f) and the multi-word segment records;
//   - add/get/slot round-trip through the method-first macros, including a
//     writable slot and a hostile index;
//   - every scalar width keeps its own size, including a negative i32, a full
//     range u64, and a 16-bit u16;
//   - a multi-word value (ld128) is stored whole through List_addSlot/memcpy,
//     not truncated to one engine word;
//   - the object stamp (Buffer) stores a pointer word and returns the pointer;
//   - growth past the requested capacity keeps every element;
//   - null handles are cold-safe (no crash, safe defaults);
//   - two instantiations are distinct C types over one engine block layout;
//   - List(List(u32)) nests at depth 2 and stores the inner handle;
//   - every VEX_LIST_FAMILY entry constructs, reports its stride, and frees,
//     and every entry also gets its nested depth-2 stamp exercised.
//
// The engine's own accessors are not called here: generic/classgeneric_repo.h
// owns the method-first names in this translation unit, and the typed macros
// are the surface under test.

#include <stdio.h>

#include "generic/generic.h"
#include "oop/vexspoke_type.h"
#include "primitive/buffer.h"

static i32 g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Construct, verify the slot width, and release one value-stored entry.
#define VEX_TEST_VALUE(T, ID)                                              \
    do {                                                                   \
        List(T) *list = List(T)(4);                                        \
        CHECK(list != nullptr);                                            \
        CHECK(List_stride((List*) list) == sizeof(T));                     \
        List_free(T)(list);                                                \
    } while (0);

// Construct, verify the pointer slot width, and release one object entry.
#define VEX_TEST_OBJECT(T, ID)                                             \
    do {                                                                   \
        List(T) *list = List(T)(4);                                        \
        CHECK(list != nullptr);                                            \
        CHECK(List_stride((List*) list) == sizeof(T*));                    \
        List_free(T)(list);                                                \
    } while (0);

// Construct, verify the inner-handle slot width, and release one nested entry.
#define VEX_TEST_NESTED(T, ID)                                             \
    do {                                                                   \
        List(List(T)) *list = List(List(T))(4);                            \
        CHECK(list != nullptr);                                            \
        CHECK(List_stride((List*) list) == sizeof(VEX_LIST_CLASS(T)*));    \
        CHECK(List_elementClassId((List*) list) == ID_LIST);               \
        List_free(List(T))(list);                                          \
    } while (0);

int main() {
    // --- u32: construction, metadata, round trip --------------------------
    List(u32) *numbers = List(u32)(23);
    CHECK(numbers != nullptr);
    CHECK(List_elementClassId((List*) numbers) == ID_INT);
    CHECK(List_stride((List*) numbers) == 4);
    CHECK(List_length((List*) numbers) == 0);
    CHECK(List_capacity((List*) numbers) >= 23);

    List_add(u32)(numbers, 7);
    List_add(u32)(numbers, 9);
    List_add(u32)(numbers, 11);
    CHECK(List_length((List*) numbers) == 3);
    CHECK(List_get(u32)(numbers, 0) == 7);
    CHECK(List_get(u32)(numbers, 2) == 11);
    CHECK(List_get(u32)(numbers, 3) == 0);         // == count
    CHECK(List_get(u32)(numbers, SIZE_MAX) == 0);  // hostile index

    u32 *slot = List_slot(u32)(numbers, 1);
    CHECK(slot != nullptr);
    CHECK(*slot == 9);
    *slot = 90;                                     // the typed slot is writable
    CHECK(List_get(u32)(numbers, 1) == 90);
    CHECK(List_slot(u32)(numbers, 3) == nullptr);

    // --- capacity above the legacy floor is honored ----------------------
    List(u32) *wide = List(u32)(4096);
    CHECK(wide != nullptr);
    CHECK(List_capacity((List*) wide) == 4096);
    List_free(u32)(wide);

    // --- u8: one byte per slot -------------------------------------------
    List(u8) *octets = List(u8)(4);
    CHECK(octets != nullptr);
    CHECK(List_stride((List*) octets) == 1);
    List_add(u8)(octets, 200);
    List_add(u8)(octets, 255);
    CHECK(List_get(u8)(octets, 0) == 200);
    CHECK(List_get(u8)(octets, 1) == 255);

    // --- u16: two bytes per slot -----------------------------------------
    List(u16) *halves = List(u16)(4);
    CHECK(halves != nullptr);
    CHECK(List_stride((List*) halves) == 2);
    List_add(u16)(halves, 0xBEEF);
    List_add(u16)(halves, 0x1234);
    CHECK(List_get(u16)(halves, 0) == 0xBEEF);
    CHECK(List_get(u16)(halves, 1) == 0x1234);

    // --- i32: the sign survives the engine word --------------------------
    List(i32) *signedNumbers = List(i32)(8);
    CHECK(signedNumbers != nullptr);
    CHECK(List_stride((List*) signedNumbers) == 4);
    List_add(i32)(signedNumbers, -5);
    List_add(i32)(signedNumbers, 2147483647);
    CHECK(List_get(i32)(signedNumbers, 0) == -5);
    CHECK(List_get(i32)(signedNumbers, 1) == 2147483647);

    // --- u64: the full range is one slot --------------------------------
    List(u64) *words = List(u64)(8);
    CHECK(words != nullptr);
    CHECK(List_stride((List*) words) == 8);
    u64 huge = UINT64_MAX;
    List_add(u64)(words, huge);
    CHECK(List_get(u64)(words, 0) == huge);

    // --- ld128: a multi-word value is stored whole -----------------------
    List(ld128) *wideValues = List(ld128)(4);
    CHECK(wideValues != nullptr);
    CHECK(List_stride((List*) wideValues) == sizeof(ld128));
    ld128 big = {0};
    big.integer = 0x1122334455667788ll;
    big.fraction = 0.5;
    List_add(ld128)(wideValues, big);
    ld128 back = List_get(ld128)(wideValues, 0);
    CHECK(back.integer == big.integer);
    CHECK(back.fraction == big.fraction);

    // --- object stamp: the slot holds a pointer, not a copy --------------
    Buffer *buffer = Buffer_1(4);
    CHECK(buffer != nullptr);
    List(Buffer) *buffers = List(Buffer)(8);
    CHECK(buffers != nullptr);
    CHECK(List_stride((List*) buffers) == 8);       // one pointer word
    List_add(Buffer)(buffers, buffer);
    CHECK(List_get(Buffer)(buffers, 0) == buffer);
    Buffer **bufferSlot = List_slot(Buffer)(buffers, 0);
    CHECK(bufferSlot != nullptr);
    CHECK(*bufferSlot == buffer);
    CHECK(List_get(Buffer)(buffers, 1) == nullptr);

    // --- nested depth 2: List(List(u32)) stores the inner handle ---------
    List(u32) *inner = List(u32)(4);
    CHECK(inner != nullptr);
    List_add(u32)(inner, 42);
    List(List(u32)) *outer = List(List(u32))(4);
    CHECK(outer != nullptr);
    CHECK(List_stride((List*) outer) == sizeof(List_u32*));
    CHECK(List_elementClassId((List*) outer) == ID_LIST);
    List_add(List(u32))(outer, inner);
    CHECK(List_get(List(u32))(outer, 0) == inner);
    List_u32 **innerSlot = List_slot(List(u32))(outer, 0);
    CHECK(innerSlot != nullptr);
    CHECK(*innerSlot == inner);
    CHECK(List_get(List(u32))(outer, 1) == nullptr);
    CHECK(List_get(u32)(*innerSlot, 0) == 42);

    // --- nested object list: List(List(Buffer)) --------------------------
    List(List(Buffer)) *bufferBuckets = List(List(Buffer))(4);
    CHECK(bufferBuckets != nullptr);
    List_add(List(Buffer))(bufferBuckets, buffers);
    CHECK(List_get(List(Buffer))(bufferBuckets, 0) == buffers);

    // --- growth past the requested capacity -----------------------------
    List(u32) *grown = List(u32)(2048);
    for (u32 i = 0; i < 5000; i++)
        List_add(u32)(grown, i);
    CHECK(List_length((List*) grown) == 5000);
    CHECK(List_capacity((List*) grown) >= 5000);
    CHECK(List_get(u32)(grown, 0) == 0);
    CHECK(List_get(u32)(grown, 4999) == 4999);

    // --- null handles are cold-safe -------------------------------------
    CHECK(List_get(u32)(nullptr, 0) == 0);
    CHECK(List_slot(u32)(nullptr, 0) == nullptr);
    List_add(u32)(nullptr, 1);       // no-op
    List_free(u32)(nullptr);         // no-op

    // --- distinct C types, one engine layout ----------------------------
    CHECK(sizeof(List_u8) == sizeof(List));
    CHECK(sizeof(List_u32) == sizeof(List));
    CHECK(sizeof(List_Buffer) == sizeof(List));
    CHECK(sizeof(List_List_u32) == sizeof(List));

    // --- every registry entry: construct, stride, free ------------------
    VEX_LIST_VALUES(VEX_TEST_VALUE)
    VEX_LIST_OBJECTS(VEX_TEST_OBJECT)
    VEX_LIST_FAMILY(VEX_TEST_NESTED)

    // --- release, objects before the buffer they point at ---------------
    List_free(List(Buffer))(bufferBuckets);
    List_free(List(u32))(outer);
    List_free(u32)(inner);
    List_free(ld128)(wideValues);
    List_free(u32)(grown);
    List_free(Buffer)(buffers);
    Buffer_free(buffer);
    List_free(u64)(words);
    List_free(i32)(signedNumbers);
    List_free(u16)(halves);
    List_free(u8)(octets);
    List_free(u32)(numbers);

    if (g_failures == 0) {
        printf("list_generic_test: all assertions held\n");
        return 0;
    }
    printf("list_generic_test: %d FAILURES\n", g_failures);
    return 1;
}
