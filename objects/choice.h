#ifndef OBJECTS_CHOICE_H
#define OBJECTS_CHOICE_H

#include "vex.h"

// objects/choice.h — Immutable deterministic choice / branch dispatcher.
// Ported from legacy objects/Choice.java.

typedef fn (*ChoiceCallback)(u64 objectPtr, any userdata);

typedef struct Choice Choice;

// Allocate an immutable Choice array with option object pointers and callbacks
Choice *Choice_3(const u64 *objectPtrs, const ChoiceCallback *callbacks, bytes count);

// Free choice memory

Choice *Choice_2(const Choice *init, bytes count);

#define Choice(...) CONSTRUCTOR_DISPATCH(Choice, __VA_ARGS__)

fn Choice_free(Choice *choice);

// Inspect choices
bytes   Choice_length(const Choice *choice);
u64 Choice_getObject(const Choice *choice, bytes index);

// Trigger the callback associated with choice option 'index'
fn Choice_trigger(const Choice *choice, bytes index, any userdata);

#endif
