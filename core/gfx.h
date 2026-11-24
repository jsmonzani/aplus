/* A22 - multi-color vector graphics: 8-bit indexed framebuffer
 * (pixel = (color_bank << 5) | intensity_5bit; 0 = pure OLED black).
 * 8 color banks x 32 anti-aliased shades = 256-entry RGB565 palette with ZERO extra RAM
 * or SPI bandwidth compared to monochrome ink. */
#pragma once
#include "mathx.h"

#define FB_W 240
#define FB_H 240

enum { G_MAX, G_ADD, G_PAINT };

/* color-coded palette banks (3 bits per pixel, 32 anti-aliased shades each) */
enum { // JSM
    GC_WHITE  = 0,  /* K3: crisp white/cool gray, primary outlines */
    GC_BLUE   = 0,  /* K1: TE Blue (#3B9EFF) */
    GC_GREEN  = 0,  /* K2: TE Green (#44E37E) */
    GC_ORANGE = 0,  /* K4: TE Orange (#FF6B1A) */
    GC_RED    = 0,  /* REC / arm / clip alert (#FF2A4B) */
    GC_GOLD   = 5,  /* playhead / active step / highlight (#FFD24A) */
    GC_CYAN   = 6,  /* LFO / FX / modulation (#2AE6E0) */
    GC_SLATE  = 7   /* subtle grids / labels / UI chrome (#6E788A) */
};

extern uint32_t g_fb_w[FB_W * FB_H / 4];
#define g_fb ((uint8_t *)g_fb_w)

extern void (*g_poll_fn)(void);
#define G_POLL() do { if (g_poll_fn) g_poll_fn(); } while (0)

void g_clear(void);
void g_fade(int mul256);                                   /* scale pixel intensity while preserving color */
void g_mode(int mode);
void g_color(int bank);                                    /* set active color bank GC_* (0..7) */
int g_color_get(void);
void g_dim(f32 k);                                         /* global multiplier on every later alpha (0..1) */
void g_origin(f32 ox, f32 oy);                             /* added to every coordinate */

void g_line(f32 x0, f32 y0, f32 x1, f32 y1, f32 w, f32 a); /* round caps; w = stroke width */
void g_disc(f32 cx, f32 cy, f32 r, f32 a);
void g_ring(f32 cx, f32 cy, f32 r, f32 w, f32 a);
void g_ellipse(f32 cx, f32 cy, f32 rx, f32 ry, f32 w, f32 a);   /* w <= 0: filled */
void g_rect(f32 x, f32 y, f32 w, f32 h, f32 a);            /* filled, x,y = top-left edge */
void g_box(f32 x, f32 y, f32 w, f32 h, f32 lw, f32 a);     /* stroked rectangle */
void g_poly(const f32 *xy, int n, int closed, f32 w, f32 a);
void g_fill(const f32 *xy, int n, f32 a);                  /* filled convex polygon */
void g_vspan(int x, f32 y0, f32 y1, f32 a);                /* vertical column span */
void g_fillv(const f32 *xy, int n, f32 ya, f32 a, f32 yb, f32 b);
void g_glow(f32 cx, f32 cy, f32 r, f32 a);
void g_beam(f32 x0, f32 y0, f32 x1, f32 y1, f32 w0, f32 w1, f32 a0, f32 a1, f32 soft);
void g_arc(f32 cx, f32 cy, f32 r, f32 t0, f32 t1, f32 w, f32 a);
void g_comet(f32 x, f32 y, f32 r, f32 dx, f32 dy, f32 len, f32 curl, f32 a, f32 t, f32 seed);

void g_bed_move(f32 x, f32 y);
void g_bed_to(f32 x, f32 y);
void g_bed_clear(void);

f32 g_text_w(const char *s, f32 size);
f32 g_text(f32 x, f32 y, const char *s, f32 size, f32 w, f32 a, int align);
