#ifndef OBJECTS_PROBABLE_H
#define OBJECTS_PROBABLE_H



typedef struct Probable {
    pointer object;
    u32 weight;
    u32 total;
} Probable;

#include "vex.h"

;;DRAFT
Probable *Probable_3(pointer object, u32 weight, u32 total);
Probable *Probable_2(const Probable *init, bytes count);

#define Probable(...) CONSTRUCTOR_DISPATCH(Probable, __VA_ARGS__)

fn Probable_free(Probable *p);

pointer Probable_object(Probable *p);
u32 Probable_weight(Probable *p);
u32 Probable_total(Probable *p);

fn Probable_setObject(Probable *p, pointer object);
fn Probable_setWeight(Probable *p, u32 weight);
fn Probable_setTotal(Probable *p, u32 total);
pointer Probable_get(Probable *p);

#endif
