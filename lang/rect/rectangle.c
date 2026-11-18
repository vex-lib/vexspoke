#include "vex.h"
#include "lang/rect/rectangle.h"
#include "nio/mem.h"
#include "oop/vexspoke_type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Rectangle
 * ============================================================================
 * Axis-aligned rectangle in native hardware pixel space, per the Pixel
 * Coordinate Contract: the graphics API, layout buffers, and hit-testing all
 * speak native pixels, and -1..1 NDC exists only inside vertex shaders. Four
 * f32s (x, y, width, height) with a Y-down top-left origin. Geometry
 * transforms (translate/inflate/inset) and set operations
 * (intersection/merge) are dest-last; predicates (isEmpty, containsPoint,
 * containsRect, intersects, equals) treat touching edges as contained.
 * Instances are memory-arena allocated with the ID_RECT type and freed via
 * Rectangle_free.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Rectangle (lang/rect/rectangle.c — defined in lang/rect/rectangle.h)
 * ============================================================================
 * Axis-aligned rectangle in native pixel space (the Pixel Coordinate
 * Contract: the Graphics API, layout buffers and hit-testing all speak
 * native hardware pixels; -1..1 NDC exists only inside vertex shaders).
 *
 * STRUCT FIELDS (Mirroring lang/rect/rectangle.h):
 * ----------------------------------------------------------------------------
 *   f32 x;       // top-left x, native pixels
 *   f32 y;       // top-left y, native pixels (Y-down)
 *   f32 width;   // extent along +X, native pixels
 *   f32 height;  // extent along +Y, native pixels
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Rectangle()                : constructor chooser for the zero-argument form
 *   - Rectangle(x, y, w, h)      : constructor chooser for the four-argument form
 *
 * Setters:
 *   - Rectangle_setX(r, x)
 *   - Rectangle_setY(r, y)
 *   - Rectangle_setWidth(r, width)
 *   - Rectangle_setHeight(r, height)
 *   - Rectangle_set(r, x, y, w, h)
 *
 * Getters:
 *   - Rectangle_getX(r)          : f32
 *   - Rectangle_getY(r)          : f32
 *   - Rectangle_getWidth(r)      : f32
 *   - Rectangle_getHeight(r)     : f32
 *   - Rectangle_getLeft(r)       : f32 (= x)
 *   - Rectangle_getTop(r)        : f32 (= y)
 *   - Rectangle_getRight(r)      : f32 (= x + width)
 *   - Rectangle_getBottom(r)     : f32 (= y + height)
 *
 * Core Functions:
 *   - Rectangle_copy(src, dest)
 *   - Rectangle_translate(src, dx, dy, dest)
 *   - Rectangle_inflate(src, dx, dy, dest)
 *   - Rectangle_inset(src, l, t, r, b.sh, dest)
 *   - Rectangle_intersection(a, b.sh, dest)   : disjoint -> empty (0,0,0,0)
 *   - Rectangle_merge(a, b.sh, dest)
 *   - Rectangle_isEmpty(r)                 : bool (w <= 0 || h <= 0)
 *   - Rectangle_containsPoint(r, px, py)   : bool (touching edges count)
 *   - Rectangle_containsRect(outer, inner) : bool (touching edges count)
 *   - Rectangle_intersects(a, b.sh)           : bool
 *   - Rectangle_equals(a, b.sh)               : bool
 * ============================================================================
 */


;;GETTER
static inline f32 Rectangle_height(const Rectangle *s) { return (*s).height; }
;;GETTER
static inline f32 Rectangle_width(const Rectangle *s) { return (*s).width; }
;;GETTER
static inline f32 Rectangle_x(const Rectangle *s) { return (*s).x; }
;;GETTER
static inline f32 Rectangle_y(const Rectangle *s) { return (*s).y; }

// CONSTRUCTORS
Rectangle *Rectangle_0() {
    Rectangle *r = (Rectangle*) Memory_alloc(ID_RECT, RECTANGLE_BYTES);
    if (!r) return nullptr;
    Rectangle_setX(r, 0.0f);
    Rectangle_setY(r, 0.0f);
    Rectangle_setWidth(r, 0.0f);
    Rectangle_setHeight(r, 0.0f);
    return r;
}

Rectangle *Rectangle_4(f32 x, f32 y, f32 width, f32 height) {
    Rectangle *r = (Rectangle*) Memory_alloc(ID_RECT, RECTANGLE_BYTES);
    if (!r) return nullptr;
    Rectangle_setX(r, x);
    Rectangle_setY(r, y);
    Rectangle_setWidth(r, width);
    Rectangle_setHeight(r, height);
    return r;
}

fn Rectangle_free(Rectangle *r) {
    if (r) Memory_free(r);
}

// SETTERS
;;SETTER
fn Rectangle_setX(Rectangle *r, f32 x) { if (r) (*r).x = x; }
;;SETTER
fn Rectangle_setY(Rectangle *r, f32 y) { if (r) (*r).y = y; }
;;SETTER
fn Rectangle_setWidth(Rectangle *r, f32 width) { if (r) (*r).width = width; }
;;SETTER
fn Rectangle_setHeight(Rectangle *r, f32 height) { if (r) (*r).height = height; }

fn Rectangle_set(Rectangle *r, f32 x, f32 y, f32 width, f32 height) {
    if (!r) return;
    Rectangle_setX(r, x);
    Rectangle_setY(r, y);
    Rectangle_setWidth(r, width);
    Rectangle_setHeight(r, height);
}

// GETTERS
f32 Rectangle_getX(const Rectangle *r) { return r ? Rectangle_x(r) : 0.0f; }
f32 Rectangle_getY(const Rectangle *r) { return r ? Rectangle_y(r) : 0.0f; }
f32 Rectangle_getWidth(const Rectangle *r) { return r ? Rectangle_width(r) : 0.0f; }
f32 Rectangle_getHeight(const Rectangle *r) { return r ? Rectangle_height(r) : 0.0f; }
f32 Rectangle_getLeft(const Rectangle *r) { return r ? Rectangle_x(r) : 0.0f; }
f32 Rectangle_getTop(const Rectangle *r) { return r ? Rectangle_y(r) : 0.0f; }
f32 Rectangle_getRight(const Rectangle *r) { return r ? Rectangle_x(r) + Rectangle_width(r) : 0.0f; }
f32 Rectangle_getBottom(const Rectangle *r) { return r ? Rectangle_y(r) + Rectangle_height(r) : 0.0f; }

// CORE FUNCTIONS
fn Rectangle_copy(const Rectangle *src, Rectangle *dest) {
    if (!src || !dest) return;
    Rectangle_setX(dest, Rectangle_x(src));
    Rectangle_setY(dest, Rectangle_y(src));
    Rectangle_setWidth(dest, Rectangle_width(src));
    Rectangle_setHeight(dest, Rectangle_height(src));
}

fn Rectangle_translate(const Rectangle *src, f32 dx, f32 dy, Rectangle *dest) {
    if (!src || !dest) return;
    Rectangle_setX(dest, Rectangle_x(src) + dx);
    Rectangle_setY(dest, Rectangle_y(src) + dy);
    Rectangle_setWidth(dest, Rectangle_width(src));
    Rectangle_setHeight(dest, Rectangle_height(src));
}

fn Rectangle_inflate(const Rectangle *src, f32 dx, f32 dy, Rectangle *dest) {
    if (!src || !dest) return;
    Rectangle_setX(dest, Rectangle_x(src) - dx);
    Rectangle_setY(dest, Rectangle_y(src) - dy);
    Rectangle_setWidth(dest, Rectangle_width(src) + dx + dx);
    Rectangle_setHeight(dest, Rectangle_height(src) + dy + dy);
}

fn Rectangle_inset(const Rectangle *src, f32 l, f32 t, f32 r, f32 b, Rectangle *dest) {
    if (!src || !dest) return;
    Rectangle_setX(dest, Rectangle_x(src) + l);
    Rectangle_setY(dest, Rectangle_y(src) + t);
    Rectangle_setWidth(dest, Rectangle_width(src) - l - r);
    Rectangle_setHeight(dest, Rectangle_height(src) - t - b);
    if (Rectangle_width(dest) < 0.0f) Rectangle_setWidth(dest, 0.0f);
    if (Rectangle_height(dest) < 0.0f) Rectangle_setHeight(dest, 0.0f);
}

fn Rectangle_intersection(const Rectangle *a, const Rectangle *b, Rectangle *dest) {
    if (!a || !b || !dest) return;
    f32 l = (Rectangle_x(a) > Rectangle_x(b)) ? Rectangle_x(a) : Rectangle_x(b);
    f32 t = (Rectangle_y(a) > Rectangle_y(b)) ? Rectangle_y(a) : Rectangle_y(b);
    f32 rA = Rectangle_x(a) + Rectangle_width(a);
    f32 rB = Rectangle_x(b) + Rectangle_width(b);
    f32 bA = Rectangle_y(a) + Rectangle_height(a);
    f32 bB = Rectangle_y(b) + Rectangle_height(b);
    f32 r = (rA < rB) ? rA : rB;
    f32 bot = (bA < bB) ? bA : bB;
    if (r <= l || bot <= t) {
        Rectangle_setX(dest, 0.0f);
        Rectangle_setY(dest, 0.0f);
        Rectangle_setWidth(dest, 0.0f);
        Rectangle_setHeight(dest, 0.0f);
        return;
    }
    Rectangle_setX(dest, l);
    Rectangle_setY(dest, t);
    Rectangle_setWidth(dest, r - l);
    Rectangle_setHeight(dest, bot - t);
}

fn Rectangle_merge(const Rectangle *a, const Rectangle *b, Rectangle *dest) {
    if (!a || !b || !dest) return;
    f32 l = (Rectangle_x(a) < Rectangle_x(b)) ? Rectangle_x(a) : Rectangle_x(b);
    f32 t = (Rectangle_y(a) < Rectangle_y(b)) ? Rectangle_y(a) : Rectangle_y(b);
    f32 rA = Rectangle_x(a) + Rectangle_width(a);
    f32 rB = Rectangle_x(b) + Rectangle_width(b);
    f32 bA = Rectangle_y(a) + Rectangle_height(a);
    f32 bB = Rectangle_y(b) + Rectangle_height(b);
    f32 r = (rA > rB) ? rA : rB;
    f32 bot = (bA > bB) ? bA : bB;
    Rectangle_setX(dest, l);
    Rectangle_setY(dest, t);
    Rectangle_setWidth(dest, r - l);
    Rectangle_setHeight(dest, bot - t);
}

bool Rectangle_isEmpty(const Rectangle *r) {
    if (!r) return true;
    return Rectangle_width(r) <= 0.0f || Rectangle_height(r) <= 0.0f;
}

bool Rectangle_containsPoint(const Rectangle *r, f32 px, f32 py) {
    if (!r) return false;
    if (px < Rectangle_x(r) || py < Rectangle_y(r)) return false;
    if (px > Rectangle_x(r) + Rectangle_width(r) || py > Rectangle_y(r) + Rectangle_height(r)) return false;
    return true;
}

bool Rectangle_containsRect(const Rectangle *outer, const Rectangle *inner) {
    if (!outer || !inner) return false;
    if (Rectangle_x(inner) < Rectangle_x(outer) || Rectangle_y(inner) < Rectangle_y(outer)) return false;
    if (Rectangle_x(inner) + Rectangle_width(inner) > Rectangle_x(outer) + Rectangle_width(outer)) return false;
    if (Rectangle_y(inner) + Rectangle_height(inner) > Rectangle_y(outer) + Rectangle_height(outer)) return false;
    return true;
}

bool Rectangle_intersects(const Rectangle *a, const Rectangle *b) {
    if (!a || !b) return false;
    if (Rectangle_x(a) + Rectangle_width(a) <= Rectangle_x(b)) return false;
    if (Rectangle_x(b) + Rectangle_width(b) <= Rectangle_x(a)) return false;
    if (Rectangle_y(a) + Rectangle_height(a) <= Rectangle_y(b)) return false;
    if (Rectangle_y(b) + Rectangle_height(b) <= Rectangle_y(a)) return false;
    return true;
}

bool Rectangle_equals(const Rectangle *a, const Rectangle *b) {
    if (!a || !b) return false;
    return Rectangle_x(a) == Rectangle_x(b) && Rectangle_y(a) == Rectangle_y(b) &&
           Rectangle_width(a) == Rectangle_width(b) && Rectangle_height(a) == Rectangle_height(b);
}
