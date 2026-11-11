# vexspoke — Repo-Local Living Preferences

## Constitution Link

gist.md (`../../metadata/gists/preferences/gist.md`) is the compressed law list; all universal laws bind this repository. Read one on demand: `./tools/prefs law "<title>"`. Universal laws are inherited, never restated.

Tier: core (R2 CPU computation, behavior, network protocols). Owns: pure format algorithms, coordinate-agnostic vectors, reactive generic engine, `net/` protocol clients/servers, higher-level io policy (`io/`, VexHome layout). Borrows: Relational Engine filesystem IO/NIO, native Memory/MemoryArena/Transient allocator, relational symbol table.

## Exclusive Laws

| Law Title | Scope | Enforcement |
| **Coordinate-Agnostic Vector Law** | R2 Relational memory Substrate | Mandatory for `vexspoke` |
| **Self-Describing memory Block Law** | R2 Relational memory Substrate | Mandatory for `vexspoke` |
| **BitPool Slot Segregation Law** | R2 Relational memory Substrate | Mandatory for `vexspoke` |
| **24-Byte Variable Slot Law** | R2 Relational memory Substrate | Mandatory for `vexspoke` |
| **Reactive Generics Law** | R2 behavior (reactive engine + generic family) | Mandatory for `vexspoke` |
| **Pure Format Algorithm Law** | R2 CPU computation (`net/`, `search/`) | Mandatory for `vexspoke` |

### Coordinate-Agnostic Vector Law

* Spatial vectors (`Vec2`, `Vec3`, `Vec4`) are never hardcoded to a Cartesian convention; anonymous unions carry semantic spatial terms — `horizontal` (Axis 0), `vertical` (Axis 1), `depth` (Axis 2), plus `w`.
* Directional getters/setters (`Vec4_getRight`, `Vec4_getUp`, `Vec4_getFront`, `Vec4_getLeft`, `Vec4_getDown`, `Vec4_getBack`) access by spatial meaning.
* `xyzw`/`rgba` are aliases over the 16-byte SIMD layout; `data[N]` for SIMD/stride access.
* Orientation transfers use `CoordFrame` basis queries (`Vec4_getXInFrame`, etc.), never ad-hoc negative signs or manual component swizzling.

### Self-Describing memory Block Law

* Every block obtained through the engine-owned `Memory_*` ABI carries the 32-byte header (`typeId`, `hash`, `length`, `pointer`) before its payload pointer.
* The header validates two ways and fails closed on either; provenance beyond that is the arena-range gate, never a magic cookie.
* The `pointer` word is the object's own address, or the value's address when the id's form bits say the type is a primitive.
* A block is never relocated after allocation; any future arena compaction must re-stamp every moved header.
* Never store redundant length or class IDs in secondary maps — the pointer carries its metadata.

### BitPool Slot Segregation Law

* Equal-stride data structures (primitives, relational rows, ring buffer cells, tokens) carve their backing once from the engine arena (`MemoryArena_alloc`) and vend slots from the engine-owned `BitPool`.
* O(1) index claim/return with a per-slot generation; no per-object allocator call.
* Objects of identical size belong in a dedicated `BitPool`.
* Slots are handed out by index; each slot carries a 32-bit generation that bumps on release, so a stale pointer is refused instead of aliased.
* Freelist link words are indices with an explicit end sentinel, never truncated pointers.
* The slot header is one 32-byte floor unit and the stride rounds to the same floor; every pool payload is 32-byte aligned exactly like a `Memory_alloc` payload.
* Growth is automatic: a full pool appends one arena-backed chunk; live payloads never move.

### 24-Byte Variable Slot Law

* Every interned variable name is strictly bounded to 23 ASCII characters plus a 1-byte NUL terminator (24 bytes total).
* Paired with an 8-byte intrusive self-pointer (`u64 self`) as the cache-aligned, power-of-two 32-byte slot record.
* Two slots pack byte-perfect into a single 64-byte CPU cache line.
* Name resolution yields permanent slot indices (`i32`) that remain valid across table rehashes.

### Reactive Generics Law

* `Reactive` is a single type-agnostic engine, exposed through `Reactive(T)` with a leaf-type argument.
* Typed classes are stamped, not hand-written: `TYPEDEF_REACTIVE(NAME)` / `IMPLEMENT_REACTIVE(NAME)` stamp any object in two lines.
* `Reactive(Probable(T))` does not exist; the merged `ReactiveProbable(T)` is the composite.
* The four channels `onSet`, `onChanged`, `onGet` (one-fire read) and `onNullptr` (empty slot) carry typed `fn(T value)` callbacks.
* `_Generic` resolves `Reactive_set`/`Reactive_get`/`Reactive_addOnChanged`/… to the per-class arm by reactive type; a bare `Reactive*` (or `nullptr`) routes to the raw engine channel.
* A metric with no observers must not fire on a hot path — a bare `Reactive_get` stays one atomic load.
* `Reactive_set` never fires observers; the owner calls `Reactive_drain`, coalescing pending writes into one `onSet`/`onChanged` batch on the owner's thread.

### Pure Format Algorithm Law

* Pure format algorithms are byte-to-byte code with no file, path, handle, device or persistent state; each takes a borrowed byte span and returns a value.
* A format algorithm takes `(const u8 *bytes, bytes length)` and returns a decoded value; it never opens a file or path.
* Decoded buffers are allocated through the engine's `Memory_*` ABI; the engine owns the storage, and the caller frees its stable pointer with `Memory_free`.
* Unsupported or hostile input returns a named code with one cold `THROW`; a corrupted stream never yields plausible output; geometry is bounded before allocation.
* Checksums/CRCs are validated; an inflate that does not match the declared geometry is a failure, not a padding opportunity.
* New formats do not pull an SDK into R2; a system library for the primitive codec (zlib for DEFLATE) is the ceiling, declared at the owning target.

## Repo-Local Extensions

* None — no managed exceptions to universal laws.

## Readiness Cross-Reference

* Feature readiness: vexspoke readiness gist.
* Open blockers: ecosystem blockers gist.
* Proof lawbook: `tests/test-preferences.md` — `./tools/prefs test "<title>"`.
* Evidence ledger: `metadata/gists/test-checklist.md` — `python3 tools/test_checklist.py check`.
