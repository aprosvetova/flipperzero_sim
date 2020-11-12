#include "flipper.h"

#include <string.h>

#define LCD_WIDTH2 64
#define LCD_HEIGHT2 32

#define SNAKE_MAX_LEN 256
#define SNAKE_GROW_AMOUNT 8

#define SNAKE_RIGHT 0
#define SNAKE_DOWN 1
#define SNAKE_LEFT 2
#define SNAKE_UP 3

typedef struct {
    int x;
    int y;
} POINT;

typedef struct {
    int len;
    int tail;
    int head;
    int grow;
    POINT body[SNAKE_MAX_LEN];
    POINT pos;
    int direction;
} SNAKE;

POINT direction_delta[4] = {
    { 1, 0 },   // right
    { 0, 1 },   // down
    { -1, 0 },  // left
    { 0, -1 },  // up
};

#define BUTTONS 4

int buttons[BUTTONS] = {
    FL_GPIO_BUTTON_RIGHT,
    FL_GPIO_BUTTON_DOWN,
    FL_GPIO_BUTTON_LEFT,
    FL_GPIO_BUTTON_UP,
};

void snake_init(SNAKE* s) {
    s->len = 0;
    s->tail = 0;
    s->head = 0;
    s->grow = 10;
    memset(s->body, 0, sizeof(s->body));
    s->pos.x = 10;
    s->pos.y = 10;
    s->direction = SNAKE_RIGHT;
}

void draw_border() {
    for (int i = 0; i < FL_LCD_WIDTH; i++) {
        flipper_pixel_set(i, 0);
        flipper_pixel_set(i, FL_LCD_HEIGHT - 1);
        if (i < FL_LCD_HEIGHT) {
            flipper_pixel_set(0, i);
            flipper_pixel_set(FL_LCD_WIDTH - 1, i);
        }
    }
}

void draw_x4(int x, int y) {
    flipper_pixel_set(x, y);
    flipper_pixel_set(x, y + 1);
    flipper_pixel_set(x + 1, y);
    flipper_pixel_set(x + 1, y + 1);
}

void fruit_new_pos(POINT* pos) {
    do {
        pos->x = flipper_random(LCD_WIDTH2 - 2) * 2 + 2;
        pos->y = flipper_random(LCD_HEIGHT2 - 2) * 2 + 2;
    } while (flipper_pixel_get(pos->x, pos->y) || flipper_pixel_get(pos->x + 1, pos->y + 1));
}

bool fruit_check(POINT* fruit, POINT* snake) {
    int dx = snake->x - fruit->x;
    int dy = snake->y - fruit->y;
    return (dx == 0 && dy == 0);
}

int get_latest_direction() {
    int best_direction = -1;
    int best_time = 0;

    for (int i = 0; i < BUTTONS; i++) {
        int b = buttons[i];
        if (flipper_gpio_get(b)) {
            int t = flipper_key_get_time(b);
            if (best_direction == -1 || (t > best_time)) {
                best_direction = i;
                best_time = t;
            }
        }
    }
    return best_direction;
}

int main() {
    if (!flipper_init(0))
        return 1;

    SNAKE snake;
    snake_init(&snake);

    POINT fruit_pos;
    fruit_new_pos(&fruit_pos);

    bool quit = false;
    while (!quit) {
        flipper_gpio_update();

        if (flipper_gpio_get(FL_GPIO_BUTTON_BACK)) {
            snake_init(&snake);
            fruit_new_pos(&fruit_pos);
        }

        if (flipper_gpio_get(FL_GPIO_SIMULATOR_EXIT)) {
            quit = true;
            break;
        }

        int new_direction = get_latest_direction();
        if (new_direction != -1 && new_direction != snake.direction) {
            snake.direction = new_direction;
        }

        flipper_pixel_reset();

        // draw border
        draw_border();

        POINT new_pos;
        new_pos.x = snake.pos.x + direction_delta[snake.direction].x * 2;
        new_pos.y = snake.pos.y + direction_delta[snake.direction].y * 2;

        // draw body
        for (int i = 0; i < snake.len; i++) {
            POINT* p = &snake.body[(snake.tail + i) % SNAKE_MAX_LEN];
            draw_x4(p->x, p->y);
        }

        // check that new pixel(s) are empty
        if (!flipper_pixel_get(new_pos.x, new_pos.y) &&
            !flipper_pixel_get(new_pos.x + 1, new_pos.y + 1)) {
            snake.pos = new_pos;

            if (snake.len < snake.grow) {
                snake.body[snake.head] = snake.pos;
                snake.head = (snake.head + 1) % SNAKE_MAX_LEN;
                snake.len++;
            } else {
                snake.body[snake.head] = snake.pos;
                snake.tail = (snake.tail + 1) % SNAKE_MAX_LEN;
                snake.head = (snake.head + 1) % SNAKE_MAX_LEN;
            }
        }

        if (fruit_check(&snake.pos, &fruit_pos)) {
            snake.grow += SNAKE_GROW_AMOUNT;
            if (snake.grow > SNAKE_MAX_LEN)
                snake.grow = SNAKE_MAX_LEN;
            fruit_new_pos(&fruit_pos);
        }

        // draw fruit
        draw_x4(fruit_pos.x, fruit_pos.y);

        flipper_lcd_update();
        flipper_lcd_constant_fps();
    }

    return 0;
}