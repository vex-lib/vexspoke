#ifndef GENERIC_LIST_STAMPS_H
#define GENERIC_LIST_STAMPS_H

#include "vex.h"

#include "generic/list_tmpl.h"
#include "oop/vexspoke_type.h"
#include "primitive/buffer.h"

// generic/list_stamps.h — the stamped List family.
//
// Each block is one instantiation: the class, its typed API, and the chooser
// line that turns `List_u32(23)` into a constructor call while `List_u32 *`
// stays the type. A macro cannot define another macro, so the chooser line is
// written out — it is the one hand-written line per stamped type.
//
// Add a type here and to VEX_LIST_FAMILY in generic/classgeneric_repo.h: the
// stamp is the contract, and the family list is the registry.
//
// Element tokens are the ecosystem spellings (the Primitive Spelling Law):
// u8, i32, u32, u64 are scalars; Buffer is an object (stored as Buffer*).

// The generic spelling. List(u32) -> List_u32; the engine type `List` is
// untouched because a function-like macro only expands when followed by `(`.
#define List(T) VEX_CAT(List_, T)

// --- scalars ---------------------------------------------------------------

;;CLASSNAME("List_u8")
TYPEDEF_LIST_SCALAR(u8, ID_BYTE);

;;CLASSNAME("List_i32")
TYPEDEF_LIST_SCALAR(i32, ID_INT);

;;CLASSNAME("List_u32")
TYPEDEF_LIST_SCALAR(u32, ID_INT);

;;CLASSNAME("List_u64")
TYPEDEF_LIST_SCALAR(u64, ID_LONG);

// --- objects ---------------------------------------------------------------

;;CLASSNAME("List_Buffer")
TYPEDEF_LIST_OBJECT(Buffer, ID_BUFFER);

// --- constructors ----------------------------------------------------------
// One chooser per stamped type: `List_u8(...)` is the constructor, `List_u8` is
// the class. Both spellings live on the same token, which is exactly why the
// type can be written once and the capacity passed separately.

#define List_u8(...)     List_u8_2(__VA_ARGS__)
#define List_i32(...)    List_i32_2(__VA_ARGS__)
#define List_u32(...)    List_u32_2(__VA_ARGS__)
#define List_u64(...)    List_u64_2(__VA_ARGS__)
#define List_Buffer(...) List_Buffer_2(__VA_ARGS__)

#endif // GENERIC_LIST_STAMPS_H
