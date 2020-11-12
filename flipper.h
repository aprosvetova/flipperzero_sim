#pragma once

#include <stdbool.h>
#include <stdint.h>

#define FL_LCD_WIDTH 128
#define FL_LCD_HEIGHT 64

#define FL_INIT_SIMULATOR_ROTATE 1

#define FL_GPIO_BUTTON_UP 0
#define FL_GPIO_BUTTON_LEFT 1
#define FL_GPIO_BUTTON_DOWN 2
#define FL_GPIO_BUTTON_RIGHT 3
#define FL_GPIO_BUTTON_ENTER 4
#define FL_GPIO_BUTTON_BACK 5

#define FL_GPIO_SIMULATOR_EXIT 63  // must be highest value
#define FL_GPIO_COUNT (FL_GPIO_SIMULATOR_EXIT + 1)

bool flipper_init(int flags);
void flipper_close();

int flipper_get_tics();
int flipper_random(int range);

void flipper_gpio_update();
bool flipper_gpio_get(int pin);
void flipper_gpio_set(int pin);

int flipper_key_get_time(int pin);

void flipper_pixel_set(int x, int y);
void flipper_pixel_clear(int x, int y);
bool flipper_pixel_get(int x, int y);

void flipper_pixel_reset();

void flipper_lcd_update();
void flipper_lcd_constant_fps();