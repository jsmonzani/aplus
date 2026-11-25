/* h1 - COZY: lo-fi keys. An electric piano with tines, one with reeds, a felted upright, a tape
 * flute, vibes, all of them played back off a record that has been loved too much. The picture is
 * the room it is playing in: a window with weather in it and a town beyond, a cat on the sill with
 * its back to you, a record going round under a hanging lamp, a mug that is still warm. The town's
 * windows light up with the notes you hold.
 *
 *   K1 DUST    crackle and hiss                         motes drift in the lamplight
 *   K2 WOW     the record is warped                     the record wobbles, the arm rides it
 *   K3 MUFFLE  a blanket over the speaker               the curtains close
 *   K4 SWAY    tremolo, side to side                    the lamp swings, and the light with it
 */
#include "internal.h"
#include "dsp.h"

/* ---- sound ------------------------------------------------------------------------------ */
typedef struct CAlgo {
    f32 dec;                     /* seconds the note takes to die while held; 0 = it does not */
    f32 att;
    f32 idx;                     /* how much bite */
} CAlgo;
static const CAlgo CA[NALGO] = {
    /* TINE  */ {2.4f, 0.0f, 0.9f},
    /* REED  */ {1.3f, 0.0f, 1.3f},
    /* FELT  */ {2.8f, 0.004f, 0.3f},
    /* TAPE  */ {0.0f, 0.07f, 0.4f},
    /* VIBES */ {3.4f, 0.0f, 0.4f},
    /* DUSK  */ {0.0f, 0.16f, 0.5f},
    /* TOY   */ {0.75f, 0.0f, 0.8f},
    /* RADIO */ {1.9f, 0.0f, 1.0f},
};

static struct {
    int algo;
    f32 dust, wowk, muffle, sway, wow, kd, trem;
} C;
static f32 wow_ph, flut_ph, trem_ph, wander, wander_t, lp_l[2], lp_r[2], pop_e, pop_lp, hiss_lp, act, radio_s[2], hum_ph;
static uint32_t dust_rng = 0xD057D057u;
static volatile int sounding;

static void cozy_block(const Blk *b)
{
    f32 dt = b->step;
    C.algo = b->algo;
    C.dust = b->k[0];
    C.wowk = b->k[1];
    C.muffle = b->k[2];
    C.sway = b->k[3];
    wow_ph = fractf(wow_ph + 0.62f * dt);
    flut_ph = fractf(flut_ph + 5.7f * dt);
    if (dt > 0.0f) {
        wander_t += (rng_s(&dust_rng) - wander_t) * 0.01f;
        wander += (wander_t - wander) * 0.02f;
    }
    C.wow = C.wowk * C.wowk * (0.5f * tsin(wow_ph) + 0.09f * tsin(flut_ph) + 0.5f * wander);
    trem_ph = fractf(trem_ph + (3.2f + 2.4f * C.sway) * dt);
    C.trem = tsin(trem_ph);
    C.kd = CA[b->algo].dec > 0.0f ? 1.0f - onepole_k(CA[b->algo].dec, b->sr / (f32)CTRL) : 1.0f;
}

static void cozy_note(Voice *v, const Blk *b)
{
    int i;
    (void)b;
    if (!v->u[0]) v->u[0] = 0xC0270001u + v->idx * 7919u;
    if (v->env < 0.001f)
        for (i = 0; i < 6; i++)
            v->ph[i] = 0.0f;
    v->s[2] = 1.0f;                              /* the note's own dying away */
    v->s[3] = 1.0f;                              /* the hammer, the tine's ping */
    v->s[4] = 0.0f;
}

static void cozy_render(Voice *v, const Blk *b, f32 *out_l, f32 *out_r, int n)
{
    const CAlgo *a = &CA[C.algo];
    f32 pitch = v->pitch + b->bend + C.wow, f = mtof(pitch), d0 = minf(f * b->isr, 0.45f), body = v->s[2], hit = v->s[3];
    f32 p0 = v->ph[0], p1 = v->ph[1], p2 = v->ph[2], p3 = v->ph[3], amp = a->att > 0.0f ? sat(v->age / a->att) : 1.0f, fb = v->s[4];
    f32 key = clampf(1.0f - (pitch - 60.0f) * (1.0f / 60.0f), 0.5f, 1.4f);       /* high notes are thinner and shorter */
    f32 r1 = 1.0f, r2 = 14.0f, r3 = 4.0f, d1, d2, d3;
    int algo = C.algo, i;

    switch (algo) {                              /* where the other partials sit */
    default: break;
    case 1: r1 = 2.0f; break;
    case 2: r1 = 2.003f; r2 = 3.011f; r3 = 4.03f; break;
    case 3: r1 = 2.0f; break;
    case 4: r1 = 4.0f; r2 = 9.9f; break;
    case 5: r1 = 1.006f; r2 = 0.5f; break;
    case 6: r1 = 5.43f; r2 = 9.13f; break;
    }
    d1 = minf(d0 * r1, 0.45f); d2 = minf(d0 * r2, 0.45f); d3 = minf(d0 * r3, 0.45f);

    for (i = 0; i < n; i++) {
        f32 x;
        switch (algo) {
        default:
        case 0:                                  /* TINE: a soft bar, and a hard little bell that is gone at once */
            x = tsin(p0 + a->idx * (0.25f + 0.75f * body) * key * 0.5f * tsin(p1)) + 0.3f * hit * tsin(p2);
            break;
        case 1:                                  /* REED: it barks when you dig in */
            fb = tsin(p0 + a->idx * (0.2f + 0.8f * hit) * 0.3f * fb + 0.25f * body * tsin(p1));
            x = fb;
            break;
        case 2:                                  /* FELT: four partials going flat of each other, and the thump of the hammer */
            x = tsin(p0) + 0.5f * body * tsin(p1) + 0.28f * body * body * tsin(p2) + 0.12f * hit * tsin(p3)
                + 0.5f * hit * hit * noise_s(&v->u[0]) * 0.25f;
            break;
        case 3:                                  /* TAPE: a flute, more air than pipe */
            v->s[5] += (noise_s(&v->u[0]) - v->s[5]) * 0.15f;
            x = tsin(p0 + 0.12f * tsin(p1)) + 0.3f * tsin(p1) + v->s[5] * (0.5f + 1.5f * hit);
            break;
        case 4:                                  /* VIBES: a bar and its fourth harmonic, under a turning fan */
            x = (tsin(p0) + 0.35f * hit * tsin(p1) + 0.12f * hit * hit * tsin(p2)) * (1.0f - 0.35f * (0.5f + 0.5f * C.trem));
            break;
        case 5:                                  /* DUSK: two soft triangles leaning on each other */
            x = (absf(2.0f * p0 - 1.0f) * 2.0f - 1.0f) * 0.9f + (absf(2.0f * p1 - 1.0f) * 2.0f - 1.0f) * 0.7f + 0.3f * tsin(p2);
            break;
        case 6:                                  /* TOY: rods, not strings: bright, out of tune with itself, short */
            x = tsin(p0) + 0.55f * body * tsin(p1) + 0.3f * hit * tsin(p2);
            break;
        case 7:
            x = tsin(p0 + a->idx * (0.3f + 0.7f * body) * 0.45f * tsin(p1));
            break;
        }
        out_l[i] = x * 0.6f * amp * (a->dec > 0.0f ? body : 1.0f);
        out_r[i] = x * 0.6f * amp * (a->dec > 0.0f ? body : 1.0f);
        p0 += d0; if (p0 >= 1.0f) p0 -= 1.0f;
        p1 += d1; if (p1 >= 1.0f) p1 -= 1.0f;
        p2 += d2; if (p2 >= 1.0f) p2 -= 1.0f;
        p3 += d3; if (p3 >= 1.0f) p3 -= 1.0f;
    }
    v->ph[1] = p1; v->ph[2] = p2; v->ph[3] = p3;
    v->ph[0] = p0;
    v->s[2] = body * (key < 1.0f ? C.kd * (1.0f - (1.0f - C.kd) * (1.0f - key) * 3.0f) : C.kd);
    v->s[3] = hit * (1.0f - onepole_k(algo == 2 ? 0.05f : 0.12f, b->sr / (f32)n));
    v->s[4] = fb;
    sounding = 1;
}

static void cozy_post(const Blk *b, f32 *l, f32 *r, int n)
{
    f32 cut = 15000.0f * exp2fx(-C.muffle * 4.6f), k = onepole_k(1.0f / (H1_TAU * cut), b->sr), d2 = C.dust * C.dust;
    f32 depth = C.sway * 0.7f, tl = 1.0f - depth * (0.5f + 0.5f * C.trem), tr = 1.0f - depth * (0.5f - 0.5f * C.trem);
    f32 pp = d2 * 0.0012f, want = sounding ? 1.0f : 0.3f;
    Svf rf;
    int i, radio = C.algo == 7;
    sounding = 0;
    svf_set(&rf, 1300.0f, 0.9f, b->isr);
    hum_ph = fractf(hum_ph + 100.0f * (f32)n * b->isr);
    for (i = 0; i < n; i++) {
        f32 nz = noise_s(&dust_rng), x, lo, bp;
        act += (want - act) * 0.00005f;
        if (rng_f(&dust_rng) < pp)               /* a pop: a click that rings for a moment */
            pop_e = (0.25f + 0.75f * rng_f(&dust_rng)) * (rng_next(&dust_rng) & 1u ? 1.0f : -1.0f);
        pop_lp += (pop_e - pop_lp) * 0.35f;
        pop_e *= 0.9f;
        hiss_lp += (nz - hiss_lp) * 0.3f;
        /* (the pops were 0.5 of full scale: on top of a chord they were the loudest thing in the mix) */
        x = (pop_lp * 0.25f + hiss_lp * 0.02f * (0.4f + C.dust)) * C.dust * act;
        if (radio) {                             /* RADIO: a small speaker, and the mains under it */
            f32 m = (l[i] + r[i]) * 0.5f;
            SVF_TICK(rf, radio_s, m, lo, bp);
            (void)lo;
            l[i] = r[i] = softclip(bp * 2.2f) * 0.8f + 0.006f * tsin(hum_ph + (f32)i * 100.0f * b->isr);
        }
        l[i] = l[i] * tl + x;
        r[i] = r[i] * tr + x;
        lp_l[0] += (l[i] - lp_l[0]) * k; lp_l[1] += (lp_l[0] - lp_l[1]) * k;
        lp_r[0] += (r[i] - lp_r[0]) * k; lp_r[1] += (lp_r[0] - lp_r[1]) * k;
        l[i] = lp_l[1];
        r[i] = lp_r[1];
    }
}

/* ---- picture ---------------------------------------------------------------------------- */
#define WX0 98.0f
#define WX1 226.0f
#define WY0 24.0f
#define WY1 150.0f
#define NMOTE 36


static f32 hsh(uint32_t n) { n = (n ^ 61u) ^ (n >> 16); n *= 9u; n ^= n >> 4; n *= 0x27d4eb2du; n ^= n >> 15; return (f32)(n & 0xFFFFu) * (1.0f / 65536.0f); }



static void cozy_draw(const Vis *vis)
{
}

const InstDef inst_cozy = {
    "COZY",
    {255, 176, 138},
    {"DUST", "WOW", "MUFFLE", "SWAY"},
    {"TINE", "REED", "FELT", "TAPE", "VIBES", "DUSK", "TOY", "RADIO"},
    {{35, 30, 35, 25}, 0, {0, 60, 100, 50}, {0, 55, 0, 0}, {8, 4, 30, 42}, {0, 3, 1, 60}, {0, 0, 0}},
    0.3f,
    cozy_block, cozy_note, cozy_render, 0, cozy_draw, 0

};