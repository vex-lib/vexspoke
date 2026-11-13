#ifndef GENERIC_LIST_FAMILY_H
#define GENERIC_LIST_FAMILY_H

#include "vex.h"

#include "oop/vexspoke_type.h"

// generic/list_family.h — the repo's List element registry.
//
// This is the single place a reader (or a tool) learns what `List(T)` accepts.
// It is an X-list, not a translation table: the stamps in generic/list_stamps.h
// and generic/list_stamps.c expand it, so the registry and the definitions
// cannot drift.
//
// Two families, because storage differs:
//
//   VEX_LIST_VALUES   stored by value; one slot is sizeof(T) bytes, and the
//                     element class id is metadata only. The floor shapes
//                     (b16, ld128, mat2f) have no registered class number, so
//                     the stamp passes sizeof(T) explicitly and the id is 0.
//
//   VEX_LIST_OBJECTS  stored as a pointer; one slot is sizeof(T*) bytes. Every
//                     registered class with a constructor is owned by the
//                     caller, so the list holds the handle, never a copy.
//
// VEX_LIST_FAMILY is their concatenation, in declaration order: values first,
// then objects. Nested List(List(T)) is generated from VEX_LIST_FAMILY, so
// every entry here gets a depth-2 stamp without a second registry.
//
// Element ids follow oop/vexspoke_type.h. A value that has no registered id
// (b16, ld128, mat2f) passes 0: the stride is exact, and the id is only the
// block header's metadata, never the slot width.

// Value-stored elements: the scalar floor, the float shapes, and the two
// segment slot records. The scalar aliases map to the legacy numeric ids;
// `pointer` is the uintptr_t spelling of an untyped address.
#define VEX_LIST_VALUES(X) \
    X(u8,             ID_BYTE)             \
    X(u16,            ID_SHORT)            \
    X(u32,            ID_INT)              \
    X(u64,            ID_LONG)             \
    X(i8,             ID_BYTE)             \
    X(i16,            ID_SHORT)            \
    X(i32,            ID_INT)              \
    X(i64,            ID_LONG)             \
    X(f32,            ID_FLOAT)            \
    X(d64,            ID_f64)              \
    X(b16,            0)                   \
    X(pointer,        ID_LONG)             \
    X(ld128,          0)                   \
    X(vec2f,          ID_VEC2)             \
    X(vec3f,          ID_VEC3)             \
    X(vec4f,          ID_VEC4)             \
    X(mat2f,          0)                   \
    X(mat3f,          ID_MAT3)             \
    X(mat4f,          ID_MAT4)             \
    X(SegmentEntry,   ID_SEGMENT_ENTRY)    \
    X(SegmentPosting, ID_SEGMENT_POSTING)

// Pointer-stored elements: every registered class that owns its own storage.
// `List` is here too, which is what makes List(List(T)) an ordinary object
// stamp rather than a special case.
#define VEX_LIST_OBJECTS(X) \
    X(Array,           ID_ARRAYS)           \
    X(Buffer,          ID_BUFFER)           \
    X(ChunkedList,     ID_CHUNKED_LIST)     \
    X(CircleArray,     ID_CIRCLE_ARRAY)     \
    X(Class,           ID_REFLECT_CLASS)    \
    X(Deque,           ID_DEQUE)            \
    X(Exception,       ID_EXCEPTION)        \
    X(KeyMap,          ID_KEY_MAP)          \
    X(List,            ID_LIST)             \
    X(Map,             ID_MAP)              \
    X(MinHeap,         ID_MIN_HEAP)         \
    X(Octree,          ID_OCTREE)           \
    X(Point,           ID_POINT)            \
    X(Probable,        ID_PROBABLE)         \
    X(ProbableObjects, ID_PROBABLE_OBJECTS) \
    X(Queue,           ID_QUEUE)            \
    X(Random,          ID_RANDOM)           \
    X(Regex,           ID_REGEX)            \
    X(RingBuffer,      ID_RING_BUFFER)      \
    X(SegmentIndex,    ID_SEGMENT_INDEX)    \
    X(Set,             ID_SET)              \
    X(SparseSet,       ID_SPARSE_SET)       \
    X(SphereArray,     ID_SPHERE_ARRAY)     \
    X(Stack,           ID_STACK)            \
    X(Variable,        ID_REFLECT_VARIABLE)

// The whole base family, in declaration order. A nested stamp is derived from
// this list, not added to it: List(List(T)) is an object list of List_<T>.
#define VEX_LIST_FAMILY(X) \
    VEX_LIST_VALUES(X)     \
    VEX_LIST_OBJECTS(X)

// The object stamps only name T*/T**, so the registry forward-declares the
// classes instead of including 25 headers. A translation unit that creates or
// reads an element includes that class's own header; the same typedef is then
// re-stated with the same tag, which C11 allows (identical redefinition).
#define VEX_FORWARD_OBJECT(T, ID) typedef struct T T;
VEX_LIST_OBJECTS(VEX_FORWARD_OBJECT)
#undef VEX_FORWARD_OBJECT

#endif // GENERIC_LIST_FAMILY_H
