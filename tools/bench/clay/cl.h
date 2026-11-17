#ifndef CL_BENCH_H
#define CL_BENCH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLAY_IMPLEMENTATION
#include "clay.h"

#define BENCH_ROWS 50

#define COL_BG ((Clay_Color){45, 45, 50, 255})
#define COL_BTN ((Clay_Color){70, 110, 170, 255})
#define COL_BOX ((Clay_Color){90, 90, 95, 255})
#define COL_ON ((Clay_Color){90, 170, 110, 255})
#define COL_TXT ((Clay_Color){230, 230, 230, 255})

static Clay_Dimensions clb_measure(Clay_StringSlice text, Clay_TextElementConfig *cfg, void *user)
{
    (void)user;
    return (Clay_Dimensions){(float)text.length * 8.0f, (float)cfg->fontSize};
}

static void clb_error(Clay_ErrorData e)
{
    fprintf(stderr, "clay error %d: %.*s\n", (int)e.errorType, (int)e.errorText.length, e.errorText.chars);
}

typedef struct {
    int checks[BENCH_ROWS];
    float vals[BENCH_ROWS];
    char rowbuf[BENCH_ROWS][16];
    char clicks[32];
} clb;

static void clb_init(clb *b, void **mem)
{
    memset(b, 0, sizeof *b);
    uint32_t sz = Clay_MinMemorySize();
    *mem = malloc(sz);
    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(sz, *mem);
    Clay_Initialize(arena, (Clay_Dimensions){960, 640}, (Clay_ErrorHandler){clb_error, 0});
    Clay_SetMeasureTextFunction(clb_measure, 0);
    for (int i = 0; i < BENCH_ROWS; i++) b->vals[i] = i / (float)BENCH_ROWS;
}

static void clb_text(Clay_String s)
{
    CLAY_TEXT(s, CLAY_TEXT_CONFIG({.fontSize = 14, .textColor = COL_TXT}));
}

static void clb_hello(clb *b)
{
    snprintf(b->clicks, sizeof b->clicks, "clicks: %d", 0);
    CLAY({.id = CLAY_ID("window"), .layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM, .padding = CLAY_PADDING_ALL(8), .childGap = 8, .sizing = {.width = CLAY_SIZING_FIXED(300), .height = CLAY_SIZING_FIXED(200)}}, .backgroundColor = COL_BG}) {
        clb_text(CLAY_STRING("Hello"));
        clb_text(CLAY_STRING("hello world"));
        CLAY({.id = CLAY_ID("btn"), .layout = {.padding = CLAY_PADDING_ALL(6)}, .backgroundColor = COL_BTN, .cornerRadius = CLAY_CORNER_RADIUS(4)}) {
            clb_text(CLAY_STRING("Click me"));
        }
        clb_text((Clay_String){.length = (int32_t)strlen(b->clicks), .chars = b->clicks});
    }
}

static void clb_dash(clb *b)
{
    CLAY({.id = CLAY_ID("window"), .layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM, .padding = CLAY_PADDING_ALL(8), .childGap = 4, .sizing = {.width = CLAY_SIZING_FIXED(960), .height = CLAY_SIZING_FIXED(640)}}, .backgroundColor = COL_BG}) {
        for (int i = 0; i < BENCH_ROWS; i++) {
            snprintf(b->rowbuf[i], sizeof b->rowbuf[i], "row %d", i);
            CLAY({.id = CLAY_IDI("row", i), .layout = {.layoutDirection = CLAY_LEFT_TO_RIGHT, .childGap = 10, .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}}}) {
                clb_text((Clay_String){.length = (int32_t)strlen(b->rowbuf[i]), .chars = b->rowbuf[i]});
                CLAY({.id = CLAY_IDI("btn", i), .layout = {.padding = CLAY_PADDING_ALL(4)}, .backgroundColor = COL_BTN, .cornerRadius = CLAY_CORNER_RADIUS(3)}) {
                    clb_text(CLAY_STRING("Open"));
                }
                CLAY({.id = CLAY_IDI("chk", i), .layout = {.sizing = {.width = CLAY_SIZING_FIXED(16), .height = CLAY_SIZING_FIXED(16)}}, .backgroundColor = b->checks[i] ? COL_ON : COL_BOX}) {}
                clb_text(CLAY_STRING("On"));
                CLAY({.id = CLAY_IDI("trk", i), .layout = {.sizing = {.width = CLAY_SIZING_FIXED(160), .height = CLAY_SIZING_FIXED(8)}}, .backgroundColor = COL_BOX}) {
                    CLAY({.id = CLAY_IDI("fill", i), .layout = {.sizing = {.width = CLAY_SIZING_FIXED(160.0f * b->vals[i]), .height = CLAY_SIZING_FIXED(8)}}, .backgroundColor = COL_BTN}) {}
                }
                clb_text(CLAY_STRING("Value"));
            }
        }
    }
}

static int clb_frame(clb *b, int dash)
{
    Clay_BeginLayout();
    if (dash) clb_dash(b); else clb_hello(b);
    Clay_RenderCommandArray cmds = Clay_EndLayout();
    return (int)cmds.length;
}

#endif
