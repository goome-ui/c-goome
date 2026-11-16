#include "internal.h"

#if defined(_WIN32) && !defined(GOOME_NO_UI)
int gm_axp_open(goome *g) { return -1; }
void gm_axp_event(goome *g, int ev, int i, int old) {}
int gm_axp_pump(goome *g) { return -1; }
void gm_axp_close(goome *g) {}
#endif
