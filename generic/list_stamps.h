#ifndef GENERIC_LIST_STAMPS_H
#define GENERIC_LIST_STAMPS_H

#include "vex.h"

#include "generic/list_tmpl.h"
#include "generic/list_family.h"
#include "oop/vexspoke_type.h"
#include "algo/segment_index.h"

// generic/list_stamps.h — the stamped List family.
//
// The registry lives in generic/list_family.h. This header expands it twice:
// once for the base declarations, once for the depth-2 List(List(T))
// declarations. Then it writes the chooser lines that turn `List_u32(23)` into
// a constructor call while `List_u32 *` stays the type.
//
// A macro cannot define another macro, so the chooser lines are the one
// hand-written block per stamped type. They are literal on purpose: the
// registry drives the class and the functions, and these lines name the
// constructor. If a type is added to VEX_LIST_FAMILY it needs one base line
// and one nested line here.
//
// The value stamps need complete element types for their `T value` parameters,
// so the two segment slot records are reached through algo/segment_index.h.
// The object stamps need only `T*`/`T**`, so a consumer that adds a class to
// the registry does not have to include that class here; it includes the class
// where it adds or gets elements.

// The generic spelling. List(u32) -> List_u32; the engine type `List` is
// untouched because a function-like macro only expands when followed by `(`.
#define List(T) VEX_CAT(List_, T)

// --- declarations ----------------------------------------------------------

#define VEX_DECLARE_VALUE(T, ID)  TYPEDEF_LIST_VALUE(T, ID)
#define VEX_DECLARE_OBJECT(T, ID) TYPEDEF_LIST_OBJECT(T, ID)
#define VEX_DECLARE_NESTED(T, ID) VEX_LIST_DECLARE_NESTED(T, ID_LIST)

VEX_LIST_VALUES(VEX_DECLARE_VALUE)
VEX_LIST_OBJECTS(VEX_DECLARE_OBJECT)
VEX_LIST_FAMILY(VEX_DECLARE_NESTED)

// --- constructors ----------------------------------------------------------
// One chooser per stamped type: `List_u8(...)` is the constructor, `List_u8` is
// the class. Both spellings live on the same token, which is exactly why the
// type can be written once and the capacity passed separately.
//
// Base family, in VEX_LIST_FAMILY order (values then objects).

#define List_u8(...)             List_u8_2(__VA_ARGS__)
#define List_u16(...)            List_u16_2(__VA_ARGS__)
#define List_u32(...)            List_u32_2(__VA_ARGS__)
#define List_u64(...)            List_u64_2(__VA_ARGS__)
#define List_i8(...)             List_i8_2(__VA_ARGS__)
#define List_i16(...)            List_i16_2(__VA_ARGS__)
#define List_i32(...)            List_i32_2(__VA_ARGS__)
#define List_i64(...)            List_i64_2(__VA_ARGS__)
#define List_f32(...)            List_f32_2(__VA_ARGS__)
#define List_d64(...)            List_d64_2(__VA_ARGS__)
#define List_b16(...)            List_b16_2(__VA_ARGS__)
#define List_pointer(...)        List_pointer_2(__VA_ARGS__)
#define List_ld128(...)          List_ld128_2(__VA_ARGS__)
#define List_vec2f(...)          List_vec2f_2(__VA_ARGS__)
#define List_vec3f(...)          List_vec3f_2(__VA_ARGS__)
#define List_vec4f(...)          List_vec4f_2(__VA_ARGS__)
#define List_mat2f(...)          List_mat2f_2(__VA_ARGS__)
#define List_mat3f(...)          List_mat3f_2(__VA_ARGS__)
#define List_mat4f(...)          List_mat4f_2(__VA_ARGS__)
#define List_SegmentEntry(...)   List_SegmentEntry_2(__VA_ARGS__)
#define List_SegmentPosting(...) List_SegmentPosting_2(__VA_ARGS__)

#define List_Array(...)           List_Array_2(__VA_ARGS__)
#define List_Buffer(...)          List_Buffer_2(__VA_ARGS__)
#define List_ChunkedList(...)     List_ChunkedList_2(__VA_ARGS__)
#define List_CircleArray(...)     List_CircleArray_2(__VA_ARGS__)
#define List_Class(...)           List_Class_2(__VA_ARGS__)
#define List_Deque(...)           List_Deque_2(__VA_ARGS__)
#define List_Exception(...)       List_Exception_2(__VA_ARGS__)
#define List_KeyMap(...)          List_KeyMap_2(__VA_ARGS__)
#define List_List(...)            List_List_2(__VA_ARGS__)
#define List_Map(...)             List_Map_2(__VA_ARGS__)
#define List_MinHeap(...)         List_MinHeap_2(__VA_ARGS__)
#define List_Octree(...)          List_Octree_2(__VA_ARGS__)
#define List_Point(...)           List_Point_2(__VA_ARGS__)
#define List_Probable(...)        List_Probable_2(__VA_ARGS__)
#define List_ProbableObjects(...) List_ProbableObjects_2(__VA_ARGS__)
#define List_Queue(...)           List_Queue_2(__VA_ARGS__)
#define List_Random(...)          List_Random_2(__VA_ARGS__)
#define List_Regex(...)           List_Regex_2(__VA_ARGS__)
#define List_RingBuffer(...)      List_RingBuffer_2(__VA_ARGS__)
#define List_SegmentIndex(...)    List_SegmentIndex_2(__VA_ARGS__)
#define List_Set(...)             List_Set_2(__VA_ARGS__)
#define List_SparseSet(...)       List_SparseSet_2(__VA_ARGS__)
#define List_SphereArray(...)     List_SphereArray_2(__VA_ARGS__)
#define List_Stack(...)           List_Stack_2(__VA_ARGS__)
#define List_Variable(...)        List_Variable_2(__VA_ARGS__)

// Nested depth 2, derived from the same family: List(List(T)) -> List_List_<T>.

#define List_List_u8(...)             List_List_u8_2(__VA_ARGS__)
#define List_List_u16(...)            List_List_u16_2(__VA_ARGS__)
#define List_List_u32(...)            List_List_u32_2(__VA_ARGS__)
#define List_List_u64(...)            List_List_u64_2(__VA_ARGS__)
#define List_List_i8(...)             List_List_i8_2(__VA_ARGS__)
#define List_List_i16(...)            List_List_i16_2(__VA_ARGS__)
#define List_List_i32(...)            List_List_i32_2(__VA_ARGS__)
#define List_List_i64(...)            List_List_i64_2(__VA_ARGS__)
#define List_List_f32(...)            List_List_f32_2(__VA_ARGS__)
#define List_List_d64(...)            List_List_d64_2(__VA_ARGS__)
#define List_List_b16(...)            List_List_b16_2(__VA_ARGS__)
#define List_List_pointer(...)        List_List_pointer_2(__VA_ARGS__)
#define List_List_ld128(...)          List_List_ld128_2(__VA_ARGS__)
#define List_List_vec2f(...)          List_List_vec2f_2(__VA_ARGS__)
#define List_List_vec3f(...)          List_List_vec3f_2(__VA_ARGS__)
#define List_List_vec4f(...)          List_List_vec4f_2(__VA_ARGS__)
#define List_List_mat2f(...)          List_List_mat2f_2(__VA_ARGS__)
#define List_List_mat3f(...)          List_List_mat3f_2(__VA_ARGS__)
#define List_List_mat4f(...)          List_List_mat4f_2(__VA_ARGS__)
#define List_List_SegmentEntry(...)   List_List_SegmentEntry_2(__VA_ARGS__)
#define List_List_SegmentPosting(...) List_List_SegmentPosting_2(__VA_ARGS__)

#define List_List_Array(...)           List_List_Array_2(__VA_ARGS__)
#define List_List_Buffer(...)          List_List_Buffer_2(__VA_ARGS__)
#define List_List_ChunkedList(...)     List_List_ChunkedList_2(__VA_ARGS__)
#define List_List_CircleArray(...)     List_List_CircleArray_2(__VA_ARGS__)
#define List_List_Class(...)           List_List_Class_2(__VA_ARGS__)
#define List_List_Deque(...)           List_List_Deque_2(__VA_ARGS__)
#define List_List_Exception(...)       List_List_Exception_2(__VA_ARGS__)
#define List_List_KeyMap(...)          List_List_KeyMap_2(__VA_ARGS__)
#define List_List_List(...)            List_List_List_2(__VA_ARGS__)
#define List_List_Map(...)             List_List_Map_2(__VA_ARGS__)
#define List_List_MinHeap(...)         List_List_MinHeap_2(__VA_ARGS__)
#define List_List_Octree(...)          List_List_Octree_2(__VA_ARGS__)
#define List_List_Point(...)           List_List_Point_2(__VA_ARGS__)
#define List_List_Probable(...)        List_List_Probable_2(__VA_ARGS__)
#define List_List_ProbableObjects(...) List_List_ProbableObjects_2(__VA_ARGS__)
#define List_List_Queue(...)           List_List_Queue_2(__VA_ARGS__)
#define List_List_Random(...)          List_List_Random_2(__VA_ARGS__)
#define List_List_Regex(...)           List_List_Regex_2(__VA_ARGS__)
#define List_List_RingBuffer(...)      List_List_RingBuffer_2(__VA_ARGS__)
#define List_List_SegmentIndex(...)    List_List_SegmentIndex_2(__VA_ARGS__)
#define List_List_Set(...)             List_List_Set_2(__VA_ARGS__)
#define List_List_SparseSet(...)       List_List_SparseSet_2(__VA_ARGS__)
#define List_List_SphereArray(...)     List_List_SphereArray_2(__VA_ARGS__)
#define List_List_Stack(...)           List_List_Stack_2(__VA_ARGS__)
#define List_List_Variable(...)        List_List_Variable_2(__VA_ARGS__)

#endif // GENERIC_LIST_STAMPS_H
