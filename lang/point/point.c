#include "vex.h"
#include "lang/point/point.h"
#include "math/strict_math.h"
#include "nio/mem.h"
#include "oop/vexspoke_type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Point
 * ============================================================================
 * Integer-precision 2D position in native hardware pixel space (the Pixel
 * Coordinate Contract: layout, hit-testing, and the graphics API all speak
 * native pixels; NDC exists only inside vertex shaders). The struct is two
 * int32 fields, arena-allocated via Memory_alloc with the ID_POINT type tag,
 * and all arithmetic is dest-last with int64 widening so distance math never
 * overflows. Y is down, matching screen/backing-store orientation.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Point (lang/point/point.c — defined in lang/point/point.h)
 * ============================================================================
 * Integer-precision 2D position in native pixel space (the Pixel Coordinate
 * Contract: the Graphics API, layout buffers and hit-testing all speak
 * native hardware pixels; -1..1 NDC exists only inside vertex shaders).
 *
 * STRUCT FIELDS (Mirroring lang/point/point.h):
 * ----------------------------------------------------------------------------
 *   i32 x;       // horizontal pixel position (native pixels)
 *   i32 y;       // vertical pixel position (native pixels, Y-down)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Point()          : constructor chooser for the zero-argument form
 *   - Point(x, y)      : constructor chooser for the two-argument form
 *
 * Setters:
 *   - Point_setX(p, x)
 *   - Point_setY(p, y)
 *   - Point_set(p, x, y)
 *
 * Getters:
 *   - Point_getX(p)                    : i32
 *   - Point_getY(p)                    : i32
 *
 * Core Functions:
 *   - Point_copy(src, dest)
 *   - Point_add(a, b.sh, dest)
 *   - Point_sub(a, b.sh, dest)
 *   - Point_scale(a, scalar, dest)
 *   - Point_negate(src, dest)
 *   - Point_min(a, b.sh, dest)
 *   - Point_max(a, b.sh, dest)
 *   - Point_distanceSquared(a, b.sh)      : i64
 *   - Point_manhattanDistance(a, b.sh)    : i64
 *   - Point_distance(a, b.sh)             : d64
 *   - Point_equals(a, b.sh)               : bool
 * ============================================================================
 */


;;GETTER
static inline i32 Point_x(const Point *s) { return (*s).x; }
;;GETTER
static inline i32 *Point_xMut(Point *s) { return &(*s).x; }
;;SETTER
static inline fn Point_setX(Point *s, i32 v) { (*s).x = v; }
;;GETTER
static inline i32 Point_y(const Point *s) { return (*s).y; }
;;GETTER
static inline i32 *Point_yMut(Point *s) { return &(*s).y; }
;;SETTER
static inline fn Point_setY(Point *s, i32 v) { (*s).y = v; }

// CONSTRUCTORS
Point *Point_0() {
    Point *p = (Point*) Memory_alloc(ID_POINT, POINT_BYTES);
    if (!p) return nullptr;
    Point_setX(p, 0);
    Point_setY(p, 0);
    return p;
}

Point *Point_2(i32 x, i32 y) {
    Point *p = (Point*) Memory_alloc(ID_POINT, POINT_BYTES);
    if (!p) return nullptr;
    Point_setX(p, x);
    Point_setY(p, y);
    return p;
}

fn Point_free(Point *p) {
    if (p) Memory_free(p);
}

// SETTERS
fn Point_setX(Point *p, i32 x) { if (p) (*p).x = x; }
fn Point_setY(Point *p, i32 y) { if (p) (*p).y = y; }
fn Point_set(Point *p, i32 x, i32 y) {
    if (!p) return;
    Point_setX(p, x);
    Point_setY(p, y);
}

// GETTERS
i32 Point_getX(const Point *p) { return p ? Point_x(p) : 0; }
i32 Point_getY(const Point *p) { return p ? Point_y(p) : 0; }

// CORE FUNCTIONS
fn Point_copy(const Point *src, Point *dest) {
    if (!src || !dest) return;
    Point_setX(dest, Point_x(src));
    Point_setY(dest, Point_y(src));
}

fn Point_add(const Point *a, const Point *b, Point *dest) {
    if (!a || !b || !dest) return;
    Point_setX(dest, Point_x(a) + Point_x(b));
    Point_setY(dest, Point_y(a) + Point_y(b));
}

fn Point_sub(const Point *a, const Point *b, Point *dest) {
    if (!a || !b || !dest) return;
    Point_setX(dest, Point_x(a) - Point_x(b));
    Point_setY(dest, Point_y(a) - Point_y(b));
}

fn Point_scale(const Point *a, i32 scalar, Point *dest) {
    if (!a || !dest) return;
    Point_setX(dest, Point_x(a) * scalar);
    Point_setY(dest, Point_y(a) * scalar);
}

fn Point_negate(const Point *src, Point *dest) {
    if (!src || !dest) return;
    Point_setX(dest, -Point_x(src));
    Point_setY(dest, -Point_y(src));
}

fn Point_min(const Point *a, const Point *b, Point *dest) {
    if (!a || !b || !dest) return;
    Point_setX(dest, (Point_x(a) < Point_x(b)) ? Point_x(a) : Point_x(b));
    Point_setY(dest, (Point_y(a) < Point_y(b)) ? Point_y(a) : Point_y(b));
}

fn Point_max(const Point *a, const Point *b, Point *dest) {
    if (!a || !b || !dest) return;
    Point_setX(dest, (Point_x(a) > Point_x(b)) ? Point_x(a) : Point_x(b));
    Point_setY(dest, (Point_y(a) > Point_y(b)) ? Point_y(a) : Point_y(b));
}

i64 Point_distanceSquared(const Point *a, const Point *b) {
    if (!a || !b) return 0;
    i64 dx = (i64) Point_x(a) - (i64) Point_x(b);
    i64 dy = (i64) Point_y(a) - (i64) Point_y(b);
    return dx * dx + dy * dy;
}

i64 Point_manhattanDistance(const Point *a, const Point *b) {
    if (!a || !b) return 0;
    i64 dx = (i64) Point_x(a) - (i64) Point_x(b);
    i64 dy = (i64) Point_y(a) - (i64) Point_y(b);
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return dx + dy;
}

d64 Point_distance(const Point *a, const Point *b) {
    i64 sq = Point_distanceSquared(a, b);
    if (sq == 0) return 0.0;
    return StrictMath_sqrtD((d64) sq);
}

bool Point_equals(const Point *a, const Point *b) {
    if (!a || !b) return false;
    return Point_x(a) == Point_x(b) && Point_y(a) == Point_y(b);
}
