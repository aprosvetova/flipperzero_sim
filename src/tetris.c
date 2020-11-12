// https://tetris.fandom.com/wiki/Tetris_Guideline

#include "flipper.h"
#include "../img/micro4x6.xbm"
#include <string.h>

#define GRID_X 7
#define GRID_Y (24)

#define GRID_SX 5
#define GRID_SY 5

#define GRID_WIDTH 10
#define GRID_HEIGHT 20

#define NEXT_Y -4

#define FALL_DELAY 1000

#include "tetris_pieces.h"

enum GAME_STATE { PLAYING = 0, GAMEOVER };

typedef struct {
    int x;
    int y;
} POINT;

typedef struct {
    int piece;     // 0..NUM_PIECES-1
    int rotation;  // 0..3
    POINT position;
} BLOCK;

typedef struct {
    BLOCK block;
    int next_piece;
    uint8_t field[GRID_HEIGHT][GRID_WIDTH];
    int score;
    int state;
} GAME;

#define BUTTONS 6
int buttons[BUTTONS] = {
    FL_GPIO_BUTTON_RIGHT, FL_GPIO_BUTTON_DOWN, FL_GPIO_BUTTON_LEFT,
    FL_GPIO_BUTTON_UP,    FL_GPIO_BUTTON_BACK, FL_GPIO_BUTTON_ENTER,
};

void game_init(GAME *g) {
    g->block.position.x = 0;
    g->block.position.y = 0;
    g->block.piece = PIECE_L;
    g->block.rotation = 0;
    g->next_piece = flipper_random(NUM_PIECES);
    g->score = 0;
    g->state = PLAYING;
    memset(g->field, 0, sizeof(g->field));
}

void game_rand_piece(GAME *g) {
    g->block.piece = g->next_piece;
    g->block.position.x = GRID_WIDTH / 2 - pieces[g->block.piece].width / 2;
    g->block.position.y = -1;
    g->block.rotation = 0;
    g->next_piece = flipper_random(NUM_PIECES);
}

// draw 1 pixel, swap x/y because rotated
void draw_1(int x, int y) {
    flipper_pixel_set(GRID_Y + y, GRID_X + x);
}

// round block on 5x5 grid
void draw_5x5_circle(int x, int y, int radius2) {
    if (radius2 == 0)
        radius2 = 6;

    for (int j = 0; j < 5; j++) {
        for (int i = 0; i < 5; i++) {
            int dx = i - 2;
            int dy = j - 2;
            int dist = dx * dx + dy * dy;
            if (dist > 0 && dist <= radius2)
                flipper_pixel_set(GRID_Y + y * GRID_SY + j, GRID_X + x * GRID_SX + i);
        }
    }
}

void draw_border() {
    // field
    for (int j = 0; j <= GRID_HEIGHT * GRID_SY; j++) {
        draw_1(-1, j);
        draw_1(GRID_WIDTH * GRID_SX, j);
    }
    for (int i = 0; i <= GRID_WIDTH * GRID_SX; i++) {
        draw_1(i, -1);
        draw_1(i, GRID_HEIGHT * (GRID_SY));
    }
}

int get_latest_button() {
    int best_button = -1;
    int best_time = 0;

    for (int i = 0; i < BUTTONS; i++) {
        int b = buttons[i];
        if (flipper_gpio_get(b)) {
            int g = flipper_key_get_time(b);
            if (best_button == -1 || (g > best_time)) {
                best_button = b;
                best_time = g;
            }
        }
    }
    return best_button;
}

void draw_block(BLOCK *b, int is_shadow) {
    PIECE *p = &pieces[b->piece];

    for (int y = 0; y < p->height; y++) {
        for (int x = 0; x < p->width; x++) {
            if (x + b->position.x < 0 || x + b->position.x > GRID_WIDTH)
                continue;
            if (y + b->position.y < 0 || y + b->position.y > GRID_HEIGHT)
                continue;
            if (p->data[b->rotation][y * p->width + x]) {
                draw_5x5_circle(b->position.x + x, b->position.y + y, is_shadow ? 4 : 0);
            }
        }
    }
}

void draw_next_piece(int next_type) {
    PIECE *p = &pieces[next_type];
    int next_x = GRID_WIDTH - p->width - 1;
    for (int y = 0; y < p->height; y++) {
        for (int x = 0; x < p->width; x++) {
            if (p->data[0][y * p->width + x]) {
                draw_5x5_circle(next_x + x, NEXT_Y + y, 0);
            }
        }
    }
}

bool valid_block(GAME *g, BLOCK *b) {
    PIECE *p = &pieces[g->block.piece];
    for (int y = 0; y < p->height; y++) {
        for (int x = 0; x < p->width; x++) {
            if (p->data[b->rotation][y * p->width + x]) {
                if (b->position.x + x < 0 || b->position.x + x >= GRID_WIDTH)
                    return false;
                if (b->position.y + y >= GRID_HEIGHT)
                    return false;
                if (b->position.y + y < 0)
                    continue;
                if (g->field[b->position.y + y][b->position.x + x])
                    return false;
            }
        }
    }
    return true;
}

void freeze_block_on_field(GAME *g) {
    PIECE *p = &pieces[g->block.piece];
    for (int y = 0; y < p->height; y++) {
        for (int x = 0; x < p->width; x++) {
            if (p->data[g->block.rotation][y * p->width + x]) {
                if (g->block.position.y + y >= 0)
                    g->field[g->block.position.y + y][g->block.position.x + x] = 1;
            }
        }
    }

    // find lines to be removed
    int count = 0;
    int lines[GRID_HEIGHT] = { 0 };

    for (int y = 0; y < GRID_HEIGHT; y++) {
        bool all = true;
        for (int x = 0; x < GRID_WIDTH; x++) {
            if (!g->field[y][x]) {
                all = false;
                break;
            }
        }
        if (all) {
            g->score++;
            lines[y] = 1;
            count++;
        }
    }
    if (count == 0)
        return;

    // remove all tetris linescopy lines from above
    int y_src = GRID_HEIGHT-1;
    int y_dest = GRID_HEIGHT-1; 
    while (y_src >= 0 && y_dest >= 0) {
        while (y_src >= 0 && lines[y_src])
            y_src--;
        if (y_src < 0)
            break;

        if (y_dest != y_src)
            memcpy(g->field[y_dest], g->field[y_src], GRID_WIDTH);
        y_dest--;
        y_src--;
    }
    for (int y=y_dest; y>=0; y--) {
        memset(g->field[y], 0, GRID_WIDTH);
    }
}

void draw_field(GAME *g) {
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            if (g->field[y][x]) {
                draw_5x5_circle(x, y, 0);
            }
        }
    }
}

void draw_char(int x, int y, char c, int invert) {
    if (c < 0 || c > 127)
        return;
    int py = (c / 16) * font_glyph_height;
    int px = (c % 16) * font_glyph_width;

    for (int j = 0; j < font_glyph_height; j++) {
        for (int i = 0; i < font_glyph_width; i++) {
            int yy = py + j;
            int xx = px + i;
            int bits = font_bits[yy * font_width / 8 + xx / 8];
            if (((bits >> (xx & 7)) & 1) == invert)
                flipper_pixel_set(y + j, x + i);
            else
                flipper_pixel_clear(y + j, x + i);
        }
    }
}

void draw_score(GAME *g) {
    int score = g->score;
    // because rotated, use lcd height instead of width
    int x = 1 + font_glyph_width * 2;
    int y = 1;
    do {
        char c = '0' + (score % 10);
        draw_char(x, y, c, 0);
        x -= font_glyph_width;
        score /= 10;
    } while (score > 0);
}

void draw_string(int x, int y, char *s, int invert) {
    while (*s) {
        draw_char(x, y, *s, invert);
        x += font_glyph_width;
        s++;
    }
}

void draw_box(int x, int y, int width, int height) {
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            flipper_pixel_set(j + y, i + x);
        }
    }
}

// move down until stuck to find shadow position
void find_shadow(GAME *g, BLOCK *b) {
    BLOCK shadow = *b;
    while (true) {
        shadow.position.y++;
        if (!valid_block(g, &shadow)) {
            b->position.y = shadow.position.y - 1;
            return;
        }
    }
}

int main() {
    if (!flipper_init(FL_INIT_SIMULATOR_ROTATE))
        return 1;

    GAME g;
    game_init(&g);
    game_rand_piece(&g);

    bool quit = false;
    int last_button = -1;
    int last_button_time = -1;
    int last_down_time = flipper_get_tics();
    int state = PLAYING;

    while (!quit) {
        flipper_gpio_update();

        if (flipper_gpio_get(FL_GPIO_SIMULATOR_EXIT)) {
            quit = true;
            break;
        }

        int button = get_latest_button();
        if (button != -1) {
            int button_time = flipper_key_get_time(button);
            int dt = flipper_get_tics() - last_button_time;

            if (button != last_button || button_time != last_button_time || dt > 200) {
                BLOCK move_block = g.block;

                if (button == FL_GPIO_BUTTON_BACK) {
                    game_init(&g);
                    game_rand_piece(&g);
                    state = PLAYING;

                } else if (state == PLAYING) {
                    if (button == FL_GPIO_BUTTON_RIGHT) {
                        // RIGHT moves piece down until down
                        find_shadow(&g, &g.block);
                        last_down_time = 0;  // force new piece
                    } else {
                        // DOWN moves piece to the left
                        if (button == FL_GPIO_BUTTON_DOWN)
                            move_block.position.x--;

                        // UP moves piece to the right
                        if (button == FL_GPIO_BUTTON_UP)
                            move_block.position.x++;

                        // LEFT rotates
                        if (button == FL_GPIO_BUTTON_LEFT)
                            move_block.rotation = (move_block.rotation + 1) & 3;

                        if (valid_block(&g, &move_block)) {
                            g.block = move_block;
                        }
                    }
                }
                last_button = button;
                last_button_time = button_time;
            }
        }

        if (state == PLAYING) {
            if (flipper_get_tics() - last_down_time > FALL_DELAY) {
                BLOCK new_block = g.block;
                new_block.position.y++;
                if (valid_block(&g, &new_block)) {
                    g.block = new_block;
                } else {
                    // freeze current and spawn new piece
                    freeze_block_on_field(&g);
                    game_rand_piece(&g);

                    if (!valid_block(&g, &g.block)) {
                        state = GAMEOVER;
                    }
                }
                last_down_time = flipper_get_tics();
            }
        }

        flipper_pixel_reset();

        draw_border();
        draw_field(&g);

        if (state == PLAYING) {
            draw_block(&g.block, 0);

            BLOCK shadow = g.block;
            find_shadow(&g, &shadow);
            draw_block(&shadow, 1);

            draw_next_piece(g.next_piece);
        }
        draw_score(&g);

        if (state == GAMEOVER) {
            draw_box(16 - 2, 60 - 2, 39, 9);
            draw_string(16, 60, "GAME OVER", 1);
        }
        flipper_lcd_update();
        flipper_lcd_constant_fps();
    }

    return 0;
}