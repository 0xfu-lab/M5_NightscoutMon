#ifndef _UIMINIMED_H
#define _UIMINIMED_H

#include "M5NSconfig.h"

// Page 1 in the style of the M5 Minimed Monitor (320x240 display):
// pump/sensor icons and clock on top, big glucose value in a shield,
// active insulin and time in range in the bottom corners, data age below
// the shield. The area from y=220 is left free for the info line.
void drawPageMinimed(struct NSinfo *ns);

#endif
