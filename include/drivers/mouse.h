#ifndef DRIVERS_MOUSE_H
#define DRIVERS_MOUSE_H
#include <stdint.h>

typedef struct {
    int32_t x;
    int32_t y;
    uint8_t buttons; /* bit0=left, bit1=right, bit2=middle */
} mouse_state_t;

void mouse_init(void);
void mouse_get_position(int *x, int *y);
int mouse_get_buttons(void);

#endif
