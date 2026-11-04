#ifndef _HACLIENT_H
#define _HACLIENT_H

#include "M5NSconfig.h"

// Reads glucose and pump data from the Home Assistant Carelink integration
// (https://github.com/yo-han/Home-Assistant-Carelink) and fills the same
// NSinfo structure that readNightscout() fills. Returns 0 on success,
// otherwise an HTTP error code or 1001 (JSON error) / 1002 (no data).
int readHomeAssistant(struct NSinfo *ns);

#endif
