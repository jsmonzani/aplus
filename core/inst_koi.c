/* h1 - KOI: struck metal and wood over still water. A handpan, a tongue drum, a kalimba, a marimba,
 * drops, a gamelan bar, a singing bowl, rain. Every one is a handful of partials that ring and die.
 * The picture is a pond from above: stones, lily pads, a lotus, and koi under them. Every note you
 * strike lands in the water at its own place, and the fish come to it as if it were food.
 *
 *   K1 MALLET  soft felt to hard wood                    the lotus opens, the splash throws more drops
 *   K2 RIPPLE  two bars a hair apart: beating            the water moves, rings spread from every pad
 *   K3 LINGER  how long it rings                         the rings last, the fish trail a longer wake
 *   K4 SINK    lowered into the water: it bends, dulls   the fish go deep, bubbles come up
 */
#include "internal.h"
#include "dsp.h"

/* ---- sound ------------------------------------------------------------------------------ */
typedef struct KAlgo {
    f32 r[4];                    /* where the partials sit */
    f32 a[4];                    /* how loud */
    f32 d[4];                    /* seconds each takes to die, at middle C with LINGER half way */
    f32 strike;                  /* the knock of the mallet itself */
    f32 beat;                    /* Hz of beating it has even with RIPPLE at nothing */
} KAlgo;
static const KAlgo KA[NALGO] = {
    /* PAN     */ {{1.0f, 2.0f, 3.0f, 5.04f}, {1.0f, 0.5f, 0.32f, 0.07f}, {1.5f, 1.0f, 0.7f, 0.12f}, 0.5f, 0.0f},
    /* TONGUE  */ {{1.0f, 2.0f, 3.01f, 6.3f}, {1.0f, 0.22f, 0.1f, 0.05f}, {3.2f, 1.8f, 0.9f, 0.25f}, 0.25f, 0.4f},
    /* KALIMBA */ {{1.0f, 2.0f, 5.8f, 12.3f}, {1.0f, 0.12f, 0.3f, 0.1f}, {1.1f, 0.5f, 0.09f, 0.03f}, 0.6f, 0.0f},
    /* MARIMBA */ {{1.0f, 3.98f, 9.9f, 2.0f}, {1.0f, 0.32f, 0.1f, 0.04f}, {0.55f, 0.13f, 0.045f, 0.3f}, 0.7f, 0.0f},
    /* DROPS   */ {{1.0f, 2.0f, 3.2f, 4.7f}, {1.0f, 0.15f, 0.06f, 0.0f}, {0.3f, 0.12f, 0.06f, 0.05f}, 0.08f, 0.0f},
    /* GAMELAN */ {{1.0f, 2.76f, 5.4f, 8.93f}, {1.0f, 0.5f, 0.28f, 0.14f}, {2.4f, 1.5f, 0.7f, 0.3f}, 0.6f, 2.2f},
    /* BOWL    */ {{1.0f, 2.71f, 5.15f, 8.4f}, {1.0f, 0.55f, 0.26f, 0.1f}, {7.0f, 5.0f, 2.6f, 1.2f}, 0.15f, 0.7f},
    /* RAIN    */ {{1.0f, 3.0f, 6.2f, 10.1f}, {1.0f, 0.3f, 0.14f, 0.05f}, {0.35f, 0.16f, 0.07f, 0.03f}, 0.4f, 0.0f},
};

static struct {
    int algo;
    f32 mallet, ripple, linger, sink, lmul;
} K;
static f32 sink_l[2], sink_r[2];

static void koi_block(const Blk *b)
{
    K.algo = b->algo;
    K.mallet = b->k[0];
    K.ripple = b->k[1];
    K.linger = b->k[2];
    K.sink = b->k[3];
    K.lmul = exp2fx((K.linger - 0.5f) * 4.0f);
}

static void koi_note(Voice *v, const Blk *b)
{
    int i;
    (void)b;
    if (!v->u[0]) v->u[0] = 0x0C01F157u + v->idx * 7919u;
    if (v->env < 0.001f)
        for (i = 0; i < 6; i++)
            v->ph[i] = 0.0f;
    for (i = 0; i < 4; i++)
        v->s[i] = 1.0f;                          /* every partial struck at once */
    v->s[4] = 1.0f;                              /* the knock */
    v->s[5] = 1.0f;                              /* DROPS: how far below the note it still is */
    v->s[6] = 0.1f + 0.3f * rng_f(&v->u[0]);     /* RAIN: until the next drop */
    v->s[7] = 0.0f;
    v->s[8] = 1.0f;
    v->s[9] = 0.0f;                              /* the soft mallet's way in */
}

static void koi_render(Voice *v, const Blk *b, f32 *out_l, f32 *out_r, int n)
{
    const KAlgo *a = &KA[K.algo];
    f32 dt = (f32)n * b->isr, pitch = v->pitch + b->bend, m = K.mallet, ks, f, d0, d[4], g[4], d4, rd, inv, hit, hk, nlp = v->s[10], nk, att = v->s[9], da, atk, amp;
    f32 p0 = v->ph[0], p1 = v->ph[1], p2 = v->ph[2], p3 = v->ph[3], p4 = v->ph[4], strike;
    int algo = K.algo, i, k;
    f32 trim = algo == 4 ? 0.8f : (algo == 7 || algo == 3 ? 0.68f : 0.55f);

    if (algo == 7) {                             /* RAIN: while you hold it, it keeps landing, here and octaves up */
        if (v->gate) {
            v->s[6] -= dt;
            if (v->s[6] <= 0.0f) {
                static const f32 UP[6] = {0.0f, 12.0f, 12.0f, 24.0f, 19.0f, 0.0f};
                v->s[7] = UP[(rng_next(&v->u[0]) >> 8) % 6u];
                v->s[8] = 0.3f + 0.7f * rng_f(&v->u[0]);
                for (k = 0; k < 4; k++) v->s[k] = 1.0f;
                v->s[4] = 1.0f;
                v->s[6] = 0.05f + rng_f(&v->u[0]) * (0.55f - 0.42f * K.ripple);
            }
        }
    } else {
        v->s[7] = 0.0f;
        v->s[8] = 1.0f;
    }
    amp = v->s[8];
    hit = v->s[4];
    pitch += v->s[7];
    pitch -= K.sink * K.sink * 5.0f * (1.0f - exp2fx(-v->age * 4.0f));          /* SINK: the note bends as it goes under */
    if (algo == 4) {                             /* DROPS: it comes up to the note from below, fast */
        pitch -= 15.0f * v->s[5];
        v->s[5] *= exp2fx(-dt * 1.4427f / 0.028f);
    }
    ks = clampf(exp2fx((60.0f - pitch) * (1.0f / 30.0f)), 0.35f, 2.2f);       /* low bars ring longer */
    f = mtof(pitch);
    d0 = minf(f * b->isr, 0.45f);
    g[0] = a->a[0];
    g[1] = a->a[1] * (0.35f + 1.3f * m);
    g[2] = a->a[2] * (0.12f + 1.8f * m);
    g[3] = a->a[3] * (0.05f + 2.2f * m);
    for (k = 0; k < 4; k++) {
        d[k] = d0 * a->r[k];
        if (d[k] > 0.42f) { d[k] = 0.0f; g[k] = 0.0f; }                          /* too high to be a pitch any more */
        g[k] *= v->s[k];
    }
    /* RIPPLE: a second bar a few cycles a second away from the first */
    d4 = d0 + (a->beat + K.ripple * (0.5f + 6.0f * K.ripple)) * b->isr;
    rd = minf(0.85f, a->beat > 0.0f ? 0.3f + K.ripple : K.ripple * 1.5f);
    inv = 1.0f / (1.0f + rd * 0.6f);
    strike = a->strike * (0.12f + 1.5f * m * m);
    nk = 0.06f + 0.7f * m;
    hk = 1.0f - b->isr / (0.003f + 0.012f * (1.0f - m));
    atk = 0.007f * (1.0f - m) * (1.0f - m) + (algo == 4 ? 0.004f : 0.0f) + (algo == 6 ? 0.012f : 0.0f);
    da = atk > 0.0f ? b->isr / atk : 1.0f;

    for (i = 0; i < n; i++) {
        f32 x;
        nlp += (noise_s(&v->u[0]) - nlp) * nk;
        x = g[0] * (tsin(p0) + rd * tsin(p4)) * inv + g[1] * tsin(p1) + g[2] * tsin(p2) + g[3] * tsin(p3) + nlp * hit * strike;
        out_l[i] = x * trim * att * amp;
        out_r[i] = x * trim * att * amp;
        p0 += d0; if (p0 >= 1.0f) p0 -= 1.0f;
        p1 += d[1]; if (p1 >= 1.0f) p1 -= 1.0f;
        p2 += d[2]; if (p2 >= 1.0f) p2 -= 1.0f;
        p3 += d[3]; if (p3 >= 1.0f) p3 -= 1.0f;
        p4 += d4; if (p4 >= 1.0f) p4 -= 1.0f;
        hit *= hk;
        att += da; if (att > 1.0f) att = 1.0f;
    }
    v->ph[0] = p0; v->ph[1] = p1; v->ph[2] = p2; v->ph[3] = p3; v->ph[4] = p4;
    for (k = 0; k < 4; k++)
        v->s[k] *= exp2fx(-dt * 1.4427f / (a->d[k] * K.lmul * ks));
    v->s[4] = hit;
    v->s[9] = att;
    v->s[10] = nlp;
}

static void koi_post(const Blk *b, f32 *l, f32 *r, int n)
{
    f32 cut = 16000.0f * exp2fx(-K.sink * 4.6f), k = onepole_k(1.0f / (H1_TAU * cut), b->sr);
    int i;
    for (i = 0; i < n; i++) {                    /* SINK: heard through the water */
        sink_l[0] += (l[i] - sink_l[0]) * k; sink_l[1] += (sink_l[0] - sink_l[1]) * k;
        sink_r[0] += (r[i] - sink_r[0]) * k; sink_r[1] += (sink_r[0] - sink_r[1]) * k;
        l[i] = sink_l[1];
        r[i] = sink_r[1];
    }
}

static f32 koi_phase, koi_kick, koi_kickv, koi_lev;
static uint32_t koi_seen;

static SMALL void koi_draw(const Vis *vis)
{
    f32 dt = vis->dt;
    f32 mallet = sat(K.mallet), ripple = sat(K.ripple), linger = sat(K.linger), sink = sat(K.sink);
    f32 cx = 120.0f, cy = 114.0f + sink * 12.0f;
    f32 xy[2 * 64];
    int i;

    if (vis->fresh) {
        koi_seen = vis->notes;
        koi_phase = koi_kick = koi_kickv = koi_lev = 0.0f;
    }
    if (vis->notes != koi_seen) {
        koi_seen = vis->notes;
        koi_kickv += 2.0f + 4.0f * mallet;
    }

    spring_z(&koi_kick, &koi_kickv, 0.0f, 24.0f, 0.4f, dt);
    koi_lev += (minf(1.0f, vis->level * 3.0f) - koi_lev) * minf(1.0f, dt * lerpf(18.0f, 4.0f, linger));
    koi_phase = fractf(koi_phase + dt * (0.1f + 0.5f * ripple));

    /* Ondes concentriques (ripple + linger) */
    for (i = 0; i < 4; i++) {
        f32 u = fractf(koi_phase + (f32)i * 0.25f);
        f32 fade = (1.0f - u) * lerpf(0.9f, 0.45f, sink);
        g_color((i & 1) ? GC_BLUE : GC_GREEN);
        g_ring(cx, cy, lerpf(12.0f, 70.0f + 8.0f * koi_kick, u), 1.5f + 2.0f * mallet * (1.0f - u), fade * (0.3f + 0.7f * linger));
    }

    /* Anneau modulé (ripple + sink) */
    for (i = 0; i < 64; i++) {
        f32 th = (f32)i / 64.0f;
        f32 s = vis->scope[(vis->scope_pos + i * 4) & (SCOPE_N - 1)];
        f32 r = lerpf(52.0f, 34.0f, sink) + (s * 14.0f + sin_turns((2.0f + 6.0f * ripple) * th + koi_phase) * 8.0f * ripple);
        xy[2 * i]     = cx + cos_turns(th) * r;
        xy[2 * i + 1] = cy + sin_turns(th) * r;
    }
    g_color(GC_ORANGE);
    g_poly(xy, 64, 1, 2.0f, 0.4f + 0.6f * koi_lev);

    /* Impact central (mallet + sink) */
    g_color(GC_WHITE);
    g_disc(cx, cy, 4.0f + 8.0f * mallet * (0.3f + 0.7f * koi_lev) + 3.0f * koi_kick, lerpf(0.9f, 0.4f, sink));
}
const InstDef inst_koi = {
    "KOI",
    {104, 238, 150},
    {"MALLET", "RIPPLE", "LINGER", "SINK"},
    {"PAN", "TONGUE", "KALIMBA", "MARIMBA", "DROPS", "GAMELAN", "BOWL", "RAIN"},
    {{50, 30, 50, 8}, 0, {0, 60, 100, 78}, {0, 55, 0, 0}, {22, 5, 46, 55}, {0, 3, 1, 60}, {0, 0, 0}},
    0.3f,
    koi_block, koi_note, koi_render, koi_post, koi_draw,
};