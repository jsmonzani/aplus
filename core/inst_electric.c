/* A22 - ELECTRIC: Boutique Electro-Mechanical Keys (FM + Additive + Stereo Tremolo).
 *
 * Eight studio-tuned keyboard voices: Suitcase Rhodes, Wurlitzer 200A, Crystal DX-EP,
 * Funky D6 Clavinet, B3 Tonewheel Organ, Felted Upright Piano, Vibraphone, and Tape Keys.
 * True stereo autopan tremolo & rotary width, velocity-sensitive tine bark, and warm tone.
 *
 *   K1 (BLUE)   BARK    FM overtone drive & velocity bite
 *   K2 (GREEN)  TINE    metallic strike, bell sparkle & hammer click
 *   K3 (WHITE)  TREM    stereo autopan tremolo / Leslie rotary depth
 *   K4 (ORANGE) WARMTH  cabinet warmth / felt damping / tape age */
#include "internal.h"
#include "dsp.h"

typedef struct EAlgo {
    f32 dec;             /* natural note decay time (s), 0 = sustain */
    f32 att;             /* onset time (s) */
    f32 bark;            /* base bark FM index */
    f32 tine;            /* base tine harmonic level */
} EAlgo;

static const EAlgo EA[NALGO] = {
    /* 0 RHODES */ {2.8f, 0.0f,   0.52f, 0.42f},
    /* 1 WURLI  */ {1.5f, 0.0f,   0.85f, 0.30f},
    /* 2 DX-EP  */ {3.1f, 0.0f,   0.65f, 0.58f},
    /* 3 CLAV   */ {0.75f,0.0f,   0.92f, 0.45f},
    /* 4 ORGAN  */ {0.0f, 0.002f, 0.35f, 0.45f},
    /* 5 FELT   */ {3.2f, 0.004f, 0.25f, 0.28f},
    /* 6 VIBES  */ {3.8f, 0.0f,   0.20f, 0.52f},
    /* 7 TAPE   */ {2.5f, 0.008f, 0.48f, 0.32f},
};

static struct {
    int algo;
    f32 bark, tine, trem, warmth;
    f32 trem_l, trem_r, rot_ph, wow, kd;
} E;
static f32 trem_ph, wow_ph, flut_ph, elec_t_was;

static void elec_block(const Blk *b)
{
    f32 dt = b->step;
    f32 rate = (b->algo == 4) ? (1.2f + 5.6f * b->k[2]) : (2.6f + 3.8f * b->k[2]);
    f32 tr, dp;
    E.algo = b->algo & 7;
    E.bark = b->k[0];
    E.tine = b->k[1];
    E.trem = b->k[2];
    E.warmth = b->k[3];

    if (b->time < elec_t_was) trem_ph = wow_ph = flut_ph = 0.0f;
    elec_t_was = b->time;
    trem_ph = fractf(trem_ph + rate * dt);
    wow_ph = fractf(wow_ph + 0.58f * dt);
    flut_ph = fractf(flut_ph + 5.9f * dt);
    E.rot_ph = trem_ph;
    tr = tsin(trem_ph);
    dp = E.trem * 0.72f;
    E.trem_l = 1.0f - dp * (0.5f + 0.5f * tr);
    E.trem_r = 1.0f - dp * (0.5f - 0.5f * tr);
    E.wow = (E.algo == 7) ? (0.04f + 0.35f * E.warmth) * (0.65f * tsin(wow_ph) + 0.2f * tsin(flut_ph)) : 0.0f;
    E.kd = EA[E.algo].dec > 0.0f ? 1.0f - onepole_k(EA[E.algo].dec, b->sr / (f32)CTRL) : 1.0f;
}

static void elec_note(Voice *v, const Blk *b)
{
    int i;
    (void)b;
    if (!v->u[0]) v->u[0] = 0xE1EC0001u + v->idx * 6551u;
    if (v->env < 0.001f) {
        for (i = 0; i < 6; i++) v->ph[i] = 0.0f;
        for (i = 0; i < 12; i++) v->s[i] = 0.0f;
    }
    v->s[0] = 1.0f;   /* body decay */
    v->s[1] = 1.0f;   /* tine / hammer strike decay */
    v->s[2] = 0.0f;   /* feedback reed */
}

/* one render for every algorithm (eight always_inline copies cost about 25 KB of flash) */
static void elec_render_a(Voice *v, const Blk *b, f32 *out_l, f32 *out_r,
                          int n, int algo)
{
    const EAlgo *a = &EA[E.algo];
    f32 pitch = v->pitch + b->bend + E.wow;
    f32 f = mtof(pitch);
    f32 d0 = minf(f * b->isr, 0.44f);
    f32 key = clampf(1.0f - (pitch - 60.0f) * (1.0f / 54.0f), 0.42f, 1.35f);
    f32 vel = lerpf(1.0f, 0.30f + 0.90f * v->vel_f, b->vel_sens);
    f32 bark = a->bark * (0.25f + 1.45f * E.bark) * vel * lerpf(1.0f, 0.35f + 1.0f * v->menv, b->menv_amt);
    f32 tine = a->tine * (0.10f + 1.65f * E.tine) * vel;
    f32 body = v->s[0], hit = v->s[1], fb = v->s[2];
    f32 p0 = v->ph[0], p1 = v->ph[1], p2 = v->ph[2], p3 = v->ph[3], p4 = v->ph[4];
    f32 d1 = d0, d2 = d0, d3 = d0, d4 = d0;
    f32 amp = a->att > 0.0f ? sat(v->age / a->att) : 1.0f;
    f32 cut = 14000.0f * exp2fx(-E.warmth * 3.8f);
    f32 lpk = onepole_k(1.0f / (H1_TAU * cut), b->sr);
    f32 lpl = v->s[4], lpr = v->s[5];
    int i;

    switch (algo) {
    case 0: /* RHODES: fundamental + 1x bark + 7x/14x metallic tine */
    case 7: /* TAPE */
        d1 = minf(d0 * 1.0018f, 0.44f);
        d2 = minf(d0 * 7.0f, 0.44f);
        d3 = minf(d0 * 14.0f, 0.44f);
        break;
    case 1: /* WURLI: 2x reed overtone + 4x attack ping */
        d1 = minf(d0 * 2.0f, 0.44f);
        d2 = minf(d0 * 4.0f, 0.44f);
        d3 = minf(d0 * 9.0f, 0.44f);
        break;
    case 2: /* DX-EP: stereo detuned pairs + 14x crystal bell */
        d1 = minf(d0 * 1.0032f, 0.44f);
        d2 = minf(d0 * 1.0f, 0.44f);
        d3 = minf(d0 * 14.0f, 0.44f);
        d4 = minf(d0 * 1.0032f * 14.0f, 0.44f);
        break;
    case 3: /* CLAV: bright odd/even plucked comb */
        d1 = minf(d0 * 2.0f, 0.44f);
        d2 = minf(d0 * 3.0f, 0.44f);
        d3 = minf(d0 * 5.0f, 0.44f);
        break;
    case 4: /* ORGAN: 16', 8', 5-1/3', 4', 2' drawbars */
        d1 = minf(d0 * 0.5f, 0.44f);
        d2 = minf(d0 * 1.5f, 0.44f);
        d3 = minf(d0 * 2.0f, 0.44f);
        d4 = minf(d0 * 4.0f, 0.44f);
        break;
    case 5: /* FELT: slightly stretched acoustic piano partials */
        d1 = minf(d0 * 2.002f, 0.44f);
        d2 = minf(d0 * 3.008f, 0.44f);
        d3 = minf(d0 * 4.022f, 0.44f);
        d4 = minf(d0 * 1.0014f, 0.44f);
        break;
    case 6: /* VIBES: bar + 4x + 10x mallet chime */
        d1 = minf(d0 * 4.0f, 0.44f);
        d2 = minf(d0 * 9.92f, 0.44f);
        break;
    }

    for (i = 0; i < n; i++) {
        f32 xl = 0.0f, xr = 0.0f;
        switch (algo) {
        case 0: /* RHODES */
        case 7: /* TAPE */ {
            f32 mod = bark * key * (0.20f + 0.80f * body) * 0.55f * tsin(p0);
            f32 bell = tine * hit * (0.55f * tsin(p2) + 0.45f * hit * tsin(p3));
            xl = tsin(p0 + mod) + bell;
            xr = tsin(p1 + mod * 0.96f) + bell * 0.92f;
            if (algo == 7) {
                f32 nz = noise_s(&v->u[0]) * 0.015f * E.warmth;
                xl = softclip(xl * (1.0f + 0.6f * E.warmth)) + nz;
                xr = softclip(xr * (1.0f + 0.6f * E.warmth)) + nz;
            }
            break;
        }
        case 1: /* WURLI: asymmetric reed bite + 4x/9x metallic attack ping */ {
            f32 m = bark * (0.18f + 0.82f * hit) * 0.38f;
            f32 reed;
            fb = tsin(p0 + m * fb + 0.30f * body * bark * tsin(p1));
            reed = fb + 0.16f * E.bark * fb * fb + tine * 0.35f * hit * tsin(p2) + 0.14f * hit * hit * tsin(p3);
            xl = softclip(reed * (1.0f + 0.82f * E.bark));
            xr = softclip((reed - 0.08f * hit * tsin(p2)) * (1.0f + 0.78f * E.bark));
            break;
        }
        case 2: /* DX-EP */ {
            f32 m = bark * key * (0.28f + 0.72f * body) * 0.55f;
            xl = 0.72f * tsin(p0 + m * tsin(p2)) + 0.32f * tine * hit * tsin(p3 + 0.25f * tsin(p2));
            xr = 0.72f * tsin(p1 + m * tsin(p2)) + 0.32f * tine * hit * tsin(p4 + 0.25f * tsin(p2));
            break;
        }
        case 3: /* CLAV */ {
            f32 x = tsin(p0 + bark * (0.35f + 0.95f * body) * tsin(p1))
                  + 0.42f * body * tsin(p2 + 0.5f * bark * tsin(p0))
                  + 0.28f * tine * hit * tsin(p3);
            xl = softclip(x * 1.45f) * 0.78f;
            xr = xl;
            break;
        }
        case 4: /* ORGAN (B3 Drawbar + Key Click + Stereo Rotary) */ {
            f32 click = hit * hit * noise_s(&v->u[0]) * 0.25f * tine;
            f32 drw = 0.45f * tsin(p1) + 0.55f * tsin(p0) + (0.22f + 0.38f * E.bark) * tsin(p2)
                    + (0.18f + 0.35f * E.tine) * tsin(p3) + 0.22f * E.tine * tsin(p4) + click;
            drw = softclip(drw * (1.0f + 1.2f * E.bark)) * 0.72f;
            xl = drw * (0.85f + 0.15f * tsin(E.rot_ph + 0.15f));
            xr = drw * (0.85f - 0.15f * tsin(E.rot_ph + 0.15f));
            break;
        }
        case 5: /* FELT: low-passed wooden felt hammer thud + stretched partials */ {
            f32 h_lp = v->s[3] + 0.16f * (noise_s(&v->u[0]) - v->s[3]);
            f32 thump = hit * hit * h_lp * 0.44f * (0.35f + E.tine);
            f32 hi = (0.38f + 0.18f * E.bark) * body * tsin(p1) + 0.20f * body * body * tsin(p2) + 0.10f * hit * tsin(p3);
            v->s[3] = h_lp;
            xl = 0.76f * tsin(p0) + hi + thump;
            xr = 0.76f * tsin(p4) + hi * 0.94f + thump;
            break;
        }
        default: /* 6 VIBES: resonant aluminum bar + 4x/10x overtone + stereo motor shimmer */ {
            f32 ov = 0.34f * tine * hit * tsin(p1) + 0.14f * tine * hit * hit * tsin(p2);
            xl = tsin(p0) + ov;
            xr = tsin(p0 + 0.015f * E.trem) + ov * 0.92f;
            break;
        }
        }
        xl = xl * 0.65f * amp * (a->dec > 0.0f ? body : 1.0f) * E.trem_l;
        xr = xr * 0.65f * amp * (a->dec > 0.0f ? body : 1.0f) * E.trem_r;
        lpl += (xl - lpl) * lpk;
        lpr += (xr - lpr) * lpk;
        out_l[i] = lpl;
        out_r[i] = lpr;

        p0 += d0; if (p0 >= 1.0f) p0 -= 1.0f;
        p1 += d1; if (p1 >= 1.0f) p1 -= 1.0f;
        p2 += d2; if (p2 >= 1.0f) p2 -= 1.0f;
        p3 += d3; if (p3 >= 1.0f) p3 -= 1.0f;
        p4 += d4; if (p4 >= 1.0f) p4 -= 1.0f;
    }

    v->ph[0] = p0; v->ph[1] = p1; v->ph[2] = p2; v->ph[3] = p3; v->ph[4] = p4;
    v->s[0] = body * (key < 1.0f ? E.kd * (1.0f - (1.0f - E.kd) * (1.0f - key) * 2.5f) : E.kd);
    {
        f32 h_tau = algo == 4 ? 0.015f : (algo == 5 ? 0.06f : (0.09f + 0.07f * key));
        v->s[1] = hit * (1.0f - onepole_k(h_tau, b->sr / (f32)n));
    }
    v->s[2] = fb;
    v->s[4] = lpl; v->s[5] = lpr;
}

static void elec_render(Voice *v, const Blk *b, f32 *out_l, f32 *out_r, int n)
{
    elec_render_a(v, b, out_l, out_r, n, E.algo & 7);
}

/* ---- 4-Color Graphic: Mechanical Tines, B3 Drawbars, Vibes Fans & Stereo Tremolo -- */
static f32 tine_vib[12], tine_vel[12];
static uint32_t el_seen[NVOICE];

static void elec_draw(const Vis *vis)
{
    f32 dt = vis->dt, bark = vis->k[0], tine = vis->k[1], trem = vis->k[2], warm = vis->k[3];
    f32 lev = minf(1.0f, vis->level * 3.0f);
    int algo = vis->algo & 7, i;

    if (vis->fresh) {
        for (i = 0; i < 12; i++) tine_vib[i] = tine_vel[i] = 0.0f;
        for (i = 0; i < NVOICE; i++) el_seen[i] = vis->v[i].serial;
    }
    for (i = 0; i < NVOICE; i++) {
        if (vis->v[i].on && vis->v[i].serial != el_seen[i]) {
            int pc = ((int)(vis->v[i].pitch + 0.5f) % 12 + 12) % 12;
            el_seen[i] = vis->v[i].serial;
            tine_vel[pc] += 18.0f + 22.0f * bark;
        }
    }
    for (i = 0; i < 12; i++)
        spring_z(&tine_vib[i], &tine_vel[i], 0.0f, 26.0f + (f32)i * 1.4f, 0.18f, dt);

    /* Harp frame rail (Slate) */
    g_color(GC_SLATE);
    g_line(22.0f, 184.0f, 218.0f, 184.0f, 2.5f, 0.55f);
    g_line(22.0f, 54.0f, 218.0f, 54.0f, 1.2f, 0.28f);

    /* Warmth glow arc behind pickup rail (Orange = K4 WARMTH) */
    g_color(GC_ORANGE);
    g_glow(120.0f, 128.0f, 46.0f + 56.0f * warm, 0.24f + 0.42f * warm * 0.7f);
    g_arc(120.0f, 184.0f, 18.0f + 10.0f * warm, 0.5f, 1.0f, 2.2f, 0.6f + 0.4f * warm);

    if (algo == 4) {
        /* ORGAN: 9 Sliding B3 Drawbars + Spinning Leslie Rotary Horn */
        static const int DBC[9] = {GC_ORANGE, GC_ORANGE, GC_WHITE, GC_WHITE, GC_BLUE, GC_WHITE, GC_GREEN, GC_GREEN, GC_WHITE};
        static const f32 DB0[9] = {0.75f, 0.55f, 0.85f, 0.65f, 0.45f, 0.50f, 0.35f, 0.30f, 0.40f};
        f32 r_ang = vis->t * (1.2f + 4.8f * trem);
        for (i = 0; i < 9; i++) {
            f32 x = 32.0f + (f32)i * 22.0f;
            f32 pull = clampf(DB0[i] * (i < 4 ? (0.7f + 0.4f * warm) : (0.35f + 0.85f * (i & 1 ? bark : tine))), 0.12f, 0.96f);
            f32 act = sat(absf(tine_vib[i]) * 0.4f);
            f32 y = 64.0f + pull * 102.0f + act * 5.0f;
            g_color(GC_SLATE);
            g_line(x, 60.0f, x, 178.0f, 1.4f, 0.32f);
            g_color(DBC[i]);
            g_line(x, 60.0f, x, y, 4.2f, 0.82f + 0.18f * act);
            g_rect(x - 6.0f, y - 4.0f, 12.0f, 8.0f, 0.95f);
        }
        /* Spinning Leslie horn above drawbars */
        g_color(GC_GOLD);
        g_ellipse(120.0f, 42.0f, 38.0f, 7.0f, 1.3f, 0.45f);
        g_line(120.0f - cos_turns(r_ang) * 34.0f, 42.0f - sin_turns(r_ang) * 5.5f,
               120.0f + cos_turns(r_ang) * 34.0f, 42.0f + sin_turns(r_ang) * 5.5f, 2.4f, 0.95f);
        g_disc(120.0f + cos_turns(r_ang) * 34.0f, 42.0f + sin_turns(r_ang) * 5.5f, 3.4f, 1.0f);
        g_disc(120.0f - cos_turns(r_ang) * 34.0f, 42.0f - sin_turns(r_ang) * 5.5f, 3.4f, 1.0f);
        g_color(GC_WHITE);
        return;
    }

    /* 12 vibrating vertical tines / resonator tubes & pickup coils (Blue = K1 BARK, Green = K2 TINE) */
    for (i = 0; i < 12; i++) {
        f32 x = 32.0f + (f32)i * 16.0f;
        f32 len = 112.0f - (f32)i * 4.5f;
        f32 ytop = 182.0f - len;
        f32 bend = clampf(tine_vib[i] * (0.6f + 0.8f * bark), -10.0f, 10.0f);
        f32 act = sat(absf(tine_vib[i]) * 0.45f);
        if (algo == 6) {
            /* VIBES: graduated tubular resonator + spinning motor pulley vane at top */
            f32 vane = absf(cos_turns(vis->t * (1.0f + 2.8f * trem) + (f32)i * 0.08f));
            g_color(GC_SLATE);
            g_box(x - 5.0f, ytop, 10.0f, len, 1.2f, 0.38f + 0.45f * act);
            g_color(GC_BLUE);
            g_line(x + bend * 0.5f, 180.0f, x + bend * 0.5f, ytop + 8.0f, 2.4f, 0.65f + 0.35f * act);
            g_color(GC_GREEN);
            g_ellipse(x, ytop + 5.0f, 4.5f, 1.2f + 3.4f * vane, 1.6f, 0.85f);
        } else {
            /* Tonebar + vibrating tine rod + magnetic pickup coil */
            g_color(GC_SLATE);
            g_line(x - 3.2f, 182.0f, x - 3.2f, ytop + 14.0f, 2.0f, 0.32f);
            g_color(GC_BLUE);
            g_line(x, 182.0f, x + bend * 0.4f, ytop + len * 0.5f, 1.6f + 1.0f * act, 0.65f + 0.35f * act);
            g_line(x + bend * 0.4f, ytop + len * 0.5f, x + bend, ytop, 1.8f + 1.4f * act, 0.85f + 0.15f * act);
            /* magnetic pickup lug above tine tip (Green) */
            g_color(GC_GREEN);
            g_rect(x - 2.5f, ytop - 8.0f, 5.0f, 5.0f, 0.55f + 0.45f * tine);
            g_disc(x + bend, ytop, 2.2f + 2.8f * tine + 2.2f * act, 0.75f + 0.25f * act);
        }
        /* Hammer strike block at bottom */
        g_color(act > 0.12f ? GC_ORANGE : GC_SLATE);
        g_rect(x - 3.5f, 185.0f - 6.0f * act, 7.0f, 4.0f, 0.28f + 0.72f * act);
    }

    /* Dual-trace stereo autopan tremolo wave across the upper rail (White L / Cyan R = K3 TREM) */
    {
        f32 xy[2 * 48], xy2[2 * 48];
        f32 amp = 2.0f + 14.0f * trem * (0.4f + 0.6f * lev);
        for (i = 0; i < 48; i++) {
            f32 u = (f32)i * (1.0f / 47.0f);
            f32 w = amp * sin_turns(u * (1.5f + 2.5f * trem) + vis->t * (1.2f + 2.4f * trem));
            xy[2 * i]      = 24.0f + u * 192.0f;
            xy[2 * i + 1]  = 43.0f + w;
            xy2[2 * i]     = 24.0f + u * 192.0f;
            xy2[2 * i + 1] = 43.0f - w * 0.85f;
        }
        g_color(GC_CYAN);
        g_poly(xy2, 48, 0, 1.4f, 0.42f + 0.35f * trem);
        g_color(GC_WHITE);
        g_poly(xy, 48, 0, 2.0f, 0.68f + 0.32f * trem);
    }
    g_color(GC_WHITE);
}

const InstDef inst_electric = {
    "ELECTRIC",
    {68, 227, 126},
    {"BARK", "TINE", "TREM", "WARMTH"},
    {"RHODES", "WURLI", "DX-EP", "CLAV", "ORGAN", "FELT", "VIBES", "TAPE"},
    {
        {42, 48, 30, 22},            /* K1..K4 defaults */
        0,                           /* algo: RHODES */
        {0, 50, 72, 36},             /* Amp ADSR */
        {55, 34, 75, 15},            /* Mod/Filter Env */
        {0, 42, 0, 1},               /* LFO */
        {0, 3, 1, 70},               /* Arp */
        {0, 50, 42}                  /* Mix */
    },
    0.40f,
    elec_block, elec_note, elec_render, 0, elec_draw, 0
};