/* A22 - DICE: the four knobs of an algorithm, rolled near where that algorithm sits.
 *
 * A roll moves each knob from the algorithm's own sweet spot (ALGO_K0 in h1.c, through knobs_default)
 * by up to SPAN either way, then keeps it inside LO..HI. So it changes the sound without leaving the
 * algorithm: a knob is never rolled far from what makes the algorithm what it is. The spans are per
 * engine knob:
 *   - pitch-like knobs barely move (KIT TUNE, FM-4 RATIO), and so do discrete ones (STACK CHORD);
 *   - a knob whose sweet spot is 0 is a feature switched off (CHIP's SWEEP and ARP on most algorithms)
 *     and stays 0;
 *   - low-pass knobs have a floor (LO) so a roll cannot shut the sound, and the knobs that run away
 *     (feedback, resonance, fold, grit, crush) a ceiling (HI). */
#include "internal.h"

typedef struct DiceKnob { uint8_t span, lo, hi; } DiceKnob;

static const DiceKnob DICE[][4] = {
    /* FM-4      RATIO, DEPTH, FEEDBK, FILTER */
    {{10, 0, 100}, {18, 15, 90}, {16, 0, 70}, {16, 45, 100}},
    /* ELECTRIC  BARK, TINE, TREM, WARMTH */
    {{20, 0, 100}, {20, 0, 100}, {20, 0, 75}, {20, 0, 100}},
    /* STACK     SPREAD, CHORD, DRIFT, GLOW */
    {{18, 0, 100}, {10, 0, 100}, {18, 0, 100}, {16, 40, 100}},
    /* ANALOG    SHAPE, TZFM, CUTOFF, RESO */
    {{22, 0, 100}, {18, 0, 100}, {16, 35, 100}, {16, 0, 80}},
    /* SHAPER    WARP, FOLD, BIAS, TONE */
    {{18, 0, 100}, {16, 0, 75}, {16, 0, 100}, {14, 50, 100}},
    /* WIRE      DAMP, PLUCK, BODY, TENSION */
    {{14, 40, 100}, {18, 0, 100}, {18, 0, 100}, {16, 0, 100}},
    /* HAZE      MORPH, FORMANT, DRIFT, SHIMMER */
    {{22, 0, 100}, {22, 0, 100}, {22, 0, 100}, {22, 0, 100}},
    /* MONO      FM-BITE, CUTOFF, GLIDE, SUB-DRV */
    {{18, 0, 100}, {16, 30, 100}, {10, 0, 40}, {16, 0, 100}},
    /* KIT       TUNE, DECAY, PUNCH, FM-GRIT */
    {{6, 0, 100}, {16, 0, 100}, {16, 0, 100}, {14, 0, 70}},
    /* PERC      PITCH, MALLET, RING, SPACE */
    {{14, 0, 100}, {16, 0, 100}, {16, 25, 100}, {18, 0, 100}},
    /* WAVE      WAVE, SCAN, SPREAD, FILTER */
    {{30, 0, 100}, {20, 0, 100}, {16, 0, 100}, {14, 40, 100}},
    /* CHIP      DUTY, SWEEP, ARP, CRUSH */
    {{25, 0, 100}, {12, 0, 100}, {12, 0, 100}, {6, 0, 30}},
    /* CYCLE     WAVE, SCAN, SPREAD, FILTER (as WAVE) */
    {{30, 0, 100}, {20, 0, 100}, {16, 0, 100}, {14, 40, 100}},
    /* SKINS     TUNE, DECAY, TONE, ROOM (TUNE as KIT's; ROOM at 0 stays off) */
    {{6, 0, 100}, {16, 10, 100}, {14, 20, 100}, {16, 0, 90}},
    /* COZY JSM     TUNE, DECAY, TONE, ROOM (TUNE as KIT's; ROOM at 0 stays off) */
    {{6, 0, 100}, {16, 10, 100}, {14, 20, 100}, {16, 0, 90}},
};
typedef char dice_rows[sizeof DICE / sizeof DICE[0] == NINST ? 1 : -1];

static uint32_t dice_next(uint32_t *rng)        /* xorshift32 */
{
    uint32_t x = *rng ? *rng : 0x6D2B79F5u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *rng = x;
}

void dice_roll(int inst, int algo, uint32_t *rng, uint8_t out[4])
{
    uint8_t home[4];
    int k;
    if (inst < 0 || inst >= NINST) inst = 0;
    if (algo < 0 || algo >= NALGO) algo = 0;
    knobs_home(inst, algo, home);
    for (k = 0; k < 4; k++) {
        const DiceKnob *d = &DICE[inst][k];
        int span = d->span, v = home[k];
        if (v != 0 && span > 0)
            v += (int)(dice_next(rng) % (uint32_t)(2 * span + 1)) - span;
        if (v != 0 || home[k] != 0)                /* (a 0 that is home stays 0, whatever LO is) */
            v = iclamp(v, iclamp(d->lo, 0, home[k]), d->hi > home[k] ? d->hi : home[k]);
        out[k] = (uint8_t)v;
    }
}
