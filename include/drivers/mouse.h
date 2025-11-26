#ifndef DRIVERS_MOUSE_H
#define DRIVERS_MOUSE_H
#include <stdint.h>

typedef struct {
    int32_t x;
    int32_t y;
    uint8_t buttons; /* bit0=left, bit1=right, bit2=middle */
} mouse_state_t;

typedef void (*mouse_handler_t)(int dx, int dy, uint8_t buttons);

void mouse_init(void);
void mouse_get_state(mouse_state_t *out);
void mouse_set_handler(mouse_handler_t handler);

#endif
