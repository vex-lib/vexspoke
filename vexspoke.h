//
// vexspoke.h — THE vexspoke surface, one include.
//
// Everything vexspoke lives behind this single door:
//   #include "vexspoke.h"
//
// Sub-modules stay includable individually; this header is the door engine
// code and applications use to reach all of vexspoke at once.
//
// One door, everything below it. vex.h first (the floor), then every
// vexspoke subsystem in dependency order: low-level storage and types first,
// then language, math, reactives, and the higher-tier subsystems last.
//
// The c23 free-hijack (c23/free.h) is deliberately NOT included here: it
// redirects the standard `free` to c23_free and is opt-in per translation
// unit (see nio/mem.c's note). Include "c23/free.h" yourself where you want
// that behavior.

#ifndef VEXSPOKE_H
#define VEXSPOKE_H

#include "vex.h"

// --- floor-adjacent: types, storage, structs ---------------------------------

#include "oop/vexspoke_type.h"
#include "oop/stride.h"

#include "util/arrays.h"
#include "util/hash.h"
#include "util/random.h"

#include "struct/array.h"
#include "struct/chunked_list.h"
#include "struct/circle_array.h"
#include "struct/collection.h"
#include "struct/deque.h"
#include "struct/exhaustion.h"
#include "struct/list.h"
#include "struct/map.h"
#include "struct/minheap.h"
#include "struct/octree.h"
#include "struct/queue.h"
#include "struct/ring.h"
#include "struct/set.h"
#include "struct/sparseset.h"
#include "struct/sphere_array.h"
#include "struct/stack.h"

// --- scalar vocabulary and language ------------------------------------------

#include "lang/str.h"
#include "lang/point/point.h"
#include "lang/rect/rectangle.h"

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

// --- math --------------------------------------------------------------------

#include "math/coord_frame.h"
#include "math/fast_math.h"
#include "math/strict_math.h"
#include "math/vex-math.h"

// --- c23 ---------------------------------------------------------------------

#include "c23/equals.h"
#include "c23/fn.h"
#include "c23/type_overload.h"
#include "c23/zero.h"

// --- reactive core -----------------------------------------------------------

#include "reactive/reactive.h"
#include "reactive/dispatch.h"
#include "reactive/generic.h"
#include "reactive/reactive_object.h"
#include "reactive/reactive_primitive.h"
#include "reactive/reactive_probable.h"
#include "reactive/reactive_probable_tmpl.h"
#include "reactive/reactive_tmpl.h"

// --- objects -----------------------------------------------------------------

#include "objects/choice.h"
#include "objects/future.h"
#include "objects/global.h"
#include "objects/local.h"
#include "objects/passive.h"
#include "objects/probable.h"
#include "objects/probable_objects.h"
#include "objects/reactive.h"

// --- relational engine seam --------------------------------------------------

#include "re/type/re_type.h"

// --- algorithms --------------------------------------------------------------

#include "algo/bvh.h"
#include "algo/dijkstra.h"
#include "algo/kd_tree.h"
#include "algo/segment_index.h"
#include "algo/sort.h"
#include "algo/spatial.h"
#include "algo/tree_sit.h"

// --- CLI ---------------------------------------------------------------------

#include "cli/console.h"

// --- deferred ----------------------------------------------------------------

#include "deferred/dispatch.h"

// --- engine ------------------------------------------------------------------

#include "engine/loop.h"

// --- event -------------------------------------------------------------------

#include "event/keyhandler.h"
#include "event/mousehandler.h"
#include "event/touchhandler.h"

// --- exception ---------------------------------------------------------------

#include "exception/exception.h"
#include "exception/try.h"
#include "exception/try_code.h"
#include "exception/try_ptr.h"
#include "exception/try_value.h"

// --- input -------------------------------------------------------------------

#include "input/focus.h"
#include "input/gamepad.h"
#include "input/gesture.h"
#include "input/hardware_event.h"
#include "input/key.h"
#include "input/key_map.h"
#include "input/mouse.h"
#include "input/piano_key.h"
#include "input/touch.h"
#include "input/turntable.h"

// --- io ----------------------------------------------------------------------

#include "io/io.h"

// --- net ---------------------------------------------------------------------

#include "net/net.h"

// --- reflection --------------------------------------------------------------

#include "reflection/class.h"
#include "reflection/field.h"
#include "reflection/method.h"
#include "reflection/struct.h"
#include "reflection/variable.h"

// --- regex -------------------------------------------------------------------

#include "regex/charclass.h"
#include "regex/nodelist.h"
#include "regex/parser.h"
#include "regex/program.h"
#include "regex/regex.h"
#include "regex/span.h"
#include "regex/vm.h"

// --- search ------------------------------------------------------------------

#include "search/calc.h"
#include "search/find.h"
#include "search/spotlight.h"

// --- security ----------------------------------------------------------------

#include "security/argon2.h"
#include "security/base32.h"
#include "security/blake2b.h"
#include "security/chacha20.h"
#include "security/chacha20poly1305.h"
#include "security/crypto.h"
#include "security/hkdf.h"
#include "security/hmac.h"
#include "security/keychain.h"
#include "security/otpauth.h"
#include "security/password.h"
#include "security/poly1305.h"
#include "security/secure_random.h"
#include "security/sha1.h"
#include "security/totp.h"
#include "security/touchid.h"

// --- spoke -------------------------------------------------------------------

#include "spoke/bespoke.h"

// --- system ------------------------------------------------------------------

#include "system/system.h"

// --- thread ------------------------------------------------------------------

#include "thread/compute.h"
#include "thread/draw.h"
#include "thread/event.h"
#include "thread/networking.h"
#include "thread/reactive.h"
#include "thread/scripting.h"
#include "thread/thread.h"
#include "thread/ui.h"

// --- time --------------------------------------------------------------------

#include "time/calendar.h"
#include "time/clock.h"
#include "time/datetime.h"
#include "time/nanotime.h"

#endif // VEXSPOKE_H
