#define SDL_MAIN_HANDLED

#include <SDL.h>
#include <SDL_image.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "flipper.h"

#define UI_BG_WIDTH 823
#define UI_BG_HEIGHT 365

#define UI_SCR_LEFT 238
#define UI_SCR_TOP 71

//#define TICK_INTERVAL 20   // 50 FPS -> 1000/50 = 20 ms
#define TICK_INTERVAL 40  // 25 FPS -> 1000/25 = 40 ms

#define HIGHLIGHT_TIME 100
#define NUM_HIGHLIGHT_BUTTONS 6

#define UI_NORMAL
//#define UI_GREEN

#ifdef UI_GREEN
#define UI_BG_PNG "img/ui_background2.png"
#define LCD_COLOR_FG 0xff060701  // black
#define LCD_COLOR_BG 0xff8edc4a  // flipper green
#endif

#ifdef UI_NORMAL
#define UI_BG_PNG "img/ui_background.png"
#define LCD_COLOR_FG 0xff363636  // grey
#define LCD_COLOR_BG 0xfffea652  // flipper orange
#endif

#define UI_HIGHTLIGHT_PNG "img/ui_highlight.png"

typedef struct HIGHLIGHT_BUTTON HIGHLIGHT_BUTTON;
struct HIGHLIGHT_BUTTON {
    int gpio;
    int x;
    int y;
};

HIGHLIGHT_BUTTON highlight_buttons[NUM_HIGHLIGHT_BUTTONS] = {
    {
        FL_GPIO_BUTTON_UP,
        574,
        44,
    },
    {
        FL_GPIO_BUTTON_LEFT,
        521,
        97,
    },
    {
        FL_GPIO_BUTTON_DOWN,
        574,
        148,
    },
    {
        FL_GPIO_BUTTON_RIGHT,
        626,
        97,
    },
    {
        FL_GPIO_BUTTON_ENTER,
        573,
        97,
    },
    {
        FL_GPIO_BUTTON_BACK,
        681,
        150,
    },
};

// keyboard to gpio in rotated mode
int map_rotated_button[6] = {
    FL_GPIO_BUTTON_LEFT,   // UP -> LEFT
    FL_GPIO_BUTTON_DOWN,   // LEFT -> DOWN
    FL_GPIO_BUTTON_RIGHT,  // DOWN -> RIGHT
    FL_GPIO_BUTTON_UP,     // RIGHT -> UP
    FL_GPIO_BUTTON_ENTER,  // ENTER -> ENTER
    FL_GPIO_BUTTON_BACK,   // BACK  -> BACK
};

////////////////////////////////////////////////////////////////

SDL_Window* window;
SDL_Renderer* renderer;
SDL_Texture* screen;

SDL_Texture* ui_highlight;
SDL_Texture* ui_background;
bool ui_rotate = false;

uint32_t* lcd_buffer;

uint8_t gpio_state[FL_GPIO_COUNT] = { 0 };
int32_t key_time[FL_GPIO_COUNT] = { 0 };

////////////////////////////////////////////////////////////////

bool flipper_init(int flags) {
    srand((unsigned int)time(0));

    memset(gpio_state, 0, sizeof(gpio_state));
    memset(key_time, 0, sizeof(key_time));

    int init = SDL_Init(SDL_INIT_EVERYTHING);
    if (init != 0) {
        printf("flipper_init: SDL_Init %s\n", IMG_GetError());
        return false;
    }

    int img_flags = IMG_INIT_PNG;
    if ((IMG_Init(img_flags) & img_flags) != img_flags) {
        printf("flipper_init: IMG_Init %s\n", IMG_GetError());
        return false;
    }

    int width = UI_BG_WIDTH;
    int height = UI_BG_HEIGHT;

    if (flags & FL_INIT_SIMULATOR_ROTATE) {
        ui_rotate = true;
        width = UI_BG_HEIGHT;
        height = UI_BG_WIDTH;
    }

    window = SDL_CreateWindow("Flipper Zero Simulator", SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED, width, height, 0);

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        printf("flipper_init: SDL_CreateRenderer\n");
        return false;
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    screen = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STATIC,
                               FL_LCD_WIDTH, FL_LCD_HEIGHT);
    if (!screen) {
        printf("flipper_init: SDL_CreateTexture screen\n");
        return false;
    }

    lcd_buffer = (uint32_t*)calloc(FL_LCD_WIDTH * FL_LCD_HEIGHT * sizeof(uint32_t), 1);
    if (!lcd_buffer) {
        printf("flipper_init: calloc pixels\n");
        return false;
    }

    ui_background = IMG_LoadTexture(renderer, UI_BG_PNG);
    if (!ui_background) {
        printf("flipper_init: IMG_LoadTexture %s\n", UI_BG_PNG);
        return false;
    }

    ui_highlight = IMG_LoadTexture(renderer, UI_HIGHTLIGHT_PNG);
    if (!ui_highlight) {
        printf("flipper_init: IMG_LoadTexture %s\n", UI_HIGHTLIGHT_PNG);
        return false;
    }

    SDL_SetTextureBlendMode(ui_highlight, SDL_BLENDMODE_ADD);

    flipper_pixel_reset();
    flipper_lcd_update();

    return true;
}

void flipper_close() {
    SDL_DestroyTexture(screen);
    free(lcd_buffer);
    SDL_DestroyTexture(ui_background);
    SDL_DestroyTexture(ui_highlight);

    SDL_DestroyWindow(window);
    SDL_Quit();
}

void flipper_pixel_set(int x, int y) {
    if (x < 0 || x >= FL_LCD_WIDTH)
        return;
    if (y < 0 || y >= FL_LCD_HEIGHT)
        return;

    lcd_buffer[y * FL_LCD_WIDTH + x] = LCD_COLOR_FG;
}

void flipper_pixel_clear(int x, int y) {
    if (x < 0 || x >= FL_LCD_WIDTH)
        return;
    if (y < 0 || y >= FL_LCD_HEIGHT)
        return;

    lcd_buffer[y * FL_LCD_WIDTH + x] = LCD_COLOR_BG;
}

bool flipper_pixel_get(int x, int y) {
    if (x < 0 || x >= FL_LCD_WIDTH)
        return 0;
    if (y < 0 || y >= FL_LCD_HEIGHT)
        return 0;

    return lcd_buffer[y * FL_LCD_WIDTH + x] == LCD_COLOR_FG;
}

// fill screen with background color
void flipper_pixel_reset() {
    for (int i = 0; i < FL_LCD_WIDTH * FL_LCD_HEIGHT; i++) {
        lcd_buffer[i] = LCD_COLOR_BG;
    }
}

void flipper_lcd_update() {
    // draw the background image to the window
    if (ui_rotate) {
        SDL_Rect destRect;
        destRect.x = 0;
        destRect.y = 0;
        destRect.w = UI_BG_WIDTH;
        destRect.h = UI_BG_HEIGHT;

        SDL_Point rotatePoint;
        rotatePoint.x = UI_BG_HEIGHT / 2;
        rotatePoint.y = UI_BG_HEIGHT / 2;  // what?

        SDL_RenderCopyEx(renderer, ui_background, NULL, &destRect, 90, &rotatePoint, 0);
    } else
        SDL_RenderCopy(renderer, ui_background, NULL, NULL);

    // add button highlights
    int32_t now = SDL_GetTicks();

    HIGHLIGHT_BUTTON* hb = highlight_buttons;
    for (int i = 0; i < NUM_HIGHLIGHT_BUTTONS; i++) {
        if (flipper_gpio_get(hb[i].gpio) && now - key_time[hb[i].gpio] < HIGHLIGHT_TIME) {
            SDL_Rect destRect;
            destRect.w = 64;
            destRect.h = 64;
            if (ui_rotate) {
                destRect.x = UI_BG_HEIGHT - 64 - hb[i].y;
                destRect.y = hb[i].x;
            } else {
                destRect.x = hb[i].x;
                destRect.y = hb[i].y;
            }

            // draw the image to the window
            SDL_RenderCopy(renderer, ui_highlight, NULL, &destRect);
        }
    }

    // update texture from pixels
    SDL_UpdateTexture(screen, NULL, lcd_buffer, FL_LCD_WIDTH * sizeof(uint32_t));

    // copy texture to screen (2x scale)
    if (ui_rotate) {
        SDL_Rect destRect;
        destRect.x = UI_BG_HEIGHT - FL_LCD_HEIGHT - UI_SCR_TOP;
        destRect.y = UI_SCR_LEFT;
        destRect.w = FL_LCD_WIDTH * 2;
        destRect.h = FL_LCD_HEIGHT * 2;

        SDL_Point p;
        p.x = FL_LCD_HEIGHT / 2;
        p.y = FL_LCD_HEIGHT / 2;

        SDL_RenderCopyEx(renderer, screen, NULL, &destRect, 90, &p, SDL_FLIP_VERTICAL);
    } else {
        SDL_Rect destRect;
        destRect.x = UI_SCR_LEFT;
        destRect.y = UI_SCR_TOP;
        destRect.w = FL_LCD_WIDTH * 2;
        destRect.h = FL_LCD_HEIGHT * 2;

        SDL_RenderCopy(renderer, screen, NULL, &destRect);
    }

    SDL_RenderPresent(renderer);
}

void flipper_lcd_constant_fps() {
    static uint32_t next_frame = 0;
    if (next_frame == 0)
        next_frame = SDL_GetTicks() + TICK_INTERVAL;

    int32_t remaining = next_frame - SDL_GetTicks();
    if (remaining > 0)
        SDL_Delay(remaining);
    next_frame += TICK_INTERVAL;
}

static int key_to_gpio(int key) {
    if (ui_rotate && (key >= 0 && key < 6))
        return map_rotated_button[key];
    return key;
}

void flipper_gpio_update() {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
            bool is_down = event.type == SDL_KEYDOWN;

            switch (event.key.keysym.sym) {
                case SDLK_UP:
                    gpio_state[key_to_gpio(FL_GPIO_BUTTON_UP)] = is_down;
                    key_time[key_to_gpio(FL_GPIO_BUTTON_UP)] = SDL_GetTicks();
                    break;
                case SDLK_DOWN:
                    gpio_state[key_to_gpio(FL_GPIO_BUTTON_DOWN)] = is_down;
                    key_time[key_to_gpio(FL_GPIO_BUTTON_DOWN)] = SDL_GetTicks();
                    break;
                case SDLK_LEFT:
                    gpio_state[key_to_gpio(FL_GPIO_BUTTON_LEFT)] = is_down;
                    key_time[key_to_gpio(FL_GPIO_BUTTON_LEFT)] = SDL_GetTicks();
                    break;
                case SDLK_RIGHT:
                    gpio_state[key_to_gpio(FL_GPIO_BUTTON_RIGHT)] = is_down;
                    key_time[key_to_gpio(FL_GPIO_BUTTON_RIGHT)] = SDL_GetTicks();
                    break;
                case SDLK_BACKSPACE:
                    gpio_state[FL_GPIO_BUTTON_BACK] = is_down;
                    key_time[FL_GPIO_BUTTON_BACK] = SDL_GetTicks();
                    break;
                case SDLK_RETURN:
                    gpio_state[FL_GPIO_BUTTON_ENTER] = is_down;
                    key_time[FL_GPIO_BUTTON_ENTER] = SDL_GetTicks();
                    break;

                case SDLK_ESCAPE:
                case SDLK_q: gpio_state[FL_GPIO_SIMULATOR_EXIT] = 1; break;
            }
            break;
        }
        if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                gpio_state[FL_GPIO_SIMULATOR_EXIT] = 1;
            }
        }
    }
}

bool flipper_gpio_get(int pin) {
    if (pin < 0 || pin >= sizeof(gpio_state))
        return false;
    return gpio_state[pin] == 1;
}

int flipper_key_get_time(int pin) {
    if (pin < 0 || pin >= sizeof(gpio_state))
        return 0;
    if (gpio_state[pin] == 0)
        return 0;

    return key_time[pin];
}

int flipper_random(int range) {
    return rand() % range;
}

int flipper_get_tics() {
    return SDL_GetTicks();
}