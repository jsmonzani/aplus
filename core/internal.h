/* A22 - FM workstation core internals. Not for ports. */
#pragma once
#include <stddef.h>
#include "h1.h"
#include "mathx.h"
#include "gfx.h"

#define NVOICE 8
#define NSTEP 64                 /* step sequencer max steps */
#define NSNOTE 8                 /* polyphony per step */
#define NINST 14                 /* 14 engines: FM-4, ELECTRIC, STACK, ANALOG, SHAPER, WIRE, HAZE, MONO, KIT, PERC,
                                  * WAVE, CHIP, CYCLE, SKINS (new ones go at the end: a saved tape names engines by number) */
#define NSLOT 16                 /* state slots for engines: room for four more than NINST (state version 5) */
#define NALGO 8                  /* 8 curated algorithms/presets per engine */
#define NTRACK 4                 /* 4-Track Virtual Tape (T1..T4) */
#define NSNAP 8                  /* 8 instant user patch snapshots (SAVE + white keys 1..8) */
#define NFX 8                    /* 8 selectable FX engines (new ones go at the end: a state names them by number) */
#define NFXSLOT 12               /* state slots for effects: room for four more than NFX (state version 5) */
#define NLEV 2048                /* tape events across all 4 tracks (notes + K1..K4 motion) */
#define NRIFF 64              /* Riff sequencer max steps */
#define NSHAKE 6               /* Shake max bouncing physics balls */
#define CTRL 32                  /* frames per control block (~0.72 ms at 44.1 kHz) */
#define SCOPE_N 512

#if defined(H1_DEVICE) && !defined(__APPLE__)   /* (the host simulator builds with H1_DEVICE too; Mach-O
                                                * has no plain ".pool" section, and the host needs none) */
#define H1_POOL __attribute__((section(".pool")))
#else
#define H1_POOL
#endif

/* ---- voices --------------------------------------------------------------------------- */
typedef struct Voice {
    uint8_t stage;               /* 0 off, 1 attack, 2 decay, 3 sustain, 4 release */
    uint8_t gate;
    uint8_t note;
    uint8_t idx;
    uint8_t part;                /* PART(inst, algo), 0 = unused */
    uint8_t seq;                 /* 1: started by tape/sequencer */
    uint8_t trk;                 /* 0..NTRACK-1 (tape track owner) */
    uint8_t vel;                 /* MIDI/step velocity 1..127 */
    uint8_t declick;             /* 1: stolen voice declick active */
    uint8_t shed;                /* 1: let go fast, the audio running out of time (eng_late) */
    uint32_t serial;             /* increments on every note-on */
    uint32_t stamp;              /* allocation timestamp */
    f32 pitch, target;           /* MIDI note (gliding) and target */
    f32 env, env_prev;           /* Amp ADSR */
    f32 menv, menv_prev;         /* Mod / Filter exponential envelope (0..1) */
    f32 vel_f;                   /* normalized velocity 0..1 */
    f32 age;                     /* seconds since note-on */
    f32 pan;                     /* -1..1 */
    f32 r1, r2;                  /* per-note randoms 0..1 */
    f32 ph[8];                   /* oscillator phases in turns */
    f32 s[16];                   /* instrument DSP state (filters, feedback, decays) */
    uint32_t u[4];
    uint32_t q[4];
    f32 last_l, last_r;          /* last stereo sample for seamless voice-steal declick */
    f32 tail_l, tail_r;
} Voice;

/* ---- saved parameters ----------------------------------------------------------------- */
typedef struct InstParams {
    uint8_t knob0[4];            /* default K1..K4 */
    uint8_t algo;                /* 0..7 */
    uint8_t env[4];              /* Amp ADSR: attack, decay, sustain, release (0..100) */
    uint8_t menv[4];             /* Mod/Filter Env: depth (0..100), decay (0..100), vel (0..100), punch (0..100) */
    uint8_t lfo[4];              /* shape (0..6), rate (0..100), depth (0..100), target (0..7) */
    uint8_t arp[4];              /* mode, rate, range/grav, gate/bounce */
    uint8_t mix[3];              /* [IP_CUT] volume cut 0..100, [IP_PAN] 0..100 (50=C), [IP_SEND] FX send 0..100 */
} InstParams;
#define IP_CUT 0
#define IP_PAN 1
#define IP_SEND 2

/* 4-Track Tape Track state: each track remembers its patch, mixer channel & mute */
typedef struct Track {
    uint8_t inst;
    uint8_t algo;
    uint8_t vol;                 /* 0..100 */
    uint8_t pan;                 /* 0..100 (50 = center) */
    uint8_t send;                /* 0..100 (FX send) */
    uint8_t mute;                /* bit 0 = mute, bit 1 = solo */
    uint8_t knob[4];             /* recorded K1..K4 snapshot (updated by automation during playback) */
    uint8_t env[4];
    uint8_t menv[4];
    uint8_t lfo[4];
    uint8_t has_snap;            /* 1 = track owns a recorded snapshot */
    uint8_t rsv;
} Track;

/* Instant Patch Snapshot (SAVE + White Keys 1..8) */
typedef struct Snap {
    uint8_t used;
    uint8_t inst;
    uint8_t algo;
    uint8_t fx_type;
    uint8_t knob[4];
    uint8_t env[4];
    uint8_t menv[4];
    uint8_t lfo[4];
    uint8_t fx_p[4];
} Snap;

#define H1_MAGIC 0x32324132u     /* "2A22" - isolated from h1 saves */
#define H1_VERSION 5u     /* 3: chord memory; 4: twelve engines (WAVE, CHIP); 5: room for 16 engines and 12
                           * effects (NSLOT, NFXSLOT), and how many effects it was saved with (nfx) */

/* Sequencer / Tape mode enums */
enum { SEQ_TAPE = 0, SEQ_STEP = 1 };
enum { ARP_OFF = 0, ARP_RIFF, ARP_SHAKE, ARP_UP, ARP_DOWN, ARP_UPDN, ARP_RAND, ARP_CHORD, NARPMODE };
enum { FX_PLATE = 0, FX_DELAY, FX_FILTER, FX_SHIFT, FX_CHORUS, FX_TANK, FX_LOFI, FX_THUMP };
enum { TAPE_SPD_HALF = 0, TAPE_SPD_1X, TAPE_SPD_2X, TAPE_SPD_REV };
enum { TRICK_OFF = 0, TRICK_BRAKE, TRICK_STUT8, TRICK_STUT16, TRICK_STUT32 };
enum { CLICK_REC = 0, CLICK_ON, CLICK_OFF };
enum { CHORD_OFF = 0, CHORD_MEMORY, CHORD_SCALE, CHORD_SHAPE };
#define CHORD_MAX 6              /* notes a chord can have */
#define CHORD_NSCALE 6           /* SCALE's types: TRIAD, 7TH, SUS2, SUS4, ADD9, 6TH */
#define CHORD_NSHAPE 8           /* SHAPE's: MAJ, MIN, SUS4, 7, MIN7, MAJ7, POWER, OCT */

typedef struct H1State {
    uint32_t magic;
    uint8_t version;
    uint8_t inst;                /* active instrument 0..NINST-1 */
    uint8_t track;               /* active tape track 0..NTRACK-1 */
    int8_t octave;               /* -2..2 */
    uint8_t hud;                 /* 1: pin bottom knob strip on HOME */
    uint8_t keys[4];             /* root, scale, glide, hold */
    uint8_t glo[4];              /* tempo (40..240), swing (0..100), tune (0..100, 50=0), bright (20..100) */
    uint8_t seq[8];              /* [0] mode (TAPE/STEP), [1] bars log2 (0..5 -> 1..32), [2] meter (0..4),
                                  * [3] snap (FREE/1/16/1/8), [4] step grid (1/8,1/16,1/32), [5] click,
                                  * [6] multi-track (always 1 by default), [7] tape pitch-link (0=free, 1=tape) */
    uint8_t tape[4];             /* [0] speed (0.5x/1x/2x/REV), [1] loop_in bar, [2] loop_out bar (0=full), [3] trick */
    uint8_t fx_type;             /* 0..NFX-1 */
    uint8_t fx_on;               /* 1 = enabled */
    uint8_t fx_p[NFXSLOT][4];    /* 4 parameters per FX engine (slots from NFX on: unused) */
    uint8_t master[4];           /* Master Bus: LOW (0..100), MID, HIGH, DRIVE */
    uint8_t shake[4];          /* Shake: GRAVITY, SPIN, BOUNCE, SCALE */
    uint8_t riff_len;         /* 0..NRIFF */
    uint8_t riff_root;        /* base note for transposition (default 60) */
    uint8_t riff[NRIFF];   /* 0 = rest, 1..128 = MIDI note + 1 */
    uint8_t chord[4];            /* chord memory: [0] mode (CHORD_*), [1] type (SCALE: 0..5, SHAPE: 0..7),
                                  * [2] voicing (CLOSE, OPEN, DROP2, +BASS), [3] strum (0..100: 0..60 ms) */
    uint8_t chord_n;             /* MEMORY's shape: 1..CHORD_MAX notes */
    uint8_t chord_mem[CHORD_MAX]; /* its notes as semitones above its lowest (chord_mem[0] = 0) */
    uint8_t ninst;               /* engines it was saved with: slots from there on load as new */
    uint8_t nfx;                 /* effects it was saved with (version 4: 8): the same for effects */
    uint8_t rsv[2];
    InstParams ip[NSLOT];
    uint8_t knob[NSLOT][NALGO][4];
    Track trk[NTRACK];
    Snap snap[NSNAP];
    uint8_t steps[NSTEP][NSNOTE];
    uint8_t step_trk[NSTEP][NSNOTE];
    uint8_t step_vel[NSTEP][NSNOTE];
} H1State;

#define KNOBS(inst, algo) (st.knob[(uint32_t)(inst) < NSLOT ? (uint32_t)(inst) : 0u][(uint32_t)(algo) < NALGO ? (uint32_t)(algo) : 0u])
#define KNOBS_NOW(inst) KNOBS(inst, st.ip[inst].algo)
void knobs_default(int inst, int algo);
void knobs_home(int inst, int algo, uint8_t out[4]);   /* an algorithm's own K1..K4 (h1.c) */
void dice_roll(int inst, int algo, uint32_t *rng, uint8_t out[4]);   /* DICE: K1..K4 near home (dice.c) */
#define INST_VOL(inst) (100 - (st.ip[inst].mix[IP_CUT] <= 100 ? st.ip[inst].mix[IP_CUT] : 0))

#define PART(inst, algo) ((uint8_t)(1u + (uint32_t)(inst) * NALGO + ((uint32_t)(algo) < NALGO ? (uint32_t)(algo) : 0u)))
#define PART_SOME(p) ((uint32_t)(p) - 1u < (uint32_t)(NINST * NALGO))
#define PART_INST(p) (PART_SOME(p) ? (int)(((uint32_t)(p) - 1u) / NALGO) : 0)
#define PART_ALGO(p) (PART_SOME(p) ? (int)(((uint32_t)(p) - 1u) % NALGO) : 0)
#define NALGOS(i) NALGO
static inline int part_ok(uint32_t p) { return PART_SOME(p); }

extern H1State st;

#ifdef __clang__
#define SMALL __attribute__((minsize))
#define APART __attribute__((minsize, noinline))
#else
#define SMALL
#define APART
#endif

/* ---- instrument DSP block context ----------------------------------------------------- */
typedef struct Blk {
    f32 k[4];                    /* K1..K4 (0..1), smoothed + LFO modulated */
    int algo;
    f32 sr, isr;
    f32 lfo;                     /* -1..1 */
    f32 bend;                    /* semitones (tune + LFO + tape varispeed pitch) */
    f32 time;                    /* seconds since boot */
    f32 bpm;
    f32 step;                    /* delta time (CTRL * isr on first call per block, else 0) */
    f32 menv_amt;                /* 0..1 Mod/Filter Env depth */
    f32 vel_sens;                /* 0..1 velocity sensitivity */
    f32 punch;                   /* 0..1 transient punch */
} Blk;

/* ---- visualizer snapshot -------------------------------------------------------------- */
typedef struct VVoice {
    uint8_t on, gate, note, vel, seq, trk;
    uint32_t serial;
    f32 pitch, env, menv, age;
} VVoice;

typedef struct VBall {
    f32 x, y, vx, vy, hit;       /* Shake physics ball state for UI */
    uint8_t on, note;
} VBall;

typedef struct Vis {
    f32 t, dt;
    f32 k[4];
    int algo, algo_prev;
    f32 algo_t;
    f32 enter;
    int fresh;
    VVoice v[NVOICE];
    f32 level;
    f32 trk_lvl[NTRACK];         /* live per-track VU meters (0..1) for Mixer */
    f32 lfo;
    f32 beat;
    uint32_t notes;
    f32 last_pitch;
    uint32_t hand;
    f32 hand_pitch;
    const f32 *scope;            /* the whole mix, after the effect (the top bar's meter, the FX page) */
    int scope_pos;
    f32 own_level;               /* the track in hand alone, before the effect: what the HOME picture reacts to */
    const f32 *own_scope;        /* (same scope_pos) */
    VBall balls[NSHAKE];       /* Shake visualization */
    f32 tomb_angle;
    uint32_t *rng;
} Vis;

typedef struct InstDef {
    const char *name;
    uint8_t ink[3];              /* accent RGB for simulator chrome */
    const char *knob[4];         /* K1 Blue, K2 Green, K3 White, K4 Orange */
    const char *algo[NALGO];
    InstParams def;
    f32 gain;
    void (*block)(const Blk *b);
    void (*note)(Voice *v, const Blk *b);
    void (*render)(Voice *v, const Blk *b, f32 *out_l, f32 *out_r, int n);
    void (*post)(const Blk *b, f32 *l, f32 *r, int n);
    void (*draw)(const Vis *vis);
    uint8_t mono;                /* 1: monophonic legato priority (MONO) */
} InstDef;

extern const InstDef inst_fm4, inst_electric, inst_stack, inst_analog, inst_shaper;
extern const InstDef inst_wire, inst_haze, inst_mono, inst_kit, inst_perc, inst_wave, inst_chip, inst_cycle;
//extern const InstDef inst_cozy; // JSM
extern const InstDef inst_skins;
f32 perc_mode_ratio(int algo, int m);    /* PERC's mode table (tests/perc.c) */
#ifndef H1_DEVICE
void perc_test_modes(int n);             /* tests/perc.c: PERC with only its first n modes */
void fx_test_no_sleep(int on);           /* tests/sleep.c: DELAY, SHIFT, CHORUS and TANK never asleep */
uint32_t fx_test_slept(int fx);          /* tests/sleep.c: the blocks an effect has slept */
void wire_test_whole_row(int on);        /* tests/sleep.c: WIRE clears its whole row on every note */
#endif
extern const InstDef *const insts[];

/* ---- pages ---------------------------------------------------------------------------- */
enum { PG_HOME = 0, PG_FX, PG_MIX, PG_ENV, PG_LFO, PG_GLO, PG_ARP, PG_SEQ, NPAGE };
enum { PK_PCT, PK_ENUM, PK_INT };

typedef struct ParamDef {
    const char *name;
    uint8_t kind;
    uint8_t max, min;
    int8_t show_off;
    const char *const *opts;
} ParamDef;

typedef struct PageDef {
    const char *title;
    ParamDef p[4];
} PageDef;

extern const PageDef pages[NPAGE];
const ParamDef *page_param(int page, int i);
uint8_t *page_value(int page, int i);
int ui_subpage(void);

/* ---- engine (synth.c) ----------------------------------------------------------------- */
enum {
    EV_NOTE_ON = 1, EV_NOTE_OFF, EV_PANIC, EV_INST, EV_UNLATCH,
    EV_REC, EV_PLAY, EV_CLEAR,
    EV_STEP_MOVE, EV_STEP_REST, EV_SEQ_MODE,
    EV_TRACK,                    /* switch active tape track 0..3 */
    EV_CUT,                     /* cut the active track's loop slice to the clipboard */
    EV_PASTE,                     /* drop clipboard onto active track at current bar */
    EV_AUTOMATION,               /* record knob automation during tape rec/dub */
    EV_RIFF_REC,              /* toggle riff step capture */
    EV_RIFF_CLEAR,            /* clear riff sequence */
    EV_RIFF_REST,             /* add rest to riff sequence */
    EV_SNAP_SAVE,                /* save patch to snapshot 0..7 */
    EV_SNAP_LOAD,                /* load patch from snapshot 0..7 */
    EV_MIDI_OFF,                  /* MIDI's all notes off (CC 123): the notes MIDI plays end, the tape plays on */
    EV_DEL_NOTE                     /* JSM delete note index (a) on track ((vel >> 1) & 3) */
};
enum { LOOP_EMPTY, LOOP_IDLE, LOOP_ARMED, LOOP_REC, LOOP_PLAY, LOOP_DUB };

typedef struct SeqInfo {
    int mode;                    /* SEQ_TAPE or SEQ_STEP */
    int loop_state;
    f32 loop_pos;                /* 0..1 across total tape */
    f32 loop_in, loop_out;       /* 0..1 loop bracket */
    int playing, write;          /* step mode */
    int nsteps, per_bar, bars;
    int cursor, at;
    int riff_rec, riff_at;
    int clip_count;              /* events cut to the clipboard */
    uint8_t trk_dens[NTRACK][32];/* 32-column visual density map per track for Tape Deck! */
} SeqInfo;

void eng_seq_info(SeqInfo *si);
int seq_bar_ticks(void);
int seq_grid_ticks(void);
int seq_accent(int tick_in_bar);

void eng_init(uint32_t sample_rate);
void eng_post(int type, int a);
void eng_post_note(int on, int note, int vel, int from_midi);
void eng_post_auto(int knob, int val);
void eng_post_del(int trk, int idx); // JSM

extern void (*eng_midi_fn)(uint8_t status, uint8_t d1, uint8_t d2);

const uint8_t *eng_loop_blob(uint32_t *size);
int eng_loop_load(const uint8_t *b, uint32_t n);
uint32_t eng_loop_gen(void);
int eng_loop_taking(void);
int eng_loop_quiet(void);
int eng_layer_count(void);
int eng_layer_rec(void);
int eng_loop_used(void);
uint32_t eng_loop_full(void);
void eng_render(int32_t *lr, uint32_t n);
void eng_late(int late);
uint32_t eng_saved_n(void);
uint32_t eng_clock_steps(int inst);       /* blocks an engine's own clocks have moved (tests) */
uint32_t eng_shed_count(void);
void eng_stage_clock(uint32_t (*clock)(void));
uint32_t eng_stages(uint32_t out[H1_NSTG]);
void eng_snapshot(Vis *vis);
uint32_t eng_sounding_lo(void);
int eng_seq_leds(uint32_t *out, int page_open, int blink);
uint32_t eng_voices(void);
int eng_quiet(void);
uint32_t eng_events_dropped(void);
void eng_two_cores(int (*run)(void (*fn)(uint32_t), uint32_t arg), int (*wait)(void));
int eng_seq_has(int trk);
int eng_seq_has(int trk); // JSM
int eng_tape_note_info(int trk, int idx, uint16_t *tick, uint8_t *note, int *total); // JSM
extern Voice voices[NVOICE];
extern uint32_t eng_rng;
int scale_snap(int note);

/* FX engine (fx.c) */
extern const char *const FX_NAMES[NFX];
extern const char *const FX_KNOBS[NFX][4];
extern const uint8_t FX_DEFAULTS[NFX][4];
void fx_init(f32 sr);
void fx_reset(void);
void fx_send_block(const f32 *sl, const f32 *sr, f32 *dl, f32 *dr, int fx_type, const uint8_t p[4], f32 bpm, int fx_on);
void fx_master_block(f32 *l, f32 *r, const uint8_t master[4], int trick, f32 bpm);
void fx_two_cores(int (*run)(void (*fn)(uint32_t), uint32_t arg), int (*wait)(void));

/* UI (ui.c) */
void ui_init(void);
void ui_frame(uint32_t now_ms);
int ui_page(void);
