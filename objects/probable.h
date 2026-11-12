#ifndef OBJECTS_PROBABLE_H
#define OBJECTS_PROBABLE_H

#include "vex.h"

typedef struct Probable {
    pointer object;
    u32 weight;
    u32 total;
} Probable;

;;DRAFT
Probable *Probable_3(pointer object, u32 weight, u32 total);
Probable *Probable_2(const Probable *init, bytes count);

#define Probable(...) CONSTRUCTOR_DISPATCH(Probable, __VA_ARGS__)

void Probable_free(Probable *p);

pointer Probable_object(Probable *p);
u32 Probable_weight(Probable *p);
u32 Probable_total(Probable *p);

void Probable_setObject(Probable *p, pointer object);
void Probable_setWeight(Probable *p, u32 weight);
void Probable_setTotal(Probable *p, u32 total);
pointer Probable_get(Probable *p);

#endif
