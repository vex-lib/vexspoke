//
// vexspoke.h — THE vexspoke surface, one include.
//
//   #include "vexspoke.h"
//
// Everything vexspoke lives behind this single door. It includes the floor
// door (vex.h) first, then every vexspoke *directory* header — one per
// directory, per the Directory Header Law. Sub-modules and individual class
// headers stay includable directly; this header is the door engine code and
// applications use to reach all of vexspoke at once.
//
// NOTE: c23/free.h is deliberately NOT included here. It redirects the
// standard `free` to c23_free and is opt-in per translation unit (see the
// note in c23/free.h and nio/mem.c). Include "c23/free.h" yourself where you
// want that behavior.

#ifndef VEXSPOKE_H
#define VEXSPOKE_H

#include "vex.h"

// --- floor-adjacent: types, storage, structs, scalar vocabulary -----------

#include "algo/algo.h"
#include "c23/c23.h"
#include "event/event.h"
#include "exception/exception.h"
#include "generic/generic.h"
#include "io/io.h"
#include "lang/lang.h"
#include "math/math.h"
#include "net/net.h"
#include "objects/objects.h"
#include "oop/oop.h"
#include "primitive/primitive.h"
#include "regex/regex.h"
#include "search/search.h"
#include "security/security.h"
#include "struct/struct.h"
#include "system/system.h"
#include "thread/thread.h"
#include "time/time.h"
#include "util/util.h"

// --- CLI / deferred / engine / spoke (single-header subsystems) ----------

#include "cli/console.h"
#include "deferred/dispatch.h"
#include "engine/loop.h"
#include "spoke/bespoke.h"

#endif // VEXSPOKE_H
