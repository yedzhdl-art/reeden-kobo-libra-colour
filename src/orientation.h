#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Portrait auto-rotation state.
   Kobo's kernel accelerometer driver reports orientation as a synthetic
   EV_MSC/MSC_RAW value on the main NTX input node (0x17 = portrait down,
   0x18 = portrait up, 0x19/0x1a = landscape). Raw ABS_X/Y/Z from a dedicated
   motion node is kept only as a fallback. This module NEVER writes to the
   kernel framebuffer rotation: the flip is applied in software. */
typedef struct {
    int x,y,z;              /* last committed accelerometer sample (fallback) */
    int px,py,pz;           /* sample being assembled */
    bool have,pending;
    int baseline_sign;      /* sign of Y while the screen is at rotation 0 */
    int stable_sign;
    uint64_t stable_since;
    uint64_t hold_ns;
    bool enabled,flip,debug;
    unsigned reports;
    /* EV_MSC/MSC_RAW path (primary) */
    bool use_msc;
    int msc_rotation;       /* 0 or 180 from MSC_RAW */
    uint64_t msc_changed_at;
    uint64_t msc_debounce_ns;
} Orientation;

void orientation_init(Orientation *o);
/* Feed one evdev event. Handles EV_MSC/MSC_RAW directly and EV_ABS + SYN_REPORT
   for the fallback accelerometer path. */
void orientation_feed(Orientation *o,int type,int code,int value);
/* Returns the desired rotation (0 or 180), or -1 to keep the current one. */
int orientation_tick(Orientation *o);
int orientation_probe(void);
