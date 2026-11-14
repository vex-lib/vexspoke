#ifndef C23_FREE_H
#define C23_FREE_H

#include "vex.h"

// c23/free.h — The Relational Destructor Dispatcher.
// 
// In vex, everything is a pointer and every block carries its type in a header.
// This function overloads the standard free(any), automatically routing to the
// correct destructor (e.g. Probable_free) based on the runtime type ID, before 
// actually reclaiming the memory block via Memory_free.

typedef fn (*DestructorFn)(any ptr);

// Register a custom destructor hook for a specific 64-bit type ID.
fn Destructor_register(u64 typeId, DestructorFn fn);
DestructorFn Destructor_lookup(u64 typeId);

fn c23_free(any ptr);

// Hijack the standard free call.
// Note: nio/mem.c must NOT include this file (or must #undef free), 
// because Memory_free needs to call the real libc free() to release the block.
#define free(ptr) c23_free(ptr)

#endif // C23_FREE_H
