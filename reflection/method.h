#ifndef REFLECTION_METHOD_H
#define REFLECTION_METHOD_H

#include "vex.h"
#include "substrate/variable/variable_slot.h"

// reflection/method.h — the Method metadata record.
//
// A Method is a folded name (the atom's grammar), a callable
// (any (*)(any)), and a target (the owner / default receiver). A Class
// owns a list of these. The kind is the header typeId (TYPE_REFLECT_METHOD).
// Block layout: [MemoryHeader 16][name 24][invoke 8][target 8] = 40 Bytes.
//
// COLD PATH ONLY (the Cold-Only Reflection Law).

#define REFLECT_METHOD_NAME_BYTES VARIABLE_SLOT_NAME_BYTES
#define REFLECT_METHOD_NAME_MAX VARIABLE_SLOT_NAME_MAX

typedef any (*MethodFn)(any);

typedef struct Method {
    char name[REFLECT_METHOD_NAME_BYTES]; // folded name (atom grammar)
    MethodFn invoke;                      // the callable
    any target;                         // the owner / default receiver
} Method;

_Static_assert(sizeof(Method) == 40u, "Method must stay 40 Bytes");

bool Method_init(Method *self, const char *name, MethodFn invoke, any target);
Method *Method_0();
Method *Method_1(const char *name);
Method *Method_2(const char *name, MethodFn invoke);
Method *Method_3(const char *name, MethodFn invoke, any target);
#define Method(...) CONSTRUCTOR_DISPATCH(Method, __VA_ARGS__)
fn Method_free(Method *self);

u64 Method_kind(const Method *self);
bool Method_check(const Method *self, u64 typeId);
any Method_call(Method *self, any arg);

bool Method_setName(Method *self, const char *name);
const char *Method_getName(const Method *self);
fn Method_setInvoke(Method *self, MethodFn invoke);
MethodFn Method_getInvoke(const Method *self);
fn Method_setTarget(Method *self, any target);
any Method_getTarget(const Method *self);

fn Method_toString(const Method *self, char *dest, bytes cap, bool *outTruncated);
fn Method_toStringStruct(const Method *self, char *dest, bytes cap, bool *outTruncated);

#endif
