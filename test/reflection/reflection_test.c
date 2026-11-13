#include "vex.h"

#include <stdio.h>

#include "nio/mem.h"
#include "oop/vexspoke_type.h"
#include "reflection/class.h"
#include "reflection/field.h"
#include "reflection/method.h"
#include "reflection/struct.h"
#include "reflection/variable.h"

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: ReflectionTest (reflection_test/test.c)
 * LEVEL: L3 — Module Code (headless verification harness)
 * ============================================================================
 * Headless suite for the reflection hierarchy: Variable (base) -> Field
 * (embeds a Variable + setter) -> Struct (a list of Fields) -> Class (a Struct
 * layout + Methods). Per-kind layouts, header-kind identity, the atom name
 * grammar, callables (read/write/call/construct), the embedded-Variable
 * relationship, string projections, and null-safety.
 *
 * STRUCT FIELDS: none — procedural test harness.
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - main(void)
 * ============================================================================
 */

static int g_fail = 0;
static int g_checks = 0;

#define CHECK(cond)                                                       \
    do {                                                                  \
        g_checks++;                                                       \
        if (!(cond)) {                                                    \
            g_fail++;                                                     \
            printf("  FAIL line %d: %s\n", __LINE__, #cond);              \
        }                                                                 \
    } while (0)

// Test callable that f64s the pointer-encoded argument.
static any f64It(any arg) {
    return (any) (pointer) (((pointer) arg) * 2u);
}

typedef struct Holder {
    pointer value;
} Holder;

// Reads the test Holder's value through the reflection getter signature.
static any holderGet(any receiver) {
    return (any) (pointer) ((*(Holder*) receiver).value);
}

// Writes the test Holder's value through the reflection setter signature.
static void holderSet(any receiver, any value) {
    (*(Holder*) receiver).value = (pointer) value;
}

// Pins the byte sizes of each reflection record, including embedded fields.
static void testLayouts(void) {
    printf("[1] per-kind layouts stay fixed\n");
    CHECK(sizeof(Variable) == 40u);
    CHECK(sizeof(Field) == 72u);   // embedded Variable (40) + setter + physical layout
    CHECK(sizeof(Method) == 40u);
    CHECK(sizeof(Struct) == 40u);
    CHECK(sizeof(Class) == 56u);   // name + construct + layout + methods + count + pad
}

// Verifies each reflective class carries and checks its own type identity.
static void testKinds(void) {
    printf("[2] kind == the header typeId\n");

    Class *c = Class("Widget");
    Struct *s = Struct("Layout");
    Method *m = Method("update");
    Variable *v = Variable("health");

    CHECK(c && Class_check(c, TYPE_REFLECT_CLASS) && Class_kind(c) == TYPE_REFLECT_CLASS);
    CHECK(s && Struct_check(s, TYPE_REFLECT_STRUCT));
    CHECK(m && Method_check(m, TYPE_REFLECT_METHOD));
    CHECK(v && Variable_check(v, TYPE_REFLECT_VARIABLE));

    // Exactly one kind each.
    CHECK(!Class_check(c, TYPE_REFLECT_METHOD));
    CHECK(!Method_check(m, TYPE_REFLECT_STRUCT));
    CHECK(!Struct_check(s, TYPE_REFLECT_VARIABLE));

    Class_free(c);
    Struct_free(s);
    Method_free(m);
    Variable_free(v);
}

// Checks folded dotted names and rejects malformed, overlong, and spaced names.
static void testNames(void) {
    printf("[3] names use the atom grammar (fold + dotted)\n");

    Method *m = Method("Update.Physics");
    CHECK(m != nullptr);
    if (m) {
        CHECK(strcmp(Method_getName(m), "update.physics") == 0);
        CHECK(Method_setName(m, "step") == true);
        CHECK(strcmp(Method_getName(m), "step") == 0);
        CHECK(Method_setName(m, "bad name") == false); // space rejected
        CHECK(strcmp(Method_getName(m), "step") == 0);
        Method_free(m);
    }
    CHECK(Method("a..b.sh") == nullptr);
    CHECK(Field(".x") == nullptr);
    CHECK(Variable("abcdefghijklmnopqrstuvwx") == nullptr); // 24
}

// Checks Field's first-member Variable layout and delegated read/write accessors.
static void testFieldEmbedsVariable(void) {
    printf("[4] field embeds variable: name/read/target delegate, write is the setter\n");

    Holder holder = { 0u };
    Field *f = Field("position.x", holderGet, holderSet, &holder);
    CHECK(f != nullptr);
    if (!f)
        return;

    // The embedded Variable is the field's first member.
    Variable *inner = Field_getVariable(f);
    CHECK(inner != nullptr);
    CHECK((pointer) inner == (pointer) f);
    CHECK(Variable_read(inner, &holder) == (any) (pointer) 0u);

    CHECK(strcmp(Field_getName(f), "position.x") == 0);
    CHECK(Field_getRead(f) == holderGet);
    CHECK(Field_getSet(f) == holderSet);
    CHECK(Field_getTarget(f) == &holder);

    CHECK(Field_read(f, &holder) == (any) (pointer) 0u);
    Field_write(f, &holder, (any) (pointer) 42u);
    CHECK(holder.value == 42u);
    CHECK(Field_read(f, &holder) == (any) (pointer) 42u);

    Field_free(f);
}

// Checks Field physical metadata, derived stride, explicit size, flags, and null defaults.
static void testFieldLayout(void) {
    printf("[8] field physical layout: typeId derives size, offset/flags round-trip\n");

    Field *f = Field("position.x", holderGet, holderSet, nullptr);
    CHECK(f != nullptr);
    if (!f)
        return;

    CHECK(Field_getTypeId(f) == 0u);
    CHECK(Field_getOffset(f) == 0u);
    CHECK(Field_getSize(f) == 0u);
    CHECK(Field_getFlags(f) == REFLECT_FIELD_NONE);

    // typeId derives the byte width from Stride for a known class id.
    Field_setTypeId(f, TYPE_INT_SINGLETON);
    CHECK(Field_getTypeId(f) == TYPE_INT_SINGLETON);
    CHECK(Field_getSize(f) == 4u);

    Field_setOffset(f, 16u);
    CHECK(Field_getOffset(f) == 16u);

    // An explicit size overrides the derived width.
    Field_setSize(f, 8u);
    CHECK(Field_getSize(f) == 8u);

    Field_setFlags(f, REFLECT_FIELD_KEY | REFLECT_FIELD_NULLABLE | REFLECT_FIELD_INDEXED);
    CHECK(Field_isKey(f) && Field_isNullable(f) && Field_isIndexed(f));
    CHECK(Field_getFlags(f) == (REFLECT_FIELD_KEY | REFLECT_FIELD_NULLABLE | REFLECT_FIELD_INDEXED));

    bool truncated = true;
    char buf[220];
    Field_toStringStruct(f, buf, sizeof(buf), &truncated);
    CHECK(truncated == false && strstr(buf, "offset=16") != nullptr && strstr(buf, "size=8") != nullptr);

    // Null-safety across the physical accessors.
    CHECK(Field_getTypeId(nullptr) == 0u);
    CHECK(Field_getOffset(nullptr) == 0u);
    CHECK(Field_getSize(nullptr) == 0u);
    CHECK(Field_getFlags(nullptr) == REFLECT_FIELD_NONE);
    CHECK(!Field_isKey(nullptr) && !Field_isNullable(nullptr) && !Field_isIndexed(nullptr));
    Field_setOffset(nullptr, 5u); // no crash

    Field_free(f);
}

// Checks Struct field admission, indexed access, growth, and row address stability.
static void testStruct(void) {
    printf("[5] struct: a growable list of field rows\n");

    Struct *s = Struct("player");
    CHECK(s != nullptr);
    if (!s)
        return;
    CHECK(Struct_isEmpty(s) == true);
    CHECK(Struct_getSize(s) == 0u);
    Struct_setSize(s, 64u);
    CHECK(Struct_getSize(s) == 64u);
    CHECK(Struct_getSize(nullptr) == 0u);

    Field *f = Field("position.x", holderGet, holderSet, nullptr);
    for (u32 i = 0u; i < 100u; i++)
        CHECK(Struct_add(s, f) == i);
    CHECK(Struct_count(s) == 100u);
    CHECK(Struct_isEmpty(s) == false);

    // Rows are stable + readable.
    Field *row = Struct_get(s, 37u);
    CHECK(row != nullptr);
    CHECK(strcmp(Field_getName(row), "position.x") == 0);
    CHECK(Struct_get(s, 100u) == nullptr);

    // Stable across growth: row 0's address never moves.
    Field *row0 = Struct_get(s, 0u);
    for (u32 i = 100u; i < 300u; i++)
        Struct_add(s, f);
    CHECK(Struct_get(s, 0u) == row0);
    CHECK(Struct_count(s) == 300u);

    Field_free(f);
    Struct_free(s);
}

// Checks Class layout/constructor dispatch, method registration, and callable access.
static void testClass(void) {
    printf("[6] class: struct layout + methods + construct\n");

    Struct *layout = Struct("player");
    Field *fx = Field("position.x", holderGet, holderSet, nullptr);
    Field *fy = Field("position.y", holderGet, holderSet, nullptr);
    Struct_add(layout, fx);
    Struct_add(layout, fy);

    Method *m1 = Method("update", f64It, nullptr);
    Method *m2 = Method("render", f64It, nullptr);

    Class *c = Class("PlayerClass", layout, f64It);
    CHECK(c != nullptr);
    if (!c) {
        Field_free(fx);
        Field_free(fy);
        Method_free(m1);
        Method_free(m2);
        Struct_free(layout);
        return;
    }
    CHECK(Class_check(c, TYPE_REFLECT_CLASS));
    CHECK(Class_getLayout(c) == layout);
    CHECK(Class_getConstruct(c) == f64It);
    CHECK(Class_construct(c, (any) (pointer) 5u) == (any) (pointer) 10u);

    CHECK(Class_addMethod(c, m1) == 0u);
    CHECK(Class_addMethod(c, m2) == 1u);
    CHECK(Class_methodCount(c) == 2u);
    CHECK(Class_getMethod(c, 100u) == nullptr);

    Method *got = Class_getMethod(c, 1u);
    CHECK(strcmp(Method_getName(got), "render") == 0);
    CHECK(Method_call(got, (any) (pointer) 3u) == (any) (pointer) 6u);

    // The layout's fields are reachable through the class.
    Struct *viaClass = Class_getLayout(c);
    Field *fieldRow = Struct_get(viaClass, 0u);
    CHECK(strcmp(Field_getName(fieldRow), "position.x") == 0);

    Class_free(c);
    Field_free(fx);
    Field_free(fy);
    Method_free(m1);
    Method_free(m2);
    Struct_free(layout);
}

// Checks bounded value/structure projections, truncation, hierarchy null safety, and freeing.
static void testStringsAndNull(void) {
    printf("[7] string projections + null-safety\n");

    Variable *v = Variable("health", f64It, nullptr);
    Method *m = Method("step", f64It, nullptr);
    char buf[160];
    bool truncated = true;
    Variable_toString(v, buf, sizeof(buf), &truncated);
    CHECK(truncated == false && strcmp(buf, "Variable(health)") == 0);
    Variable_toStringStruct(v, buf, sizeof(buf), &truncated);
    CHECK(truncated == false && strncmp(buf, "Variable { name=\"health\", read=0x", 33) == 0);
    Method_toString(m, buf, sizeof(buf), &truncated);
    CHECK(truncated == false && strcmp(buf, "Method(step)") == 0);

    char tiny[4];
    Variable_toString(v, tiny, sizeof(tiny), &truncated);
    CHECK(truncated == true);
    truncated = true;
    Variable_toString(nullptr, buf, sizeof(buf), &truncated);
    CHECK(truncated == false && strcmp(buf, "nullptr") == 0);

    // Null-safety across the hierarchy.
    CHECK(Class_kind(nullptr) == 0u && Class_check(nullptr, TYPE_REFLECT_CLASS) == false);
    CHECK(Struct_count(nullptr) == 0u && Struct_get(nullptr, 0u) == nullptr);
    CHECK(Field_getName(nullptr) == nullptr);
    CHECK(Variable_getTarget(nullptr) == nullptr && Variable_read(nullptr, nullptr) == nullptr);
    CHECK(Method_call(nullptr, nullptr) == nullptr);
    Struct_free(nullptr);
    Class_free(nullptr);
    Field_free(nullptr);
    Method_free(nullptr);
    Variable_free(nullptr);
    CHECK(true);

    Variable_free(v);
    Method_free(m);
}

// Runs layout, identity, name, field, struct, class, and projection contract cases.
int main(void) {
    printf("=== Reflection (hierarchy) Test Suite ===\n\n");

    testLayouts();
    testKinds();
    testNames();
    testFieldEmbedsVariable();
    testFieldLayout();
    testStruct();
    testClass();
    testStringsAndNull();

    printf("\n%d checks, %d failed\n", g_checks, g_fail);
    if (g_fail == 0)
        printf("=== Reflection Test Suite: ALL PASS ===\n");
    return g_fail == 0 ? 0 : 1;
}
