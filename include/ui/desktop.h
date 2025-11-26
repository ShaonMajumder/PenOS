#ifndef UI_DESKTOP_H
#define UI_DESKTOP_H

#include <stdint.h>

void desktop_start(void);
void desktop_handle_mouse(int dx, int dy, uint8_t buttons);

#endif
