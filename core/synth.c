/* A22 - Stereo Dual-Core Synth Engine, 4-Track Virtual Tape Deck (with K1-K4 automation,
 * Loop In/Out, Cut & Paste, Varispeed & Reverse), RIFF & SHAKE physics sequencers,
 * 64-step Pattern Sequencer, Classic Arpeggiator, Per-Track Mixer & FX Sends, Master Limiter. */
#include "internal.h"
#include "dsp.h"

f32 sin_tab[SIN_N + 1];
H1State st;
Voice voices[NVOICE];
uint32_t eng_rng = 0x2F6E2B1u;

const InstDef *const insts[] = {
    &inst_fm4, &inst_electric, &inst_stack, &inst_analog, &inst_shaper,
    &inst_wire, &inst_haze,  &inst_mono, &inst_kit, &inst_perc, &inst_wave, &inst_chip,
    &inst_cycle, &inst_skins
    , &inst_koi, &inst_knot // JSM
};
typedef char a22_insts_all[sizeof insts / sizeof insts[0] == NINST ? 1 : -1];

/* Per-algorithm gain trim calibrated via 88-WAV RMS QA so all 80 presets sit evenly (~-19.5 dBFS) */
static const f32 algo_gain[][NALGO] = {
    /* 0 FM-4      */ {1.05f, 1.00f, 1.00f, 1.25f, 1.20f, 1.12f, 1.38f, 1.35f},
    /* 1 ELECTRIC  */ {1.25f, 1.22f, 1.55f, 1.65f, 1.72f, 1.25f, 1.12f, 1.32f},
    /* 2 STACK   */ {1.30f, 2.15f, 1.05f, 1.25f, 1.38f, 1.58f, 1.38f, 1.48f},
    /* 3 ANALOG     */ {1.35f, 1.08f, 1.85f, 1.32f, 1.35f, 2.05f, 1.38f, 1.72f},
    /* 4 SHAPER   */ {1.05f, 1.22f, 1.12f, 1.28f, 1.06f, 1.30f, 1.08f, 1.05f},
    /* 5 WIRE    */ {1.50f, 1.48f, 1.46f, 1.38f, 1.20f, 1.38f, 0.72f, 0.76f},
    /* 6 HAZE */ {1.22f, 0.92f, 1.20f, 1.08f, 1.26f, 1.32f, 1.20f, 1.12f},
    /* 7 MONO */   {1.92f, 1.58f, 1.38f, 1.24f, 1.24f, 1.20f, 1.44f, 1.28f},
    /* 8 KIT      */ {1.25f, 1.42f, 1.45f, 1.42f, 1.40f, 1.45f, 1.45f, 1.75f},
    /* 9 PERC      */ {1.12f, 1.32f, 1.14f, 1.38f, 2.10f, 1.10f, 1.14f, 1.92f},
    /* 10 WAVE     */ {0.47f, 0.50f, 0.52f, 1.08f, 0.49f, 0.54f, 0.52f, 0.48f},
    /* 11 CHIP     */ {0.50f, 0.53f, 0.39f, 0.55f, 0.50f, 0.52f, 1.10f, 0.58f},
    /* 12 CYCLE    */ {0.484f, 0.467f, 0.465f, 0.480f, 0.477f, 0.461f, 0.460f, 0.473f},
    /* 13 SKINS    */ {1.040f, 0.875f, 1.308f, 0.854f, 0.927f, 0.788f, 1.166f, 0.797f},
    /* 14 KOI    */ {1.000f, 1.000f, 1.189f, 1.189f, 1.122f, 1.000f, 0.750f, 1.259f},
    /* 15 KNOT   */ {1.000f, 0.891f, 1.000f, 0.668f, 0.841f, 1.259f, 0.841f, 0.841f},
};
typedef char a22_gain_rows[sizeof algo_gain / sizeof algo_gain[0] == NINST ? 1 : -1];

static f32 SR = 44100.0f, ISR = 1.0f / 44100.0f;
static int cur_i, cur_trk;
static Blk live_blk;

typedef struct Lfo { f32 ph, sh, drift; } Lfo;
typedef struct Mod {
    f32 kmod;
    int kmod_k;
    f32 vol, panmod, bend, lfo;
    f32 att, kd, sus, kr;          /* Amp ADSR rates */
    f32 mkd, menv_amt, vel_sens, punch; /* Mod/Filter Env rates */
} Mod;

typedef struct TRun {
    Blk b;
    f32 knob_s[4];                 /* smoothed knobs, as set (what live keys play) */
    f32 auto_s[4];                 /* smoothed knobs with the tape's automation (what the tape and steps play) */
    uint8_t auto_k[4];             /* automated K1..K4 override during tape playback */
    uint8_t auto_on[4];
    Mod m;
    Lfo lf;
    f32 vol_s, send_s, pan_s;
    f32 peak_out;                  /* live VU meter 0..1 */
    f32 last_pitch;
    uint32_t at;
    uint8_t owner;
} TRun;

static TRun trun[NTRACK];
static uint32_t blkno = 16;
/* The block an engine's own clocks (ELECTRIC's tremolo and wow, STACK's and HAZE's drift, ANALOG's and MONO's PWM:
 * b->step in their block) last moved in. Per engine, not per track: an engine on two tracks ran them twice as fast. */
static uint32_t inst_adv[NINST], inst_steps[NINST];
uint32_t eng_clock_steps(int inst) { return inst >= 0 && inst < NINST ? inst_steps[inst] : 0u; }
static uint32_t lfo_rng = 0x1F0C1F0Cu;
static f32 run_time, blk_time;
static uint32_t stamp_ctr;
static f32 last_pitch = 60.0f;
static volatile uint32_t note_count, hand_count;
static volatile f32 hand_pitch, level_out, lfo_out, beat_out;
static volatile uint32_t silent_blocks;          /* blocks the output has been silent (eng_quiet) */

static f32 scope[SCOPE_N];
static f32 own_scope[SCOPE_N];                   /* the track in hand alone, before the effect: the HOME picture's */
static volatile uint32_t scope_w;

/* ---- lock-free event queue (main loop -> audio IRQ) ----------------------------------- */
static volatile uint32_t evq[64];
static volatile uint32_t ev_w, ev_r;

/* The queue's last 8 slots are kept for note-offs: full, it dropped whatever came, a note-off too, and that note hung.
 * What it drops is counted (eng_events_dropped). */
static uint32_t ev_dropped;
uint32_t eng_events_dropped(void) { return ev_dropped; }
static void post(uint32_t e)
{
    uint32_t w = ev_w, nx = (w + 1u) & 63u, used = (w - ev_r) & 63u;
    if (nx == ev_r || (used >= 63u - 8u && ((e >> 8) & 255u) != EV_NOTE_OFF)) {
        ev_dropped++;
        return;
    }
    evq[w] = e;
    ev_w = nx;
}
//JSM

void eng_post_del(int trk, int idx)
{
    post(((uint32_t)EV_DEL_NOTE << 8) | ((uint32_t)idx & 255u) | (((uint32_t)(trk & 3)) << 17));
}
void eng_post(int type, int a) { post(((uint32_t)type << 8) | ((uint32_t)a & 255u)); }
void eng_post_note(int on, int note, int vel, int from_midi)
{
    post(((uint32_t)(on ? EV_NOTE_ON : EV_NOTE_OFF) << 8) | ((uint32_t)note & 127u) |
         ((uint32_t)iclamp(vel, 1, 127) << 16) | (from_midi ? 1u << 23 : 0u));
}
void eng_post_auto(int knob, int val)
{
    post(((uint32_t)EV_AUTOMATION << 8) | ((uint32_t)(knob & 3)) | ((uint32_t)iclamp(val, 0, 100) << 16));
}

/* ---- MIDI out ------------------------------------------------------------------------- */
void (*eng_midi_fn)(uint8_t status, uint8_t d1, uint8_t d2);
static void mout(int on, int note, int vel)
{
    void (*fn)(uint8_t, uint8_t, uint8_t) = eng_midi_fn;
    if (fn && note >= 0 && note < 128)
        fn(on ? 0x90u : 0x80u, (uint8_t)note, (uint8_t)(on ? iclamp(vel, 1, 127) : 0));
}

void mout_channel(int on, int note, int vel, int c) // JSM midi out on a channel (1..16)
{
    void (*fn)(uint8_t, uint8_t, uint8_t) = eng_midi_fn;
    if (fn && note >= 0 && note < 128 && c >= 1 && c <= 16) {
            uint8_t ch = (uint8_t)((c - 1) & 0x0F);
            uint8_t status = (uint8_t)((on ? 0x90u : 0x80u) | ch);
            uint8_t velocity = (uint8_t)(on ? iclamp(vel, 1, 127) : 0);
            fn(status, (uint8_t)note, velocity);
        }
}
void dsp_init(void)
{
    int i;
    for (i = 0; i <= SIN_N; i++)
        sin_tab[i] = sin_turns((f32)i / (f32)SIN_N);
}

/* ---- scales --------------------------------------------------------------------------- */
static const uint16_t SCALE_MASK[8] = {
    0xFFF, /* CHROM */
    0xAB5, /* MAJOR */
    0x5AD, /* MINOR */
    0x295, /* PENTA */
    0x4A9, /* MPENT */
    0x4E9, /* BLUES */
    0x6AD, /* DORIAN */
    0x555, /* WHOLE */
};

int scale_snap(int note)
{
    uint32_t mask = SCALE_MASK[st.keys[1] < 8 ? st.keys[1] : 0];
    int root = st.keys[0] % 12, i;
    for (i = 0; i < 12; i++) {
        int pc = ((note - i - root) % 12 + 12) % 12;
        if (mask & (1u << pc))
            return note - i;
    }
    return note;
}

/* Bumped wherever the engine writes what the state saves (steps, riffs, snapshots, a track's settings): the panel
 * marks the state dirty when it moves (ui_frame). Before, only the panel marked it, so a pattern or a riff reached
 * flash only if some later setting changed. */
static uint32_t saved_n;
uint32_t eng_saved_n(void) { return saved_n; }

/* ---- track snapshot helpers ----------------------------------------------------------- */
static void track_snap_live(int t)
{
    int k, inst = cur_i, algo = st.ip[inst].algo & 7;   /* (the engine's own: st.inst is the panel's) */
    Track *tr = &st.trk[t & 3];
    uint8_t was[sizeof(Track)];
    const uint8_t *now_b = (const uint8_t *)tr;
    size_t b;
    for (b = 0; b < sizeof was; b++) was[b] = now_b[b];   /* (bytes: the core links no libc) */
    tr->inst = (uint8_t)inst;
    tr->algo = (uint8_t)algo;
    tr->has_snap = 1;
    for (k = 0; k < 4; k++) {
        tr->knob[k] = KNOBS(inst, algo)[k];
        tr->env[k]  = st.ip[inst].env[k];
        tr->menv[k] = st.ip[inst].menv[k];
        tr->lfo[k]  = st.ip[inst].lfo[k];
    }
    for (b = 0; b < sizeof was; b++)             /* (called on every recorded note: marked only when it changed) */
        if (was[b] != now_b[b]) { saved_n++; break; }
}

static void track_sync_to_live(int t)
{
    Track *tr = &st.trk[t & 3];
    int inst = tr->inst < NINST ? tr->inst : 0;
    int algo = tr->algo & 7;
    int k;
    cur_trk = t & 3;
    st.track = (uint8_t)cur_trk;
    cur_i = inst;
    st.inst = (uint8_t)inst;
    st.ip[inst].algo = (uint8_t)algo;
    if (tr->has_snap) {
        for (k = 0; k < 4; k++) {
            KNOBS(inst, algo)[k] = tr->knob[k];
            st.ip[inst].env[k]   = tr->env[k];
            st.ip[inst].menv[k]  = tr->menv[k];
            st.ip[inst].lfo[k]   = tr->lfo[k];
        }
    }
}

static int track_muted(int t)
{
    int i, any_solo = 0;
    for (i = 0; i < NTRACK; i++)
        if (st.trk[i].mute & 2u)
            any_solo = 1;
    if (any_solo)
        return !(st.trk[t & 3].mute & 2u);
    return (st.trk[t & 3].mute & 1u) != 0;
}

/* ---- modulation & envelopes ----------------------------------------------------------- */
static void mod_run(Mod *m, const uint8_t lfo[4], const uint8_t env[4], const uint8_t menv[4],
                    Lfo *lf, uint32_t *rng, f32 pitch_shift)
{
    f32 dt = (f32)CTRL * ISR;
    f32 rate = 0.08f * exp2fx((f32)lfo[1] * 0.078f);
    f32 depth = (f32)lfo[2] * 0.01f;
    f32 prev = lf->ph, v = 0.0f;
    int shape = lfo[0], tgt = lfo[3];

    lf->ph = fractf(lf->ph + rate * dt);
    switch (shape) {
    default:
    case 0: v = tsin(lf->ph); break;                                      /* SINE */
    case 1: v = 4.0f * absf(lf->ph - 0.5f) - 1.0f; break;                 /* TRI */
    case 2: v = 2.0f * lf->ph - 1.0f; break;                              /* RAMP */
    case 3: v = lf->ph < 0.5f ? 1.0f : -1.0f; break;                      /* SQR */
    case 4: if (lf->ph < prev) lf->sh = rng_s(rng); v = lf->sh; break;    /* RAND S&H */
    case 5:                                                                /* DRIFT */
        if (lf->ph < prev) lf->sh = rng_s(rng);
        lf->drift += (lf->sh - lf->drift) * minf(1.0f, rate * dt * 4.5f);
        v = lf->drift;
        break;
    case 6:                                                                /* ENV follower */
        v = clampf(level_out * 3.0f - 0.2f, -1.0f, 1.0f);
        break;
    }

    m->lfo = v;
    m->kmod = 0.0f;
    m->kmod_k = -1;
    m->vol = 1.0f;
    m->panmod = 0.0f;
    m->bend = ((f32)st.glo[2] - 50.0f) * 0.02f + pitch_shift;
    if (depth > 0.0f) {
        switch (tgt) {
        case 0: m->bend += v * depth * 2.0f; break;
        case 1: case 2: case 3: case 4:
            m->kmod_k = tgt - 1;
            m->kmod = v * depth * 0.5f;
            break;
        case 5: m->vol = clampf(1.0f + v * depth * 0.65f, 0.1f, 1.6f); break;
        case 6: m->panmod = v * depth * 0.9f; break;
        default: break;
        }
    }

    /* Amp ADSR rates */
    {
        f32 at = 0.001f + 0.002f * exp2fx((f32)env[0] * 0.095f);
        f32 dc = 0.025f * exp2fx((f32)env[1] * 0.075f);
        f32 rl = 0.015f * exp2fx((f32)env[3] * 0.088f);
        m->att = env[0] == 0 ? 1.0f : minf(1.0f, dt / at);
        m->kd  = onepole_k(dc, SR / (f32)CTRL);
        m->sus = (f32)env[2] * 0.01f;
        m->kr  = onepole_k(rl, SR / (f32)CTRL);
    }
    /* Mod / Filter Envelope */
    {
        f32 md = 0.014f * exp2fx((f32)menv[1] * 0.078f);   /* 14 ms .. 3.1 s */
        m->mkd = onepole_k(md, SR / (f32)CTRL);
        m->menv_amt = (f32)menv[0] * 0.01f;
        m->vel_sens = (f32)menv[2] * 0.01f;
        m->punch    = (f32)menv[3] * 0.01f;
    }
}

static f32 tape_pitch_shift(void)
{
    if (!st.seq[7]) return 0.0f;
    if (st.tape[0] == TAPE_SPD_HALF) return -12.0f;
    if (st.tape[0] == TAPE_SPD_2X)   return 12.0f;
    return 0.0f;
}

static const Blk *track_set(int t, int part, int is_seq)
{
    TRun *r = &trun[t & 3];
    const Track *tr = &st.trk[t & 3];
    int inst = PART_INST(part), algo = PART_ALGO(part), k;
    /* The track in hand plays as it is set now, live keys and recorded part alike; another track plays its
     * snapshot. Once a block per track (the first call, from block(), is the track in hand's live one), so both
     * the knobs as set and the knobs with the tape's automation are kept, and the varispeed shift is added
     * after: the recorded part on the track in hand gets its automation and its shift as the others do. */
    int live = (t & 3) == cur_trk;
    const uint8_t *env  = (live || !tr->has_snap) ? st.ip[inst].env  : tr->env;
    const uint8_t *menv = (live || !tr->has_snap) ? st.ip[inst].menv : tr->menv;
    const uint8_t *lfo  = (live || !tr->has_snap) ? st.ip[inst].lfo  : tr->lfo;
    const uint8_t *kn   = (live || !tr->has_snap) ? KNOBS(inst, algo) : tr->knob;

    if (r->at != blkno) {
        if (r->at != blkno - 1u) {
            for (k = 0; k < 4; k++) {
                r->knob_s[k] = (f32)kn[k] * 0.01f;
                r->auto_s[k] = (f32)(r->auto_on[k] ? r->auto_k[k] : kn[k]) * 0.01f;
            }
        }
        r->at = blkno;
        mod_run(&r->m, lfo, env, menv, &r->lf, &lfo_rng, 0.0f);
        for (k = 0; k < 4; k++) {
            r->knob_s[k] += ((f32)kn[k] * 0.01f - r->knob_s[k]) * 0.08f;
            r->auto_s[k] += ((f32)(r->auto_on[k] ? r->auto_k[k] : kn[k]) * 0.01f - r->auto_s[k]) * 0.08f;
        }
        r->b.sr = SR;
        r->b.isr = ISR;
        r->b.time = blk_time;
        r->b.bpm = (f32)iclamp(st.glo[0], 40, 240);
    }
    r->b.algo = algo;
    for (k = 0; k < 4; k++)
        r->b.k[k] = is_seq ? r->auto_s[k] : r->knob_s[k];
    if (r->m.kmod_k >= 0)
        r->b.k[r->m.kmod_k] = sat(r->b.k[r->m.kmod_k] + r->m.kmod);
    r->b.lfo = r->m.lfo;
    r->b.bend = r->m.bend + (is_seq ? tape_pitch_shift() : 0.0f);
    r->b.menv_amt = r->m.menv_amt;
    r->b.vel_sens = r->m.vel_sens;
    r->b.punch = r->m.punch;
    r->b.step = inst_adv[inst] == blkno ? 0.0f : (f32)CTRL * ISR;
    if (inst_adv[inst] != blkno) inst_steps[inst]++;
    inst_adv[inst] = blkno;
    r->owner = (uint8_t)part;
    insts[inst]->block(&r->b);
    return &r->b;
}

/* ---- voices --------------------------------------------------------------------------- */
static uint8_t live_part(void) { return PART(cur_i, st.ip[cur_i].algo & 7); }

static void voice_on_ex(int note, int part, int trk, int seq, int vel)
{
    Voice *v = 0;
    const Blk *b;
    f32 was_l, was_r;
    int i, j, inst = PART_INST(part), mono = insts[inst]->mono, legato = 0;
    if (note < 0 || note > 127 || !part_ok((uint32_t)part))
        return;
    /* monophonic legato priority (e.g. MONO) */
    if (mono) {
        for (i = 0; i < NVOICE && !v; i++)
            if (voices[i].stage && voices[i].part == part && voices[i].trk == trk && voices[i].seq == seq) {
                v = &voices[i];
                if (v->gate) legato = 1;
            }
    }
    /* retrigger matching sounding note on same track/part */
    for (i = 0; i < NVOICE && !v; i++)
        if (voices[i].stage && voices[i].gate && voices[i].note == note && voices[i].part == part && voices[i].trk == trk)
            v = &voices[i];
    /* free slot */
    for (i = 0; i < NVOICE && !v; i++)
        if (!voices[i].stage)
            v = &voices[i];
    /* steal oldest releasing voice */
    if (!v) {
        uint32_t best = 0xFFFFFFFFu;
        for (i = 0; i < NVOICE; i++)
            if (!voices[i].gate && voices[i].stamp < best) {
                best = voices[i].stamp;
                v = &voices[i];
            }
    }
    /* steal oldest active voice */
    if (!v) {
        uint32_t best = 0xFFFFFFFFu;
        for (i = 0; i < NVOICE; i++)
            if (voices[i].stamp < best) {
                best = voices[i].stamp;
                v = &voices[i];
            }
    }
    was_l = v->stage ? v->last_l : 0.0f;
    was_r = v->stage ? v->last_r : 0.0f;
    if (v->part && PART_INST(v->part) != inst) {
        uint32_t serial = v->serial;
        uint8_t idx = v->idx, *p = (uint8_t *)v;
        for (j = 0; j < (int)sizeof(Voice); j++) p[j] = 0;
        v->serial = serial;
        v->idx = idx;
    }
    v->last_l = was_l;
    v->last_r = was_r;
    v->declick = legato ? 0u : (uint8_t)(was_l != 0.0f || was_r != 0.0f);
    v->shed = 0;
    v->part = (uint8_t)part;
    v->trk  = (uint8_t)(trk & 3);
    v->seq  = (uint8_t)seq;
    v->vel  = (uint8_t)iclamp(vel, 1, 127);
    v->vel_f = (f32)v->vel * (1.0f / 127.0f);
    v->target = (f32)note;
    if (!seq) {
        v->pitch = st.keys[2] ? last_pitch : (f32)note;
        last_pitch = (f32)note;
    } else {
        v->pitch = st.keys[2] && trun[v->trk].last_pitch > 0.0f ? trun[v->trk].last_pitch : (f32)note;
    }
    trun[v->trk].last_pitch = (f32)note;
    v->note = (uint8_t)note;
    v->gate = 1;
    if (legato) {
        if (v->stage == 4) v->stage = 1;
        v->menv = maxf(v->menv, 0.55f);
        v->menv_prev = v->menv;
    } else {
        v->stage = 1;
        v->menv = v->menv_prev = 1.0f;
        v->age = 0.0f;
    }
    v->stamp = ++stamp_ctr;
    v->pan = clampf(((f32)note - 64.0f) * (1.0f / 44.0f), -0.38f, 0.38f);
    v->r1 = rng_f(&eng_rng);
    v->r2 = rng_f(&eng_rng);
    b = track_set(v->trk, part, seq);
    insts[inst]->note(v, b);
    v->serial++;
    if (inst == cur_i && (v->trk & 3) == cur_trk) {   /* (the picture hears its own track's notes, not the jam's) */
        note_count++;
        if (!seq) {
            hand_pitch = (f32)note;
            hand_count++;
        }
    }
}

static void voice_off_ex(int note, int trk, int seq)
{
    int i;
    for (i = 0; i < NVOICE; i++)
        if (voices[i].stage && voices[i].gate && voices[i].note == note &&
            voices[i].trk == (trk & 3) && voices[i].seq == seq) {
            voices[i].gate = 0;
            voices[i].stage = 4;
        }
}

static void voice_on(int note, int vel) { voice_on_ex(note, live_part(), cur_trk, 0, vel); }
/* a key let go: its live voice ends on whichever track it started (the track may have changed since) */
static void voice_off(int note)
{
    int t;
    for (t = 0; t < NTRACK; t++)
        voice_off_ex(note, t, 0);
}

static void voices_release_all(void)
{
    int i;
    for (i = 0; i < NVOICE; i++)
        if (voices[i].stage) {
            voices[i].gate = 0;
            voices[i].stage = 4;
        }
}

/* ---- clock helpers -------------------------------------------------------------------- */
static const uint16_t METER_TICKS[5] = {384, 288, 480, 288, 336};
static const uint8_t GRID_TICKS[3]   = {48, 24, 12};
static const f32 RATE_TICKS[6]       = {96.0f, 48.0f, 32.0f, 24.0f, 16.0f, 12.0f};

int seq_bar_ticks(void)  { return METER_TICKS[st.seq[2] < 5 ? st.seq[2] : 0]; }
int seq_grid_ticks(void) { return GRID_TICKS[st.seq[4] < 3 ? st.seq[4] : 1]; }
int seq_accent(int tick)
{
    if (tick == 0) return 2;
    switch (st.seq[2]) {
    case 3: return tick == 144;
    case 4: return tick == 96 || tick == 192;
    default: return tick % 96 == 0;
    }
}
static int step_per_bar(void) { return seq_bar_ticks() / seq_grid_ticks(); }
static int tape_bars(void)    { return 1 << (st.seq[1] < 6 ? st.seq[1] : 2); }   /* 1..32 bars */
static int step_bars(void)
{
    int bars = tape_bars(), pb = step_per_bar();
    while (bars > 1 && bars * pb > NSTEP) bars >>= 1;
    return bars;
}
static int step_count(void) { return imin(NSTEP, step_bars() * step_per_bar()); }

/* ---- forward decl for tape recording from sequencers ---------------------------------- */
static void seq_to_tape(int note, int on, int vel);

/* ---- Sequencers: RIFF, SHAKE, Classic ARP ----------------------------------- */
static uint8_t held[16], held_vel[16];
static int nheld, arp_i, arp_stepn, arp_was;
static f32 arp_acc = 1e9f, arp_off;
static uint8_t arp_cur[16];
static int arp_ncur;

/* Riff state */
static int riff_rec, riff_at;

/* Shake 2D hexagonal physics cage */
typedef struct TBall {
    f32 x, y, vx, vy, hit, gate_t;
    uint8_t on, note, playing, bounces;
    uint8_t gone;                 /* bounced out: off the picture, its last note still ringing out its gate */
} TBall;
static TBall tballs[NSHAKE];
static f32 tomb_angle;

static void arp_release(void)
{
    int i;
    for (i = 0; i < arp_ncur; i++) {
        voice_off(arp_cur[i]);
        mout(0, arp_cur[i], 0);
        seq_to_tape(arp_cur[i], 0, 0);
    }
    arp_ncur = 0;
}

static void arp_play(int note, int vel)
{
    if (note < 0 || note > 127 || arp_ncur >= 16)
        return;
    voice_on(note, vel);
    mout(1, note, vel);
    seq_to_tape(note, 1, vel);
    arp_cur[arp_ncur++] = (uint8_t)note;
}

static void shake_drop(int note)
{
    int i, slot = 0;
    for (i = 0; i < NSHAKE; i++)
        if (!tballs[i].on) { slot = i; break; }
    if (i == NSHAKE)                             /* reuse a ball at random */
        slot = (int)(rng_next(&eng_rng) % NSHAKE);
    if (tballs[slot].playing) {
        voice_off(tballs[slot].note);
        mout(0, tballs[slot].note, 0);
        seq_to_tape(tballs[slot].note, 0, 0);
    }
    tballs[slot].on = 1;
    tballs[slot].note = (uint8_t)iclamp(note, 0, 127);
    tballs[slot].x = (rng_f(&eng_rng) - 0.5f) * 0.25f;
    tballs[slot].y = -0.32f;
    tballs[slot].vx = (rng_f(&eng_rng) - 0.5f) * 0.6f;
    tballs[slot].vy = 0.05f;
    tballs[slot].hit = 1.0f;
    tballs[slot].gate_t = 0.0f;
    tballs[slot].playing = 0;
    tballs[slot].bounces = 0;
    tballs[slot].gone = 0;
}

static void shake_clear(void)
{
    int i;
    for (i = 0; i < NSHAKE; i++) {
        if (tballs[i].playing) {
            voice_off(tballs[i].note);
            mout(0, tballs[i].note, 0);
            seq_to_tape(tballs[i].note, 0, 0);
        }
        tballs[i].on = tballs[i].playing = tballs[i].gone = 0;
    }
}

static void shake_tick(f32 dt)
{
    f32 grav = 0.35f + ((f32)st.shake[0] * 0.01f) * 4.2f;
    f32 spin = ((f32)st.shake[1] - 50.0f) * 0.02f * 0.55f;   /* turns/sec */
    f32 bounce = 0.42f + ((f32)st.shake[2] * 0.01f) * 0.56f;
    f32 cage_r = 0.58f + ((f32)st.shake[3] * 0.01f) * 0.32f;
    int i, s;

    tomb_angle = fractf(tomb_angle + spin * dt);

    for (i = 0; i < NSHAKE; i++) {
        TBall *b = &tballs[i];
        if (!b->on) continue;
        if (b->playing) {
            b->gate_t -= dt;
            if (b->gate_t <= 0.0f) {
                voice_off(b->note);
                mout(0, b->note, 0);
                seq_to_tape(b->note, 0, 0);
                b->playing = 0;
            }
        }
        if (b->gone) {                           /* (its note over, the ball is free) */
            if (!b->playing) b->on = b->gone = 0;
            continue;
        }
        b->hit = maxf(0.0f, b->hit - dt * 5.0f);
        b->vy += grav * dt;
        b->x += b->vx * dt;
        b->y += b->vy * dt;

        /* test collision against 6 rotating hexagonal walls */
        for (s = 0; s < 6; s++) {
            f32 ang = tomb_angle + (f32)s * (1.0f / 6.0f);
            f32 nx = cos_turns(ang), ny = sin_turns(ang);
            f32 dist = b->x * nx + b->y * ny;
            if (dist > cage_r) {
                f32 vn = b->vx * nx + b->vy * ny;
                /* push back inside wall */
                b->x -= nx * (dist - cage_r);
                b->y -= ny * (dist - cage_r);
                if (vn > 0.0f) {
                    f32 tx = -ny, ty = nx;
                    int vel = iclamp((int)(vn * 95.0f), 28, 127);
                    b->vx -= (1.0f + bounce) * vn * nx;
                    b->vy -= (1.0f + bounce) * vn * ny;
                    /* rotating cage imparts tangential kick! */
                    b->vx += tx * spin * 1.1f;
                    b->vy += ty * spin * 1.1f;
                    b->hit = 1.0f;
                    if (vn > 0.22f) {
                        if (b->playing) {
                            voice_off(b->note);
                            mout(0, b->note, 0);
                            seq_to_tape(b->note, 0, 0);
                        }
                        voice_on(b->note, vel);
                        mout(1, b->note, vel);
                        seq_to_tape(b->note, 1, vel);
                        b->playing = 1;
                        b->gate_t = 0.14f;
                        b->bounces++;
                        if (!st.keys[3] && nheld == 0 && b->bounces > 5) {
                            b->gone = 1;         /* out: the note it just struck still ends */
                            break;
                        }
                    }
                }
            }
        }
    }
}

static void arp_fire(const InstParams *ip, f32 len)
{
    int mode = ip->arp[0];
    f32 gate = (f32)ip->arp[3] * 0.01f;
    arp_release();
    if (!nheld) return;

    if (mode == ARP_RIFF) {
        if (st.riff_len == 0) {
            arp_play(held[nheld - 1], held_vel[nheld - 1]);
        } else {
            int step_v = st.riff[riff_at % st.riff_len];
            int trans = (int)held[nheld - 1] - (int)st.riff_root;
            if (step_v > 0) {
                int note = iclamp(step_v - 1 + trans, 0, 127);
                arp_play(note, held_vel[nheld - 1]);
            }
            riff_at = (riff_at + 1) % st.riff_len;
        }
        arp_off = gate >= 0.99f ? 1e9f : len * maxf(0.08f, gate);
        return;
    }

    /* Classic ARP modes */
    {
        uint8_t s[16], sv[16];
        int n = nheld, i, j, range = iclamp(ip->arp[2], 1, 4), total, idx = 0;
        for (i = 0; i < n; i++) { s[i] = held[i]; sv[i] = held_vel[i]; }
        for (i = 1; i < n; i++) {
            uint8_t x = s[i], xv = sv[i];
            for (j = i; j > 0 && s[j - 1] > x; j--) { s[j] = s[j - 1]; sv[j] = sv[j - 1]; }
            s[j] = x; sv[j] = xv;
        }
        total = n * range;
        if (mode == ARP_CHORD) {
            int oct = arp_i % range;
            for (i = 0; i < n; i++) arp_play(s[i] + 12 * oct, sv[i]);
            arp_i++;
        } else {
            switch (mode) {
            case ARP_DOWN: idx = total - 1 - (arp_i % total); break;
            case ARP_UPDN: {
                int per = total > 1 ? 2 * total - 2 : 1, p = arp_i % per;
                idx = p < total ? p : per - p;
                break;
            }
            case ARP_RAND: idx = (int)(rng_next(&eng_rng) % (uint32_t)total); break;
            default:       idx = arp_i % total; break;
            }
            arp_i++;
            arp_play(s[idx % n] + 12 * (idx / n), sv[idx % n]);
        }
        arp_off = gate >= 0.99f ? 1e9f : len * maxf(0.08f, gate);
    }
}

static void arp_tick(const InstParams *ip, f32 dticks, f32 dt)
{
    int mode = ip->arp[0];
    f32 step, sw, len;
    if (mode == ARP_SHAKE) {
        if (arp_was && arp_was != ARP_SHAKE) arp_release();
        arp_was = ARP_SHAKE;
        shake_tick(dt);
        return;
    }
    if (arp_was == ARP_SHAKE && mode != ARP_SHAKE)
        shake_clear();

    if (mode == ARP_OFF || (mode == ARP_RIFF && riff_rec)) {
        if (arp_was) {
            arp_release();
            arp_was = 0;
            nheld = 0;
        }
        return;
    }
    arp_was = mode;
    if (arp_ncur) {
        arp_off -= dticks;
        if (arp_off <= 0.0f) arp_release();
    }
    if (!nheld) {
        arp_acc = 1e9f;
        arp_i = 0;
        riff_at = 0;
        return;
    }
    step = RATE_TICKS[ip->arp[1] < 6 ? ip->arp[1] : 3];
    sw = (f32)st.glo[1] * 0.01f * 0.34f;
    len = ((arp_stepn - 1) & 1) ? step * (1.0f - sw) : step * (1.0f + sw);
    arp_acc += dticks;
    if (arp_acc >= len) {
        if (arp_acc > 1e8f) { arp_acc = 0.0f; arp_stepn = 0; }
        else arp_acc -= len;
        arp_fire(ip, (arp_stepn & 1) ? step * (1.0f - sw) : step * (1.0f + sw));
        arp_stepn++;
    }
}

static void route(int note, int on, int vel, int from_midi)
{
    int mode = st.ip[cur_i].arp[0], i;
    if (mode == ARP_RIFF && riff_rec) {
        if (on) {
            if (st.riff_len == 0) st.riff_root = (uint8_t)note;
            if (st.riff_len < NRIFF) st.riff[st.riff_len++] = (uint8_t)(note + 1);
            saved_n++;
            voice_on(note, vel);
        } else {
            voice_off(note);
        }
        if (!from_midi) mout(on, note, vel);
        return;
    }
    if (mode == ARP_SHAKE) {
        if (on) {
            if (nheld < 16) held[nheld++] = (uint8_t)note;
            shake_drop(note);
        } else {
            int found = 0;
            for (i = 0; i < nheld; i++)
                if (held[i] == note) {
                    for (; i + 1 < nheld; i++) held[i] = held[i + 1];
                    nheld--;
                    found = 1;
                    break;
                }
            if (!found) {                        /* (pressed before the mode changed: its own voice, */
                voice_off(note);                 /*  and the note on MIDI out it sent then) */
                if (!from_midi) mout(0, note, 0);
            }
        }
        return;
    }
    if (mode == ARP_OFF) {
        if (on) voice_on(note, vel);
        else voice_off(note);
        if (!from_midi) mout(on, note, vel);
        return;
    }
    if (on) {
        for (i = 0; i < nheld; i++)
            if (held[i] == note) { held_vel[i] = (uint8_t)vel; return; }
        if (nheld < 16) {
            held_vel[nheld] = (uint8_t)vel;
            held[nheld++] = (uint8_t)note;
        }
    } else {
        int found = 0;
        for (i = 0; i < nheld; i++)
            if (held[i] == note) {
                for (; i + 1 < nheld; i++) { held[i] = held[i + 1]; held_vel[i] = held_vel[i + 1]; }
                nheld--;
                found = 1;
                break;
            }
        if (!found) {                            /* (pressed before the mode changed: its own voice, */
            voice_off(note);                     /*  and the note on MIDI out it sent then) */
            if (!from_midi) mout(0, note, 0);
        }
    }
}

/* ---- 4-Track Virtual Tape Deck (notes + velocity + K1-K4 motion + Cut/Paste) ---------- */
enum { LEV_OFF = 0, LEV_ON = 1, LEV_AUTO = 2 };
typedef struct LEv {
    uint16_t tick;
    uint8_t d1;      /* note (0..127) or knob (0..3) */
    uint8_t flags;   /* bits 0..2 kind, bits 3..5 snap, bit 7 fresh */
    uint8_t trk;     /* 0..3 */
    // JSM remove uint8_t part;    /* PART(inst, algo) */
    uint8_t d2;      /* vel or knob value */
    uint8_t rsv;
} LEv;
static struct { uint8_t head[12]; LEv ev[NLEV]; } loop_mem;
#define lev loop_mem.ev
static int nlev;
typedef char a22_lev_8b[sizeof(LEv) == 8 && sizeof loop_mem == 12 + 8 * NLEV ? 1 : -1];

/* Cut/Paste clipboard (up to 512 events = 4 KB) */
#define NCLIP 512
static LEv clip_ev[NCLIP];
static int nclip;

static volatile uint32_t loop_gen, loop_full;
static volatile int loop_state = LOOP_EMPTY;
static volatile f32 loop_pos, loop_len = 1536.0f;
static uint8_t rec_part[128], rec_trk[128];
static f32 rec_shift[128];                       /* SNAP's move of an open note's on: its off moves as much */
static uint32_t rec_open[4];
static int nopen, loop_fresh;

static void rec_forget(void)
{
    rec_open[0] = rec_open[1] = rec_open[2] = rec_open[3] = 0;
    nopen = 0;
}

static int lev_find(f32 t)
{
    int lo = 0, hi = nlev;
    while (lo < hi) {
        int mid = (lo + hi) >> 1;
        if ((f32)lev[mid].tick < t) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

static LEv *lev_insert(uint16_t tick)
{
    int i = nlev;
    if (nlev >= NLEV) return 0;
    while (i > 0 && lev[i - 1].tick > tick) {
        lev[i] = lev[i - 1];
        i--;
    }
    nlev++;
    return &lev[i];
}

static void tape_bounds(f32 *in_t, f32 *out_t)
{
    f32 bt = (f32)seq_bar_ticks();
    int bars = tape_bars();
    f32 tot = (f32)bars * bt;
    int bi = st.tape[1] < bars ? st.tape[1] : 0;
    int bo = (st.tape[2] > bi && st.tape[2] <= bars) ? st.tape[2] : bars;
    loop_len = tot;
    *in_t = (f32)bi * bt;
    *out_t = (f32)bo * bt;
}

static void tape_release_seq(void)
{
    int n;
    for (n = 0; n < NVOICE; n++)
        if (voices[n].stage && voices[n].gate && voices[n].seq) {
            voices[n].gate = 0;
            voices[n].stage = 4;
            mout(0, voices[n].note, 0);
        }
}

static void tape_release_trk(int t)              /* one track's tape notes let go (over MIDI out too) */
{
    int n;
    for (n = 0; n < NVOICE; n++)
        if (voices[n].stage && voices[n].gate && voices[n].seq && (voices[n].trk & 3) == (t & 3)) {
            voices[n].gate = 0;
            voices[n].stage = 4;
            mout(0, voices[n].note, 0);
        }
}

static void tape_fire_ev(const LEv *e, int reverse)
{
    int kind = e->flags & 7;
    int trk = e->trk & 3;
    if (kind == LEV_AUTO) {
        int k = e->d1 & 3;
        trun[trk].auto_k[k] = e->d2 <= 100 ? e->d2 : 100;
        trun[trk].auto_on[k] = 1;
        return;
    }
    {
        int on = (kind == LEV_ON);
        //int part = part_ok(e->part) ? e->part : PART(st.trk[trk].inst, st.trk[trk].algo);
        int part = PART(st.trk[trk].inst, st.trk[trk].algo); // JSM Always ignore instrument changes, take the track instrument everytime
        int vel = e->d2 ? e->d2 : 110;
        if (reverse) on = !on;
        if (on && track_muted(trk))              /* (muted: no new notes, but offs still end what sounds) */
            return;
        if (on) {
            voice_on_ex(e->d1 & 127, part, trk, 1, vel);
            mout_channel(1, e->d1 & 127, vel, 1 + trk); // JSM
        } else {
            voice_off_ex(e->d1 & 127, trk, 1);
            mout_channel(0, e->d1 & 127, 0, 1 + trk); // JSM
        }
    }
}

static void tape_play_fwd(f32 a, f32 b)
{
    int i;
    for (i = lev_find(a); i < nlev && (f32)lev[i].tick < b; i++)
        //if (!(lev[i].flags & 0x80u))
        if (LEV_OK(lev[i]) && !(lev[i].flags & 0x80u)) // JSM
            tape_fire_ev(&lev[i], 0);
}

static void tape_play_rev(f32 hi, f32 lo)
{
    int i;
    for (i = lev_find(hi) - 1; i >= 0 && (f32)lev[i].tick >= lo; i--)
        //if (!(lev[i].flags & 0x80u))
        if (LEV_OK(lev[i]) && !(lev[i].flags & 0x80u)) // JSM
            tape_fire_ev(&lev[i], 1);
}

/* ---- metronome ------------------------------------------------------------------------ */
static f32 click_env, click_ph, click_ph2, click_f, click_pe, click_lp, click_amp;
static uint32_t click_rng = 0x7407C0DEu;
static int click_at = -1;
static f32 free_pos;
static int free_on;
static int click_mode(void) { return st.seq[5] < 3 ? st.seq[5] : CLICK_REC; }

static void click_run(f32 pos, int sound)
{
    int bt = seq_bar_ticks(), tick = ((int)pos % bt + bt) % bt, e = (int)pos / 24;
    if (e != click_at && sound && seq_accent(tick - tick % 24)) {
        click_env = 1.0f;
        click_pe = 1.0f;
        click_ph = click_ph2 = 0.0f;
        click_f = tick < 24 ? 440.0f : 310.0f;
        click_amp = tick < 24 ? 0.28f : 0.21f;
    }
    click_at = e;
}

static void tape_tick(f32 dticks)
{
    f32 in_t, out_t, a, b;
    int i, s = loop_state, rev = (st.tape[0] == TAPE_SPD_REV);
    if (s != LOOP_REC && s != LOOP_PLAY && s != LOOP_DUB)
        return;
    tape_bounds(&in_t, &out_t);
    if (st.tape[0] == TAPE_SPD_HALF) dticks *= 0.5f;
    else if (st.tape[0] == TAPE_SPD_2X) dticks *= 2.0f;

    a = loop_pos;
    if (a < in_t || a > out_t || (!rev && a == out_t)) {
        /* BARS turned down or IN/OUT moved while it plays: the playhead is outside the loop. It starts again at the
         * loop's beginning (its end in REV) and plays nothing on the way; once it fired every note at once, block
         * after block, until it came back inside. */
        tape_release_seq();
        for (i = 0; i < nlev; i++) lev[i].flags &= 0x7Fu;
        a = rev ? out_t : in_t;
        click_at = -1;
    }
    if (rev) {
        b = a - dticks;
        if (s != LOOP_REC) tape_play_rev(a, maxf(in_t, b));
        click_run(maxf(0.0f, b), s == LOOP_PLAY ? click_mode() == CLICK_ON : click_mode() != CLICK_OFF);
        if (b < in_t) {
            b += (out_t - in_t);
            tape_release_seq();
            for (i = 0; i < nlev; i++) lev[i].flags &= 0x7Fu;
            if (s == LOOP_REC) { loop_state = LOOP_PLAY; rec_forget(); track_snap_live(cur_trk); }
            click_at = -1;
            tape_play_rev(out_t, b);             /* (a take just closed too: its first note is there) */
        }
    } else {
        b = a + dticks;
        if (s != LOOP_REC) tape_play_fwd(a, minf(out_t, b));
        click_run(b, s == LOOP_PLAY ? click_mode() == CLICK_ON : click_mode() != CLICK_OFF);
        if (b >= out_t) {
            b -= (out_t - in_t);
            tape_release_seq();
            for (i = 0; i < nlev; i++) lev[i].flags &= 0x7Fu;
            if (s == LOOP_REC) { loop_state = LOOP_PLAY; rec_forget(); track_snap_live(cur_trk); }
            click_at = -1;
            tape_play_fwd(in_t, b);              /* (a take just closed too: the first key started it at IN, and that
                                                  * note was skipped on the first pass) */
        }
    }
    loop_pos = b;
}

static void tape_record_note(int note, int on, int vel)
{
    static const f32 Q[3] = {0.0f, 24.0f, 48.0f};
    f32 in_t, out_t, t = loop_pos, q = Q[st.seq[3] < 3 ? st.seq[3] : 0];
    int part, trk, kept;
    LEv *e;
    if (st.tape[0] == TAPE_SPD_REV)              /* (not in REV: its ons and offs would come out swapped) */
        return;
    tape_bounds(&in_t, &out_t);
    note &= 127;
    kept = (rec_open[note >> 5] >> (note & 31)) & 1;
    if (!on && !kept)                            /* (a key held into the take: an off alone could cut a note) */
        return;
    if (on) {
        if (kept || nlev + nopen + 2 > NLEV) {
            if (!kept) loop_full++;
            return;
        }
        part = live_part();
        trk = cur_trk;
        rec_part[note] = (uint8_t)part;
        rec_trk[note] = (uint8_t)trk;
        track_snap_live(trk);
    } else {
        part = rec_part[note] ? rec_part[note] : live_part();
        trk = rec_trk[note] & 3;
    }
    /* SNAP moves the on to the grid, and the off as far, so the note keeps its length and its off never comes before
     * its on (a short note played just early once sounded until the tape came round). Past the end: from IN. */
    if (on)
        rec_shift[note] = q > 0.0f ? (f32)ifloor(t / q + 0.5f) * q - t : 0.0f;
    t += rec_shift[note];
    if (t >= out_t) t -= out_t - in_t;
    if (kept) {
        rec_open[note >> 5] &= ~(1u << (note & 31));
        nopen--;
    }
    e = lev_insert((uint16_t)clampf(t, 0.0f, loop_len - 1.0f));
    if (!e) return;
    e->tick  = (uint16_t)clampf(t, 0.0f, loop_len - 1.0f);
    e->d1    = (uint8_t)note;
    //e->flags = (uint8_t)((on ? LEV_ON : LEV_OFF) | 0x80u);
    e->flags = LEV_MAKE_FLAGS((on ? LEV_ON : LEV_OFF), h1_cur_snap(), 1); //JSM
    e->trk   = (uint8_t)trk;
    // JSM e->part  = (uint8_t)part;
    e->d2    = (uint8_t)(on ? iclamp(vel, 1, 127) : 0);
    e->rsv   = 0;
    if (on) {
        rec_open[note >> 5] |= 1u << (note & 31);
        nopen++;
    }
    loop_gen++;
}

static void tape_record_auto(int knob, int val)
{
    LEv *e;
    uint16_t tick;
    if (st.seq[0] != SEQ_TAPE || (loop_state != LOOP_REC && loop_state != LOOP_DUB))
        return;
    if (nlev + nopen + 4 > NLEV)
        return;
    tick = (uint16_t)clampf(loop_pos, 0.0f, loop_len - 1.0f);
    e = lev_insert(tick);
    if (!e) return;
    e->tick  = tick;
    e->d1    = (uint8_t)(knob & 3);
    //e->flags = (uint8_t)(LEV_AUTO | 0x80u);
    e->flags = LEV_MAKE_FLAGS(LEV_AUTO , h1_cur_snap(), 1); //JSM
    e->trk   = (uint8_t)cur_trk;
    // JSM e->part  = live_part();
    e->d2    = (uint8_t)iclamp(val, 0, 100);
    e->rsv   = 0;
    trun[cur_trk].auto_k[knob & 3] = e->d2;
    trun[cur_trk].auto_on[knob & 3] = 1;
    loop_gen++;
}

static void seq_to_tape(int note, int on, int vel)
{
    if (st.seq[0] == SEQ_TAPE && (loop_state == LOOP_REC || loop_state == LOOP_DUB))
        tape_record_note(note, on, vel);
}

static void tape_stop(void)
{
    rec_forget();
    tape_release_seq();
    if (loop_state == LOOP_REC || loop_state == LOOP_DUB)
        track_snap_live(cur_trk);
    loop_state = nlev ? LOOP_IDLE : LOOP_EMPTY;
    loop_pos = (f32)(st.tape[1] * seq_bar_ticks());
}

/* CUT: the track in hand's events from IN to OUT go into the clip. A note that began before IN keeps its end on the
 * tape (or it would sound until the tape came round); a note still open at OUT gets an end in the clip; past what
 * the clip holds, events stay on the tape (they were lost); what the track sounds is let go. */
static void tape_cut(void)
{
    // JSM static uint8_t opart[128];
    uint32_t open[4] = {0u, 0u, 0u, 0u};
    f32 in_t, out_t;
    int i, w = 0, nop = 0, p;
    tape_bounds(&in_t, &out_t);
    nclip = 0;
    for (i = 0; i < nlev; i++) {
        const LEv *e = &lev[i];
        int kind = e->flags & 7, is_open;
        int in = (e->trk & 3) == cur_trk && (f32)e->tick >= in_t && (f32)e->tick < out_t;
        p = e->d1 & 127;
        is_open = (int)((open[p >> 5] >> (p & 31)) & 1u);
        if (in && kind == LEV_OFF && !is_open)
            in = 0;                              /* (the end of a note begun before IN, or one left on the tape) */
        if (in && kind != LEV_OFF && nclip + nop + (kind == LEV_ON && !is_open ? 2 : 1) > NCLIP)
            in = 0;                              /* (the clip is full: it stays on the tape. Every event but the end
                                                  * of a note it holds needs a place, and a new note one for its end
                                                  * too: knob moves took places unchecked until 0.4.0) */
        if (in) {
            clip_ev[nclip] = *e;
            clip_ev[nclip].tick = (uint16_t)((f32)e->tick - in_t);
            clip_ev[nclip].flags &= 0x7Fu;
            nclip++;
            if (kind == LEV_ON && !is_open) {
                open[p >> 5] |= 1u << (p & 31);
                //JSM opart[p] = e->part;
                nop++;
            } else if (kind == LEV_OFF) {
                open[p >> 5] &= ~(1u << (p & 31));
                nop--;
            }
        } else {
            lev[w++] = *e;
        }
    }
    for (p = 0; p < 128 && nop > 0; p++)         /* notes open at OUT: their ends, in the clip */
        if ((open[p >> 5] >> (p & 31)) & 1u) {
            LEv *c = &clip_ev[nclip++];
            c->tick = (uint16_t)maxf(0.0f, out_t - in_t - 1.0f);
            c->d1 = (uint8_t)p;
            c->flags = LEV_OFF;
            c->trk = (uint8_t)cur_trk;
            //JSM c->part = opart[p];
            c->d2 = 0;
            c->rsv = 0;
            nop--;
        }
    nlev = w;
    tape_release_trk(cur_trk);
    if (!nlev) loop_state = LOOP_EMPTY;
    loop_gen++;
}

static void tape_paste(void)
{
    int i, bt = seq_bar_ticks();
    uint32_t base_bar = (uint32_t)loop_pos / (uint32_t)bt;
    uint32_t base_tick = base_bar * (uint32_t)bt;
    f32 in_t, out_t;
    if (!nclip) return;
    if (nlev + nclip > NLEV) {                   /* (all of it or none: a part could leave an on without its off) */
        loop_full++;
        return;
    }
    tape_bounds(&in_t, &out_t);
    track_snap_live(cur_trk);
    base_tick = in_t;// JSM paste at in_t instead of loop_pos
    for (i = 0; i < nclip && nlev < NLEV; i++) {
        uint32_t tk = (base_tick + clip_ev[i].tick) % (uint32_t)loop_len;
        LEv *e = lev_insert((uint16_t)tk);
        if (!e) break;
        *e = clip_ev[i];
        e->tick = (uint16_t)tk;
        e->trk  = (uint8_t)cur_trk;
        //JSM e->part = live_part();
        e->flags &= 0x7Fu;
    }
    if (loop_state == LOOP_EMPTY && nlev) loop_state = LOOP_IDLE;
    loop_gen++;
}
//JSM
int eng_tape_ev_free(void)
{
    int used = nlev + nopen + 2;
    return used < NLEV ? (NLEV - used) : 0;
}
int eng_tape_note_info(int trk, int idx, uint16_t *tick, uint8_t *note, uint8_t *vel, int *total)
{
    int i, count = 0, found = 0;
    trk &= 3;
    for (i = 0; i < nlev; i++) {
        if ((lev[i].trk & 3) == trk && LEV_KIND(lev[i].flags) == LEV_ON && LEV_OK(lev[i])) {
            if (count == idx) {
                if (tick) *tick = lev[i].tick;
                if (note) *note = (uint8_t)(lev[i].d1 & 127u);
                if (vel)  *vel  = lev[i].d2 ? lev[i].d2 : 110u;
                found = 1;
            }
            count++;
        }
    }
    if (total) *total = count;
    return found;
}

void eng_tape_note_edit(int trk, int idx, int d_tick, int d_vel)
{
    int i, count = 0, on_pos = -1, off_pos = -1, max_t;
    uint16_t on_tick = 0;
    uint8_t target_note = 0;
    trk &= 3;
    if (idx < 0) return;

    for (i = 0; i < nlev; i++) {
        if ((lev[i].trk & 3) == trk && 
        LEV_KIND(lev[i].flags) == LEV_ON &&
        LEV_OK(lev[i])) {
            if (count == idx) {
                on_pos = i;
                on_tick = lev[i].tick;
                target_note = (uint8_t)(lev[i].d1 & 127u);
                break;
            }
            count++;
        }
    }
    if (on_pos < 0) return;

    if (d_vel) {
        int nv = iclamp((int)(lev[on_pos].d2 ? lev[on_pos].d2 : 110) + d_vel, 1, 127);
        lev[on_pos].d2 = (uint8_t)nv;
        saved_n++;
        loop_gen++;
    }

    if (d_tick) {
        LEv on_ev = lev[on_pos], off_ev;
        int has_off = 0, new_on, new_off, clamped_delta;

        for (i = on_pos + 1; i < nlev; i++) {
            if ((lev[i].trk & 3) == trk && (lev[i].d1 & 127u) == target_note &&
                (lev[i].flags & 7) == LEV_OFF && lev[i].tick >= on_tick) {
                off_pos = i;
                break;
            }
        }
        if (off_pos < 0) {
            for (i = 0; i < nlev; i++) {
                if (i != on_pos && (lev[i].trk & 3) == trk &&
                    (lev[i].d1 & 127u) == target_note && (lev[i].flags & 7) == LEV_OFF) {
                    off_pos = i;
                    break;
                }
            }
        }

        max_t = (int)loop_len > 0 ? (int)loop_len - 1 : 65535;
        new_on = iclamp((int)on_ev.tick + d_tick, 0, max_t);
        clamped_delta = new_on - (int)on_ev.tick;
        if (clamped_delta == 0) return;

        if (off_pos >= 0) {
            off_ev = lev[off_pos];
            new_off = iclamp((int)off_ev.tick + clamped_delta, new_on, max_t);
            off_ev.tick = (uint16_t)new_off;
            has_off = 1;
        }
        on_ev.tick = (uint16_t)new_on;

        /* retire on_pos et off_pos de lev[] */
        for (i = 0; i < nlev; i++) {
            if (i == on_pos || i == off_pos) continue;
            int dst = i;
            if (on_pos >= 0 && i > on_pos) dst--;
            if (off_pos >= 0 && i > off_pos) dst--;
            lev[dst] = lev[i];
        }
        if (on_pos >= 0) nlev--;
        if (off_pos >= 0) nlev--;

        /* réinsère on_ev puis off_ev en conservant l'ordre chronologique */
        i = nlev;
        while (i > 0 && lev[i - 1].tick > on_ev.tick) {
            lev[i] = lev[i - 1];
            i--;
        }
        lev[i] = on_ev;
        nlev++;

        if (has_off) {
            i = nlev;
            while (i > 0 && lev[i - 1].tick > off_ev.tick) {
                lev[i] = lev[i - 1];
                i--;
            }
            lev[i] = off_ev;
            nlev++;
        }

        saved_n++;
        loop_gen++;
    }
}
static void tape_del_note(int trk, int note_idx)
{
    int i, count = 0, on_pos = -1, off_pos = -1;
    uint16_t on_tick = 0;
    uint8_t target_note = 0;
    trk &= 3;
    if (note_idx < 0) return;

    for (i = 0; i < nlev; i++) {
        if ((lev[i].trk & 3) == trk && (lev[i].flags & 7) == LEV_ON) {
            if (count == note_idx) {
                on_pos = i;
                on_tick = lev[i].tick;
                target_note = (uint8_t)(lev[i].d1 & 127u);
                break;
            }
            count++;
        }
    }
    if (on_pos < 0) return;

    /* Find matching LEV_OFF on the same track and note at or after on_tick */
    for (i = on_pos + 1; i < nlev; i++) {
        if ((lev[i].trk & 3) == trk && (lev[i].d1 & 127u) == target_note &&
            (lev[i].flags & 7) == LEV_OFF && lev[i].tick >= on_tick) {
            off_pos = i;
            break;
        }
    }
    /* Fallback: if wrapped past loop end, look from start for earliest LEV_OFF */
    if (off_pos < 0) {
        for (i = 0; i < nlev; i++) {
            if (i != on_pos && (lev[i].trk & 3) == trk &&
                (lev[i].d1 & 127u) == target_note && (lev[i].flags & 7) == LEV_OFF) {
                off_pos = i;
                break;
            }
        }
    }

    voice_off_ex(target_note, trk, 1);
    mout(0, target_note, 0);

    for (i = 0; i < nlev; i++) {
        if (i == on_pos || i == off_pos) continue;
        if ((i > on_pos ? i - 1 : i) - (i > off_pos ? 1 : 0) < nlev) {
            int dst = i;
            if (on_pos >= 0 && i > on_pos) dst--;
            if (off_pos >= 0 && i > off_pos) dst--;
            lev[dst] = lev[i];
        }
    }
    if (on_pos >= 0) nlev--;
    if (off_pos >= 0) nlev--;
    if (!nlev) {
        loop_state = LOOP_EMPTY;
        loop_pos = 0.0f;
    }
    saved_n++;
    loop_gen++;
}
/* ---- 64-Step Pattern Sequencer -------------------------------------------------------- */
static volatile int step_playing, step_write, step_cursor, step_at;
static f32 step_acc, step_off;
static uint8_t step_snd[NSNOTE], step_st[NSNOTE];
static int step_nsnd, step_gesture;

static void step_release(void)
{
    int i;
    for (i = 0; i < step_nsnd; i++) {
        voice_off_ex(step_snd[i], step_st[i], 1);
        mout(0, step_snd[i], 0);
    }
    step_nsnd = 0;
}

static void step_fire(int i, f32 len)
{
    int j;
    step_release();
    for (j = 0; j < NSNOTE; j++)
        if (st.steps[i][j]) {
            int n = st.steps[i][j] - 1;
            int trk = st.step_trk[i][j] & 3;
            int vel = st.step_vel[i][j] ? st.step_vel[i][j] : 120;
            if (!track_muted(trk)) {
                int part = PART(st.trk[trk].inst, st.trk[trk].algo);
                voice_on_ex(n, part, trk, 1, vel);
                mout(1, n, vel);
                step_snd[step_nsnd] = (uint8_t)n;
                step_st[step_nsnd++] = (uint8_t)trk;
            }
        }
    step_off = len;
}

static f32 step_len(int i)
{
    f32 g = (f32)seq_grid_ticks(), sw = st.seq[4] < 2 ? (f32)st.glo[1] * 0.01f * 0.34f : 0.0f;
    return (i & 1) ? g * (1.0f - sw) : g * (1.0f + sw);
}

static void step_stop(void)
{
    step_release();
    step_playing = 0;
    step_at = 0;
    step_acc = 0.0f;
}

static void step_tick(f32 dticks)
{
    int n = step_count();
    if (step_nsnd) {
        step_off -= dticks;
        if (step_off <= 0.0f) step_release();
    }
    if (step_cursor >= n) step_cursor = 0;
    if (!step_playing) return;
    step_acc += dticks;
    if (step_at >= n || step_acc >= step_len(step_at)) {
        int at = step_at;
        step_acc = at >= n ? 0.0f : step_acc - step_len(at);
        at = at + 1 >= n ? 0 : at + 1;
        if (at == 0) click_at = -1;
        step_at = at;
        step_fire(at, step_len(at) * 0.85f);
    }
    click_run((f32)(step_at * seq_grid_ticks()) + step_acc, click_mode() == CLICK_ON);
}

static void step_put(int i, int note, int vel)
{
    int j;
    track_snap_live(cur_trk);
    saved_n++;
    for (j = 0; j < NSNOTE; j++)
        if (st.steps[i][j] == note + 1 && st.step_trk[i][j] == cur_trk) {
            st.step_vel[i][j] = (uint8_t)iclamp(vel, 1, 127);
            return;
        }
    for (j = 0; j < NSNOTE - 1 && st.steps[i][j]; j++) ;
    st.steps[i][j] = (uint8_t)(note + 1);
    st.step_trk[i][j] = (uint8_t)cur_trk;
    st.step_vel[i][j] = (uint8_t)iclamp(vel, 1, 127);
}

static void step_clear_trk(int i, int trk)
{
    int j, k = 0;
    saved_n++;
    for (j = 0; j < NSNOTE; j++)
        if (st.steps[i][j] && (trk >= 0 && st.step_trk[i][j] != trk)) {
            st.steps[i][k] = st.steps[i][j];
            st.step_trk[i][k] = st.step_trk[i][j];
            st.step_vel[i][k] = st.step_vel[i][j];
            k++;
        }
    for (; k < NSNOTE; k++)
        st.steps[i][k] = st.step_trk[i][k] = st.step_vel[i][k] = 0;
}

static void step_record(int note, int on, int vel)
{
    int n = step_count();
    if (step_playing) {
        if (on) {
            int i = step_at;
            if (step_acc > step_len(i) * 0.5f) i = i + 1 >= n ? 0 : i + 1;
            step_put(i, note, vel);
        }
        return;
    }
    if (on) {
        if (step_gesture++ == 0) step_clear_trk(step_cursor, cur_trk);
        step_put(step_cursor, note, vel);
    } else if (step_gesture > 0 && --step_gesture == 0) {
        step_cursor = step_cursor + 1 >= n ? 0 : step_cursor + 1;
    }
}

static void free_tick(f32 dticks)
{
    int cm = click_mode(), s = loop_state, run;
    if (st.seq[0] == SEQ_STEP) run = !step_playing && cm == CLICK_ON;
    else run = (s == LOOP_ARMED && cm != CLICK_OFF) || ((s == LOOP_EMPTY || s == LOOP_IDLE) && cm == CLICK_ON);
    if (!run) { free_on = 0; return; }
    if (!free_on) { free_on = 1; free_pos = 0.0f; click_at = -1; }
    else {
        free_pos += dticks;
        if (free_pos >= (f32)seq_bar_ticks()) { free_pos -= (f32)seq_bar_ticks(); click_at = -1; }
    }
    click_run(free_pos, 1);
}

/* ---- live note handling + hold latch -------------------------------------------------- */
static uint32_t latched[4], latched_midi[4];
static uint32_t midi_down[4];                    /* notes held down over MIDI in now (EV_MIDI_OFF) */
static int phys_down;

static void eff_note(int note, int on, int vel, int from_midi)
{
    if (st.seq[0] == SEQ_STEP) {
        if (step_write) step_record(note, on, vel);
        route(note, on, vel, from_midi);
        return;
    }
    if (on && loop_state == LOOP_ARMED) {
        f32 in_t, out_t;
        int i;
        tape_bounds(&in_t, &out_t);
        if (loop_fresh && nlev) {
            for (i = 0; i < nlev; i++) lev[i].flags &= 0x7Fu;
            loop_pos = st.tape[0] == TAPE_SPD_REV ? out_t : in_t;
            click_at = -1;
            loop_state = LOOP_DUB;
        } else {
            if (loop_fresh) { nlev = 0; rec_forget(); loop_gen++; }
            loop_pos = st.tape[0] == TAPE_SPD_REV ? out_t : in_t;
            click_at = -1;
            loop_state = LOOP_REC;
        }
        arp_acc = 1e9f;
    }
    if ((loop_state == LOOP_REC || loop_state == LOOP_DUB) && st.ip[cur_i].arp[0] == ARP_OFF)
        tape_record_note(note, on, vel);
    route(note, on, vel, from_midi);
}

/* ---- chord memory: one key, a chord ----------------------------------------------------
 * A key (not MIDI in) becomes up to CHORD_MAX notes, each going on through eff_note, so the tape, the steps,
 * the arpeggiator and MIDI out all see the chord's notes. Each key keeps the notes it started in a slot, so
 * letting it go ends exactly those, whatever the settings are by then. A note two held chords share sounds
 * once and ends with the last of them (ch_cnt). STRUM sends the notes upward a gap apart, counted in blocks
 * (blkno), and a key let go mid-strum drops what had not sounded yet. */
static const int8_t CH_SHAPE[CHORD_NSHAPE][4] = {          /* semitones; -1 none */
    {0, 4, 7, -1}, {0, 3, 7, -1}, {0, 5, 7, -1}, {0, 4, 7, 10},   /* MAJ MIN SUS4 7 */
    {0, 3, 7, 10}, {0, 4, 7, 11}, {0, 7, 12, -1}, {0, 12, -1, -1}, /* MIN7 MAJ7 POWER OCT */
};
static const int8_t CH_DEGREE[CHORD_NSCALE][4] = {         /* steps of the scale above the root */
    {0, 2, 4, -1}, {0, 2, 4, 6}, {0, 1, 4, -1},                 /* TRIAD 7TH SUS2 */
    {0, 3, 4, -1}, {0, 2, 4, 8}, {0, 2, 4, 5},                  /* SUS4 ADD9 6TH */
};
#define CH_SLOTS 24
typedef struct ChSlot {
    uint8_t used, root, midi, vel, n, sent;                    /* sent: bit i, note i has gone on */
    uint8_t note[CHORD_MAX];
    uint32_t due[CHORD_MAX];                                   /* blkno at which note i goes on */
} ChSlot;
static ChSlot chs[CH_SLOTS];
static uint8_t ch_cnt[128];

static int ch_scale_up(int root, int steps)                  /* the note `steps` scale notes above root */
{
    uint32_t mask = SCALE_MASK[st.keys[1] < 8 ? st.keys[1] : 0];
    int r = st.keys[0] % 12, n = root, k = 0;
    if (mask == 0xFFFu) mask = SCALE_MASK[1];                /* CHROM has no chords of its own: major */
    while (k < steps && n < 160) {
        n++;
        if (mask & (1u << (((n - r) % 12 + 12) % 12))) k++;
    }
    return n;
}

static int chord_build(int root, uint8_t *out)
{
    int nt[CHORD_MAX + 1], n = 0, i, j, mode = st.chord[0], m = 0;
    if (mode == CHORD_OFF || insts[cur_i]->mono) {
        out[0] = (uint8_t)root;
        return 1;
    }
    if (mode == CHORD_MEMORY) {
        for (i = 0; i < st.chord_n && i < CHORD_MAX; i++) nt[n++] = root + st.chord_mem[i];
    } else if (mode == CHORD_SCALE) {
        const int8_t *d = CH_DEGREE[st.chord[1] % CHORD_NSCALE];
        for (i = 0; i < 4 && d[i] >= 0; i++) nt[n++] = ch_scale_up(root, d[i]);
    } else {
        const int8_t *d = CH_SHAPE[st.chord[1] % CHORD_NSHAPE];
        for (i = 0; i < 4 && d[i] >= 0; i++) nt[n++] = root + d[i];
    }
    for (i = 1; i < n; i++)                                  /* low to high */
        for (j = i; j > 0 && nt[j - 1] > nt[j]; j--) { int t = nt[j]; nt[j] = nt[j - 1]; nt[j - 1] = t; }
    if (st.chord[2] == 1 && n >= 3) nt[1] += 12;             /* OPEN */
    else if (st.chord[2] == 2 && n >= 3) nt[n - 2] -= 12;    /* DROP2 */
    else if (st.chord[2] == 3 && n < CHORD_MAX) nt[n++] = root - 12;   /* +BASS */
    for (i = 1; i < n; i++)
        for (j = i; j > 0 && nt[j - 1] > nt[j]; j--) { int t = nt[j]; nt[j] = nt[j - 1]; nt[j - 1] = t; }
    for (i = 0; i < n; i++)                                  /* on the keyboard, each once */
        if (nt[i] >= 0 && nt[i] <= 127 && (m == 0 || out[m - 1] != nt[i]))
            out[m++] = (uint8_t)nt[i];
    if (!m) out[m++] = (uint8_t)iclamp(root, 0, 127);
    return m;
}

static void ch_on(const ChSlot *c, int i)
{
    int n = c->note[i];
    if (ch_cnt[n]++ == 0) eff_note(n, 1, c->vel, c->midi);
    if (!ch_cnt[n]) ch_cnt[n] = 255;
}

static void ch_release(ChSlot *c)
{
    int i;
    for (i = 0; i < c->n; i++) {
        int n = c->note[i];
        if (!(c->sent & (1u << i))) continue;                /* (never sounded: nothing to end) */
        if (ch_cnt[n] && --ch_cnt[n] == 0) eff_note(n, 0, 0, c->midi);
    }
    c->used = 0;
}

static void chord_key(int note, int on, int vel, int from_midi)
{
    ChSlot *c = 0;
    uint32_t gap;
    int i;
    for (i = 0; i < CH_SLOTS; i++)                           /* a key already holding a chord lets it go */
        if (chs[i].used && chs[i].root == note && chs[i].midi == (uint8_t)from_midi) {
            ch_release(&chs[i]);
            if (!on) return;
        }
    if (!on) { eff_note(note, 0, 0, from_midi); return; }
    if (from_midi || st.chord[0] == CHORD_OFF) { eff_note(note, 1, vel, from_midi); return; }
    for (i = 0; i < CH_SLOTS && !c; i++)
        if (!chs[i].used) c = &chs[i];
    if (!c) { eff_note(note, 1, vel, from_midi); return; }  /* (24 chords held: the key plays alone) */
    c->used = 1;
    c->root = (uint8_t)note;
    c->midi = (uint8_t)from_midi;
    c->vel = (uint8_t)vel;
    c->n = (uint8_t)chord_build(note, c->note);
    c->sent = 0;
    gap = (uint32_t)((f32)st.chord[3] * 0.0006f / ((f32)CTRL * ISR) + 0.5f);   /* 0..60 ms, in blocks */
    for (i = 0; i < c->n; i++) {
        c->due[i] = blkno + gap * (uint32_t)i;
        if (i == 0 || !gap) {
            c->sent |= (uint8_t)(1u << i);
            ch_on(c, i);
        }
    }
}

static void chord_tick(void)                                 /* the strum's notes as they come due */
{
    int s, i;
    for (s = 0; s < CH_SLOTS; s++) {
        ChSlot *c = &chs[s];
        if (!c->used) continue;
        for (i = 1; i < c->n; i++)
            if (!(c->sent & (1u << i)) && (int32_t)(blkno - c->due[i]) >= 0) {
                c->sent |= (uint8_t)(1u << i);
                ch_on(c, i);
            }
    }
}

static void chord_clear(void)                                /* (the voices are let go elsewhere) */
{
    int i;
    for (i = 0; i < CH_SLOTS; i++) chs[i].used = 0;
    for (i = 0; i < 128; i++) ch_cnt[i] = 0;
}

static void unlatch(void)
{
    int n;
    for (n = 0; n < 128; n++)
        if (latched[n >> 5] & (1u << (n & 31)))
            chord_key(n, 0, 0, (int)(latched_midi[n >> 5] >> (n & 31) & 1u));
    for (n = 0; n < 4; n++) latched[n] = latched_midi[n] = 0;
}

static void snap_save(int idx)
{
    Snap *sn = &st.snap[idx & 7];
    int k, inst = cur_i, algo = st.ip[inst].algo & 7;
    saved_n++;
    sn->used = 1;
    sn->inst = (uint8_t)inst;
    sn->algo = (uint8_t)algo;
    sn->fx_type = st.fx_type;
    for (k = 0; k < 4; k++) {
        sn->knob[k] = KNOBS(inst, algo)[k];
        sn->env[k]  = st.ip[inst].env[k];
        sn->menv[k] = st.ip[inst].menv[k];
        sn->lfo[k]  = st.ip[inst].lfo[k];
        sn->fx_p[k] = st.fx_p[st.fx_type][k];
    }
}

static void snap_load(int idx)
{
    const Snap *sn = &st.snap[idx & 7];
    int k;
    if (!sn->used || sn->inst >= NINST) return;
    cur_i = sn->inst;
    st.inst = sn->inst;
    st.ip[cur_i].algo = sn->algo & 7;
    st.fx_type = sn->fx_type < NFX ? sn->fx_type : 0;
    for (k = 0; k < 4; k++) {
        KNOBS(cur_i, st.ip[cur_i].algo)[k] = sn->knob[k];
        st.ip[cur_i].env[k]  = sn->env[k];
        st.ip[cur_i].menv[k] = sn->menv[k];
        st.ip[cur_i].lfo[k]  = sn->lfo[k];
        st.fx_p[st.fx_type][k] = sn->fx_p[k];
    }
    track_snap_live(cur_trk);
}

static void handle(uint32_t e)
{
    int type = (int)((e >> 8) & 255u), a = (int)(e & 255u);
    int vel = (int)((e >> 16) & 127u), from_midi = (int)(e >> 23 & 1u);
    if (!vel) vel = 127;

    switch (type) {
    case EV_NOTE_ON:
        if (from_midi) midi_down[a >> 5] |= 1u << (a & 31);
        if (st.keys[3]) {
            if (phys_down == 0) unlatch();
            latched[a >> 5] |= 1u << (a & 31);
            if (from_midi) latched_midi[a >> 5] |= 1u << (a & 31);
            else latched_midi[a >> 5] &= ~(1u << (a & 31));
        }
        phys_down++;
        chord_key(a, 1, vel, from_midi);
        break;
    case EV_NOTE_OFF:
        if (from_midi) midi_down[a >> 5] &= ~(1u << (a & 31));
        if (phys_down > 0) phys_down--;
        if (!st.keys[3]) chord_key(a, 0, 0, from_midi);
        break;
    case EV_MIDI_OFF: {
        /* MIDI's all notes off, which a DAW sends on stop and a USB keyboard unplugged sends here: the notes MIDI
         * holds, or HOLD keeps for it, end. The tape, the pattern and the panel's own keys go on; it was a full hush
         * (EV_PANIC), which stopped the tape. */
        int n;
        for (n = 0; n < 128; n++) {
            uint32_t bit = 1u << (n & 31);
            if (midi_down[n >> 5] & bit) {
                midi_down[n >> 5] &= ~bit;
                if (phys_down > 0) phys_down--;
                if (!st.keys[3]) chord_key(n, 0, 0, 1);
            }
            if ((latched[n >> 5] & bit) && (latched_midi[n >> 5] & bit)) {
                latched[n >> 5] &= ~bit;
                latched_midi[n >> 5] &= ~bit;
                chord_key(n, 0, 0, 1);
            }
        }
        break;
    }
    case EV_UNLATCH:
        unlatch();
        break;
    case EV_PANIC:
        midi_down[0] = midi_down[1] = midi_down[2] = midi_down[3] = 0;
        unlatch();
        arp_release();
        shake_clear();
        tape_stop();
        step_stop();
        voices_release_all();
        chord_clear();
        nheld = phys_down = 0;
        if (!a && eng_midi_fn) eng_midi_fn(0xB0u, 123u, 0u);
        break;
    case EV_INST:
        if (a < NINST) {
            cur_i = a;
            st.inst = (uint8_t)a;
            st.trk[cur_trk].inst = (uint8_t)a;
            st.trk[cur_trk].algo = st.ip[a].algo & 7;
        }
        break;
    case EV_TRACK:
        if (a < NTRACK) {
            track_snap_live(cur_trk);
            track_sync_to_live(a);
        }
        break;
    case EV_AUTOMATION:
        tape_record_auto(a & 3, (int)((e >> 16) & 127u));
        break;
    case EV_CUT:
        tape_cut();
        break;
    case EV_PASTE:
        tape_paste();
        break;
    case EV_DEL_NOTE: // JSM
        tape_del_note((int)((e >> 17) & 3u), a);
        break;        
    case EV_RIFF_REC:
        riff_rec = !riff_rec;
        if (riff_rec) { st.riff_len = 0; riff_at = 0; saved_n++; }
        break;
    case EV_RIFF_CLEAR:
        st.riff_len = 0;
        riff_at = 0;
        saved_n++;
        break;
    case EV_RIFF_REST:
        if (riff_rec && st.riff_len < NRIFF) {
            st.riff[st.riff_len++] = 0;
            saved_n++;
        }
        break;
    case EV_SNAP_SAVE:
        snap_save(a & 7);
        break;
    case EV_SNAP_LOAD:
        snap_load(a & 7);
        break;
    case EV_REC:
        if (st.seq[0] == SEQ_STEP) {
            step_write = !step_write;
            step_gesture = 0;
            break;
        }
        if ((loop_state == LOOP_EMPTY || loop_state == LOOP_IDLE || loop_state == LOOP_PLAY) &&
            st.tape[0] == TAPE_SPD_REV)
            break;                               /* (no recording in REV: the panel says so) */
        if (loop_state == LOOP_EMPTY || loop_state == LOOP_IDLE) {
            loop_fresh = 1;
            loop_state = LOOP_ARMED;
        } else if (loop_state == LOOP_ARMED) {
            loop_state = nlev ? LOOP_IDLE : LOOP_EMPTY;
        } else if (loop_state == LOOP_REC || loop_state == LOOP_DUB) {
            int i;
            for (i = 0; i < nlev; i++) lev[i].flags &= 0x7Fu;
            rec_forget();
            track_snap_live(cur_trk);
            loop_state = nlev ? LOOP_PLAY : LOOP_EMPTY;
        } else if (loop_state == LOOP_PLAY) {
            loop_state = LOOP_DUB;
        }
        break;
    case EV_PLAY:
        if (st.seq[0] == SEQ_STEP) {
            if (step_playing) step_stop();
            else {
                step_playing = 1;
                step_at = 0;
                step_acc = 0.0f;
                click_at = -1;
                step_fire(0, step_len(0) * 0.85f);
            }
            break;
        }
        if (loop_state == LOOP_PLAY || loop_state == LOOP_DUB || loop_state == LOOP_REC || loop_state == LOOP_ARMED) {
            tape_stop();
        } else if (nlev) {
            f32 in_t, out_t;
            int i;
            tape_bounds(&in_t, &out_t);
            for (i = 0; i < nlev; i++) lev[i].flags &= 0x7Fu;
            loop_pos = st.tape[0] == TAPE_SPD_REV ? out_t : in_t;
            click_at = -1;
            loop_state = LOOP_PLAY;
        }
        break;
    case EV_CLEAR:
        if (st.seq[0] == SEQ_STEP) {
            int i;
            step_release();
            for (i = 0; i < NSTEP; i++)
                step_clear_trk(i, a ? (a - 1) : -1);
            step_cursor = 0;
        } else if (a) {
            /* clear only one track (a - 1) */
            int t = (a - 1) & 3, i, w = 0;
            tape_release_trk(t);                 /* (that track's notes, not every track's) */
            if (t == cur_trk) rec_forget();
            for (i = 0; i < nlev; i++)
                // if ((lev[i].trk & 3) != t) lev[w++] = lev[i]; // JSM
                if ( ((lev[i].trk & 3) != t) || !LEV_OK(lev[i]) ) lev[w++] = lev[i]; // JSM
            nlev = w;
            for (i = 0; i < 4; i++) trun[t].auto_on[i] = 0;
            if (!nlev) { loop_state = LOOP_EMPTY; loop_pos = 0.0f; }
            loop_gen++;
        } else {
            int t, k;
            tape_stop();
            nlev = 0;
            loop_state = LOOP_EMPTY;
            for (t = 0; t < NTRACK; t++)
                for (k = 0; k < 4; k++) trun[t].auto_on[k] = 0;
            loop_gen++;
        }
        break;
    case EV_STEP_MOVE: {
        int n = step_count();
        step_cursor = ((step_cursor + (a - 128)) % n + n) % n;
        break;
    }
    case EV_STEP_REST:
        if (st.seq[0] == SEQ_STEP && step_write) {
            step_clear_trk(step_cursor, cur_trk);
            step_cursor = (step_cursor + 1) % step_count();
        }
        break;
    case EV_SEQ_MODE:
        tape_stop();
        step_stop();
        step_write = 0;
        break;
    default: break;
    }
}

/* ---- Dual-Core Stereo Voice Block Render ---------------------------------------------- */
static f32 vt_l[NVOICE][CTRL], vt_r[NVOICE][CTRL];
static struct {
    const InstDef *def;
    const Blk *b;
    f32 att, kd, sus, kr, mkd, kg, dt;
    f32 gain, panmod;                            /* the part's gain (engine, track LFO, algorithm) and pan LFO */
} vj;
static int (*c2_run)(void (*fn)(uint32_t), uint32_t arg);
static int (*c2_wait)(void);

void eng_two_cores(int (*run)(void (*fn)(uint32_t), uint32_t arg), int (*wait)(void))
{
    c2_run = run;
    c2_wait = wait;
    fx_two_cores(run, wait);                    /* (PLATE's two halves: fx.c) */
}

/* A voice's output as it goes into its track: its envelope ramped across the block, its gain (velocity) and pan, the
 * declick of a voice taken over, written back over vt_l / vt_r. Run by the core that rendered the voice, so the first
 * core only adds the voices up, in voice order (block): the sum is the same to the bit as when it did all of it. */
H1_HOT static void voice_out(Voice *v, int i)
{
    f32 e0 = v->env_prev, e1 = v->env, de, g, gl, gr, pan, pl, pr, tail_l, tail_r, sll = 0.0f, srr = 0.0f;
    f32 *ol = vt_l[i], *or_ = vt_r[i];
    int n;
    de = (e1 - e0) * (1.0f / (f32)CTRL);
    /* velocity dynamics curve: expressive but retains low-velocity presence */
    g = vj.gain * (0.22f + 0.78f * v->vel_f * v->vel_f);
    pan = clampf(v->pan + vj.panmod, -1.0f, 1.0f);
    pan_gains(pan, &pl, &pr);
    gl = g * pl;
    gr = g * pr;
    if (v->declick) {
        v->tail_l = v->last_l - ol[0] * e0 * gl;
        v->tail_r = v->last_r - or_[0] * e0 * gr;
        v->declick = 0;
    }
    tail_l = v->tail_l;
    tail_r = v->tail_r;
    for (n = 0; n < CTRL; n++) {
        sll = ol[n] * e0 * gl + tail_l;
        srr = or_[n] * e0 * gr + tail_r;
        e0 += de;
        tail_l *= 0.984f;
        tail_r *= 0.984f;
        ol[n] = sll;
        or_[n] = srr;
    }
    if (absf(tail_l) < 1e-6f) tail_l = 0.0f;
    if (absf(tail_r) < 1e-6f) tail_r = 0.0f;
    v->tail_l = tail_l;
    v->tail_r = tail_r;
    v->last_l = sll;
    v->last_r = srr;
    if (v->stage == 4 && v->env <= 0.0f) {
        v->stage = 0;
        v->last_l = v->last_r = v->tail_l = v->tail_r = 0.0f;
    }
}

static void voice_block(int i)
{
    Voice *v = &voices[i];
    f32 e0 = v->env, m0 = v->menv;
    switch (v->stage) {
    case 1:
        v->env += vj.att;
        if (v->env >= 1.0f) { v->env = 1.0f; v->stage = 2; }
        break;
    case 2:
    case 3:
        v->env += (vj.sus - v->env) * vj.kd;
        break;
    default:
        if (v->shed)                             /* (eng_late: gone in 14 blocks, 10 ms, a ramp: no click) */
            v->env -= v->env * 0.35f + 0.001f;
        else
            v->env -= v->env * vj.kr + 0.00002f;
        if (v->env <= 0.0003f) v->env = 0.0f;
        break;
    }
    v->env_prev = e0;
    v->menv_prev = m0;
    v->menv -= v->menv * vj.mkd;
    if (v->menv < 0.0002f) v->menv = 0.0f;
    v->pitch += (v->target - v->pitch) * vj.kg;
    vj.def->render(v, vj.b, vt_l[i], vt_r[i], CTRL);
    v->age += vj.dt;
    H1_FAR(voice_out)(v, i);                     /* (from RAM: H1_HOT) */
}

/* Whether the port's last audio half went out late (rendered after the DMA had moved on), told once a render from where
 * h1_render is called, so in the engine's own context. Late half after half, the main loop starves too (the screen, the
 * keys and MASTER stop). Two late halves in a row, not a near miss and not one passing late half: one voice lets go
 * fast. It is the quietest of those already let go; else the oldest of what the player is not holding (the tape's
 * notes, another track's); else the oldest of all. One at a time: the next only once that one has died away. */
static uint32_t late_run, ovl_shed;
void eng_late(int late)
{
    Voice *v = 0;
    int i;
    late_run = late ? late_run + 1u : 0u;
    if (late_run < 2u)
        return;
    for (i = 0; i < NVOICE; i++)
        if (voices[i].stage && voices[i].shed)
            return;                              /* (one still dying away) */
    for (i = 0; i < NVOICE; i++)                 /* the quietest of those let go */
        if (voices[i].stage == 4 && (!v || voices[i].env < v->env))
            v = &voices[i];
    if (!v)
        for (i = 0; i < NVOICE; i++)             /* else the oldest not under the player's hands */
            if (voices[i].stage && (voices[i].seq || voices[i].trk != cur_trk) && (!v || voices[i].stamp < v->stamp))
                v = &voices[i];
    if (!v)
        for (i = 0; i < NVOICE; i++)             /* else the oldest */
            if (voices[i].stage && (!v || voices[i].stamp < v->stamp))
                v = &voices[i];
    if (!v)
        return;
    v->gate = 0;
    v->stage = 4;
    v->shed = 1;
    ovl_shed++;
}

uint32_t eng_shed_count(void) { return ovl_shed; }

static void voices_job(uint32_t mask)
{
    int i;
    for (i = 0; i < NVOICE; i++)
        if (mask >> i & 1u)
            voice_block(i);
}

static f32 sub_l, sub_r;
#define SUB_K (H1_TAU * 18.0f * ISR)

/* ---- where a render's time goes (h1_stages) -------------------------------------------------------------------
 * stg_mark(k) gives the time since the last mark to stage k. A render's stages add up in stg_acc; the render with the
 * largest total keeps its stages in stg_worst until h1_stages takes them. Nothing is read without a clock. */
static uint32_t (*stg_clock)(void);
static uint32_t stg_t0, stg_acc[H1_NSTG], stg_worst[H1_NSTG], stg_worst_total;

static inline void stg_mark(int k)
{
    if (stg_clock) {
        uint32_t t = stg_clock();
        stg_acc[k] += t - stg_t0;
        stg_t0 = t;
    }
}

void eng_stage_clock(uint32_t (*clock)(void)) { stg_clock = clock; }

uint32_t eng_stages(uint32_t out[H1_NSTG])
{
    uint32_t total = stg_worst_total;
    int k;
    for (k = 0; k < H1_NSTG; k++) {
        out[k] = stg_worst[k];
        stg_worst[k] = 0;
    }
    stg_worst_total = 0;
    return total;
}

static void block(f32 *L, f32 *R)
{
    static f32 sl[CTRL], sr[CTRL], tl[CTRL], tr[CTRL];
    f32 dt = (f32)CTRL * ISR, bpm, dticks, kg, peak = 0.0f;
    int i, n, t, live;

    stg_mark(H1_STG_MASTER);                    /* (the limiter and output since the last block) */
    blkno++;
    bpm = (f32)iclamp(st.glo[0], 40, 240);
    dticks = bpm * (96.0f / 60.0f) * dt;
    beat_out = fractf(beat_out + dticks * (1.0f / 96.0f));
    blk_time = run_time;
    run_time += dt;

    live = live_part();
    live_blk = *track_set(cur_trk, live, 0);
    lfo_out = live_blk.lfo;

    while (ev_r != ev_w) {
        uint32_t e = evq[ev_r];
        ev_r = (ev_r + 1u) & 63u;
        handle(e);
    }
    chord_tick();

    if (st.seq[0] == SEQ_STEP) step_tick(dticks);
    else tape_tick(dticks);
    free_tick(dticks);
    arp_tick(&st.ip[cur_i], dticks, dt);

    stg_mark(H1_STG_SEQ);
    kg = st.keys[2] ? onepole_k(0.004f * exp2fx((f32)st.keys[2] * 0.08f), SR / (f32)CTRL) : 1.0f;

    for (n = 0; n < CTRL; n++)
        L[n] = R[n] = sl[n] = sr[n] = 0.0f;

    /* Render track by track (T1..T4) with per-track volume, pan, mute/solo, VU & FX send! */
    for (t = 0; t < NTRACK; t++) {
        TRun *r = &trun[t];
        uint16_t keys[NVOICE];
        int nk = 0, p, j;
        f32 tpeak = 0.0f;
        f32 tv = track_muted(t) ? 0.0f : ((f32)st.trk[t].vol * 0.01f);
        f32 ts = (f32)st.trk[t].send * 0.01f;
        f32 tp = ((f32)st.trk[t].pan - 50.0f) * 0.02f;
        f32 g_vol, p_l, p_r;

        tv = tv * tv;
        r->vol_s  += (tv - r->vol_s) * 0.08f;
        r->send_s += (ts - r->send_s) * 0.08f;
        r->pan_s  += (tp - r->pan_s) * 0.08f;

        for (i = 0; i < NVOICE; i++) {
            uint16_t k;
            if (!voices[i].stage || voices[i].trk != t) continue;
            k = (uint16_t)(voices[i].part | ((uint16_t)voices[i].seq << 8));
            for (j = 0; j < nk && keys[j] != k; j++) ;
            if (j == nk) keys[nk++] = k;
        }
        if (!nk) {
            r->peak_out *= 0.88f;
            if (t == cur_trk)
                for (n = 0; n < CTRL; n++) own_scope[(scope_w + n) & (SCOPE_N - 1)] = 0.0f;
            continue;
        }
        for (n = 0; n < CTRL; n++) tl[n] = tr[n] = 0.0f;

        for (p = 0; p < nk; p++) {
            int part = keys[p] & 255, is_seq = keys[p] >> 8;
            int inst = PART_INST(part), algo = PART_ALGO(part);
            const InstDef *def = insts[inst];
            const Blk *b = track_set(t, part, is_seq);
            uint32_t mine = 0, other = 0, all = 0;
            int nv = 0, odd;

            for (i = 0; i < NVOICE; i++)
                if (voices[i].stage && voices[i].trk == t &&
                    voices[i].part == part && voices[i].seq == is_seq) {
                    all |= 1u << i;
                    nv++;
                }
            /* every other voice to the second core, and of an odd number the larger half (this core also hands out
             * the job, waits for it and adds everything up); a voice alone stays here */
            odd = nv & 1;
            for (i = 0, nv = 0; i < NVOICE; i++)
                if (all >> i & 1u) {
                    if (c2_run && all != (1u << i) && (nv & 1) != odd) other |= 1u << i;
                    else mine |= 1u << i;
                    nv++;
                }

            vj.def = def;
            vj.b   = b;
            vj.att = r->m.att;
            vj.kd  = r->m.kd;
            vj.sus = r->m.sus;
            vj.kr  = r->m.kr;
            vj.mkd = r->m.mkd;
            vj.kg  = kg;
            vj.dt  = dt;
            vj.gain = def->gain * r->m.vol * algo_gain[inst][algo];
            vj.panmod = r->m.panmod;

            stg_mark(H1_STG_MIX);
            if (other) {
                int on2 = c2_run(voices_job, other);
                voices_job(mine);
                stg_mark(H1_STG_VOICES);
                if (!on2 || c2_wait() != 0) voices_job(other);
                stg_mark(H1_STG_WAIT);
            } else {
                voices_job(mine);
                stg_mark(H1_STG_VOICES);
            }

            for (i = 0; i < NVOICE; i++) {           /* the voices into the track, in voice order (voice_out) */
                const f32 *vl = vt_l[i], *vr = vt_r[i];
                if (!(all >> i & 1u)) continue;
                for (n = 0; n < CTRL; n++) {
                    tl[n] += vl[n];
                    tr[n] += vr[n];
                }
            }
        }

        /* Apply track mixer channel: volume, pan, VU peak, dry bus + FX send bus */
        g_vol = r->vol_s;
        pan_gains(r->pan_s, &p_l, &p_r);
        p_l *= 1.414f * g_vol;
        p_r *= 1.414f * g_vol;
        for (n = 0; n < CTRL; n++) {
            f32 xl = tl[n] * p_l, xr = tr[n] * p_r;
            f32 am = maxf(absf(xl), absf(xr));
            if (am > tpeak) tpeak = am;
            L[n] += xl;
            R[n] += xr;
            sl[n] += xl * r->send_s;
            sr[n] += xr * r->send_s;
        }
        if (t == cur_trk)                        /* (written where the master scope's next block goes: one scope_pos) */
            for (n = 0; n < CTRL; n++) own_scope[(scope_w + n) & (SCOPE_N - 1)] = (tl[n] * p_l + tr[n] * p_r) * 0.5f;
        r->peak_out += (tpeak - r->peak_out) * (tpeak > r->peak_out ? 0.5f : 0.04f);
    }

    /* Sub-sonic DC removal (18 Hz) */
    for (n = 0; n < CTRL; n++) {
        f32 x = L[n], y = R[n];
        sub_l += SUB_K * (x - sub_l);
        sub_r += SUB_K * (y - sub_r);
        if (absf(sub_l) < 1e-12f) sub_l = 0.0f;
        if (absf(sub_r) < 1e-12f) sub_r = 0.0f;
        L[n] = x - sub_l;
        R[n] = y - sub_r;
    }

    stg_mark(H1_STG_MIX);
    /* 8-Engine Selectable FX Rack + Master Bus (EQ + Tape Drive + Tape Brake/Stutter) */
    fx_send_block(sl, sr, L, R, st.fx_type, st.fx_p[st.fx_type < NFX ? st.fx_type : 0], bpm, st.fx_on);
    stg_mark(H1_STG_FX);
    fx_master_block(L, R, st.master, st.tape[3], bpm);

    /* Scope & master meter */
    {
        uint32_t w = scope_w;
        for (n = 0; n < CTRL; n++) {
            f32 m = (L[n] + R[n]) * 0.5f, am = absf(m);
            scope[w & (SCOPE_N - 1)] = m;
            w++;
            if (am > peak) peak = am;
        }
        scope_w = w;
        level_out += (peak - level_out) * (peak > level_out ? 0.5f : 0.03f);
        if (peak < 1e-4f && click_env <= 0.001f) { if (silent_blocks < 100000u) silent_blocks++; }
        else silent_blocks = 0;
    }

    /* Woodblock metronome click (post-FX) */
    if (click_env > 0.001f) {
        for (n = 0; n < CTRL; n++) {
            f32 c;
            click_lp += (noise_s(&click_rng) - click_lp) * 0.2f;
            c = (tsin(click_ph) + 0.22f * click_pe * tsin(click_ph2) + 0.45f * click_pe * click_lp) * click_env * click_amp;
            click_ph += click_f * (1.0f + 1.2f * click_pe) * ISR; if (click_ph >= 1.0f) click_ph -= 1.0f;
            click_ph2 += click_f * 2.76f * ISR; if (click_ph2 >= 1.0f) click_ph2 -= 1.0f;
            click_pe *= 0.9972f;
            click_env *= 0.99925f;
            L[n] += c;
            R[n] += c;
        }
    }
}

/* ---- Look-Ahead Brickwall Limiter (0.7 ms look-ahead) --------------------------------- */
static inline f32 limit(f32 x)
{
    f32 a = absf(x);
    if (!(a > 0.6f)) return a <= 0.6f ? x : 0.0f;
    a = 0.6f + 0.4f * softclip((a - 0.6f) * 2.5f);
    return x < 0.0f ? -a : a;
}

#define LIM_T 0.78f
#define LIM_REL 0.0072f
static f32 lim_g = 1.0f, lim_need = 1.0f;

static f32 lim_peak(const f32 *l, const f32 *r)
{
    f32 p = 0.0f;
    int i;
    for (i = 0; i < CTRL; i++) {
        f32 a = absf(l[i]), b = absf(r[i]);
        if (a > p) p = a;
        if (b > p) p = b;
    }
    return p > LIM_T ? LIM_T / p : 1.0f;
}

static f32 NL_buf[CTRL], NR_buf[CTRL];
static uint32_t eng_have, eng_pos;

static void lim_block(f32 *L, f32 *R)
{
    f32 need_now = lim_need, g0 = lim_g, g1, dg;
    int i;
    for (i = 0; i < CTRL; i++) { L[i] = NL_buf[i]; R[i] = NR_buf[i]; }
    block(NL_buf, NR_buf);
    lim_need = lim_peak(NL_buf, NR_buf);
    g1 = g0 + (1.0f - g0) * LIM_REL;
    if (g1 > need_now) g1 = need_now;
    if (g1 > lim_need) g1 = lim_need;
    if (g1 > 0.99999f && g0 > 0.99999f) { lim_g = 1.0f; return; }
    dg = (g1 - g0) * (1.0f / (f32)CTRL);
    for (i = 0; i < CTRL; i++) {
        g0 += dg;
        L[i] *= g0;
        R[i] *= g0;
    }
    lim_g = g1;
}

#ifdef H1_FUZZ
uint32_t h1_fuzz_nan;
#endif

/* the limited mix out as 24-bit samples (eng_render): from RAM */
H1_HOT static void eng_out(int32_t *lr, const f32 *L, const f32 *R, uint32_t take)
{
    uint32_t i;
    for (i = 0; i < take; i++) {
#ifdef H1_FUZZ
        if (L[i] != L[i] || R[i] != R[i])
            h1_fuzz_nan++;                       /* (tests/fuzz.c: a sound that stopped being numbers) */
#endif
        lr[0] = (int32_t)(limit(L[i]) * 7549747.0f);
        lr[1] = (int32_t)(limit(R[i]) * 7549747.0f);
        lr += 2;
    }
}

void eng_render(int32_t *lr, uint32_t n)
{
    static f32 L[CTRL], R[CTRL];
    int k;
    if (stg_clock) {
        stg_t0 = stg_clock();
        for (k = 0; k < H1_NSTG; k++) stg_acc[k] = 0;
    }
    while (n) {
        uint32_t take;
        if (!eng_have) { lim_block(L, R); eng_have = CTRL; eng_pos = 0; }
        take = eng_have < n ? eng_have : n;
        H1_FAR(eng_out)(lr, L + eng_pos, R + eng_pos, take);
        lr += 2u * take;
        eng_pos += take;
        eng_have -= take;
        n -= take;
    }
    if (stg_clock) {
        uint32_t total = 0;
        stg_mark(H1_STG_MASTER);
        for (k = 0; k < H1_NSTG; k++) total += stg_acc[k];
        if (total > stg_worst_total) {
            stg_worst_total = total;
            for (k = 0; k < H1_NSTG; k++) stg_worst[k] = stg_acc[k];
        }
    }
}

SMALL void eng_init(uint32_t sample_rate)
{
    int i;
    if (sample_rate) { SR = (f32)sample_rate; ISR = 1.0f / SR; }
    dsp_init();
    fx_init(SR);
    eng_rng = 0x2F6E2B1u;
    lfo_rng = 0x1F0C1F0Cu;
    blkno = 16;
    run_time = blk_time = 0.0f;
    stamp_ctr = 0;
    eng_have = eng_pos = 0;
    for (i = 0; i < CTRL; i++) NL_buf[i] = NR_buf[i] = 0.0f;
    {
        uint8_t *p = (uint8_t *)trun;
        uint32_t k;
        for (k = 0; k < sizeof trun; k++) p[k] = 0;
    }
    for (i = 0; i < NTRACK; i++) {
        f32 v = (f32)st.trk[i].vol * 0.01f;
        trun[i].vol_s = v * v;
        trun[i].send_s = (f32)st.trk[i].send * 0.01f;
        trun[i].pan_s = ((f32)st.trk[i].pan - 50.0f) * 0.02f;
    }
    for (i = 0; i < NVOICE; i++) {
        uint8_t *p = (uint8_t *)&voices[i];
        uint32_t k;
        for (k = 0; k < sizeof(Voice); k++) p[k] = 0;
        voices[i].idx = (uint8_t)i;
    }
    cur_trk = st.track < NTRACK ? st.track : 0;
    cur_i   = st.inst < NINST ? st.inst : 0;
    for (i = 0; i < 4; i++)
        live_blk.k[i] = (f32)KNOBS_NOW(cur_i)[i] * 0.01f;
    lim_g = lim_need = 1.0f;
    loop_state = LOOP_EMPTY;
    nlev = nclip = 0;
    rec_forget();
    loop_pos = 0.0f;
    loop_len = (f32)(tape_bars() * seq_bar_ticks());
    for (i = 0; i < 4; i++) latched[i] = latched_midi[i] = midi_down[i] = 0;
    chord_clear();
    phys_down = nheld = arp_ncur = 0;
    arp_acc = 1e9f;
    step_playing = step_write = step_cursor = step_at = step_nsnd = step_gesture = 0;
    step_acc = step_off = 0.0f;
    riff_rec = riff_at = 0;
    shake_clear();
    free_on = 0;
    click_at = -1;
    ev_r = ev_w;
    sub_l = sub_r = 0.0f;
}

/* ---- main-loop introspection ---------------------------------------------------------- */
void eng_snapshot(Vis *vis)
{
    int i;
    for (i = 0; i < NVOICE; i++) {
        const Voice *v = &voices[i];
        vis->v[i].on     = (v->stage != 0) && (PART_INST(v->part) == cur_i) && (v->trk & 3) == cur_trk;
        vis->v[i].gate   = v->gate;
        vis->v[i].note   = v->note;
        vis->v[i].vel    = v->vel;
        vis->v[i].seq    = v->seq;
        vis->v[i].trk    = v->trk;
        vis->v[i].serial = v->serial;
        vis->v[i].pitch  = v->pitch;
        vis->v[i].env    = v->env;
        vis->v[i].menv   = v->menv;
        vis->v[i].age    = v->age;
    }
    for (i = 0; i < 4; i++) {
        vis->k[i] = live_blk.k[i];
        vis->trk_lvl[i] = minf(1.0f, trun[i].peak_out * 2.2f);
    }
    vis->level = level_out;
    {   /* the track in hand's share of the mix's level: alone it is the mix's level exactly, as the pictures were
         * made for (a track's own peak is taken before the limiter, up to 1.7 times hotter) */
        f32 sum = trun[0].peak_out + trun[1].peak_out + trun[2].peak_out + trun[3].peak_out;
        vis->own_level = sum > 1e-6f ? level_out * (trun[cur_trk & 3].peak_out / sum) : 0.0f;
    }
    vis->own_scope = own_scope;
    vis->lfo = lfo_out;
    vis->beat = beat_out;
    vis->notes = note_count;
    vis->last_pitch = last_pitch;
    vis->hand = hand_count;
    vis->hand_pitch = hand_pitch;
    vis->scope = scope;
    vis->scope_pos = (int)(scope_w & (SCOPE_N - 1));
    for (i = 0; i < NSHAKE; i++) {
        vis->balls[i].on = tballs[i].on && !tballs[i].gone;
        vis->balls[i].note = tballs[i].note;
        vis->balls[i].x = tballs[i].x;
        vis->balls[i].y = tballs[i].y;
        vis->balls[i].vx = tballs[i].vx;
        vis->balls[i].vy = tballs[i].vy;
        vis->balls[i].hit = tballs[i].hit;
    }
    vis->tomb_angle = tomb_angle;
}

/* The tape's picture and its white keys read every event, twice a frame, on the main loop. What they read only
 * changes with what is recorded (loop_gen and the count) and the tape's length, so each keeps its last answer and
 * reads the events again only when one of those has moved. */
static struct { uint32_t gen; int n; f32 len; uint8_t dens[NTRACK][32]; } seq_dens = {0xFFFFFFFFu, -1, -1.0f, {{0}}};
static struct { uint32_t gen; int n; f32 len; uint32_t m; } seq_keys = {0xFFFFFFFFu, -1, -1.0f, 0};

void eng_seq_info(SeqInfo *si)
{
    f32 in_t, out_t;
    int i, t, c, ne = nlev;
    tape_bounds(&in_t, &out_t);
    si->mode = st.seq[0];
    si->loop_state = loop_state;
    si->loop_pos = loop_len > 0.0f ? sat(loop_pos / loop_len) : 0.0f;
    si->loop_in  = loop_len > 0.0f ? sat(in_t / loop_len) : 0.0f;
    si->loop_out = loop_len > 0.0f ? sat(out_t / loop_len) : 1.0f;
    si->playing = step_playing;
    si->write = step_write;
    si->nsteps = step_count();
    si->per_bar = step_per_bar();
    si->bars = st.seq[0] == SEQ_STEP ? step_bars() : tape_bars();
    si->cursor = step_cursor;
    si->at = step_at;
    si->riff_rec = riff_rec;
    si->riff_at = riff_at;
    si->clip_count = nclip;
    for (t = 0; t < NTRACK; t++)
        for (c = 0; c < 32; c++)
            si->trk_dens[t][c] = 0;
    if (st.seq[0] == SEQ_STEP) {
        /* the PATTERN page's lanes: the steps' notes by their track, and the place in the pattern (the tape's
         * notes, which these lanes showed before, are not what plays in STEP) */
        int n = step_count(), j;
        si->loop_in = 0.0f;
        si->loop_out = 1.0f;
        if (step_playing && n > 0) {
            int at = imin(step_at, n - 1);
            si->loop_pos = sat(((f32)at + sat(step_acc / maxf(1.0f, step_len(at)))) / (f32)n);
        } else
            si->loop_pos = step_write && n > 0 ? sat((f32)imin(step_cursor, n - 1) / (f32)n) : 0.0f;
        for (i = 0; i < n; i++)
            for (j = 0; j < NSNOTE; j++)
                if (st.steps[i][j]) {
                    uint8_t *d = &si->trk_dens[st.step_trk[i][j] & 3][i * 32 / n];
                    if (*d < 15) (*d)++;
                }
    } else if (loop_len > 0.0f) {
        uint32_t gen = loop_gen;
        if (gen != seq_dens.gen || ne != seq_dens.n || loop_len != seq_dens.len) {
            for (t = 0; t < NTRACK; t++)
                for (c = 0; c < 32; c++)
                    seq_dens.dens[t][c] = 0;
            for (i = 0; i < ne && i < NLEV; i++) {
                if ((LEV_OK(lev[i])) && (lev[i].flags & 7) == LEV_ON) { // JSM only count notes on the current snap
                    int col = iclamp((int)((f32)lev[i].tick * 32.0f / loop_len), 0, 31);
                    uint8_t *d = &seq_dens.dens[lev[i].trk & 3][col];
                    if (*d < 15) (*d)++;
                }
            }
            seq_dens.gen = gen;
            seq_dens.n = ne;
            seq_dens.len = loop_len;
        }
        for (t = 0; t < NTRACK; t++)
            for (c = 0; c < 32; c++)
                si->trk_dens[t][c] = seq_dens.dens[t][c];
    }
}

uint32_t eng_sounding_lo(void)
{
    uint32_t m = 0;
    int i, base = 53 + 12 * st.octave;
    for (i = 0; i < NVOICE; i++)
        if (voices[i].stage && voices[i].gate) {
            int k = (int)voices[i].note - base;
            if (k >= 0 && k < H1_NKEYS) m |= 1u << k;
        }
    return m;
}

int eng_seq_leds(uint32_t *out, int page_open, int blink)
{
    /* 16 white keys in FM-1's 27-key layout (F3..G5): the step pattern, or the tape's notes, with the place
     * running across. A pattern or tape longer than 16 sixteenths shares a key between ceil(n / 16) of them. */
    static const uint8_t WKEY[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
    uint32_t m = 0;
    int i, n, per, at = -1, wr = -1;
    if (st.seq[0] == SEQ_STEP) {
        if (!step_playing && !step_write && !page_open)
            return 0;
        n = step_count();
        per = (n + 15) / 16;
        for (i = 0; i < n; i++)
            if (st.steps[i][0])
                m |= 1u << WKEY[i / per];
        if (step_playing)
            at = imin(step_at, n - 1) / per;
        else if (step_write)
            wr = imin(step_cursor, n - 1) / per;
    } else {
        int s = loop_state, ne = nlev;
        if (s != LOOP_REC && s != LOOP_PLAY && s != LOOP_DUB && !(page_open && s == LOOP_IDLE))
            return 0;
        n = imax(1, (int)loop_len / 24);
        per = (n + 15) / 16;
        if (loop_gen != seq_keys.gen || ne != seq_keys.n || loop_len != seq_keys.len) {   /* (seq_dens) */
            uint32_t gen = loop_gen;
            for (i = 0; i < ne; i++)
                if ((lev[i].flags & 7u) == LEV_ON)
                    m |= 1u << WKEY[imin(lev[i].tick / 24, n - 1) / per];
            seq_keys.gen = gen;
            seq_keys.n = ne;
            seq_keys.len = loop_len;
            seq_keys.m = m;
        }
        m = seq_keys.m;
        if (s != LOOP_IDLE)
            at = imin((int)loop_pos / 24, n - 1) / per;
    }
    if (at >= 0)
        m ^= 1u << WKEY[at];
    if (wr >= 0)                                            /* the step being written blinks */
        m = blink ? m | 1u << WKEY[wr] : m & ~(1u << WKEY[wr]);
    *out = m;
    return 1;
}

int eng_seq_has(int trk)
{
    int i, j, ne = nlev;
    if (st.seq[0] == SEQ_STEP) {
        for (i = 0; i < NSTEP; i++)
            for (j = 0; j < NSNOTE; j++)
                if (st.steps[i][j] && (st.step_trk[i][j] & 3) == (trk & 3)) return 1;
        return 0;
    }
    for (i = 0; i < ne && i < NLEV; i++)
        if ((lev[i].trk & 3) == (trk & 3)) return 1;
    return 0;
}

uint32_t eng_voices(void)
{
    uint32_t n = 0;
    int i;
    for (i = 0; i < NVOICE; i++) n += voices[i].stage != 0;
    return n;
}

/* ---- 4-Track Tape flash persistence (h1_loop) ----------------------------------------- */
#define LOOP_MAGIC 0x54323241u   /* "A22T" */
#define LOOP_HEAD 12u
static void put16(uint8_t *d, uint32_t v) { d[0] = (uint8_t)v; d[1] = (uint8_t)(v >> 8); }
static uint32_t get16(const uint8_t *b)   { return (uint32_t)b[0] | (uint32_t)b[1] << 8; }

const uint8_t *eng_loop_blob(uint32_t *size)
{
    int ne = nlev;
    uint8_t *h = loop_mem.head;
    put16(h, LOOP_MAGIC & 0xFFFFu);
    put16(h + 2, LOOP_MAGIC >> 16);
    h[4] = 1;
    h[5] = 0;
    put16(h + 6, (uint32_t)ne);
    put16(h + 8, (uint32_t)loop_len);
    put16(h + 10, 0);
    *size = LOOP_HEAD + (uint32_t)ne * 8u;
    return h;
}

int eng_loop_load(const uint8_t *b, uint32_t n)
{
    uint32_t ne, len, i, k = 0;
    if (!b || n < LOOP_HEAD || get16(b) != (LOOP_MAGIC & 0xFFFFu) || get16(b + 2) != (LOOP_MAGIC >> 16) || b[4] != 1)
        return -1;
    ne = get16(b + 6);
    len = get16(b + 8);
    if (ne > NLEV || n < LOOP_HEAD + ne * 8u || len < 24u || len > 32u * 480u)
        return -1;
    tape_release_seq();
    for (i = 0; i < ne; i++) {
        const uint8_t *e = b + LOOP_HEAD + i * 8u;
        uint32_t tick = get16(e);
        LEv *d;
        /* keep what lies past the loop's present length: shortening the tape (bars, loop in/out) leaves
         * those events in memory to come back when it is lengthened, and a power cycle must not lose them */
        if (tick >= 32u * 480u) continue;
        nlev = (int)k;
        d = lev_insert((uint16_t)tick);
        if (!d) break;
        d->tick  = (uint16_t)tick;
        d->d1    = e[2] & 127u;
        // d->flags = e[3] & 7u;
        d->flags = e[3] & ~0x80u;   /* JSM keep kind (0..2) and snap (3..5), without bit fresh (7) */
        d->trk   = e[4] & 3u;
        // JSM d->part  = part_ok(e[5]) ? e[5] : PART(st.trk[d->trk].inst, st.trk[d->trk].algo);
        d->d2    = e[6] & 127u;
        d->rsv   = 0;
        k++;
    }
    nlev = (int)k;
    rec_forget();
    loop_len = (f32)len;
    loop_pos = 0.0f;
    loop_state = nlev ? LOOP_IDLE : LOOP_EMPTY;
    loop_gen++;
    return 0;
}

uint32_t eng_loop_gen(void)   { return loop_gen; }
void eng_loop_gen_force_inc(void) {loop_gen++; } // JSM force refresh, used for forcing the tape to draw after loading a snap
int eng_loop_taking(void)     { return loop_state == LOOP_REC || loop_state == LOOP_DUB; }
int eng_loop_quiet(void)      { return loop_state == LOOP_EMPTY || loop_state == LOOP_IDLE; }
/* Silent, for a flash erase (it stalls the sound about 50 ms): no voice, the tape and the pattern stopped, no key held
 * for the arpeggiator, and the output (reverb and delay tails, the metronome) at rest for 300 ms. "No voice" alone
 * was true in a tail, in a rest of the tape or pattern, between arpeggiator notes and under the metronome. */
int eng_quiet(void)
{
    return eng_voices() == 0 && eng_loop_quiet() && !step_playing && nheld == 0 &&
           silent_blocks >= (uint32_t)(0.3f * 44100.0f / (f32)CTRL);
}
int eng_layer_count(void)
{
    int t, c = 0;
    for (t = 0; t < NTRACK; t++) if (eng_seq_has(t)) c++;
    return c;
}
int eng_layer_rec(void)       { return (loop_state == LOOP_REC || loop_state == LOOP_DUB) ? (cur_trk + 1) : 0; }
int eng_loop_used(void)       { return nlev; }
uint32_t eng_loop_full(void)  { return loop_full; }
