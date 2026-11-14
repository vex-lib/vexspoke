#ifndef PRIMITIVE_PRIMITIVE_H
#define PRIMITIVE_PRIMITIVE_H

// primitive/primitive.h — THE primitive directory header.
// One include for the whole scalar vocabulary: bool, byte, int, long, short,
// char_sequence, float/f32, d64, fixed32/fixed64, brain, buffer, vex-string.
// Sub-headers stay includable individually; this header is the door engine
// code uses.

#include "vex.h"
#include "primitive/bool.h"
#include "primitive/brain.h"
#include "primitive/buffer.h"
#include "primitive/byte.h"
#include "primitive/char_sequence.h"
#include "primitive/double.h"
#include "primitive/fixed32.h"
#include "primitive/fixed64.h"
#include "primitive/int.h"
#include "primitive/long.h"
#include "primitive/pack.h"
#include "primitive/short.h"
#include "primitive/vex-float.h"
#include "primitive/vex-string.h"

#endif // PRIMITIVE_PRIMITIVE_H
