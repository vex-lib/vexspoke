#ifndef SYSTEM_IMAGE_MAC_H
#define SYSTEM_IMAGE_MAC_H

#include "vex.h"

// Decodes a PNG or JPG from memory into a raw RGBA8 pixel buffer using macOS CoreGraphics.
// Returns a malloc'd byte array that the caller must free().
// Outputs the width and height into outW and outH.
any ImageMac_decode(const_any fileData, bytes fileSize, bytes *outW, bytes *outH);

#endif // SYSTEM_IMAGE_MAC_H
