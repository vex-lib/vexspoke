#ifndef OOP_TYPE_H
#define OOP_TYPE_H

#include "vex.h"
#include "re/type/re_type.h"

// oop/vexspoke_type.h — vexspoke's OWN class registry (the One Type Registry Law).
//
// The shared type ALGEBRA — the mask constants, SUGAR_VEX, FORM_*/PROJ_*/
// MOD_*/WRAP* constants, ARCH_*, Type_make/Type_class/Type_form/Type_project,
// the form/modifier/wrapper predicates, Type_arch, and the parent-chain
// resolver (Type_registerParents/Type_registerBareParents/Type_getParentClass/
// Type_isA) — is OWNED by Relational Engine in `type/type.h` and included
// above. The 32-byte block header lives with the allocator there too
// (MemoryHeader in nio/mem.h).
//
// This file keeps ONLY vexspoke's class numbers: the legacy 1-based ID_* list
// and the TYPE_* full-id macros built from it. Bare ID_* constants carry no
// project byte and name vexspoke's own class space only — cross-project
// dispatch ships full ids (PROJ_* | FORM_* | class). Other projects keep their
// own *-type.h and grant their parent chains through the RE algebra.

// Shared built-in scalar, geometry, and plain-container ID_* macros (0x0001..0x002E,
// ID_CHUNKED_LIST) live in `re/type/re_type.h`, included above. This file adds the
// vexspoke-only ID_* constants and wraps every TYPE_* below once.

#define ID_PACK         0x0017u
#define ID_HASH         0x0019u
#define ID_CLASS        0x001Au
#define ID_PROBABLE          0x0025u
#define ID_PROBABLE_OBJECTS  0x0026u
#define ID_FILE         0x0030u
#define ID_BOOL         0x0031u
#define ID_BRAIN        0x0032u
#define ID_FIXED32      0x0033u
#define ID_WS_CLIENT    0x0034u
#define ID_PROCESS_SPAWN 0x0035u
#define SPOKE_ID_STRING_POOL  0x0036u
#define ID_CIRCLE_ARRAY 0x0037u
#define ID_SPHERE_ARRAY 0x0038u
#define ID_OCTREE       0x0039u
#define ID_CRYPTO       0x003Au
#define ID_SECURE_RANDOM 0x003Bu
#define ID_ARGON2       0x003Cu
#define ID_HKDF         0x003Du
#define ID_COMMAND      0x0064u

// --- OBJECT WRAPPERS & MODIFIERS (Legacy 0x006F..0x0075) ---
#define ID_CHOICE     0x006Fu
#define ID_FUTURE     0x0070u
#define ID_PASSIVE    0x0071u
#define ID_REACTIVE   0x0072u
#define ID_TRANSIENT  0x0073u
#define ID_LOCAL      0x0074u
#define ID_GLOBAL     0x0075u

// --- BUFFER FAMILY (Legacy 0x4A / 0x50..0x63) ---
#define ID_BUFFER                  0x004Au
#define ID_ACCUMULUATION_BUFFER    0x0050u
#define ID_AMBIENT_BUFFER          0x0051u
#define ID_COLOR_BUFFER            0x0052u
#define ID_DEFAULT_PIXEL_BUFFER    0x0053u
#define ID_DEPTH_BUFFER            0x0054u
#define ID_FILTER_BUFFER           0x0055u
#define ID_FRAME_BUFFER            0x0056u
#define ID_HEIGHT_BUFFER           0x0057u
#define ID_LIGHT_BUFFER            0x0058u
#define ID_MATERIAL_RESOLVE        0x0059u
#define ID_MOTION_VECTOR_BUFFER    0x005Au
#define ID_NORMAL_BUFFER           0x005Bu
#define ID_PHYSICAL_BUFFER         0x005Cu
#define ID_POST_PROCESSING_BUFFER  0x005Du
#define ID_REFLECTIVITY_BUFFER     0x005Eu
#define ID_SHADOW_BUFFER           0x005Fu
#define ID_SPECULAR_BUFFER         0x0060u
#define ID_STENCIL_BUFFER          0x0061u
#define ID_TRANSPARENCY_BUFFER     0x0062u
#define ID_VISIBILITY_BUFFER       0x0063u

// --- SYSTEM CLASSES ---
#define ID_DISPLAY_MONITOR              0x0066u
#define TYPE_DISPLAY_MONITOR_SINGLETON  (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | ID_DISPLAY_MONITOR)

// --- THREAD CLASSES (the real OS threads; sync primitives live in atomic/) ---
#define ID_THREAD             0x0080u
#define ID_THREAD_NETWORKING  0x0081u
#define ID_THREAD_EVENT       0x0082u
#define ID_THREAD_DRAW        0x0083u
#define ID_THREAD_SCRIPTING   0x0084u
#define ID_THREAD_UI          0x0085u
// The thread block resumed past the later families: 0x0086..0x00A7 are taken
// (audio, input, reflection, the reactive scalars), so the two new roles append
// here rather than reuse a live id.
#define ID_THREAD_REACTIVE    0x00A8u
#define ID_THREAD_COMPUTE     0x00A9u

// --- SEARCH / PATTERN CLASSES ---
// The compiled regular expression (regex/), appended past the thread block.
#define ID_REGEX              0x00AAu

#define TYPE_THREAD_SINGLETON             (SUGAR_VEX | PROJ_HOTCWAP | FORM_SINGLETON | ID_THREAD)
#define TYPE_THREAD_NETWORKING_SINGLETON  (SUGAR_VEX | PROJ_HOTCWAP | FORM_SINGLETON | ID_THREAD_NETWORKING)
#define TYPE_THREAD_EVENT_SINGLETON       (SUGAR_VEX | PROJ_HOTCWAP | FORM_SINGLETON | ID_THREAD_EVENT)
#define TYPE_THREAD_DRAW_SINGLETON        (SUGAR_VEX | PROJ_HOTCWAP | FORM_SINGLETON | ID_THREAD_DRAW)
#define TYPE_THREAD_SCRIPTING_SINGLETON   (SUGAR_VEX | PROJ_HOTCWAP | FORM_SINGLETON | ID_THREAD_SCRIPTING)
#define TYPE_THREAD_UI_SINGLETON          (SUGAR_VEX | PROJ_HOTCWAP | FORM_SINGLETON | ID_THREAD_UI)
#define TYPE_THREAD_REACTIVE_SINGLETON    (SUGAR_VEX | PROJ_HOTCWAP | FORM_SINGLETON | ID_THREAD_REACTIVE)
#define TYPE_THREAD_COMPUTE_SINGLETON     (SUGAR_VEX | PROJ_HOTCWAP | FORM_SINGLETON | ID_THREAD_COMPUTE)

// --- AUDIO CLASSES (native playback seam) ---
#define ID_AUDIO_CLIP   0x0086u
#define ID_AUDIO_VOICE  0x0087u
#define ID_AUDIO        0x0088u

#define TYPE_AUDIO_CLIP_SINGLETON   (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | ID_AUDIO_CLIP)
#define TYPE_AUDIO_VOICE_SINGLETON  (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | ID_AUDIO_VOICE)
#define TYPE_AUDIO_SINGLETON        (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | ID_AUDIO)

// --- INPUT CLASSES (binding registry) ---
#define ID_KEY_MAP      0x0089u

#define TYPE_KEY_MAP_SINGLETON      (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | ID_KEY_MAP)
#define TYPE_KEY_MAP_ARRAY          (SUGAR_VEX | PROJ_VEXSPOKE | FORM_ARRAY     | ID_KEY_MAP)

// --- CONTAINER / EXCEPTION / REFLECTION CLASSES ---
#define ID_EXCEPTION    0x008Bu

#define TYPE_EXCEPTION_SINGLETON    (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | ID_EXCEPTION)

// --- REFLECTION CLASSES (the 32-byte name box atom) ---
#define SPOKE_ID_VARIABLE_SLOT 0x008Cu
#define SPOKE_ID_VARIABLE_HASH_MAP 0x008Du
#define ID_SEGMENT_INDEX 0x0090u
#define ID_SEGMENT_ENTRY 0x0091u
#define ID_SEGMENT_POSTING 0x0092u
#define SPOKE_ID_VARIABLE_MINI_MAP 0x0093u
#define ID_REFLECT_CLASS 0x0094u
#define ID_REFLECT_FIELD 0x0095u
#define ID_REFLECT_VARIABLE 0x0096u
#define ID_REFLECT_METHOD 0x0097u
#define ID_REFLECT_STRUCT 0x0099u
#define ID_REACTIVE_BOOL 0x009Au
#define ID_REACTIVE_CHAR 0x009Bu
#define ID_REACTIVE_SHORT 0x009Cu
#define ID_REACTIVE_INT 0x009Du
#define ID_REACTIVE_LONG 0x009Eu
#define ID_REACTIVE_FLOAT 0x009Fu
#define ID_REACTIVE_f64 0x00A0u
#define ID_REACTIVE_STRING 0x00A1u
#define ID_REACTIVE_IF64 0x00A2u
#define ID_REACTIVE_LD128 0x00A3u
#define ID_REACTIVE_BYTE 0x00A6u
#define ID_REACTIVE_PROBABLE 0x00A7u

#define SPOKE_TYPE_VARIABLE_SLOT          (SUGAR_VEX | PROJ_VEXSPOKE | FORM_STRUCT_SINGLETON | SPOKE_ID_VARIABLE_SLOT)
#define SPOKE_TYPE_VARIABLE_HASH_MAP      (SUGAR_VEX | PROJ_VEXSPOKE | FORM_STRUCT_SINGLETON | SPOKE_ID_VARIABLE_HASH_MAP)
#define TYPE_SEGMENT_INDEX          (SUGAR_VEX | PROJ_VEXSPOKE | FORM_STRUCT_SINGLETON | ID_SEGMENT_INDEX)
#define SPOKE_TYPE_VARIABLE_MINI_MAP      (SUGAR_VEX | PROJ_VEXSPOKE | FORM_STRUCT_SINGLETON | SPOKE_ID_VARIABLE_MINI_MAP)
#define TYPE_REFLECT_CLASS          (SUGAR_VEX | PROJ_VEXSPOKE | FORM_STRUCT_SINGLETON | ID_REFLECT_CLASS)
#define TYPE_REFLECT_FIELD          (SUGAR_VEX | PROJ_VEXSPOKE | FORM_STRUCT_SINGLETON | ID_REFLECT_FIELD)
#define TYPE_REFLECT_VARIABLE       (SUGAR_VEX | PROJ_VEXSPOKE | FORM_STRUCT_SINGLETON | ID_REFLECT_VARIABLE)
#define TYPE_REFLECT_METHOD         (SUGAR_VEX | PROJ_VEXSPOKE | FORM_STRUCT_SINGLETON | ID_REFLECT_METHOD)
#define TYPE_REFLECT_STRUCT         (SUGAR_VEX | PROJ_VEXSPOKE | FORM_STRUCT_SINGLETON | ID_REFLECT_STRUCT)
#define TYPE_REACTIVE_BOOL          (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | WRAP_REACTIVE | ID_REACTIVE_BOOL)
#define TYPE_REACTIVE_CHAR          (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | WRAP_REACTIVE | ID_REACTIVE_CHAR)
#define TYPE_REACTIVE_SHORT         (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | WRAP_REACTIVE | ID_REACTIVE_SHORT)
#define TYPE_REACTIVE_INT           (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | WRAP_REACTIVE | ID_REACTIVE_INT)
#define TYPE_REACTIVE_LONG          (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | WRAP_REACTIVE | ID_REACTIVE_LONG)
#define TYPE_REACTIVE_FLOAT         (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | WRAP_REACTIVE | ID_REACTIVE_FLOAT)
#define TYPE_REACTIVE_f64        (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | WRAP_REACTIVE | ID_REACTIVE_f64)
#define TYPE_REACTIVE_STRING        (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | WRAP_REACTIVE | ID_REACTIVE_STRING)
#define TYPE_REACTIVE_IF64          (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | WRAP_REACTIVE | ID_REACTIVE_IF64)
#define TYPE_REACTIVE_LD128         (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | WRAP_REACTIVE | ID_REACTIVE_LD128)
#define TYPE_REACTIVE_BYTE          (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | WRAP_REACTIVE | ID_REACTIVE_BYTE)
#define TYPE_REACTIVE_PROBABLE      (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | WRAP_REACTIVE | WRAP2_PROBABLE | ID_REACTIVE_PROBABLE)

// --- DOWNSTREAM CLASS SPACE (owned per project, NOT listed here) ---
// Each project numbers its classes from 1 in its own *-type.h: darkbase-engine
// (engine/oop/darkbase_type.h) and samplerate (oop/samplerate_type.h) ship real
// registries today; darling's src/c23/darling-type.h is still an empty draft, and
// graphvex/hotcwap ship no registry file yet. This file keeps vexspoke-owned
// IDs only, in the
// legacy 1-based list that predates the project byte (vexspoke primitives
// pass bare ids to the allocator's BitPool keyed by these numbers). Central
// logic must never include downstream ID files (upstream builds standalone) —
// parent chains resolve through the RE Type_registerParents seam, keyed by the
// project byte, never by class ranges. Thread constants stay here (vexspoke's
// thread.c may not include hotcwap) but carry PROJ_HOTCWAP in their TYPE_* macros.

#define SPOKE_TYPE_STRING_POOL    (SUGAR_VEX | PROJ_VEXSPOKE | FORM_ARRAY     | SPOKE_ID_STRING_POOL)

#define TYPE_CIRCLE_ARRAY            (SUGAR_VEX | PROJ_VEXSPOKE | FORM_ARRAY     | ID_CIRCLE_ARRAY)
#define TYPE_SPHERE_ARRAY            (SUGAR_VEX | PROJ_VEXSPOKE | FORM_ARRAY     | ID_SPHERE_ARRAY)
#define TYPE_OCTREE                  (SUGAR_VEX | PROJ_VEXSPOKE | FORM_STRUCT_SINGLETON | ID_OCTREE)
#define TYPE_CRYPTO                  (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | ID_CRYPTO)
#define TYPE_SECURE_RANDOM_SINGLETON (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | ID_SECURE_RANDOM)
#define TYPE_ARGON2                  (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | ID_ARGON2)
#define TYPE_HKDF                    (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | ID_HKDF)
#define TYPE_MIN_HEAP                (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | ID_MIN_HEAP)
#define TYPE_RANDOM                  (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | ID_RANDOM)
#define TYPE_REGEX                   (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | ID_REGEX)
#define TYPE_PROBABLE                (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | WRAP2_PROBABLE | ID_PROBABLE)
#define TYPE_PROBABLE_ARRAY          (SUGAR_VEX | PROJ_VEXSPOKE | FORM_ARRAY     | WRAP2_PROBABLE | ID_PROBABLE)
#define TYPE_PROBABLE_OBJECTS_ARRAY  (SUGAR_VEX | PROJ_VEXSPOKE | FORM_ARRAY | WRAP2_PROBABLE_OBJECTS | ID_PROBABLE_OBJECTS)
#define TYPE_CHOICE                  (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | WRAP2_CHOICE | ID_CHOICE)
#define TYPE_CHOICE_ARRAY            (SUGAR_VEX | PROJ_VEXSPOKE | FORM_ARRAY | WRAP2_CHOICE | ID_CHOICE)
#define TYPE_FUTURE                  (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | WRAP2_FUTURE | ID_FUTURE)
#define TYPE_FUTURE_ARRAY            (SUGAR_VEX | PROJ_VEXSPOKE | FORM_ARRAY | WRAP2_FUTURE | ID_FUTURE)
#define TYPE_GLOBAL                  (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | MOD_GLOBAL | ID_GLOBAL)
#define TYPE_GLOBAL_ARRAY            (SUGAR_VEX | PROJ_VEXSPOKE | FORM_ARRAY | MOD_GLOBAL | ID_GLOBAL)
#define TYPE_LOCAL                   (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | MOD_LOCALE | ID_LOCAL)
#define TYPE_LOCAL_ARRAY             (SUGAR_VEX | PROJ_VEXSPOKE | FORM_ARRAY | MOD_LOCALE | ID_LOCAL)
#define TYPE_PASSIVE                 (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | WRAP_PROACTIVE | ID_PASSIVE)
#define TYPE_PASSIVE_ARRAY           (SUGAR_VEX | PROJ_VEXSPOKE | FORM_ARRAY | WRAP_PROACTIVE | ID_PASSIVE)
#define TYPE_REACTIVE                (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | WRAP_REACTIVE | ID_REACTIVE)
#define TYPE_REACTIVE_ARRAY          (SUGAR_VEX | PROJ_VEXSPOKE | FORM_ARRAY | WRAP_REACTIVE | ID_REACTIVE)
#define TYPE_PROBABLE_OBJECTS        (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | WRAP2_PROBABLE_OBJECTS | ID_PROBABLE_OBJECTS)
#define SPOKE_TYPE_FILE_SINGLETON          (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | ID_FILE)
#define TYPE_WS_CLIENT_SINGLETON     (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | ID_WS_CLIENT)
#define TYPE_PROCESS_SPAWN_SINGLETON (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | ID_PROCESS_SPAWN)
#define TYPE_COMMAND_SINGLETON       (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | ID_COMMAND)

#endif
