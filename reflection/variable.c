// reflection/variable.c — the Variable metadata record (base of the hierarchy).

#include "vex.h"
#include "reflection/variable.h"

#include "nio/mem.h"
#include "oop/vexspoke_type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Variable
 * ============================================================================
 * The base reflection kind: a 40-byte record holding a folded name (the atom's
 * grammar), a reader, and a target. A Field embeds one. The kind is the header
 * typeId (TYPE_REFLECT_VARIABLE). Records of a kind are stored segregated (one
 * homogeneous array per kind) per the BitPool Slot Segregation Law.
 *
 * Lifetime: arena-allocated (TYPE_REFLECT_VARIABLE) or embedded (a Field embeds
 * one with no header). Cold path only (the Cold-Only Reflection Law); getters
 * are null-safe.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Variable (reflection/variable.c)
 * ============================================================================
 * the Variable metadata record (name + reader + target).
 *
 * STRUCT FIELDS (Mirroring reflection/variable.h):
 * ----------------------------------------------------------------------------
 *   Variable {
 *     char name[24];      // folded name (atom grammar)
 *     VariableReadFn read; // the reader
 *     any target;       // the value / owner
 *   }
 *
 * PRIVATE HELPERS (kept file-local, pure logic, no behavior of their own): none.
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Constructors: (.h) Variable_0() .. _3(name, read, target), _init, _free
 * Public Core Functions: (.h) Variable_kind, _check, _read
 * Public Setters: (.h) Variable_setName / _setRead / _setTarget
 * Public Getters: (.h) Variable_getName / _getRead / _getTarget
 * Public String Projections: (.h) Variable_toString / _toStringStruct
 * ============================================================================
 */


;;GETTER
static inline char *Variable_nameMut(Variable *s) { return (*s).name; }
;;GETTER
static inline const char *Variable_name(const Variable *s) { return (*s).name; }
;;GETTER
static inline VariableReadFn Variable_readRaw(const Variable *s) { return (*s).read; }
;;SETTER
static inline fn Variable_setReadRaw(Variable *s, VariableReadFn v) { (*s).read = v; }
;;GETTER
static inline any Variable_target(const Variable *s) { return (*s).target; }
;;SETTER
static inline fn Variable_setTargetRaw(Variable *s, any v) { (*s).target = v; }

// CONSTRUCTORS

bool Variable_init(Variable *self, const char *name, VariableReadFn read, any target) {
    if (self == nullptr)
        return false;
    char folded[REFLECT_VARIABLE_NAME_BYTES];
    if (!VariableSlot_foldName(name, folded))
        return false;
    memset(self, 0, sizeof(*self));
    memcpy(Variable_nameMut(self), folded, strlen(folded) + 1);
    Variable_setReadRaw(self, read);
    Variable_setTargetRaw(self, target);
    return true;
}

Variable *Variable_0() {
    Variable *self = (Variable*) Memory_alloc(TYPE_REFLECT_VARIABLE, sizeof(Variable));
    if (self == nullptr)
        return nullptr;
    memset(self, 0, sizeof(*self));
    return self;
}

Variable *Variable_1(const char *name) {
    return Variable_3(name, nullptr, nullptr);
}

Variable *Variable_2(const char *name, VariableReadFn read) {
    return Variable_3(name, read, nullptr);
}

Variable *Variable_3(const char *name, VariableReadFn read, any target) {
    Variable *self = Variable_0();
    if (self == nullptr)
        return nullptr;
    if (!Variable_init(self, name, read, target)) {
        Memory_free(self);
        return nullptr;
    }
    return self;
}

fn Variable_free(Variable *self) {
    if (self != nullptr)
        Memory_free(self);
}

// CORE FUNCTIONS

u64 Variable_kind(const Variable *self) {
    if (self == nullptr)
        return 0u;
    return Memory_type((any) self);
}

bool Variable_check(const Variable *self, u64 typeId) {
    if (self == nullptr)
        return false;
    return Memory_type((any) self) == typeId;
}

any Variable_read(Variable *self, any arg) {
    if (self == nullptr || Variable_readRaw(self) == nullptr)
        return nullptr;
    return Variable_readRaw(self)(arg);
}

// SETTERS

bool Variable_setName(Variable *self, const char *name) {
    if (self == nullptr)
        return false;
    char folded[REFLECT_VARIABLE_NAME_BYTES];
    if (!VariableSlot_foldName(name, folded))
        return false;
    memset(Variable_nameMut(self), 0, sizeof ((Variable){0}).name);
    memcpy(Variable_nameMut(self), folded, strlen(folded) + 1);
    return true;
}

fn Variable_setRead(Variable *self, VariableReadFn read) {
    if (self == nullptr)
        return;
    Variable_setReadRaw(self, read);
}

fn Variable_setTarget(Variable *self, any target) {
    if (self == nullptr)
        return;
    Variable_setTargetRaw(self, target);
}

// GETTERS

const char *Variable_getName(const Variable *self) {
    return self ? Variable_name(self) : nullptr;
}

VariableReadFn Variable_getRead(const Variable *self) {
    if (self == nullptr)
        return nullptr;
    return Variable_readRaw(self);
}

any Variable_getTarget(const Variable *self) {
    if (self == nullptr)
        return nullptr;
    return Variable_target(self);
}

// STRING PROJECTIONS (the toString Law)

fn Variable_toString(const Variable *self, char *dest, bytes cap, bool *outTruncated) {
    if (outTruncated)
        *outTruncated = false;
    if (dest == nullptr || cap == 0u)
        return;
    if (self == nullptr) {
        snprintf(dest, cap, "nullptr");
        return;
    }
    i32 written = snprintf(dest, cap, "Variable(%s)", Variable_name(self)[0] == '\0' ? "?" : Variable_name(self));
    if (written < 0 || (bytes) written >= cap) {
        if (outTruncated)
            *outTruncated = true;
    }
}

fn Variable_toStringStruct(const Variable *self, char *dest, bytes cap, bool *outTruncated) {
    if (outTruncated)
        *outTruncated = false;
    if (dest == nullptr || cap == 0u)
        return;
    if (self == nullptr) {
        snprintf(dest, cap, "nullptr");
        return;
    }
    i32 written = snprintf(dest, cap, "Variable { name=\"%s\", read=0x%llx, target=0x%llx }",
                           Variable_name(self),
                           (u64) (pointer) Variable_readRaw(self),
                           (u64) (pointer) Variable_target(self));
    if (written < 0 || (bytes) written >= cap) {
        if (outTruncated)
            *outTruncated = true;
    }
}
