/* h1 - KNOT: bells and pure tones, added up from sine partials that each die away at their own
 * rate. The picture is a harmonograph: every sounding note is a pendulum going round at its own
 * pitch, the pen hangs from all of them, and the line it leaves is the harmony itself. An octave
 * is a three-leafed rose, a fifth has five petals, and because the keyboard is tempered
 * none of them quite closes, so the figure turns slowly for as long as the chord rings.
 *
 *   K1 BLOOM   the upper partials                      petals grow on the figure
 *   K2 RING    how long it rings                       the line stays out longer before it spirals in
 *   K3 TWIST   the partials stretch out of tune        the figure leaves the page and tumbles
 *   K4 WARP    the partials bend each other            the line shivers
 */
#include "internal.h"
#include "dsp.h"

/* ---- sound ------------------------------------------------------------------------------ */
#define NPART 4
typedef struct KAlgo {
    f32 rat[NPART], amp[NPART];
    f32 dec;                                     /* how long it rings, against the knob */
    f32 hi;                                      /* how much faster the upper partials die */
    f32 oct;                                     /* octaves up or down */
    f32 swell;                                   /* partials that arrive late (the gong) */
    f32 e1, e2;                                  /* the two petals the picture draws */
} KAlgo;

static const KAlgo KA[NALGO] = {
    {{2.0f, 3.0f, 4.0f, 5.0f}, {0.5f, 0.28f, 0.14f, 0.08f}, 1.0f, 0.5f, 0, 0, 2.0f, 3.0f},                /* PURE */
    {{1.183f, 1.506f, 2.0f, 2.662f}, {0.45f, 0.5f, 0.6f, 0.35f}, 1.3f, 0.35f, 0, 0, 2.0f, 2.662f},        /* BELL */
    {{2.756f, 5.404f, 8.933f, 13.34f}, {0.6f, 0.4f, 0.25f, 0.12f}, 0.7f, 1.3f, 0, 0, 2.756f, 5.404f},     /* GLASS */
    {{2.0f, 3.0f, 4.0f, 6.0f}, {0.6f, 0.5f, 0.4f, 0.3f}, 40.0f, 0.0f, 0, 0, 3.0f, 4.0f},                  /* ORGAN */
    {{1.34f, 1.72f, 2.25f, 2.91f}, {0.6f, 0.55f, 0.5f, 0.4f}, 2.2f, 0.15f, -1, 1.0f, 1.34f, 2.25f},       /* GONG */
    {{4.0f, 6.27f, 10.0f, 17.5f}, {0.4f, 0.3f, 0.16f, 0.08f}, 0.45f, 2.2f, 1, 0, 4.0f, 6.0f},             /* BOX */
    {{2.0f, 3.01f, 4.17f, 5.43f}, {0.55f, 0.6f, 0.45f, 0.3f}, 1.6f, 0.4f, 0, 0, 3.01f, 5.43f},            /* CHIME */
    {{0.5f, 1.004f, 1.498f, 2.007f}, {0.7f, 0.6f, 0.3f, 0.2f}, 3.0f, 0.1f, -1, 0.4f, 0.5f, 1.498f},       /* VOID */
};

static struct {
    int algo;
    f32 bloom, twist, warp;
    f32 kd[NPART + 1];                           /* per-block decay of the fundamental and each partial */
    f32 tau;                                     /* the fundamental's decay time, for the picture */
} N;

static void knot_block(const Blk *b)
{
    const KAlgo *a = &KA[b->algo];
    f32 tau = 0.18f * exp2fx(b->k[1] * 5.2f) * a->dec, rate = b->sr / (f32)CTRL;
    int i;
    N.algo = b->algo;
    N.bloom = b->k[0];
    N.twist = b->k[2];
    N.warp = b->k[3];
    N.tau = tau;
    N.kd[0] = 1.0f - onepole_k(tau, rate);
    for (i = 0; i < NPART; i++)
        N.kd[i + 1] = 1.0f - onepole_k(tau / (1.0f + a->hi * (f32)(i + 1)), rate);
}

static void knot_note(Voice *v, const Blk *b)
{
    int i;
    (void)b;
    if (!v->u[0]) v->u[0] = 0x6B4E07A1u + v->idx * 5381u;
    for (i = 0; i <= NPART; i++)
        v->s[i] = 1.0f;                          /* the partials' levels */
    v->s[5] = 1.0f;                              /* the strike */
    if (v->env < 0.001f)
        for (i = 0; i < 6; i++)
            v->ph[i] = 0.0f;
}

static void knot_render(Voice *v, const Blk *b, f32 *out_l, f32 *out_r, int n)
{
    const KAlgo *a = &KA[N.algo];
    f32 f = mtof(v->pitch + b->bend + 12.0f * a->oct) * b->isr, d[NPART], amp[NPART], g0 = v->s[0], strike = v->s[5];
    f32 stretch = N.twist * N.twist, warp = N.warp * N.warp * 0.5f, late = sat(v->age * 1.4f);
    int i, j;
    for (j = 0; j < NPART; j++) {
        f32 r = a->rat[j];
        r *= 1.0f + stretch * 0.06f * (r - 1.0f) * (j & 1 ? 1.0f : -0.6f);     /* TWIST pulls the partials apart */
        d[j] = minf(f * r, 0.45f);
        /* (a partial near half the rate fades out, as a glide or TWIST takes it there, instead of being
         * cut off between one block and the next: a click) */
        amp[j] = a->amp[j] * (0.15f + 1.1f * N.bloom) * v->s[j + 1] * sat((0.42f - d[j]) * 25.0f);
        if (a->swell > 0.0f)
            amp[j] *= lerpf(1.0f, late * (1.0f + (f32)j * 0.3f), a->swell);
    }
    for (i = 0; i < n; i++) {
        f32 x0 = tsin(v->ph[0]), pm = x0 * warp, x = x0 * g0;
        for (j = 0; j < NPART; j++) {
            x += amp[j] * tsin(v->ph[j + 1] + pm * (f32)(j + 1));
            v->ph[j + 1] += d[j];
            if (v->ph[j + 1] >= 1.0f) v->ph[j + 1] -= 1.0f;
        }
        if (strike > 0.002f) {
            x += strike * 0.35f * (0.3f + N.bloom) * noise_s(&v->u[0]);
            strike *= 0.993f;
        }
        v->ph[0] += f;
        if (v->ph[0] >= 1.0f) v->ph[0] -= 1.0f;
        out_l[i] = x * 0.6f;
        out_r[i] = x * 0.6f;
    }
    v->s[5] = strike;
    v->s[0] *= N.kd[0];                          /* each partial rings down at its own rate */
    for (j = 0; j < NPART; j++)
        v->s[j + 1] *= N.kd[j + 1];
}

/* ---- picture ---------------------------------------------------------------------------- */
#define NP 384
static f32 flow, ref_p = 60.0f, ref_v, e1s = 2.0f, e2s = 3.0f, scale_s = 1.0f, pop, popv, rot, idle = 1.0f;
static uint32_t seen;

static void knot_draw(const Vis *vis)
{
    const KAlgo *a = &KA[vis->algo];
    f32 dt = vis->dt, t = vis->t, bloom = vis->k[0], ring = vis->k[1], twist = vis->k[2], warp = vis->k[3], mt = vis->algo_t;
    f32 amp[NVOICE], rat[NVOICE], sum = 0.0f, lo = 200.0f, damp, sc, ca, sa, tilt, ct, sti, style_a, px = 0.0f, py = 0.0f, pz = 0.0f, zamp;
    f32 U = 6.0f, lastx[4] = {0}, lasty[4] = {0};
    static f32 hx[NP + 1], hy[NP + 1];
    int i, k, nv = 0, style = mt < 0.5f ? vis->algo_prev : vis->algo, step = 1, idx[NVOICE];

    if (vis->fresh) {
        seen = vis->notes;
        e1s = a->e1; e2s = a->e2;
        pop = popv = 0.0f;
        idle = 1.0f;
    }
    if (vis->notes != seen) {
        seen = vis->notes;
        popv += 2.6f;
    }
    spring_z(&pop, &popv, 0.0f, 20.0f, 0.3f, dt);
    e1s += (a->e1 - e1s) * minf(1.0f, dt * 5.0f);       /* the petals morph from one algorithm's to the next */
    e2s += (a->e2 - e2s) * minf(1.0f, dt * 5.0f);
    style_a = absf(2.0f * mt - 1.0f);
    style_a = 0.25f + 0.75f * style_a;

    /* the pendulums: every sounding note, as loud as its bell still is */
    for (i = 0; i < NVOICE; i++) {
        const VVoice *v = &vis->v[i];
        f32 e;
        if (!v->on)
            continue;
        e = v->env * (0.25f + 0.75f * exp2fx(-v->age / (N.tau * 0.9f + 0.05f)));
        if (e < 0.012f)
            continue;
        amp[nv] = e;
        rat[nv] = v->pitch;
        idx[nv] = i;
        if (v->pitch < lo) lo = v->pitch;
        sum += e;
        nv++;
    }
    idle += ((sum < 0.2f ? 1.0f : 0.0f) - idle) * minf(1.0f, dt * 2.5f);
    if (nv) {
        if (absf(lo - ref_p) > 30.0f) { ref_p = lo; ref_v = 0.0f; }
        spring(&ref_p, &ref_v, lo, 7.0f, dt);
    }
    for (i = 0; i < nv; i++) {                   /* the lowest note goes round one way, the rest the other: an
                                                  * interval of p to q then draws p + q petals */
        f32 r = exp2fx((rat[i] - ref_p) * (1.0f / 12.0f));
        rat[i] = rat[i] > lo + 0.5f ? -r : r;
    }
    if (idle > 0.02f && nv < NVOICE) {           /* nothing ringing: the figure at rest, small */
        amp[nv] = 0.2f * idle;
        rat[nv] = 1.0f;
        idx[nv] = 3;
        sum += amp[nv];
        nv++;
    }

    flow += dt * (0.05f + 0.1f * sat(sum));
    rot += dt * (0.02f + 0.22f * twist * twist);
    zamp = twist;
    damp = 0.5f * (1.0f - ring) * (1.0f - ring) * (1.0f - ring) + 0.012f;
    sc = 104.0f / maxf(0.3f, sum * (1.0f + 0.42f * bloom)) * (1.0f + 0.1f * pop);
    scale_s += (sc - scale_s) * minf(1.0f, dt * 6.0f);
    sc = scale_s;
    ca = cos_turns(rot * zamp);
    sa = sin_turns(rot * zamp);
    tilt = 0.09f * twist * sin_turns(t * 0.05f) + 0.06f * twist;
    ct = cos_turns(tilt);
    sti = sin_turns(tilt);
    if (style == 5) step = 6;                    /* BOX: few, straight strokes */

    for (k = 0; k <= NP; k += step) {
        f32 u = (f32)k * (U / (f32)NP), env = exp2fx(-damp * u * 1.44f), x = 0.0f, y = 0.0f, z = 0.0f, w, al, q, X, Y, Z;
        for (i = 0; i < nv; i++) {
            f32 ph = rat[i] * (u + flow) + (f32)idx[i] * 0.137f, e = amp[i];
            f32 c = cos_turns(ph), s = sin_turns(ph);
            x += e * c;
            y += e * s;
            if (bloom > 0.01f) {                 /* the petals: two smaller wheels going the other way */
                f32 p1 = -ph * e1s + 0.11f, p2 = ph * (e2s + 1.0f) + 0.3f;
                x += e * bloom * (0.36f * cos_turns(p1) + 0.1f * cos_turns(p2));
                y += e * bloom * (0.36f * sin_turns(p1) + 0.1f * sin_turns(p2));
            }
            if (zamp > 0.01f)
                z += e * sin_turns(ph * 1.5f + 0.17f + (f32)idx[i] * 0.21f);
            if (warp > 0.01f) {
                f32 wv = warp * warp * 0.1f * e * sin_turns(ph * 9.0f + t * 0.6f);
                x += wv * c;
                y += wv * s;
            }
        }
        x *= env; y *= env; z *= env * zamp * 0.8f;
        X = x * ca + z * sa;                     /* turn it about the upright, tip it a little */
        Z = -x * sa + z * ca;
        Y = y * ct - Z * sti;
        Z = y * sti + Z * ct;
        q = 1.0f / (1.0f - Z * 0.22f);
        X = 120.0f + X * sc * q;
        Y = 122.0f + Y * sc * q;
        w = 1.3f + 1.6f * sat(0.5f + Z * 0.7f) * (0.4f + 0.6f * env);
        al = (0.3f + 0.7f * sat(0.55f + Z * 0.6f)) * (0.25f + 0.75f * env) * style_a * sat(0.35f + sum * 1.5f);
        hx[k] = X;
        hy[k] = Y;
        if (k) {
            switch (style) {
            default:                             /* PURE: the line */
                g_line(px, py, X, Y, w, al);
                break;
            case 1:                              /* BELL: the line and its echo, turned half round */
                g_line(px, py, X, Y, w, al);
                g_line(240.0f - px, 244.0f - py, 240.0f - X, 244.0f - Y, 1.1f, al * 0.3f);
                break;
            case 2:                              /* GLASS: beads */
                if (k % 2 == 0)
                    g_disc(X, Y, 0.8f + w * 0.55f, al);
                break;
            case 3: {                            /* ORGAN: three of it, a rose window */
                int m;
                f32 dx0 = px - 120.0f, dy0 = py - 122.0f, dx1 = X - 120.0f, dy1 = Y - 122.0f;
                g_line(px, py, X, Y, w, al);
                for (m = 1; m < 3; m++) {
                    f32 c3 = cos_turns((f32)m / 3.0f), s3 = sin_turns((f32)m / 3.0f);
                    g_line(120.0f + dx0 * c3 - dy0 * s3, 122.0f + dx0 * s3 + dy0 * c3, 120.0f + dx1 * c3 - dy1 * s3, 122.0f + dx1 * s3 + dy1 * c3, 1.2f, al * 0.45f);
                }
                break;
            }
            case 4: {                            /* GONG: a ribbon */
                f32 nx = -(Y - py), ny = X - px, l = nx * nx + ny * ny, off = 2.2f + 2.5f * sat(0.5f + Z);
                if (l > 1e-4f) {
                    l = rsqrtf(l) * off;
                    nx *= l; ny *= l;
                    if (k > step)
                        g_line(lastx[0], lasty[0], X + nx, Y + ny, 1.2f, al);
                    if (k > step)
                        g_line(lastx[1], lasty[1], X - nx, Y - ny, 1.2f, al);
                    if (k % 6 == 0)
                        g_line(X + nx, Y + ny, X - nx, Y - ny, 1.0f, al * 0.6f);
                    lastx[0] = X + nx; lasty[0] = Y + ny;
                    lastx[1] = X - nx; lasty[1] = Y - ny;
                }
                break;
            }
            case 5:                              /* BOX: pins and thread */
                g_line(px, py, X, Y, 1.3f, al);
                g_disc(X, Y, 2.0f, al);
                break;
            case 6:                              /* CHIME: string art, every point tied to one further on */
                if (k >= 40 && k % 2 == 0)
                    g_line(hx[k - 40], hy[k - 40], X, Y, 1.0f, al * 0.75f);
                break;
            case 7:                              /* VOID: a dark cord through a field of rings */
                break;
            }
        }
        px = X; py = Y; pz = Z;
    }
    (void)pz;
    if (style == 7) {
        for (i = 0; i < 9; i++) {
            f32 rr = 12.0f + (f32)i * 13.0f + 5.0f * sin_turns(t * 0.05f + (f32)i * 0.13f);
            g_ring(120.0f, 122.0f, rr, 1.2f, (0.2f + 0.5f * sat(sum) * (0.5f + 0.5f * sin_turns(t * 0.21f - (f32)i * 0.11f))) * style_a);
        }
        g_bed_move(hx[0], hy[0]);                /* the cord's bed: the rings stop short of it */
        for (k = step; k <= NP; k += step)
            g_bed_to(hx[k], hy[k]);
        g_bed_clear();
        for (k = step; k <= NP; k += step)
            g_line(hx[k - step], hy[k - step], hx[k], hy[k], 1.6f, style_a * (1.0f - 0.7f * (f32)k / (f32)NP));
    }
    /* the pen */
    g_disc(hx[0], hy[0], 2.6f + 1.5f * sat(sum) + 2.0f * maxf(0.0f, pop), style_a);
}

const InstDef inst_knot = {
    "KNOT",
    {196, 164, 255},
    {"BLOOM", "RING", "TWIST", "WARP"},
    {"PURE", "BELL", "GLASS", "ORGAN", "GONG", "BOX", "CHIME", "VOID"},
    {{35, 60, 12, 0}, 0, {0, 60, 100, 78}, {0, 55, 0, 0}, {12, 4, 38, 50}, {0, 3, 1, 60}, {0, 0, 0}},
    0.27f,
    knot_block, knot_note, knot_render, 0, knot_draw,
};