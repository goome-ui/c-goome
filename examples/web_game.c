#include <stdio.h>
#include <string.h>

#include "goome.h"

enum { FW = 360, FH = 500, COLS = 8, ROWS = 6, BW = 40, BH = 16, MAXP = 96 };
enum { MENU, SERVE, PLAY, PAUSED, OVER };

static const double PW = 64, PH = 10, PY = FH - 34, BR = 6;
static const char *const diffs[] = {"Easy", "Normal", "Hard"};
static const double base_speed[] = {280, 340, 420};
static const int base_lives[] = {5, 3, 2};
static const uint32_t row_col[ROWS] = {0xFFFF5D73, 0xFFFF9A4D, 0xFFFFD24A, 0xFF6FDC5F, 0xFF4FB3F7, 0xFFB57CFF};

typedef struct {
    double x, y, vx, vy, life;
    uint32_t col;
} spark;

static goome *g;
static goome_ui *u;
static spark sparks[MAXP];
static char alive[ROWS][COLS];
static int state = MENU, diff = 1, level, score, best, lives, combo, left, total, fresh;
static double px = FW / 2, want = FW / 2, bx, by, bvx, bvy, speed;
static double field_top = 130, fx, fy, fs = 1;
static long last;

static double clampd(double v, double a, double b) { return v < a ? a : v > b ? b : v; }

static double root(double v)
{
    if (v <= 0) return 0;
    double r = v > 1 ? v / 2 : v;
    for (int i = 0; i < 24; i++) r = (r + v / r) / 2;
    return r;
}

static void fill_bricks(void)
{
    int shape = (level - 1) % 3;
    left = 0;
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            int on = shape == 0 || (shape == 1 && (r + c) % 2 == 0) || (shape == 2 && r % 2 == 0);
            alive[r][c] = (char)on;
            left += on;
        }
    total = left;
}

static void serve(void)
{
    state = SERVE;
    bvx = bvy = 0;
}

static void start(void)
{
    level = 1, score = 0, combo = 0, fresh = 0;
    lives = base_lives[diff];
    speed = base_speed[diff];
    px = want = FW / 2;
    memset(sparks, 0, sizeof(sparks));
    fill_bricks();
    serve();
}

static void aim(double rel)
{
    bvx = rel * 0.85 * speed;
    bvy = -root(speed * speed - bvx * bvx);
}

static void launch(void)
{
    if (state != SERVE) return;
    aim(want > px ? 0.35 : -0.35);
    state = PLAY;
}

static void pause_game(void)
{
    if (state == PLAY || state == SERVE) state = PAUSED;
}

static void burst(double x, double y, uint32_t col)
{
    int made = 0;
    for (int i = 0; i < MAXP && made < 7; i++) {
        if (sparks[i].life > 0) continue;
        double a = (double)((i * 37 + score) % 100) / 100;
        sparks[i] = (spark){x, y, (a - 0.5) * 220, -40 - (double)((i * 53) % 90), 0.5, col};
        made++;
    }
}

static void hit_brick(int r, int c)
{
    alive[r][c] = 0;
    left--;
    combo++;
    score += (ROWS - r) * 10 * (combo > 5 ? 5 : combo);
    burst(c * (BW + 4) + 6 + BW / 2, 50 + r * 20 + BH / 2, row_col[r]);
}

static int brick_at(double x, double y)
{
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            if (!alive[r][c]) continue;
            double bxl = c * (BW + 4) + 6, byt = 50 + r * 20;
            if (x > bxl - BR && x < bxl + BW + BR && y > byt - BR && y < byt + BH + BR) {
                hit_brick(r, c);
                return 1;
            }
        }
    return 0;
}

static void lose_life(void)
{
    lives--;
    combo = 0;
    if (lives > 0) {
        serve();
        return;
    }
    fresh = score > best;
    if (fresh) best = score;
    state = OVER;
    printf("game over, score %d\n", score);
}

static void next_level(void)
{
    level++;
    speed += 25;
    fill_bricks();
    serve();
    char msg[24];
    snprintf(msg, sizeof(msg), "level %d", level);
    goome_ui_toast(u, msg, 1200);
    printf("level %d\n", level);
}

static void step(double dt)
{
    for (int i = 0; i < MAXP; i++) {
        if (sparks[i].life <= 0) continue;
        sparks[i].life -= dt;
        sparks[i].x += sparks[i].vx * dt;
        sparks[i].y += sparks[i].vy * dt;
        sparks[i].vy += 500 * dt;
    }
    want = clampd(want, PW / 2, FW - PW / 2);
    px += (want - px) * clampd(dt * 22, 0, 1);
    if (state == SERVE) {
        bx = px, by = PY - BR;
        return;
    }
    if (state != PLAY) return;
    int n = (int)(speed * dt / 4) + 1;
    double ds = dt / n;
    for (int i = 0; i < n; i++) {
        bx += bvx * ds;
        if (bx < BR) bx = BR, bvx = -bvx;
        if (bx > FW - BR) bx = FW - BR, bvx = -bvx;
        if (brick_at(bx, by)) bx -= bvx * ds, bvx = -bvx;
        by += bvy * ds;
        if (by < BR) by = BR, bvy = -bvy;
        if (brick_at(bx, by)) by -= bvy * ds, bvy = -bvy;
        if (bvy > 0 && by + BR >= PY && by + BR <= PY + PH + 8 && bx >= px - PW / 2 - BR && bx <= px + PW / 2 + BR) {
            by = PY - BR;
            if (speed < base_speed[diff] + 220) speed += 4;
            aim(clampd((bx - px) / (PW / 2), -1, 1));
            combo = 0;
        }
        if (by > FH + BR) {
            lose_life();
            return;
        }
        if (left == 0) {
            next_level();
            return;
        }
    }
}

static void game_key(const goome_event *ev)
{
    int k = ev->key;
    if (k == GOOME_K_LEFT || k == 'a') want -= 44;
    if (k == GOOME_K_RIGHT || k == 'd') want += 44;
    if (k == GOOME_K_SPACE || k == GOOME_K_ENTER || k == GOOME_K_UP) launch();
    if (k == 'p' || k == GOOME_K_ESC) pause_game();
    if (k == 'r') start();
}

static int in_field(double x, double y) { return x >= fx && x <= fx + FW * fs && y >= fy && y <= fy + FH * fs; }

int goome_web_init(void)
{
    int err;
    g = goome_open(&(goome_opts){.title = "Brickout", .mode = GOOME_MODE_WINDOW, .width = 400, .height = 720, .min_width = 300, .min_height = 480,
                                 .window_flags = GOOME_WINDOW_RESIZABLE | GOOME_WINDOW_CENTER},
                   &err);
    if (!g) {
        fprintf(stderr, "cant open a window: %s\n", goome_strerror(err));
        return 1;
    }
    u = goome_ui_new(g);
    if (!u) return 1;
    goome_ui_theme_make(goome_ui_theme(u), 0xFFFF6FA8, 1, 10);
    level = 1;
    lives = base_lives[diff];
    fill_bricks();
    last = goome_now_ms();
    return 0;
}

void goome_web_done(void)
{
    goome_ui_free(u);
    goome_close(g);
}

static void draw_field(goome_canvas *c)
{
    goome_draw_round_rect(c, fx, fy, FW * fs, FH * fs, 8 * fs, 0xFF11131A);
    for (int r = 0; r < ROWS; r++)
        for (int k = 0; k < COLS; k++)
            if (alive[r][k]) goome_draw_round_rect(c, fx + (k * (BW + 4) + 6) * fs, fy + (50 + r * 20) * fs, BW * fs, BH * fs, 3 * fs, row_col[r]);
    for (int i = 0; i < MAXP; i++) {
        if (sparks[i].life <= 0) continue;
        uint32_t a = (uint32_t)(clampd(sparks[i].life * 2, 0, 1) * 255);
        goome_draw_rect(c, fx + sparks[i].x * fs, fy + sparks[i].y * fs, 3 * fs, 3 * fs, a << 24 | (sparks[i].col & 0xFFFFFF));
    }
    goome_draw_round_rect(c, fx + (px - PW / 2) * fs, fy + PY * fs, PW * fs, PH * fs, 5 * fs, goome_ui_theme(u)->accent);
    if (state == SERVE || state == PLAY || state == PAUSED) {
        goome_draw_circle(c, fx + bx * fs, fy + by * fs, BR * 2.2 * fs, 0x30FFFFFF);
        goome_draw_circle(c, fx + bx * fs, fy + by * fs, BR * fs, 0xFFFFFFFF);
    }
    char line[48];
    if (state == SERVE) {
        const char *msg = "click, tap or press space";
        goome_draw_text(c, fx + (FW * fs - goome_text_width(c, msg, 0)) / 2, fy + (PY - 40) * fs, msg, 0, 0xFFE8EAF0);
    }
    if (combo >= 2) {
        snprintf(line, sizeof(line), "x%d combo", combo > 5 ? 5 : combo);
        goome_draw_text(c, fx + (FW - 8) * fs - goome_text_width(c, line, 0), fy + 12 * fs, line, 0, 0xFFFFD24A);
    }
}

static void menus(void)
{
    int open = 1;
    if (state == MENU && goome_ui_modal_begin(u, "Brickout", &open)) {
        goome_ui_wrapped(u, "Break every brick. Move with the mouse, your finger or the arrow keys. Click, tap or space launches the ball, p pauses.");
        goome_ui_space(u, 6);
        goome_ui_radio(u, diffs[0], &diff, 0);
        goome_ui_same_line(u);
        goome_ui_radio(u, diffs[1], &diff, 1);
        goome_ui_same_line(u);
        goome_ui_radio(u, diffs[2], &diff, 2);
        goome_ui_tooltip(u, "faster ball, fewer lives");
        if (goome_ui_button(u, "Play")) start();
        goome_ui_modal_end(u);
    }
    if (state == MENU && !open) start();
    open = 1;
    if (state == PAUSED && goome_ui_modal_begin(u, "Paused", &open)) {
        goome_ui_label(u, "Level %d, score %d", level, score);
        if (goome_ui_button(u, "Resume")) state = PLAY;
        if (goome_ui_button(u, "Restart")) start();
        goome_ui_modal_end(u);
    }
    if (state == PAUSED && !open) state = PLAY;
    open = 1;
    if (state == OVER && goome_ui_modal_begin(u, "Game over", &open)) {
        goome_ui_heading(u, "No lives left");
        goome_ui_label(u, "Score %d on level %d", score, level);
        if (fresh) goome_ui_badge(u, "new best", 0xFF4DB36A);
        else goome_ui_label(u, "Best is %d", best);
        if (goome_ui_button(u, "Play again")) start();
        goome_ui_modal_end(u);
    }
    if (state == OVER && !open) start();
}

int goome_web_frame(void)
{
    goome_event ev;
    while (goome_poll(g, &ev, 16) > 0) {
        if (ev.type == GOOME_QUIT || ev.type == GOOME_CLOSE) return 0;
        int playing = state == SERVE || state == PLAY;
        if (playing && ev.type == GOOME_KEY) {
            game_key(&ev);
            continue;
        }
        goome_ui_feed(u, &ev);
        if (!playing) continue;
        if (ev.type == GOOME_MOTION && in_field(ev.x, ev.y)) want = (ev.x - fx) / fs;
        if (ev.type == GOOME_DOWN && in_field(ev.x, ev.y)) want = (ev.x - fx) / fs, launch();
    }
    long now = goome_now_ms();
    double dt = clampd((double)(now - last) / 1000, 0, 0.05);
    last = now;
    step(dt);

    goome_monitor m;
    goome_monitors(g, &m, 1);
    if (!goome_ui_begin(u, 0)) return 1;
    goome_theme *th = goome_ui_theme(u);
    goome_ui_app(u, "Brickout", 0);
    goome_ui_row(u, 3);
    goome_ui_label(u, "Score %d", score);
    goome_ui_label(u, "Best %d", best);
    goome_ui_label(u, "Lives %d", lives);
    goome_ui_row(u, 2);
    if (goome_ui_button(u, state == PAUSED ? "Resume" : "Pause")) {
        if (state == PAUSED) state = PLAY;
        else pause_game();
    }
    goome_ui_tooltip(u, "p or esc works too");
    if (goome_ui_button(u, "Restart")) start();
    goome_ui_tooltip(u, "r works too");
    char tag[24];
    snprintf(tag, sizeof(tag), "level %d", level);
    goome_ui_progress(u, total ? 1 - (double)left / total : 0, tag);
    goome_ui_item it;
    goome_ui_custom(u, "field", m.area.w - 2 * th->pad, clampd(m.area.h - field_top - th->pad, 120, 4000), 0, &it);
    field_top = it.y;
    fs = clampd(it.w / FW, 0.2, 4) < it.h / FH ? clampd(it.w / FW, 0.2, 4) : it.h / FH;
    fx = it.x + (it.w - FW * fs) / 2;
    fy = it.y + (it.h - FH * fs) / 2;
    goome_canvas *c = goome_ui_canvas(u);
    if (c) draw_field(c);
    menus();
    goome_ui_app_end(u);
    goome_ui_end(u);
    return 1;
}

int main(void)
{
    if (goome_web_init()) return 1;
    while (goome_web_frame()) {
    }
    goome_web_done();
    return 0;
}
