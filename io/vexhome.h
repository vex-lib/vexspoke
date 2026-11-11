#ifndef IO_VEXHOME_H
#define IO_VEXHOME_H

#include "vex.h"

// io/vexhome.h — per-user vexgraph layout policy above re File.
//
// VexHome resolves the host's user-data root and derives the
// projects/logs/fonts/placeholder/cache paths. It creates directories through
// re/io/file; it owns no storage primitive of its own.

const char *VexHome_root();
const char *VexHome_projects();
const char *VexHome_logs();
const char *VexHome_fonts();
const char *VexHome_placeholder();
const char *VexHome_cache(const char *subsystem);
const char *VexHome_cacheIndex(const char *subsystem);
bool VexHome_cacheEnsure(const char *subsystem);
bool VexHome_ensure();
const char *VexHome_defaultLogPath();

#endif
