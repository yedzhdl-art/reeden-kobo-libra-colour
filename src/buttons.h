#pragma once
#include <stdbool.h>

/* direction: -1 = previous page, +1 = next page */
typedef void (*ButtonTurnFn)(int direction);

typedef struct {
    int fd;
    char path[64];
    unsigned short prev[24];
    unsigned short next[24];
    unsigned nprev, nnext;
    bool debug;
} Buttons;

/* Returns 0 on success, -1 when disabled or no suitable device exists.
   REEDEN_BUTTONS=0/off disables; a /dev/input/eventN value forces a device. */
int buttons_open(Buttons *b);
/* Attach to an already-open descriptor (the host reads keys/power/gyro from a
   single node). The descriptor is not owned by this module. */
int buttons_attach(Buttons *b, int fd, const char *path);
/* Drain the button device. */
void buttons_process(Buttons *b, ButtonTurnFn fn);
/* Feed one evdev event. */
void buttons_feed(Buttons *b, int type, int code, int value, ButtonTurnFn fn);
void buttons_close(Buttons *b);
/* Diagnostic: list every input device and its relevant capability bits. */
int buttons_probe(void);
