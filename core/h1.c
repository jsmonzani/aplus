/* A22 - start-up defaults, saved state persistence, and public introspection. */
#include "internal.h"

/* Curated K1..K4 sweet spots for all 80 factory presets (10 engines x 8 algorithms).
 * Algorithm 0 of each engine matches insts[inst]->def.knob0. */
static const uint8_t ALGO_K0[][NALGO][4] = {
    /* 0 FM-4      (K1 RATIO, K2 DEPTH, K3 FEEDBK, K4 FILTER):
     *   KEYS, BASS, BELL, PAD, BRASS, PLUCK, ORGAN, ALIEN */
    {{25, 58, 20, 76}, {34, 64, 36, 54}, {52, 56, 16, 86}, {30, 44, 48, 64},
     {38, 66, 44, 72}, {46, 68, 24, 78}, {28, 42, 30, 80}, {64, 62, 54, 74}},
    /* 1 ELECTRIC  (K1 BARK, K2 TINE, K3 TREM, K4 WARMTH):
     *   RHODES, WURLI, DX-EP, CLAV, ORGAN, FELT, VIBES, TAPE */
    {{42, 48, 30, 22}, {64, 34, 38, 32}, {48, 72, 26, 12}, {72, 62, 10, 18},
     {52, 56, 62, 26}, {26, 32, 12, 48}, {28, 64, 54, 24}, {46, 36, 28, 62}},
    /* 2 STACK     (K1 SPREAD, K2 CHORD, K3 DRIFT, K4 GLOW):
     *   CLOUD, CHORD, JUNO, GLASS, ORGAN, DRONE, CHOIR, DREAM */
    {{56, 35, 34, 66}, {62, 44, 28, 70}, {68, 42, 32, 72}, {64, 58, 42, 82},
     {48, 54, 24, 74}, {72, 52, 56, 58}, {60, 48, 38, 76}, {66, 68, 58, 62}},
    /* 3 ANALOG    (K1 SHAPE, K2 TZFM, K3 CUTOFF, K4 RESO):
     *   DUAL, TZFM, SUPER, SUB, SYNC, ACID, BRASS, CHORD */
    {{35, 28, 62, 26}, {44, 56, 68, 32}, {28, 64, 78, 22}, {48, 42, 52, 34},
     {58, 34, 66, 46}, {32, 24, 42, 74}, {26, 36, 58, 28}, {38, 46, 66, 26}},
    /* 4 SHAPER    (K1 WARP, K2 FOLD, K3 BIAS, K4 TONE):
     *   CZ-SAW, CZ-RES, BUCHLA, PLUCK, METRIC, CYBER, GLASSY, CRUSH */
    {{48, 45, 22, 74}, {62, 38, 28, 72}, {52, 64, 36, 70}, {58, 54, 26, 78},
     {54, 52, 44, 72}, {60, 58, 32, 68}, {42, 34, 38, 84}, {56, 48, 30, 68}},
    /* 5 WIRE      (K1 DAMP, K2 PLUCK, K3 BODY, K4 TENSION):
     *   NYLON, STEEL, HARP, KOTO, CLAV, BASS, SITAR, BOWED */
    {{64, 54, 48, 24}, {78, 68, 44, 42}, {84, 42, 62, 22}, {70, 74, 52, 56},
     {58, 82, 40, 34}, {54, 46, 68, 16}, {76, 66, 56, 68}, {74, 58, 64, 32}},
    /* 6 HAZE      (K1 MORPH, K2 FORMANT, K3 DRIFT, K4 SHIMMER):
     *   AETHER, VOX, SOLAR, ICE, MIST, ABYSS, PRISM, GHOST */
    {{46, 48, 38, 56}, {54, 52, 34, 46}, {62, 42, 44, 68}, {58, 66, 32, 78},
     {34, 38, 56, 44}, {64, 30, 48, 38}, {56, 60, 42, 74}, {48, 58, 68, 52}},
    /* 7 MONO      (K1 FM-BITE, K2 CUTOFF, K3 GLIDE, K4 SUB-DRV):
     *   ACID, REESE, SH-LEAD, ZAP-BASS, GROWL, SUB-808, LASER, HOOVER */
    {{42, 46, 18, 52}, {54, 44, 28, 64}, {38, 64, 22, 46}, {58, 54, 10, 58},
     {66, 52, 20, 62}, {28, 36, 24, 74}, {62, 68, 16, 44}, {56, 62, 34, 58}},
    /* 8 KIT       (K1 TUNE, K2 DECAY, K3 PUNCH, K4 FM-GRIT):
     *   808, 909, CYCLES, CRISP, LOFI, MODULAR, METAL, WORLD */
    {{50, 52, 48, 18}, {52, 48, 64, 24}, {50, 46, 56, 46}, {50, 50, 54, 16},
     {46, 44, 52, 42}, {54, 46, 66, 38}, {52, 56, 54, 58}, {50, 56, 46, 14}},
    /* 9 PERC      (K1 PITCH, K2 MALLET, K3 RING, K4 SPACE):
     *   HANDPAN, MARIMBA, KALIMBA, TABLA, WOOD, GAMELAN, STEEL, GLITCH */
    {{50, 52, 58, 52}, {50, 48, 46, 44}, {50, 62, 54, 48}, {50, 58, 56, 46},
     {50, 66, 36, 40}, {50, 60, 72, 64}, {50, 56, 60, 58}, {56, 68, 42, 62}},
    /* 10 WAVE     (K1 WAVE, K2 SCAN, K3 SPREAD, K4 FILTER):
     *   HARMON, ORGAN, VOX, DIGI, PULSE, SYNC, GLASS, GROWL */
    {{30, 20, 30, 70}, {40, 10, 24, 74}, {36, 46, 34, 58}, {50, 40, 40, 66},
     {24, 34, 30, 62}, {20, 60, 32, 70}, {40, 24, 46, 80}, {44, 52, 30, 50}},
    /* 11 CHIP     (K1 DUTY, K2 SWEEP, K3 ARP, K4 CRUSH):
     *   PULSE, TRI, NOISE, ARP, BASS, LEAD, DRUM, DUET */
    {{50, 0, 40, 10}, {50, 12, 0, 6}, {20, 0, 0, 8}, {36, 0, 60, 10},
     {50, 0, 0, 12}, {36, 6, 56, 8}, {50, 46, 52, 6}, {50, 0, 30, 10}},
    /* 12 CYCLE    (K1 WAVE, K2 SCAN, K3 SPREAD, K4 FILTER):
     *   STRING, REED, KEYS, VOICE, STEP-STR, STEP-RD, STEP-KEY, STEP-VOX (the STEP- ones scan further) */
    {{50, 20, 30, 72}, {44, 16, 24, 74}, {36, 28, 30, 78}, {50, 30, 36, 70},
     {14, 64, 30, 72}, {14, 60, 24, 74}, {14, 64, 30, 78}, {14, 70, 36, 70}},
    /* 13 SKINS    (K1 TUNE, K2 DECAY, K3 TONE, K4 ROOM):
     *   STUDIO, LIVE, TIGHT, VINTAGE, ORCH, HALL, MARCH, SHELLAC (ROOM 0: the reflections off) */
    {{50, 50, 60, 0}, {50, 60, 56, 58}, {52, 34, 66, 0}, {46, 50, 42, 14},
     {50, 54, 56, 0}, {48, 66, 50, 66}, {54, 36, 64, 0}, {46, 52, 36, 22}},
    /* 14 KOI JSM   */
    {{50, 50, 60, 0}, {50, 60, 56, 58}, {52, 34, 66, 0}, {46, 50, 42, 14},
     {50, 54, 56, 0}, {48, 66, 50, 66}, {54, 36, 64, 0}, {46, 52, 36, 22}},   
    /* 15 KNOT JSM   */
    {{50, 50, 60, 0}, {50, 60, 56, 58}, {52, 34, 66, 0}, {46, 50, 42, 14},
     {50, 54, 56, 0}, {48, 66, 50, 66}, {54, 36, 64, 0}, {46, 52, 36, 22}},      
};
typedef char a22_k0_rows[sizeof ALGO_K0 / sizeof ALGO_K0[0] == NINST ? 1 : -1];

// JSM
static uint8_t cur_song = 0;

void knobs_default(int inst, int algo)
{
    int a, k;
    if (inst < 0 || inst >= NINST)
        return;
    for (a = 0; a < NALGO; a++)
        if (algo < 0 || a == algo)
            for (k = 0; k < 4; k++)
                KNOBS(inst, a)[k] = ALGO_K0[inst][a][k];
}

void knobs_home(int inst, int algo, uint8_t out[4])
{
    int k;
    if (inst < 0 || inst >= NINST) inst = 0;
    if (algo < 0 || algo >= NALGO) algo = 0;
    for (k = 0; k < 4; k++)
        out[k] = ALGO_K0[inst][algo][k];
}

static void inst_default(int i)
{
    st.ip[i] = insts[i]->def;
    knobs_default(i, -1);
}

static void track_default(int t)
{
    /* Default 4-Track band setup:
     *   T1 (Blue):   FM-4 (KEYS)
     *   T2 (Green):  ELECTRIC (RHODES)
     *   T3 (White):  ANALOG (SUB / DUAL)
     *   T4 (Orange): KIT (808 Drums) */
    static const uint8_t DEF_INST[NTRACK] = {0, 1, 3, 8};
    static const uint8_t DEF_ALGO[NTRACK] = {0, 0, 3, 0};
    static const uint8_t DEF_SEND[NTRACK] = {42, 46, 25, 16};
    int inst = DEF_INST[t] < NINST ? DEF_INST[t] : 0;
    int algo = DEF_ALGO[t] < NALGO ? DEF_ALGO[t] : 0;
    int k;
    Track *tr = &st.trk[t];
    tr->inst = (uint8_t)inst;
    tr->algo = (uint8_t)algo;
    tr->vol = 88;
    tr->pan = 50;
    tr->send = DEF_SEND[t];
    tr->mute = 0;
    tr->has_snap = 0; // JSM I suspect has_snap is useless
    tr->rsv = 0;
    for (k = 0; k < 4; k++) {
        tr->knob[k] = ALGO_K0[inst][algo][k];
        tr->env[k]  = insts[inst]->def.env[k];
        tr->menv[k] = insts[inst]->def.menv[k];
        tr->lfo[k]  = insts[inst]->def.lfo[k];
    }
}

static void defaults(void)
{
    int i, f, k;
    uint8_t *p = (uint8_t *)&st;
    for (i = 0; i < (int)sizeof st; i++)
        p[i] = 0;
    st.magic = H1_MAGIC;
    st.version = H1_VERSION;
    st.inst = 0;                                 /* FM-4 */
    st.track = 0;                                /* T1 */
    st.hud = 1;                                  /* color-coded knob strip on by default */
    st.glo[0] = 118;                             /* tempo */
    st.glo[1] = 0;                               /* swing */
    st.glo[2] = 50;                              /* tune: center */
    st.glo[3] = 100;                             /* brightness */
    st.seq[0] = SEQ_TAPE;                        /* 4-track Tape default */
    st.seq[1] = 2;                               /* 4 bars (1 << 2) */
    st.seq[2] = 0;                               /* 4/4 */
    st.seq[3] = 1;                               /* 1/16 snap */
    st.seq[4] = 1;                               /* 1/16 step grid */
    st.seq[5] = CLICK_REC;                       /* click while recording */
    st.seq[6] = 1;                               /* multi-track enabled */
    st.seq[7] = 1;                               /* tape pitch-link ON */
    st.tape[0] = TAPE_SPD_1X;
    st.tape[1] = 0;                              /* loop in bar 0 */
    st.tape[2] = 0;                              /* loop out = full length */
    st.tape[3] = TRICK_OFF;
    st.fx_type = FX_PLATE;                      /* Lush Dattorro plate reverb default */
    st.fx_on = 1;
    for (f = 0; f < NFX; f++)
        for (k = 0; k < 4; k++)
            st.fx_p[f][k] = FX_DEFAULTS[f][k];
    st.master[0] = 52;                           /* LOW: subtle warmth */
    st.master[1] = 50;                           /* MID: flat */
    st.master[2] = 52;                           /* HIGH: subtle air */
    st.master[3] = 22;                           /* DRIVE: subtle tape glue */
    st.shake[0] = 45;                          /* GRAVITY */
    st.shake[1] = 62;                          /* SPIN (gentle clockwise) */
    st.shake[2] = 78;                          /* BOUNCE */
    st.shake[3] = 50;                          /* CAGE / SCALE */
    st.riff_len = 0;
    st.riff_root = 60;
    st.chord[0] = CHORD_OFF;                     /* chord memory off; MEMORY holds a major triad until one */
    st.chord_n = 3;                              /* is captured (EDIT + GLO with keys held) */
    st.chord_mem[0] = 0; st.chord_mem[1] = 4; st.chord_mem[2] = 7;
    st.ninst = NINST;
    st.nfx = NFX;
    for (i = 0; i < NINST; i++)
        inst_default(i);
    for (i = 0; i < NTRACK; i++)
        track_default(i);
    cur_song = 0; // JSM
}

SMALL void h1_init(uint32_t sample_rate)
{
    defaults();
    eng_init(sample_rate);
    ui_init();
}

void h1_frame(uint32_t now_ms) { ui_frame(now_ms); }
void h1_render(int32_t *lr, uint32_t n) { eng_render(lr, n); }
void h1_late(int late) { eng_late(late); }
uint32_t h1_voices_shed(void) { return eng_shed_count(); }
void h1_stage_clock(uint32_t (*clock)(void)) { eng_stage_clock(clock); }
uint32_t h1_stages(uint32_t out[H1_NSTG]) { return eng_stages(out); }

const void *h1_state(uint32_t *size)
{
    if (size)
        *size = (uint32_t)sizeof st;
    return &st;
}

typedef char a22_state_fits_store[sizeof(H1State) <= 3840u ? 1 : -1];
typedef char a22_slots_valid[NINST <= NSLOT && NFX <= NFXSLOT && NSLOT * NALGO < 255 ? 1 : -1];


uint8_t h1_cur_song(void) { return (uint8_t)(cur_song & 7u); }
void h1_set_cur_song(uint8_t snap) { cur_song = (uint8_t)(snap & 7u); }

static void bytes(void *to, const uint8_t *from, uint32_t n)
{
    uint8_t *d = (uint8_t *)to;
    while (n--)
        *d++ = *from++;
}


SMALL int h1_state_load(const void *blob, uint32_t size)
{
    const uint8_t *b = (const uint8_t *)blob;
    uint32_t magic = 0, n;
    int i, j, a, s, t;
    int ninst, nfx;
    cur_song = 0;
    if (!blob || size < offsetof(H1State, version) + 1u)
        return -1;
    for (n = 0; n < 4u; n++)
        ((uint8_t *)&magic)[n] = b[n];
    if (magic != H1_MAGIC)
        return -1;
    if (b[offsetof(H1State, version)] == H1_VERSION && size == sizeof st) {
        bytes(&st, b, (uint32_t)sizeof st);
    } else {
        return -1;
    }

    /* the engines and effects it was saved with: the slots from there on come up as new */
    ninst = st.ninst <= NINST ? st.ninst : NINST;
    nfx = st.nfx <= NFX ? st.nfx : NFX;
    for (i = ninst; i < NINST; i++)
        inst_default(i);
    for (i = nfx; i < NFX; i++)
        for (j = 0; j < 4; j++)
            st.fx_p[i][j] = FX_DEFAULTS[i][j];
    for (i = NINST; i < NSLOT; i++) {              /* (slots no engine has yet: empty) */
        uint8_t *p = (uint8_t *)&st.ip[i];
        for (n = 0; n < (uint32_t)sizeof st.ip[i]; n++)
            p[n] = 0;
        for (a = 0; a < NALGO; a++)
            for (j = 0; j < 4; j++)
                st.knob[i][a][j] = 0;
    }
    for (i = NFX; i < NFXSLOT; i++)
        for (j = 0; j < 4; j++)
            st.fx_p[i][j] = 0;

    /* clamp every loaded parameter safely */
    st.magic = H1_MAGIC;
    st.version = H1_VERSION;
    if (st.inst >= NINST) st.inst = 0;
    if (st.track >= NTRACK) st.track = 0;
    st.octave = (int8_t)iclamp(st.octave, -2, 2);
    st.hud &= 1u;
    st.keys[0] = (uint8_t)iclamp(st.keys[0], 0, 11);
    st.keys[1] = (uint8_t)iclamp(st.keys[1], 0, 7);
    st.keys[2] = (uint8_t)iclamp(st.keys[2], 0, 100);
    st.keys[3] &= 1u;
    st.glo[0] = (uint8_t)iclamp(st.glo[0], 40, 240);
    st.glo[1] = (uint8_t)iclamp(st.glo[1], 0, 100);
    st.glo[2] = (uint8_t)iclamp(st.glo[2], 0, 100);
    st.glo[3] = (uint8_t)iclamp(st.glo[3], 20, 100);
    st.seq[0] &= 1u;
    if (st.seq[1] > 5) st.seq[1] = 2;
    if (st.seq[2] > 4) st.seq[2] = 0;
    if (st.seq[3] > 2) st.seq[3] = 1;
    if (st.seq[4] > 2) st.seq[4] = 1;
    if (st.seq[5] > 2) st.seq[5] = CLICK_REC;
    st.seq[6] = 1;
    st.seq[7] &= 1u;
    if (st.tape[0] > 3) st.tape[0] = TAPE_SPD_1X;
    if (st.tape[1] > 31) st.tape[1] = 0;
    if (st.tape[2] > 32) st.tape[2] = 0;
    st.tape[3] = TRICK_OFF;
    if (st.fx_type >= NFX) st.fx_type = FX_PLATE;
    st.fx_on &= 1u;
    for (i = 0; i < NFX; i++)
        for (j = 0; j < 4; j++)
            if (st.fx_p[i][j] > 100) st.fx_p[i][j] = 100;
    if (st.fx_p[FX_DELAY][1] > 7) st.fx_p[FX_DELAY][1] = 3;
    for (j = 0; j < 4; j++) {
        if (st.master[j] > 100) st.master[j] = 50;
        if (st.shake[j] > 100) st.shake[j] = 50;
    }
    if (st.riff_len > NRIFF) st.riff_len = 0;
    if (st.riff_root > 127) st.riff_root = 60;
    for (i = 0; i < NRIFF; i++)
        if (st.riff[i] > 128) st.riff[i] = 0;
    if (st.chord[0] > CHORD_SHAPE) st.chord[0] = CHORD_OFF;
    if (st.chord[1] >= CHORD_NSHAPE) st.chord[1] = 0;
    if (st.chord[2] > 3) st.chord[2] = 0;
    if (st.chord[3] > 100) st.chord[3] = 0;
    if (st.chord_n < 1 || st.chord_n > CHORD_MAX) st.chord_n = 1;
    st.chord_mem[0] = 0;
    for (i = 1; i < CHORD_MAX; i++)
        if (st.chord_mem[i] > 48) st.chord_mem[i] = 0;

    for (i = 0; i < NINST; i++) {
        if (st.ip[i].algo >= NALGO) st.ip[i].algo = 0;
        for (j = 0; j < 4; j++) {
            if (st.ip[i].knob0[j] > 100) st.ip[i].knob0[j] = 100;
            if (st.ip[i].env[j] > 100) st.ip[i].env[j] = 100;
            if (st.ip[i].menv[j] > 100) st.ip[i].menv[j] = 100;
            for (a = 0; a < NALGO; a++)
                if (st.knob[i][a][j] > 100) st.knob[i][a][j] = 100;
        }
        if (st.ip[i].lfo[0] > 6) st.ip[i].lfo[0] = 0;
        if (st.ip[i].lfo[1] > 100) st.ip[i].lfo[1] = 50;
        if (st.ip[i].lfo[2] > 100) st.ip[i].lfo[2] = 0;
        if (st.ip[i].lfo[3] > 6) st.ip[i].lfo[3] = 0;
        if (st.ip[i].arp[0] >= NARPMODE) st.ip[i].arp[0] = 0;
        if (st.ip[i].arp[1] > 5) st.ip[i].arp[1] = 3;
        st.ip[i].arp[2] = (uint8_t)iclamp(st.ip[i].arp[2], 1, 4);
        st.ip[i].arp[3] = (uint8_t)iclamp(st.ip[i].arp[3], 5, 100);
        if (st.ip[i].mix[IP_CUT] > 100) st.ip[i].mix[IP_CUT] = 0;
        if (st.ip[i].mix[IP_PAN] > 100) st.ip[i].mix[IP_PAN] = 50;
        if (st.ip[i].mix[IP_SEND] > 100) st.ip[i].mix[IP_SEND] = 35;
    }

    for (i = 0; i < NTRACK; i++) {
        Track *tr = &st.trk[i];
        if (tr->inst >= NINST) tr->inst = 0;
        if (tr->algo >= NALGO) tr->algo = 0;
        if (tr->vol > 100) tr->vol = 88;
        if (tr->pan > 100) tr->pan = 50;
        if (tr->send > 100) tr->send = 35;
        tr->mute &= 3u;
        tr->has_snap = 0;
        for (j = 0; j < 4; j++) {
            if (tr->knob[j] > 100) tr->knob[j] = 50;
            if (tr->env[j] > 100) tr->env[j] = 50;
            if (tr->menv[j] > 100) tr->menv[j] = 50;
        }
        if (tr->lfo[0] > 6) tr->lfo[0] = 0;
        if (tr->lfo[1] > 100) tr->lfo[1] = 50;
        if (tr->lfo[2] > 100) tr->lfo[2] = 0;
        if (tr->lfo[3] > 6) tr->lfo[3] = 0;
    }

    for (i = 0; i < NSTEP; i++) {
        for (j = 0; j < NSNOTE; j++) {
            if (st.steps[i][j] > 128) st.steps[i][j] = 0;
            if (st.step_trk[i][j] >= NTRACK) st.step_trk[i][j] = 0;
            if (st.step_vel[i][j] > 127) st.step_vel[i][j] = 127;
        }
    }

    for (s = 0; s < NSONG; s++) {
        Song *sg = &st.song[s];
        sg->used &= 1u;
        if (sg->fx_type >= NFX) sg->fx_type = FX_PLATE;
        if (sg->bars > 5u) sg->bars = 2u;
        sg->glo[0] = (uint8_t)iclamp(sg->glo[0], 40, 240);
        sg->glo[1] = (uint8_t)iclamp(sg->glo[1], 0, 100);
        sg->glo[2] = (uint8_t)iclamp(sg->glo[2], 0, 100);
        sg->glo[3] = (uint8_t)iclamp(sg->glo[3], 20, 100);
        for (j = 0; j < 4; j++)
            if (sg->fx_p[j] > 100) sg->fx_p[j] = 100;
        sg->rsv = 0;

        for (t = 0; t < NTRACK; t++) {
            Snap *sn = &sg->snap[t];
            if (sn->inst >= NINST) sn->inst = 0;
            if (sn->algo >= NALGO) sn->algo = 0;
            for (j = 0; j < 4; j++) {
                if (sn->knob[j] > 100) sn->knob[j] = 50;
                if (sn->env[j] > 100) sn->env[j] = 50;
                if (sn->menv[j] > 100) sn->menv[j] = 50;
                if (sn->lfo[j] > 100) sn->lfo[j] = 0;
            }
            if (sn->lfo[0] > 6) sn->lfo[0] = 0;
            if (sn->lfo[3] > 6) sn->lfo[3] = 0;
            if (sn->mix[IP_CUT] > 100) sn->mix[IP_CUT] = 0;
            if (sn->mix[IP_PAN] > 100) sn->mix[IP_PAN] = 50;
            if (sn->mix[IP_SEND] > 100) sn->mix[IP_SEND] = 35;
            sn->rsv[0] = sn->rsv[1] = sn->rsv[2] = 0;
        }
    }

    st.ninst = NINST;
    st.nfx = NFX;
    st.rsv[0] = st.rsv[1] = 0;
    eng_init(0);
    ui_init();
    return 0;
}

SMALL int h1_state_load_OLD(const void *blob, uint32_t size)
{
    const uint8_t *b = (const uint8_t *)blob;
    uint32_t magic = 0, n;
    int i, j, a;
    int ninst, nfx;
    cur_song = 0; // JSM
    if (!blob || size < offsetof(H1State, version) + 1u)
        return -1;
    for (n = 0; n < 4u; n++)
        ((uint8_t *)&magic)[n] = b[n];
    if (magic != H1_MAGIC)
        return -1;
    if (b[offsetof(H1State, version)] == H1_VERSION && size == sizeof st) {
        bytes(&st, b, (uint32_t)sizeof st);
    } else {
        return -1;
    }
    /* the engines and effects it was saved with: the slots from there on come up as new */
    ninst = st.ninst <= NINST ? st.ninst : NINST;
    nfx = st.nfx <= NFX ? st.nfx : NFX;
    for (i = ninst; i < NINST; i++)
        inst_default(i);
    for (i = nfx; i < NFX; i++)
        for (j = 0; j < 4; j++)
            st.fx_p[i][j] = FX_DEFAULTS[i][j];
    for (i = NINST; i < NSLOT; i++) {              /* (slots no engine has yet: empty) */
        uint8_t *p = (uint8_t *)&st.ip[i];
        for (n = 0; n < (uint32_t)sizeof st.ip[i]; n++)
            p[n] = 0;
        for (a = 0; a < NALGO; a++)
            for (j = 0; j < 4; j++)
                st.knob[i][a][j] = 0;
    }
    for (i = NFX; i < NFXSLOT; i++)
        for (j = 0; j < 4; j++)
            st.fx_p[i][j] = 0;

    /* clamp every loaded parameter safely */
    st.magic = H1_MAGIC;
    st.version = H1_VERSION;
    if (st.inst >= NINST) st.inst = 0;
    if (st.track >= NTRACK) st.track = 0;
    st.octave = (int8_t)iclamp(st.octave, -2, 2);
    st.hud &= 1u;
    st.keys[0] = (uint8_t)iclamp(st.keys[0], 0, 11);
    st.keys[1] = (uint8_t)iclamp(st.keys[1], 0, 7);
    st.keys[2] = (uint8_t)iclamp(st.keys[2], 0, 100);
    st.keys[3] &= 1u;
    st.glo[0] = (uint8_t)iclamp(st.glo[0], 40, 240);
    st.glo[1] = (uint8_t)iclamp(st.glo[1], 0, 100);
    st.glo[2] = (uint8_t)iclamp(st.glo[2], 0, 100);
    st.glo[3] = (uint8_t)iclamp(st.glo[3], 20, 100);
    st.seq[0] &= 1u;
    if (st.seq[1] > 5) st.seq[1] = 2;
    if (st.seq[2] > 4) st.seq[2] = 0;
    if (st.seq[3] > 2) st.seq[3] = 1;
    if (st.seq[4] > 2) st.seq[4] = 1;
    if (st.seq[5] > 2) st.seq[5] = CLICK_REC;
    st.seq[6] = 1;
    st.seq[7] &= 1u;
    if (st.tape[0] > 3) st.tape[0] = TAPE_SPD_1X;
    if (st.tape[1] > 31) st.tape[1] = 0;
    if (st.tape[2] > 32) st.tape[2] = 0;
    st.tape[3] = TRICK_OFF;
    if (st.fx_type >= NFX) st.fx_type = FX_PLATE;
    st.fx_on &= 1u;
    for (i = 0; i < NFX; i++)
        for (j = 0; j < 4; j++)
            if (st.fx_p[i][j] > 100) st.fx_p[i][j] = 100;
    if (st.fx_p[FX_DELAY][1] > 7) st.fx_p[FX_DELAY][1] = 3;
    for (j = 0; j < 4; j++) {
        if (st.master[j] > 100) st.master[j] = 50;
        if (st.shake[j] > 100) st.shake[j] = 50;
    }
    if (st.riff_len > NRIFF) st.riff_len = 0;
    if (st.riff_root > 127) st.riff_root = 60;
    for (i = 0; i < NRIFF; i++)
        if (st.riff[i] > 128) st.riff[i] = 0;
    if (st.chord[0] > CHORD_SHAPE) st.chord[0] = CHORD_OFF;
    if (st.chord[1] >= CHORD_NSHAPE) st.chord[1] = 0;
    if (st.chord[2] > 3) st.chord[2] = 0;
    if (st.chord[3] > 100) st.chord[3] = 0;
    if (st.chord_n < 1 || st.chord_n > CHORD_MAX) st.chord_n = 1;
    st.chord_mem[0] = 0;
    for (i = 1; i < CHORD_MAX; i++)
        if (st.chord_mem[i] > 48) st.chord_mem[i] = 0;
    for (i = 0; i < NINST; i++) {
        if (st.ip[i].algo >= NALGO) st.ip[i].algo = 0;
        for (j = 0; j < 4; j++) {
            if (st.ip[i].knob0[j] > 100) st.ip[i].knob0[j] = 100;
            if (st.ip[i].env[j] > 100) st.ip[i].env[j] = 100;
            if (st.ip[i].menv[j] > 100) st.ip[i].menv[j] = 100;
            for (a = 0; a < NALGO; a++)
                if (st.knob[i][a][j] > 100) st.knob[i][a][j] = 100;
        }
        if (st.ip[i].lfo[0] > 6) st.ip[i].lfo[0] = 0;
        if (st.ip[i].lfo[1] > 100) st.ip[i].lfo[1] = 50;
        if (st.ip[i].lfo[2] > 100) st.ip[i].lfo[2] = 0;
        if (st.ip[i].lfo[3] > 6) st.ip[i].lfo[3] = 0;
        if (st.ip[i].arp[0] >= NARPMODE) st.ip[i].arp[0] = 0;
        if (st.ip[i].arp[1] > 5) st.ip[i].arp[1] = 3;
        st.ip[i].arp[2] = (uint8_t)iclamp(st.ip[i].arp[2], 1, 4);
        st.ip[i].arp[3] = (uint8_t)iclamp(st.ip[i].arp[3], 5, 100);
        if (st.ip[i].mix[IP_CUT] > 100) st.ip[i].mix[IP_CUT] = 0;
        if (st.ip[i].mix[IP_PAN] > 100) st.ip[i].mix[IP_PAN] = 50;
        if (st.ip[i].mix[IP_SEND] > 100) st.ip[i].mix[IP_SEND] = 35;
    }
    for (i = 0; i < NTRACK; i++) {
        Track *tr = &st.trk[i];
        if (tr->inst >= NINST) tr->inst = 0;
        if (tr->algo >= NALGO) tr->algo = 0;
        if (tr->vol > 100) tr->vol = 88;
        if (tr->pan > 100) tr->pan = 50;
        if (tr->send > 100) tr->send = 35;
        tr->mute &= 3u;
        tr->has_snap = 0; // &= 1u; JSM I suspect has_snap is useless
        for (j = 0; j < 4; j++) {
            if (tr->knob[j] > 100) tr->knob[j] = 50;
            if (tr->env[j] > 100) tr->env[j] = 50;
            if (tr->menv[j] > 100) tr->menv[j] = 50;
        }
        if (tr->lfo[0] > 6) tr->lfo[0] = 0;
        if (tr->lfo[1] > 100) tr->lfo[1] = 50;
        if (tr->lfo[2] > 100) tr->lfo[2] = 0;
        if (tr->lfo[3] > 6) tr->lfo[3] = 0;
    }
/*    for (s = 0; s < NSONG; s++) {

    for (i = 0; i < NSNAP; i++) {
        Snap *sn = &st.snap[i];
        sn->used &= 1u;
        if (sn->inst >= NINST) sn->used = 0;
        if (sn->algo >= NALGO) sn->algo = 0;
        if (sn->fx_type >= NFX) sn->fx_type = 0;
        for (j = 0; j < 4; j++) {
            if (sn->knob[j] > 100) sn->knob[j] = 50;
            if (sn->env[j] > 100) sn->env[j] = 50;
            if (sn->menv[j] > 100) sn->menv[j] = 50;
            if (sn->fx_p[j] > 100) sn->fx_p[j] = 50;
        }
        if (sn->lfo[0] > 6) sn->lfo[0] = 0;          // (as a track's: a bad save loaded out-of-range values) 
        if (sn->lfo[1] > 100) sn->lfo[1] = 50;
        if (sn->lfo[2] > 100) sn->lfo[2] = 0;
        if (sn->lfo[3] > 6) sn->lfo[3] = 0;
    }
    for (i = 0; i < NSTEP; i++)
        for (j = 0; j < NSNOTE; j++) {
            if (st.steps[i][j] > 128) st.steps[i][j] = 0;
            if (st.step_trk[i][j] >= NTRACK) st.step_trk[i][j] = 0;
            if (st.step_vel[i][j] > 127) st.step_vel[i][j] = 127;
        }
*/
    st.ninst = NINST;
    st.nfx = NFX;
    st.rsv[0] = st.rsv[1] = 0;
    eng_init(0);
    ui_init();
    return 0;
}

int h1_instrument(void) { return st.inst; }
const char *h1_instrument_name(int i) { return i >= 0 && i < NINST ? insts[i]->name : ""; }
int h1_algorithm(void) { return st.ip[st.inst].algo; }
int h1_page(void) { return ui_page(); }
int h1_layers(void) { return 1; }
int h1_instrument_volume(int inst) { (void)inst; return st.trk[st.track].vol; }
void h1_two_cores(int (*run)(void (*fn)(uint32_t), uint32_t arg), int (*wait)(void)) { eng_two_cores(run, wait); }
int h1_instrument_count(void) { return NINST; }
const char *h1_knob_name(int inst, int k) { return inst >= 0 && inst < NINST && k >= 0 && k < 4 ? insts[inst]->knob[k] : ""; }
const char *h1_algo_name(int inst, int a) { return inst >= 0 && inst < NINST && a >= 0 && a < NALGO ? insts[inst]->algo[a] : ""; }
uint32_t h1_voices_active(void) { return eng_voices(); }

void h1_ink(int inst, uint8_t rgb[3])
{
    int i;
    for (i = 0; i < 3; i++)
        rgb[i] = inst >= 0 && inst < NINST ? insts[inst]->ink[i] : 255;
}
void h1_set_ink(int inst, uint8_t r, uint8_t g, uint8_t b) { (void)inst; (void)r; (void)g; (void)b; }

/* ---- 4-track tape persistence (h1_loop) ----------------------------------------------- */
static uint32_t loop_saved_gen, loop_given_gen;

const void *h1_loop(uint32_t *size)
{
    uint32_t n;
    const void *p;
    loop_given_gen = eng_loop_gen();
    p = eng_loop_blob(&n);
    if (size) *size = n;
    return p;
}

int h1_loop_load(const void *blob, uint32_t size)
{
    int r = eng_loop_load((const uint8_t *)blob, size);
    loop_saved_gen = eng_loop_gen();
    return r;
}

uint32_t h1_loop_gen(void) { return eng_loop_gen(); }
int h1_loop_room(void) { return (NLEV - eng_loop_used()) / 2; }
int h1_loop_quiet(void) { return eng_loop_quiet(); }
int h1_quiet(void) { return eng_quiet(); }
int h1_loop_dirty(void) { return eng_loop_gen() != loop_saved_gen && !eng_loop_taking(); }
void h1_loop_saved(void) { loop_saved_gen = loop_given_gen; }
void h1_midi_out(void (*fn)(uint8_t status, uint8_t d1, uint8_t d2)) { eng_midi_fn = fn; }
