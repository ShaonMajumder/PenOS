#ifndef DRIVERS_KEYBOARD_H
#define DRIVERS_KEYBOARD_H
#include <stdint.h>

void keyboard_init(void);
int keyboard_read_char(void);
void keyboard_push_char(char c);

#endif
