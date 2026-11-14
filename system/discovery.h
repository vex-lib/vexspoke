#ifndef SYSTEM_DISCOVERY_H
#define SYSTEM_DISCOVERY_H

#include "vex.h"

// system/discovery.h — native hardware, display, and GPU probe (Legacy: system/SystemDiscovery.java)

fn SystemDiscovery_bootstrap();
fn SystemDiscovery_refresh();
bool SystemDiscovery_isBootstrapped();

#endif
