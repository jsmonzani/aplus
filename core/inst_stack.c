/* A22 - STACK: Polyphonic FM-Stacked Chords & Lush Stereo Pads.
 *
 * Four stacked harmonic / chord oscillators per voice with shared FM shimmer modulation,
 * organic analog tape pitch drift, stereo unison fan-out, dual-formant choir mode, and
 * a warm post-stack stereo ZDF filter driven by Mod-Env & Velocity.
 *
 *   K1 (BLUE)   SPREAD  micro-detune & stereo unison width (tight mono -> widescreen)
 *   K2 (GREEN)  CHORD   interval / chord voicing stack (Unison, 5th, m7, M7, Sus, m9, M9)
 *   K3 (WHITE)  DRIFT   analog tape pitch wander + FM shimmer overtone depth
 *   K4 (ORANGE) GLOW    post-stack warm resonant ZDF LPF + airy bloom */
#include "internal.h"
#include "dsp.h"

/* 8 musical chord / harmonic stacks (semitones above root for voices 2, 3, 4) */
static const f32 CL_CHORD[8][3] = {
    { 0.0f,  0.0f, 12.0f},   /* 0: unison + octave */
    { 7.0f, 12.0f, 19.0f},   /* 1: open 5th + oct + 12th */
    { 3.0f,  7.0f, 10.0f},   /* 2: minor 7 */
    { 4.0f,  7.0f, 11.0f},   /* 3: major 7 */
    { 5.0f,  7.0f, 12.0f},   /* 4: sus4 */
    { 3.0f,  7.0f, 14.0f},   /* 5: minor 9 */
    { 4.0f,  7.0f, 14.0f},   /* 6: major 9 */
    { 7.0f, 14.0f, 19.0f},   /* 7: ethereal stack (5th + 9th + 12th) */
};

/* Vowel formant frequencies (Hz) for CHOIR mode (OO, OH, AH, EH, EE) */
static const f32 VOWEL_F1[5] = {300.0f, 480.0f, 720.0f, 540.0f, 290.0f};
static const f32 VOWEL_F2[5] = {860.0f, 920.0f, 1220.0f, 1840.0f, 2280.0f};

static struct {
    int algo;
    f32 spread, chord, drift, glow;
    f32 w_lfo1, w_lfo2;
    f32 cut, q;
} C;
static f32 cl_ph1, cl_ph2, cl_t_was;

static void cluster_block(const Blk *b)
{
    f32 dt = b->step;
    if (b->time < cl_t_was) cl_ph1 = cl_ph2 = 0.0f;
    cl_t_was = b->time;
    cl_ph1 = fractf(cl_ph1 + (0.31f + 0.55f * b->k[2]) * dt);
    cl_ph2 = fractf(cl_ph2 + (0.73f + 0.90f * b->k[2]) * dt);

    C.algo   = b->algo & 7;
    C.spread = b->k[0];
    C.chord  = b->k[1];
    C.drift  = b->k[2];
    C.glow   = b->k[3];
    C.w_lfo1 = tsin(cl_ph1);
    C.w_lfo2 = tsin(cl_ph2);
    C.cut    = 260.0f * exp2fx(C.glow * 6.0f);       /* 260 Hz .. 16.6 kHz */
    C.q      = 0.72f + 1.45f * (1.0f - C.glow) * (0.3f + 0.7f * C.drift);
}

static void cluster_note(Voice *v, const Blk *b)
{
    int i;
    (void)b;
    if (v->env < 0.001f) {
        for (i = 0; i < 6; i++) v->ph[i] = fractf(v->r1 * (f32)(i + 1) * 0.27f + v->r2 * 0.19f);
        for (i = 0; i < 16; i++) v->s[i] = 0.0f;
    }
    v->s[13] = 1.0f;   /* initial bloom / punch */
}

/* one render for every algorithm (eight always_inline copies cost about 25 KB of flash) */
static void cluster_render_a(Voice *v, const Blk *b, f32 *out_l, f32 *out_r,
                             int n, int algo)
{
    int i;
    f32 pitch = v->pitch + b->bend;
    f32 vel = lerpf(1.0f, 0.35f + 0.85f * v->vel_f, b->vel_sens);
    f32 menv = lerpf(1.0f, 0.25f + 1.15f * v->menv, b->menv_amt);
    f32 det_st = (0.04f + 0.48f * C.spread * C.spread);
    f32 drift_amt = C.drift * (algo == 7 ? 0.34f : 0.14f);
    f32 sh_idx = (0.08f + 0.65f * C.drift) * vel * menv * clampf(exp2fx((62.0f - pitch) * (1.0f / 28.0f)), 0.30f, 1.40f);
    f32 swell = (algo == 3 || algo == 5) ? (0.35f + 0.65f * sat(v->age * 3.5f)) : 1.0f;
    f32 semi[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    f32 d[4], dm, dd;
    f32 p0 = v->ph[0], p1 = v->ph[1], p2 = v->ph[2], p3 = v->ph[3], pm = v->ph[4], pd = v->ph[5];
    f32 wid = 0.15f + 0.85f * C.spread;
    f32 fc;
    Svf fl, fm1, fm2;

    if (algo == 5) pitch -= 12.0f;  /* DRONE sits deep */

    /* Voice interval arrangement per algorithm */
    if (algo == 1 || algo == 7) {
        /* CHORD / DREAM: discrete musical chord stack on K2 */
        int ci = iclamp((int)(C.chord * 7.99f), 0, 7);
        semi[1] = CL_CHORD[ci][0];
        semi[2] = CL_CHORD[ci][1];
        semi[3] = CL_CHORD[ci][2];
    } else if (algo == 0 || algo == 3) {
        /* CLOUD / GLASS: smoothly spread unison -> 5th / octave harmonic cluster */
        f32 s = C.chord;
        semi[1] = (s > 0.35f) ? 7.0f * smooth01((s - 0.35f) * 1.54f) : 0.0f;
        semi[2] = 12.0f * smooth01(s);
        semi[3] = (s > 0.60f) ? 12.0f + 7.0f * smooth01((s - 0.60f) * 2.5f) : (12.0f * s);
    } else if (algo == 2) {
        /* JUNO: sub-octave + 3 detuned saws/pulses */
        semi[0] = -12.0f;
        semi[1] = 0.0f;
        semi[2] = (C.chord > 0.5f) ? 12.0f : 0.0f;
        semi[3] = 0.0f;
    } else if (algo == 4) {
        /* ORGAN: harmonic drawbar cluster 8', 4', 2-2/3', 2' */
        semi[0] = 0.0f;
        semi[1] = 12.0f;
        semi[2] = 19.0195f;
        semi[3] = 24.0f;
    } else if (algo == 5) {
        /* DRONE: root + 5th + octave + sub-fifth shimmer */
        semi[0] = 0.0f;
        semi[1] = 7.0f;
        semi[2] = 12.0f;
        semi[3] = 12.0f + (C.chord > 0.5f ? 7.0f : 0.0f);
    } else {
        /* CHOIR: stacked root + octave + unison detuned */
        semi[0] = 0.0f;
        semi[1] = 0.0f;
        semi[2] = 12.0f;
        semi[3] = (C.chord > 0.65f) ? 7.0f : 0.0f;
    }

    {
        /* Per-voice slow organic pitch drift */
        f32 dr_v = drift_amt * (0.6f * tsin(pd) + 0.4f * C.w_lfo1);
        f32 f_base = mtof(pitch + dr_v);
        f32 det_off[4] = {-0.85f, -0.28f, 0.31f, 0.88f};
        int k;
        for (k = 0; k < 4; k++) {
            f32 fk = f_base * exp2fx((semi[k] + det_off[k] * det_st) * (1.0f / 12.0f));
            d[k] = minf(fk * b->isr, 0.43f);
        }
        dm = minf(f_base * (algo == 3 ? 3.0f : (algo == 6 ? 2.0f : 2.0f)) * b->isr, 0.43f);
        dd = (0.18f + 0.42f * v->r2 + 0.40f * C.drift) * b->isr;
    }

    fc = clampf(C.cut * exp2fx(v->menv * b->menv_amt * 2.5f + (v->vel_f - 0.5f) * b->vel_sens * 1.2f), 110.0f, 18000.0f);
    svf_set(&fl, fc, C.q, b->isr);

    if (algo == 6) {
        f32 vx = clampf(C.chord * 4.0f + v->menv * b->menv_amt * 0.8f, 0.0f, 3.99f);
        int vi = (int)vx;
        f32 vf = vx - (f32)vi;
        f32 f1 = lerpf(VOWEL_F1[vi], VOWEL_F1[vi + 1], vf);
        f32 f2 = lerpf(VOWEL_F2[vi], VOWEL_F2[vi + 1], vf);
        svf_set(&fm1, f1, 4.2f, b->isr);
        svf_set(&fm2, f2, 5.0f, b->isr);
    }

    for (i = 0; i < n; i++) {
        f32 mod = tsin(pm);
        f32 o[4], sl, sr;

        switch (algo) {
        case 0: /* CLOUD: warm FM-shimmer sine + triangle cloud */
            o[0] = 0.65f * tsin(p0 + mod * sh_idx * 0.35f) + 0.35f * tri_wave(p0);
            o[1] = 0.60f * tsin(p1 + mod * sh_idx * 0.45f) + 0.40f * tri_wave(p1);
            o[2] = tsin(p2 + mod * sh_idx * 0.60f) * (0.45f + 0.55f * C.glow);
            o[3] = tsin(p3 + mod * sh_idx * 0.70f) * (0.30f + 0.55f * C.glow);
            break;
        case 1: /* CHORD: lush 4-note PolyBLEP saw+sine blend */
        {
            f32 br = 0.25f + 0.70f * C.glow;
            o[0] = lerpf(tsin(p0), saw_bl(p0, d[0]), br);
            o[1] = lerpf(tsin(p1), saw_bl(p1, d[1]), br) * 0.85f;
            o[2] = lerpf(tsin(p2), saw_bl(p2, d[2]), br) * 0.82f;
            o[3] = lerpf(tsin(p3), saw_bl(p3, d[3]), br) * 0.78f;
            break;
        }
        case 2: /* JUNO: fat sub + PWM + dual detuned PolyBLEP saws */
        {
            f32 pw = clampf(0.5f - 0.35f * C.chord + 0.12f * C.w_lfo2, 0.10f, 0.90f);
            o[0] = 0.65f * pulse_bl(p0, d[0], 0.5f);
            o[1] = saw_bl(p1, d[1]);
            o[2] = pulse_bl(p2, d[2], pw);
            o[3] = saw_bl(p3, d[3]);
            break;
        }
        case 3: /* GLASS: crystalline FM overtone shimmer */
            o[0] = tsin(p0 + mod * sh_idx * 0.85f);
            o[1] = tsin(p1 - mod * sh_idx * 0.75f);
            o[2] = tsin(p2 + mod * sh_idx * 1.15f) * 0.75f;
            o[3] = tsin(p3 + tsin(p2) * sh_idx * 0.95f) * 0.65f;
            break;
        case 4: /* ORGAN: vintage stacked harmonic drawbars + FM click warmth */
        {
            f32 w2 = 0.30f + 0.70f * C.chord;
            o[0] = tsin(p0 + 0.18f * mod * sh_idx);
            o[1] = tsin(p1 + 0.14f * mod * sh_idx) * 0.78f;
            o[2] = tsin(p2) * 0.55f * w2;
            o[3] = tsin(p3) * 0.45f * w2;
            break;
        }
        case 5: /* DRONE: dense evolving FM monolith */
            o[0] = 0.6f * saw_bl(p0, d[0]) + 0.4f * tsin(p0 + mod * sh_idx * 0.6f);
            o[1] = tsin(p1 + o[0] * sh_idx * 0.45f);
            o[2] = saw_bl(p2, d[2]) * 0.65f;
            o[3] = tsin(p3 + mod * sh_idx * 0.90f) * 0.65f;
            break;
        case 6: /* CHOIR: rich saw/FM stack fed into dual vowel formant filters + airy singer presence */
        {
            f32 raw_l = 0.45f * saw_bl(p0, d[0]) + 0.35f * saw_bl(p2, d[2]) + 0.25f * tsin(p1 + mod * sh_idx * 0.5f);
            f32 raw_r = 0.45f * saw_bl(p1, d[1]) + 0.35f * saw_bl(p3, d[3]) + 0.25f * tsin(p0 + mod * sh_idx * 0.5f);
            f32 lp1, lp2, b1l, b2l, b1r, b2r, hpl, hpr;
            SVF_TICK(fm1, &v->s[8],  raw_l, lp1, b1l); (void)lp1;
            SVF_TICK(fm2, &v->s[10], raw_l, lp2, b2l);
            hpl = raw_l - lp2 - b2l * fm2.k;
            SVF_TICK(fm1, &v->s[0],  raw_r, lp1, b1r); (void)lp1;
            SVF_TICK(fm2, &v->s[2],  raw_r, lp2, b2r);
            hpr = raw_r - lp2 - b2r * fm2.k;
            o[0] = (b1l * 0.90f + b2l * 0.72f + hpl * (0.10f + 0.16f * C.glow) + raw_l * 0.12f);
            o[1] = (b1r * 0.90f + b2r * 0.72f + hpr * (0.10f + 0.16f * C.glow) + raw_r * 0.12f);
            o[2] = o[0] * 0.45f;
            o[3] = o[1] * 0.45f;
            break;
        }
        default: /* DREAM: warm nostalgic tape-wobble Lo-Fi chords */
            o[0] = tape_sat((tri_wave(p0) * 0.7f + saw_bl(p0, d[0]) * 0.3f) * (1.0f + 0.5f * C.drift));
            o[1] = tape_sat((tri_wave(p1) * 0.7f + saw_bl(p1, d[1]) * 0.3f) * (1.0f + 0.5f * C.drift)) * 0.85f;
            o[2] = tape_sat(tsin(p2 + mod * sh_idx * 0.35f) * 1.15f) * 0.80f;
            o[3] = tape_sat(tri_wave(p3) * 1.1f) * 0.72f;
            break;
        }

        /* Stereo fan-out controlled by K1 SPREAD */
        {
            f32 mid = 0.28f * (o[0] + o[1] + o[2] + o[3]);
            f32 side = 0.32f * ((o[0] - o[3]) * 0.72f + (o[1] - o[2]) * 0.38f) * wid;
            f32 lp, bp;
            sl = mid + side;
            sr = mid - side;
            SVF_TICK(fl, &v->s[4], sl, lp, bp); (void)bp; sl = lp;
            SVF_TICK(fl, &v->s[6], sr, lp, bp); (void)bp; sr = lp;
        }

        out_l[i] = sl * swell;
        out_r[i] = sr * swell;

        p0 += d[0]; if (p0 >= 1.0f) p0 -= 1.0f;
        p1 += d[1]; if (p1 >= 1.0f) p1 -= 1.0f;
        p2 += d[2]; if (p2 >= 1.0f) p2 -= 1.0f;
        p3 += d[3]; if (p3 >= 1.0f) p3 -= 1.0f;
        pm += dm;   if (pm >= 1.0f) pm -= 1.0f;
        pd += dd;   if (pd >= 1.0f) pd -= 1.0f;
    }

    v->ph[0] = p0; v->ph[1] = p1; v->ph[2] = p2; v->ph[3] = p3; v->ph[4] = pm; v->ph[5] = pd;
}

static void cluster_render(Voice *v, const Blk *b, f32 *out_l, f32 *out_r, int n)
{
    cluster_render_a(v, b, out_l, out_r, n, C.algo & 7);
}

/* ---- 4-Color Vector Picture: Orbiting 4-Voice Harmonic Constellation ------------- */
static f32 cl_rot, cl_lev, cl_kick, cl_kickv;
static uint32_t cl_seen;

static void cluster_draw(const Vis *vis)
{
    f32 dt = vis->dt, spread = vis->k[0], chord = vis->k[1], drift = vis->k[2], glow = vis->k[3];
    f32 cx = 120.0f, cy = 112.0f;
    f32 sat_x[4], sat_y[4], poly[8];
    static const int ORBC[4] = {GC_BLUE, GC_GREEN, GC_WHITE, GC_ORANGE};
    int algo = vis->algo & 7, i;

    if (vis->fresh) {
        cl_seen = vis->notes;
        cl_lev = cl_kick = cl_kickv = 0.0f;
    }
    if (vis->notes != cl_seen) {
        cl_seen = vis->notes;
        cl_kickv += 4.2f;
    }
    spring_z(&cl_kick, &cl_kickv, 0.0f, 18.0f, 0.42f, dt);
    cl_lev += (minf(1.0f, vis->level * 3.5f) - cl_lev) * minf(1.0f, dt * 14.0f);
    cl_rot = fractf(cl_rot + dt * (0.05f + 0.16f * drift + 0.12f * cl_lev));

    /* Background glow aura (Orange = K4 GLOW) */
    g_color(GC_ORANGE);
    g_glow(cx, cy, 30.0f + 42.0f * glow, 0.25f + 0.35f * glow);

    /* Subtle reference crosshairs */
    g_color(GC_SLATE);
    g_ring(cx, cy, 74.0f, 1.0f, 0.22f);
    g_line(cx - 82.0f, cy, cx + 82.0f, cy, 1.0f, 0.14f);
    g_line(cx, cy - 74.0f, cx, cy + 74.0f, 1.0f, 0.14f);

    /* 4 tilted harmonic orbits & satellite nodes */
    for (i = 0; i < 4; i++) {
        f32 fi = (f32)i;
        f32 rx = 28.0f + (12.0f + 34.0f * spread) * (0.45f + 0.22f * fi) + 4.0f * cl_kick;
        f32 ry = rx * (0.38f + 0.48f * glow + 0.04f * (f32)(algo & 3));
        f32 wob = drift * 6.5f * sin_turns(cl_rot * (1.5f + 0.4f * fi) + fi * 0.25f);
        f32 ang = cl_rot * (1.0f + 0.25f * fi) + fi * (0.14f + 0.18f * chord) + (f32)algo * 0.07f;
        f32 sx = cx + cos_turns(ang) * rx + wob;
        f32 sy = cy + sin_turns(ang) * ry + wob * 0.5f;
        sat_x[i] = sx;
        sat_y[i] = sy;
        poly[2 * i] = sx;
        poly[2 * i + 1] = sy;

        g_color(ORBC[i]);
        g_ellipse(cx, cy, rx, ry, 1.3f, 0.38f + 0.25f * cl_lev);
        /* radial spoke to center */
        g_line(cx, cy, sx, sy, 1.1f, 0.22f + 0.22f * cl_lev);
    }

    /* Shaded Chord Constellation Polygon (vertical gradient fill + bright outline) */
    g_color(GC_CYAN);
    g_fillv(poly, 4, cy - 52.0f, 0.26f + 0.18f * cl_lev, cy + 52.0f, 0.04f);
    g_poly(poly, 4, 1, 1.7f, 0.55f + 0.38f * cl_lev);

    /* Center harmonic core + glowing satellite nodes */
    g_color(GC_WHITE);
    g_ring(cx, cy, 9.0f + 6.0f * cl_lev + 2.5f * cl_kick, 2.0f, 0.85f);
    g_disc(cx, cy, 4.0f + 4.0f * glow, 0.90f);

    for (i = 0; i < 4; i++) {
        f32 nr = 5.5f + 3.5f * vis->k[i] + 3.0f * cl_lev;
        g_color(ORBC[i]);
        g_glow(sat_x[i], sat_y[i], nr * 2.2f, 0.25f + 0.25f * cl_lev);
        g_disc(sat_x[i], sat_y[i], nr, 0.92f);
        g_ring(sat_x[i], sat_y[i], nr + 3.5f, 1.3f, 0.50f);
    }
    g_color(GC_WHITE);
}

const InstDef inst_stack = {
    "STACK",
    {76, 224, 122},
    {"SPREAD", "CHORD", "DRIFT", "GLOW"},
    {"CLOUD", "CHORD", "JUNO", "GLASS", "ORGAN", "DRONE", "CHOIR", "DREAM"},
    {
        {56, 35, 34, 66},            /* K1..K4 defaults */
        0,                           /* algo: CLOUD */
        {18, 52, 82, 54},            /* lush pad Amp ADSR */
        {55, 48, 60, 10},            /* Mod/Filter Env */
        {0, 28, 0, 3},               /* LFO */
        {0, 3, 1, 75},               /* Arp */
        {0, 50, 48}                  /* Mix: lush FX send */
    },
    0.34f,
    cluster_block, cluster_note, cluster_render, 0, cluster_draw, 0
};