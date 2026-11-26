/* A22 - Multi-Color Vector UI & Control Engine.
 *
 * 8-Color x 32-Shade indexed palette (zero extra RAM/SPI overhead):
 *   K1 = TE Blue   K2 = TE Green   K3 = TE White   K4 = TE Orange
 *
 * Panel Map: JSM version
 *   PRESETS     Preset/Algo (1..8) or FX Type on FX page  | EDIT+PRESETS: Switch Track T1..T4
 *   ALGORITHM   Switch Track T1..T4                       | EDIT+ALGO:    Switch Arp/Seq Mode
 *   K1..K4      Blue / Green / White / Orange parameters  | EDIT+K1..K4:  Mute T1..T4 on Mixer
 *   SELECT      Instrument (Home), Subpage flip (Pages), Step cursor | EDIT+SELECT: Track Volume
 *   MASTER      Global Volume                             | EDIT+MASTER:  Track Volume
 *   HOME        Synth/Drum Engine View                    | Hold: Panic / Hush
 *   ENV         Amp ADSR <-> Mod/Filter Env (tap cycles)
 *   FX          Selectable 8-Engine FX Rack               | EDIT+FX: Bypass toggle
 *   LFO         Modulator / Oscilloscope
 *   SEL         4-Track Mixer (VOL -> PAN -> SEND -> MASTER EQ/DRIVE)
 *   SEQ         4-Track Tape Deck (DECK <-> EDIT)    | Hold: Tape <-> Step Pattern
 *   ARP         Sequencer (RIFF / SHAKE / ARP)
 *   GLO         GLOBAL <-> KEYS (tap cycles)              | Hold: Metronome mode
 *   EDIT        Toggle K1..K4 HUD / Rest (Riff & Step) | Hold: Track Vol / Shift
 *   SAVE        Save now | SAVE+WhiteKey 1..8: Snapshots  | Hold: Reset Algo -> Inst -> All
 *   PLAY / REC  Transport | EDIT+REC: Cut | EDIT+PLAY: Paste | Hold REC: Clear Track -> All */
#include "internal.h"

/* ---- static option labels ------------------------------------------------------------- */
static const char *const O_ARPMODE[] = {"OFF", "RIFF", "SHAKE", "UP", "DOWN", "UPDN", "RAND", "CHORD"};
static const char *const O_RATE[]    = {"1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32"};
static const char *const O_SHAPE[]   = {"SINE", "TRI", "RAMP", "SQR", "RAND", "DRIFT", "ENV"};
static const char *const O_TARGET[]  = {"PITCH", "K1", "K2", "K3", "K4", "LEVEL", "PAN"};
static const char *const O_TIME[]    = {"1/32", "1/16", "1/8T", "1/8", "3/16", "1/4", "3/8", "1/2"};
static const char *const O_ROOT[]    = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char *const O_SCALE[]   = {"CHROM", "MAJOR", "MINOR", "PENTA", "MPENT", "BLUES", "DORIAN", "WHOLE"};
static const char *const O_ONOFF[]   = {"OFF", "ON"};
static const char *const O_BARS[]    = {"1", "2", "4", "8", "16", "32"};
static const char *const O_QUANT[]   = {"FREE", "1/16", "1/8"};
static const char *const O_SEQMODE[] = {"TAPE", "STEP"};
static const char *const O_GRID[]    = {"1/8", "1/16", "1/32"};
static const char *const O_CLICK[]   = {"CLICK REC", "CLICK ON", "CLICK OFF"};
static const char *const O_TRK[]     = {"T1", "T2", "T3", "T4"};
static const char *const O_SPEED[]   = {"0.5X", "1.0X", "2.0X", "REV"};
static const char *const O_TRICK[]   = {"OFF", "BRAKE", "ST 1/8", "ST 1/16", "ST 1/32"};
static const char *const O_PLINK[]   = {"FREE", "TAPE"};
static const char *const O_CHMODE[]  = {"OFF", "MEMORY", "SCALE", "SHAPE"};
static const char *const O_CHSCALE[] = {"TRIAD", "7TH", "SUS2", "SUS4", "ADD9", "6TH"};
static const char *const O_CHSHAPE[] = {"MAJ", "MIN", "SUS4", "7", "MIN7", "MAJ7", "POWER", "OCT"};
static const char *const O_CHMEM[]   = {"MEM"};
static const char *const O_VOICING[] = {"CLOSE", "OPEN", "DROP2", "+BASS"};

/* color per knob K1..K4 (and tracks T1..T4!) */
static const int KCOL[4] = {GC_BLUE, GC_GREEN, GC_WHITE, GC_ORANGE};

/* 8 white keys F3..F4 mapped to instant patch snapshots 1..8 */
static const uint8_t SNAP_KEYS[NSNAP] = {0, 2, 4, 6, 7, 9, 11, 12};

/* ---- page definitions (base templates; subpages override cleanly in page_param) ------- */
const PageDef pages[NPAGE] = {
    {"HOME",   {{0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}},
    {"FX",     {{"P1", PK_PCT, 100, 0, 0, 0}, {"P2", PK_PCT, 100, 0, 0, 0},
                {"P3", PK_PCT, 100, 0, 0, 0}, {"P4", PK_PCT, 100, 0, 0, 0}}},
    {"MIX",    {{"T1", PK_PCT, 100, 0, 0, 0}, {"T2", PK_PCT, 100, 0, 0, 0},
                {"T3", PK_PCT, 100, 0, 0, 0}, {"T4", PK_PCT, 100, 0, 0, 0}}},
    {"ENV",    {{"ATTACK", PK_PCT, 100, 0, 0, 0}, {"DECAY", PK_PCT, 100, 0, 0, 0},
                {"SUSTAIN", PK_PCT, 100, 0, 0, 0}, {"RELEASE", PK_PCT, 100, 0, 0, 0}}},
    {"LFO",    {{"SHAPE", PK_ENUM, 6, 0, 0, O_SHAPE}, {"RATE", PK_PCT, 100, 0, 0, 0},
                {"DEPTH", PK_PCT, 100, 0, 0, 0}, {"TO", PK_ENUM, 6, 0, 0, O_TARGET}}},
    {"GLOBAL", {{"TEMPO", PK_INT, 240, 40, 0, 0}, {"SWING", PK_PCT, 100, 0, 0, 0},
                {"TUNE", PK_PCT, 100, 0, -50, 0}, {"BRIGHT", PK_PCT, 100, 20, 0, 0}}},
    {"SEQ",    {{"MODE", PK_ENUM, NARPMODE - 1, 0, 0, O_ARPMODE}, {"RATE", PK_ENUM, 5, 0, 0, O_RATE},
                {"RANGE", PK_INT, 4, 1, 0, 0}, {"GATE", PK_PCT, 100, 5, 0, 0}}},
    {"TAPE",   {{"TRACK", PK_ENUM, 3, 0, 0, O_TRK}, {"BARS", PK_ENUM, 5, 0, 0, O_BARS},
                {"SPEED", PK_ENUM, 3, 0, 0, O_SPEED}, {"TRICK", PK_ENUM, 4, 0, 0, O_TRICK}}},
};

/* dynamic parameter overrides for subpages */
static ParamDef dyn_p;
static int page = PG_HOME;
static int subpage[NPAGE];

/* tape notes list subpage state */
static uint8_t notes_trk;
static uint8_t notes_sel;
static uint8_t notes_del;
static uint8_t tape_len;

int ui_page(void)    { return page; }

/* SEQ / ARP: K1 is MODE in every mode. SHAKE's four settings take a second subpage: 1/2 MODE GRAV SPIN
 * BOUNCE, 2/2 MODE CAGE. (Once SHAKE's settings took all four columns, MODE's among them, so the knob that
 * had just turned SHAKE on became GRAV and nothing on the page could turn it off: issue #1.) */
static int arp_shake(void) { return st.ip[st.inst].arp[0] == ARP_SHAKE; }
static int arp_sub(void)   { return arp_shake() && subpage[PG_ARP] == 1; }
int ui_subpage(void) { return page == PG_ARP ? arp_sub() : subpage[page]; }

static int page_nsub(int pg)
{
    switch (pg) {
    case PG_ARP: return arp_shake() ? 2 : 1;   /* SHAKE: 0 MODE GRAV SPIN BOUNCE, 1 MODE CAGE */
    case PG_MIX: return 4;   /* 0 VOL, 1 PAN, 2 FX SEND, 3 MASTER EQ/DRIVE */
    case PG_ENV: return 2;   /* 0 AMP ADSR, 1 MOD ENV */
    case PG_GLO: return 3;   /* 0 GLOBAL, 1 KEYS, 2 CHORD */
    case PG_SEQ: return 3;   /* 0 TAPE DECK, 1 TAPE EDIT (or STEP), 2 TAPE NOTES */
    default:     return 1;
    }
}

const ParamDef *page_param(int pg, int i)
{
    static const char *const MENV_N[4] = {"DEPTH", "DECAY", "VEL", "PUNCH"};
    static const char *const MAST_N[4] = {"LOW", "MID", "HIGH", "DRIVE"};
    static const char *const SHAKE_N[2][4] = {{"MODE", "GRAV", "SPIN", "BOUNCE"}, {"MODE", "CAGE", "", ""}};
    static const char *const KEYS_N[4] = {"ROOT", "SCALE", "GLIDE", "HOLD"};
    static const char *const TEDIT_N[4]= {"IN BAR", "LENGTH", "SNAP", "PITCH"}; // JSM
    int sub = subpage[pg];

    if (pg == PG_FX) {
        int ft = st.fx_type < NFX ? st.fx_type : 0;
        dyn_p.name = FX_KNOBS[ft][i];
        dyn_p.min = 0; dyn_p.show_off = 0; dyn_p.opts = 0;
        if (ft == FX_DELAY && i == 1) {
            dyn_p.kind = PK_ENUM; dyn_p.max = 7; dyn_p.opts = O_TIME;
        } else if ((ft == FX_SHIFT && i == 0) || (ft == FX_FILTER && i == 3) || (ft == FX_THUMP && i == 3)) {
            dyn_p.kind = PK_PCT; dyn_p.max = 100; dyn_p.show_off = -50;
        } else {
            dyn_p.kind = PK_PCT; dyn_p.max = 100;
        }
        return &dyn_p;
    }
    if (pg == PG_MIX) {
        dyn_p.min = 0; dyn_p.max = 100; dyn_p.kind = PK_PCT; dyn_p.opts = 0;
        if (sub == 3) {
            dyn_p.name = MAST_N[i];
            dyn_p.show_off = (i < 3) ? -50 : 0;
        } else {
            dyn_p.name = O_TRK[i];
            dyn_p.show_off = (sub == 1) ? -50 : 0;
        }
        return &dyn_p;
    }
    if (pg == PG_ENV && sub == 1) {
        dyn_p.name = MENV_N[i]; dyn_p.kind = PK_PCT; dyn_p.min = 0; dyn_p.max = 100; dyn_p.show_off = 0; dyn_p.opts = 0;
        return &dyn_p;
    }
    if (pg == PG_GLO && sub == 1) {
        dyn_p.name = KEYS_N[i]; dyn_p.min = 0; dyn_p.show_off = 0; dyn_p.opts = 0;
        if (i == 0) { dyn_p.kind = PK_ENUM; dyn_p.max = 11; dyn_p.opts = O_ROOT; }
        else if (i == 1) { dyn_p.kind = PK_ENUM; dyn_p.max = 7; dyn_p.opts = O_SCALE; }
        else if (i == 2) { dyn_p.kind = PK_PCT; dyn_p.max = 100; }
        else { dyn_p.kind = PK_ENUM; dyn_p.max = 1; dyn_p.opts = O_ONOFF; }
        return &dyn_p;
    }
    if (pg == PG_GLO && sub == 2) {
        static const char *const CHORD_N[4] = {"MODE", "TYPE", "VOICING", "STRUM"};
        int mode = st.chord[0];
        dyn_p.name = CHORD_N[i]; dyn_p.min = 0; dyn_p.show_off = 0; dyn_p.opts = 0;
        if (i == 0) { dyn_p.kind = PK_ENUM; dyn_p.max = CHORD_SHAPE; dyn_p.opts = O_CHMODE; }
        else if (i == 1) {
            dyn_p.kind = PK_ENUM;
            if (mode == CHORD_SCALE) { dyn_p.max = CHORD_NSCALE - 1; dyn_p.opts = O_CHSCALE; }
            else if (mode == CHORD_SHAPE) { dyn_p.max = CHORD_NSHAPE - 1; dyn_p.opts = O_CHSHAPE; }
            else { dyn_p.max = 0; dyn_p.opts = O_CHMEM; }
        }
        else if (i == 2) { dyn_p.kind = PK_ENUM; dyn_p.max = 3; dyn_p.opts = O_VOICING; }
        else { dyn_p.kind = PK_PCT; dyn_p.max = 100; }
        return &dyn_p;
    }
    if (pg == PG_ARP && arp_shake() && i > 0) {  /* (K1 is MODE: the page's own) */
        int s = arp_sub();
        dyn_p.name = SHAKE_N[s][i]; dyn_p.kind = PK_PCT; dyn_p.min = 0; dyn_p.opts = 0;
        dyn_p.max = (s && i > 1) ? 0 : 100;     /* 2/2's last two columns are empty */
        dyn_p.show_off = (!s && i == 2) ? -50 : 0;
        return &dyn_p;
    }
    if (pg == PG_SEQ) {
        if (st.seq[0] == SEQ_STEP) {
            dyn_p.min = 0; dyn_p.show_off = 0; dyn_p.opts = 0;
            if (i == 0) { dyn_p.name = "MODE"; dyn_p.kind = PK_ENUM; dyn_p.max = 1; dyn_p.opts = O_SEQMODE; }
            else if (i == 1) { dyn_p.name = "BARS"; dyn_p.kind = PK_ENUM; dyn_p.max = 5; dyn_p.opts = O_BARS; }
            else if (i == 2) { dyn_p.name = "TRACK"; dyn_p.kind = PK_ENUM; dyn_p.max = 3; dyn_p.opts = O_TRK; }
            else { dyn_p.name = "GRID"; dyn_p.kind = PK_ENUM; dyn_p.max = 2; dyn_p.opts = O_GRID; }
            return &dyn_p;
        }
        if (sub == 1) {
            dyn_p.name = TEDIT_N[i]; dyn_p.min = 0; dyn_p.show_off = 0; dyn_p.opts = 0;
            if (i == 0) { dyn_p.kind = PK_INT; dyn_p.max = 31; }
            else if (i == 1) { dyn_p.kind = PK_INT; dyn_p.max = 32; }
            else if (i == 2) { dyn_p.kind = PK_ENUM; dyn_p.max = 2; dyn_p.opts = O_QUANT; }
            else { dyn_p.kind = PK_ENUM; dyn_p.max = 1; dyn_p.opts = O_PLINK; }
            return &dyn_p;
        }
        // JSM SEQ 3rd page, notes list
        if (sub == 2) {
            int total = 0;
            uint8_t v = 0;
            eng_tape_note_info(st.track & 3, 0, 0, 0, 0, &total);
            if (total <= 0) notes_sel = 0;
            else if (notes_sel >= total) notes_sel = (uint8_t)(total - 1);
            dyn_p.min = 0; dyn_p.show_off = 0; dyn_p.opts = 0;
            if (i == 0) {
                dyn_p.name = "TIME"; dyn_p.kind = PK_INT;
                dyn_p.max = total > 0 ? (uint8_t)((total - 1) < 255 ? (total - 1) : 255) : 0;
            } else if (i == 1) {
                dyn_p.name = "NUDGE"; dyn_p.kind = PK_INT; dyn_p.max = 100;
            } else if (i == 2) {
                dyn_p.name = "VEL"; dyn_p.kind = PK_INT; dyn_p.min = 1; dyn_p.max = 127;
                if (total > 0 && eng_tape_note_info(st.track & 3, notes_sel, 0, 0, &v, 0))
                    dyn_p.min = 1;
            } else {
                dyn_p.name = "DEL"; dyn_p.kind = PK_ENUM; dyn_p.max = 1; dyn_p.opts = O_ONOFF;
            }
            return &dyn_p;
        }        
    }
    return &pages[pg].p[i];
}

uint8_t *page_value(int pg, int i)
{
    InstParams *ip = &st.ip[st.inst];
    int sub = subpage[pg];
    switch (pg) {
    case PG_FX:  return &st.fx_p[st.fx_type < NFX ? st.fx_type : 0][i];
    case PG_MIX:
        if (sub == 0) return &st.trk[i].vol;
        if (sub == 1) return &st.trk[i].pan;
        if (sub == 2) return &st.trk[i].send;
        return &st.master[i];
    case PG_ENV: return sub == 1 ? &ip->menv[i] : &ip->env[i];
    case PG_LFO: return &ip->lfo[i];
    case PG_GLO:
        if (sub == 2) {
            static uint8_t zero;
            if (i == 1 && st.chord[0] != CHORD_SCALE && st.chord[0] != CHORD_SHAPE) { zero = 0; return &zero; }
            return &st.chord[i];
        }
        return sub == 1 ? &st.keys[i] : &st.glo[i];
    case PG_ARP:
        if (i == 0 || !arp_shake()) return &ip->arp[i];
        if (!arp_sub()) return &st.shake[i - 1];             /* GRAV SPIN BOUNCE */
        if (i == 1) return &st.shake[3];                     /* CAGE */
        { static uint8_t empty; empty = 0; return &empty; }
    case PG_SEQ:
        if (st.seq[0] == SEQ_STEP) {
            if (i == 0) return &st.seq[0];
            if (i == 1) return &st.seq[1];
            if (i == 2) return &st.track;
            return &st.seq[4];
        }
        if (sub == 1) {
            if (i == 0) return &st.tape[1];
            if (i == 1) return &tape_len; // JSM
            if (i == 2) return &st.seq[3];
            return &st.seq[7];
        }
        if (sub == 2) { // JSM
            static uint8_t cur_val;
            uint8_t v = 0;
            if (i == 0) return &notes_sel;
            if (i == 1) { cur_val = 50; return &cur_val; }
            if (i == 2) {
                cur_val = eng_tape_note_info(st.track & 3, notes_sel, 0, 0, &v, 0) ? v : 0;
                return &cur_val;
            }
            return &notes_del;
        }
        if (i == 0) return &st.track;
        if (i == 1) return &st.seq[1];
        if (i == 2) return &st.tape[0];
        return &st.tape[3];
    default: return &KNOBS_NOW(st.inst)[i];
    }
}

/* ---- UI runtime state ----------------------------------------------------------------- */
static uint32_t now, last_ms, frame_n;
static f32 tsec, reel_ph;
static Vis vis;
static f32 vk[4], vkv[4];
static uint32_t vrng = 0x9E3779B9u;

static f32 name_t = 9.0f, page_t = 9.0f, toast_t = 9.0f, vol_t = 9.0f;
static f32 knob_touch[4] = {9.0f, 9.0f, 9.0f, 9.0f};
static int active_col = 0;
static f32 col_v[4], col_vv[4];
static char toast_s[24];
static int toast_pips = -1;
static uint32_t loop_full_was;

static int vol_seen = -1, glob_held = -1, master_raw = -1, edit_at = -1;
enum { RING_GLOBAL = 1, RING_INST };
static int ring_kind;
static f32 ring_t = 9.0f, ring_x, ring_v, ring_up = 9.0f;
#define RING_STAYS 0.9f

static SeqInfo seq;
static int seq_write_was, riff_rec_was;
static uint32_t enc_ms[H1_NENC];
static uint8_t key_note[H1_NKEYS], key_down[H1_NKEYS];
static uint8_t chord_last = CHORD_MEMORY;     /* the mode EDIT + GLO turns chords back on to */
static uint32_t btn_held, btn_ms[H1_NBTN];
static uint8_t btn_fired[H1_NBTN];

static uint16_t pal[256];
static uint8_t pal_bright_seen = 0;
static uint32_t prev_w[FB_W * FB_H / 4] H1_POOL;
#define prev_fb ((uint8_t *)prev_w)
static int force_full = 1;
#define RQ_MAX 512
static uint8_t rq[RQ_MAX][4];
static int rq_n, rq_i;
static uint32_t rq_bytes;
static int dirty;
static uint32_t dirty_ms;

static void mark_dirty(void) { dirty = 1; dirty_ms = now; }

/* ---- string helpers ------------------------------------------------------------------- */
static char *put_s(char *d, const char *s)
{
    while (*s) *d++ = *s++;
    *d = 0;
    return d;
}
static char *put_i(char *d, int v)
{
    char b[8];
    int n = 0;
    if (v < 0) { *d++ = '-'; v = -v; }
    do { b[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 7);
    while (n) *d++ = b[--n];
    *d = 0;
    return d;
}

/* CLEAR ALL?: REC held past 2.8 s asks, and the question stays on screen until it is answered. A tap of REC
 * within 3 s of letting go of the hold clears the whole tape. Anything else keeps it: waiting, another button,
 * a key, an encoder, or anything that puts up a toast of its own (so a prompt the screen no longer shows can
 * never be answered by a tap meant to record). */
#define CLEAR_ALL_WAIT 3000u
static int clear_all_asked;              /* 1: asked, REC still held; 2: REC let go, waiting for its tap */
static uint32_t clear_all_ms;            /* when REC was let go */
static int clear_all_answer;             /* its answer, once: 1 kept, 2 cleared (for a screen reader) */
int h1_clear_all_asked(void) { return clear_all_asked != 0; }
int h1_clear_all_answer(void) { int a = clear_all_answer; clear_all_answer = 0; return a; }

/* DICE (EDIT + LFO / EDIT + ENV, below): the engine and algorithm rolled, -1 for none, and its other knobs */
static uint32_t dice_rng = 0x2545F491u;
static int dice_inst = -1, dice_algo, dice_back;
static uint8_t dice_other[4];                   /* what EDIT + ENV swaps the knobs to */

static void toast(const char *a, int num, int has_num, const char *b, int pips)
{
    char *d = toast_s;
    if (clear_all_asked) clear_all_answer = 1;   /* (the question is no longer on screen: the tape is kept) */
    clear_all_asked = 0;
    if (a) d = put_s(d, a);
    if (has_num) d = put_i(d, num);
    if (b) put_s(d, b);
    toast_t = 0.0f;
    toast_pips = pips;
}

static void fit_text(f32 cx, f32 y, const char *s, f32 size, f32 max_w, f32 a)
{
    f32 w = g_text_w(s, size);
    if (w > max_w && w > 0.1f) {
        f32 ns = size * max_w / w;
        y += (size - ns) * 0.5f;
        size = ns;
    }
    g_text(cx, y, s, size, maxf(1.25f, size * 0.13f), a, 1);
}

static void clear_all_keep(void)                 /* CLEAR ALL? answered no, by anything but a tap of REC */
{
    if (clear_all_asked) toast("TAPE KEPT", 0, 0, 0, -1);
}

static APART void value_text(int pg, int i, char *out)
{
    const ParamDef *p = page_param(pg, i);
    int v = *page_value(pg, i);
    if (!p->name[0]) {                           /* an empty column (SHAKE 2/2) says nothing */
        out[0] = 0;
        return;
    }
    if (pg == PG_SEQ && st.seq[0] == SEQ_TAPE && subpage[pg] == 1 && i == 0) {
        put_i(out, v + 1);                       /* IN BAR: bars counted from 1, as its toast and the tape's */
        return;
    }
    if (pg == PG_SEQ && st.seq[0] == SEQ_TAPE && subpage[pg] == 1 && i == 1) { // JSM
        int bars = 1 << (st.seq[1] < 6 ? st.seq[1] : 2);
        if (v == 0 || v > bars) {
            put_s(out, "END");
        } else {
            put_s(put_i(out, v), "");
        }
        return;
    }
    if (pg == PG_SEQ && st.seq[0] == SEQ_TAPE && subpage[pg] == 2) { //JSM
        if (i == 0) {
            uint16_t tk = 0;
            int bt, meas, in_bar, beat, div;
            char *d;
            if (!eng_tape_note_info(st.track & 3, notes_sel, &tk, 0, 0, 0)) {
                put_s(out, "---");
                return;
            }
            bt = seq_bar_ticks();
            meas = (bt > 0 ? (int)tk / bt : 0) + 1;
            in_bar = bt > 0 ? (int)tk % bt : (int)tk;
            beat = in_bar / 96;
            div = in_bar % 96;
            d = put_i(out, meas);
            *d++ = ':';
            d = put_i(d, beat);
            *d++ = ':';
            put_i(d, div);
            return;
        }
        if (i == 1) {
            put_s(out, "NUDGE");
            return;
        }
        if (i == 2) {
            uint8_t vel = 0;
            if (!eng_tape_note_info(st.track & 3, notes_sel, 0, 0, &vel, 0)) {
                put_s(out, "---");
                return;
            }
            put_i(out, vel);
            return;
        }
    }
    if (p->kind == PK_ENUM) {
        if (pg == PG_LFO && i == 3 && v >= 1 && v <= 4)
            put_s(out, insts[st.inst]->knob[v - 1]);
        else
            put_s(out, p->opts[v <= p->max ? v : 0]);
    } else {
        int shown = v + p->show_off;
        char *d = out;
        if (p->show_off < 0 && shown > 0) *d++ = '+';
        put_i(d, shown);
    }
}

/* ---- input helpers -------------------------------------------------------------------- */
static int accel(int enc, int d)
{
    uint32_t gap = now - enc_ms[enc];
    int m = 1, ad = d < 0 ? -d : d;
    if (btn_held & (1u << H1_BTN_EDIT)) m = 1;       /* EDIT held = fine 1-unit precision */
    else if (gap < 22u) m = 6;
    else if (ad >= 3 || gap < 45u) m = 4;
    else if (ad == 2 || gap < 90u) m = 2;
    enc_ms[enc] = now;
    return d * m;
}

static APART void sync_cols(int pg)
{
    int i;
    if (pg == PG_HOME) return;
    if (pg == PG_ARP && !arp_shake()) subpage[PG_ARP] = 0;
    for (i = 0; i < 4; i++) {
        const ParamDef *p = page_param(pg, i);
        col_v[i] = p->max > p->min ? (f32)(*page_value(pg, i) - p->min) / (f32)(p->max - p->min) : 0.0f;
        col_vv[i] = 0.0f;
    }
}

static APART void set_instrument(int i)
{
    i = (i % NINST + NINST) % NINST;
    st.inst = (uint8_t)i;
    st.trk[st.track].inst = (uint8_t)i;
    st.trk[st.track].algo = st.ip[i].algo & 7;
    eng_post(EV_INST, i);
    name_t = 0.0f;
    page = PG_HOME;
    vis.fresh = 1;
    vis.enter = 0.0f;
    vis.algo = vis.algo_prev = st.ip[i].algo;
    vis.algo_t = 1.0f;
    toast(insts[i]->algo[st.ip[i].algo], 0, 0, 0, st.ip[i].algo);
    mark_dirty();
}

static void set_track(int t)
{
    t = (t % NTRACK + NTRACK) % NTRACK;
    st.track = (uint8_t)t;
    /* (the engine keeps the outgoing track's engine and algorithm, and brings the new one's, in that order:
     * changing st.inst here first had the outgoing track keep the incoming one's) */
    eng_post(EV_TRACK, t);
    vis.fresh = 1;
    vis.enter = 0.0f;                            /* (the picture is the new track's: it comes in as a new engine's) */
    toast("TRACK ", t + 1, 1, 0, -1);
    mark_dirty();
}

static APART void open_page(int pg)
{
    toast_t = 9.0f;
    if (page == pg) {
        int ns = page_nsub(pg);
        if (ns > 1) {
            subpage[pg] = (subpage[pg] + 1) % ns;
            if (pg == PG_SEQ && subpage[pg] == 2) {
                notes_trk = st.track & 3;
                notes_sel = 0;
                notes_del = 0;
            }
            page_t = 0.0f;
            sync_cols(pg);
            return;
        }
        pg = PG_HOME;
    } else {
        subpage[pg] = 0;
    }
    page = pg;
    page_t = 0.0f;
    sync_cols(pg);
}

static void set_seq_mode(int m)
{
    if (m == st.seq[0]) return;
    st.seq[0] = (uint8_t)(m & 1);
    eng_post(EV_SEQ_MODE, 0);
    if (page == PG_SEQ) sync_cols(PG_SEQ);
    mark_dirty();
}

void h1_key(int key, int down)
{
    int i;
    if (key < 0 || key >= H1_NKEYS) return;
    if (down) clear_all_keep();
    /* SAVE + White Keys 1..8: Instant Patch Snapshots! */
    if (down && (btn_held & (1u << H1_BTN_SAVE))) {
        for (i = 0; i < NSNAP; i++) {
            if (key == SNAP_KEYS[i]) {
                btn_fired[H1_BTN_SAVE] = 3;
                h1_set_cur_snap((uint8_t)i); // JSM
                if ((btn_held & (1u << H1_BTN_EDIT)) || !st.snap[i].used) {
                    eng_post(EV_SNAP_SAVE, i);
                    toast("SNAP ", i + 1, 1, " SAVED", -1);
                } else {
                    eng_post(EV_SNAP_LOAD, i);
                    dice_inst = -1;
                    toast("SNAP ", i + 1, 1, " LOADED", -1);
                }
                mark_dirty();
                return;
            }
        }
    }
    if (down) {
        int note = scale_snap(53 + key + 12 * st.octave);
        if (key_down[key]) return;
        note = iclamp(note, 0, 127);
        key_down[key] = 1;
        key_note[key] = (uint8_t)note;
        eng_post(EV_NOTE_ON, note);
    } else if (key_down[key]) {
        key_down[key] = 0;
        eng_post(EV_NOTE_OFF, key_note[key]);
    }
}

void h1_midi(uint8_t status, uint8_t d1, uint8_t d2)
{
    uint8_t type = status & 0xF0u;
    if (type == 0x90u && d2)
        eng_post_note(1, d1 & 127, d2 & 127, 1);
    else if (type == 0x80u || type == 0x90u)
        eng_post_note(0, d1 & 127, 0, 1);
    else if (type == 0xB0u && d1 == 123)
        eng_post(EV_MIDI_OFF, 0);                /* (all notes off: the tape plays on) */
    else if (type == 0xB0u && d1 >= 20 && d1 <= 23) {
        int k = d1 - 20;
        uint8_t val = (uint8_t)(((d2 & 127) * 100 + 63) / 127);
        KNOBS_NOW(st.inst)[k] = val;
        knob_touch[k] = 0.0f;
        eng_post_auto(k, val);
        mark_dirty();                            /* (a knob set over MIDI is saved as a turned one is) */
    }
}

/* ---- MASTER & Track Volume Ring ------------------------------------------------------- */
static f32 ring_value(int kind)
{
    int g = glob_held >= 0 ? glob_held : vol_seen;
    return kind == RING_INST ? (f32)st.trk[st.track].vol * 0.01f : (f32)(g < 0 ? 0 : g) * 0.001f;
}

static void ring_show(int kind)
{
    if (kind != ring_kind || ring_t >= RING_STAYS + 0.4f) {
        if (ring_t >= RING_STAYS + 0.4f) ring_up = 0.0f;
        ring_x = ring_value(kind);
        ring_v = 0.0f;
    }
    ring_kind = kind;
    ring_t = 0.0f;
    name_t = maxf(name_t, 1.05f);
}

int h1_volume_ring(void) { return ring_t < RING_STAYS + 0.4f ? ring_kind : 0; }

static void set_trk_vol(int v)
{
    v = iclamp(v, 0, 100);
    if (v != st.trk[st.track].vol) {
        st.trk[st.track].vol = (uint8_t)v;
        mark_dirty();
    }
    ring_show(RING_INST);
}

int h1_global_volume(void)
{
    int g = glob_held, e = edit_at;
    if (g < 0 && e >= 0 && (btn_held & (1u << H1_BTN_EDIT))) g = e;
    return g;
}

void h1_master(int level)
{
    int d, was = master_raw, from;
    level = iclamp(level, 0, 1000);
    master_raw = level;
    if (vol_seen < 0) { vol_seen = level; return; }
    d = level > vol_seen ? level - vol_seen : vol_seen - level;
    if (d < (vol_t < 1.2f ? 3 : 12)) return;
    from = vol_seen;
    vol_seen = level;
    vol_t = 0.0f;
    if (btn_held & (1u << H1_BTN_EDIT)) {
        if (glob_held < 0) glob_held = edit_at >= 0 ? edit_at : was;
        btn_fired[H1_BTN_EDIT] = 1;
        set_trk_vol(((level - 15) * 100 + 485) / 970);
        return;
    }
    if (glob_held >= 0) {
        int near = level > glob_held ? level - glob_held : glob_held - level;
        if (near <= 12 || (from <= glob_held && level >= glob_held) || (from >= glob_held && level <= glob_held))
            glob_held = -1;
        edit_at = -1;
    }
    ring_show(RING_GLOBAL);
}

static void octave(int d)
{
    if (d == 0) st.octave = 0;
    else st.octave = (int8_t)iclamp(st.octave + d, -2, 2);
    toast("OCT ", st.octave, 1, 0, -1);
    if (st.octave > 0) { toast_s[4] = '+'; put_i(toast_s + 5, st.octave); }
    mark_dirty();
}

/* ---- DICE: EDIT + LFO rolls the knobs of the algorithm in hand (dice.c), EDIT + ENV swaps back ---- */
static void set_knob(int i, int v);

static void dice_apply(const uint8_t k[4])
{
    int i;
    for (i = 0; i < 4; i++) set_knob(i, k[i]);  /* (as a hand would: automation while recording, CC 20-23) */
}

static void dice(void)
{
    int a = st.ip[st.inst].algo & 7, i, n;
    uint8_t r[4], *k = KNOBS_NOW(st.inst);
    dice_rng += now * 0x9E3779B9u + 1u;         /* (the press's time in it: no two power-ons roll alike) */
    for (n = 0; n < 4; n++) {                   /* (a roll that lands where the knobs are is rolled again) */
        dice_roll(st.inst, a, &dice_rng, r);
        if (r[0] != k[0] || r[1] != k[1] || r[2] != k[2] || r[3] != k[3]) break;
    }
    for (i = 0; i < 4; i++) dice_other[i] = k[i];
    dice_inst = st.inst; dice_algo = a; dice_back = 0;
    dice_apply(r);
    toast("DICE ", 0, 0, insts[st.inst]->algo[a], -1);
}

static void dice_swap(void)
{
    uint8_t was[4], *k = KNOBS_NOW(st.inst);
    int i;
    if (dice_inst != st.inst || dice_algo != (st.ip[st.inst].algo & 7)) {
        toast("NO DICE TO UNDO", 0, 0, 0, -1);
        return;
    }
    for (i = 0; i < 4; i++) was[i] = k[i];
    dice_apply(dice_other);
    for (i = 0; i < 4; i++) dice_other[i] = was[i];
    dice_back ^= 1;
    toast(dice_back ? "BEFORE DICE" : "AFTER DICE", 0, 0, 0, -1);
}

SMALL void h1_button(int btn, int down)
{
    if (btn < 0 || btn >= H1_NBTN) return;
    if (!down) {
        btn_held &= ~(1u << btn);
        if (btn == H1_BTN_REC && clear_all_asked == 1) {   /* the hold that asked let go: 3 s to tap REC */
            clear_all_asked = 2;
            clear_all_ms = now;
        }
        if (btn_fired[btn]) return;
        if (btn == H1_BTN_REC) {
            if (page == PG_ARP && st.ip[st.inst].arp[0] == ARP_RIFF)
                eng_post(EV_RIFF_REC, 0);
            else if (st.seq[0] == SEQ_TAPE && st.tape[0] == TAPE_SPD_REV &&
                     (seq.loop_state == LOOP_EMPTY || seq.loop_state == LOOP_IDLE || seq.loop_state == LOOP_PLAY))
                toast("NO REC IN REV", 0, 0, 0, -1);   /* (a take in REV would play back as held notes) */
            else
                eng_post(EV_REC, 0);
        }
        if (btn == H1_BTN_GLO)  open_page(PG_GLO);
        if (btn == H1_BTN_SEQ)  open_page(PG_SEQ);
        if (btn == H1_BTN_HOME) {
            if (page == PG_HOME) name_t = 0.0f;
            page = PG_HOME;
        }
        if (btn == H1_BTN_SAVE) {
            dirty = 1; dirty_ms = now - 5000u;
            toast(eng_quiet() ? "SAVED" : "SAVE WHEN QUIET", 0, 0, 0, -1);   /* (the save waits for silence) */
        }
        if (btn == H1_BTN_EDIT) {
            if (seq.riff_rec) {
                eng_post(EV_RIFF_REST, 0);
                toast("REST", 0, 0, 0, -1);
            } else if (seq.mode == SEQ_STEP && seq.write && page == PG_HOME) {
                eng_post(EV_STEP_REST, 0);
            } else {
                st.hud ^= 1;
                mark_dirty();
            }
        }
        return;
    }
    if (btn == H1_BTN_EDIT) edit_at = master_raw;
    btn_held |= 1u << btn;
    btn_ms[btn] = now;
    btn_fired[btn] = 0;

    /* CLEAR ALL? asked: a tap of REC (EDIT not held) clears the whole tape; any other button keeps it */
    if (clear_all_asked) {
        if (btn == H1_BTN_REC && clear_all_asked == 2 && !(btn_held & (1u << H1_BTN_EDIT)) &&
            now - clear_all_ms < CLEAR_ALL_WAIT) {
            btn_fired[btn] = 1;                  /* (its release and holds do nothing more) */
            eng_post(EV_CLEAR, 0);
            toast("TAPE CLEARED", 0, 0, 0, -1);
            clear_all_answer = 2;
            return;
        }
        clear_all_keep();
    }

    /* EDIT + Button Shift Shortcuts */
    if (btn != H1_BTN_EDIT && (btn_held & (1u << H1_BTN_EDIT))) {
        btn_fired[H1_BTN_EDIT] = 2;              /* (2: a shortcut, whose toast the volume ring leaves alone) */
        btn_fired[btn] = 3;
        if (btn == H1_BTN_FX) {
            st.fx_on ^= 1;
            toast(st.fx_on ? "FX ON" : "FX BYPASS", 0, 0, 0, -1);
            mark_dirty();
            return;
        }
        if (btn == H1_BTN_SEL) {
            st.trk[st.track].mute ^= 1u;
            toast(st.trk[st.track].mute & 1u ? "TRACK MUTE" : "TRACK ON", 0, 0, 0, -1);
            mark_dirty();
            return;
        }
        if (btn == H1_BTN_REC) {
            eng_post(EV_CUT, 0);
            toast("CUT T", st.track + 1, 1, 0, -1);
            return;
        }
        if (btn == H1_BTN_PLAY) {
            eng_post(EV_PASTE, 0);
            toast("PASTED T", st.track + 1, 1, 0, -1);
            return;
        }
        if (btn == H1_BTN_GLO) {                 /* chord memory: the keys held become the chord; none, on/off */
            int notes[H1_NKEYS], n = 0, k, j;
            for (k = 0; k < H1_NKEYS; k++)
                if (key_down[k]) notes[n++] = key_note[k];
            for (k = 1; k < n; k++)
                for (j = k; j > 0 && notes[j - 1] > notes[j]; j--) { int t = notes[j]; notes[j] = notes[j - 1]; notes[j - 1] = t; }
            if (n >= 2) {
                int m = 0;
                for (k = 0; k < n && m < CHORD_MAX; k++)
                    if (m == 0 || notes[k] - notes[0] != st.chord_mem[m - 1])
                        st.chord_mem[m++] = (uint8_t)iclamp(notes[k] - notes[0], 0, 48);
                for (k = m; k < CHORD_MAX; k++) st.chord_mem[k] = 0;
                st.chord_n = (uint8_t)m;
                st.chord[0] = CHORD_MEMORY;
                chord_last = CHORD_MEMORY;
                toast("CHORD SET ", m, 1, " NOTES", -1);
            } else if (st.chord[0] != CHORD_OFF) {
                chord_last = st.chord[0];
                st.chord[0] = CHORD_OFF;
                toast("CHORD OFF", 0, 0, 0, -1);
            } else {
                st.chord[0] = chord_last;
                toast("CHORD: ", 0, 0, O_CHMODE[chord_last], -1);
            }
            if (page == PG_GLO) sync_cols(PG_GLO);
            mark_dirty();
            return;
        }
        if (btn == H1_BTN_LFO) { dice(); return; }
        if (btn == H1_BTN_ENV) { dice_swap(); return; }
        if (btn == H1_BTN_ARP) {
            st.keys[3] ^= 1u;
            if (!st.keys[3]) eng_post(EV_UNLATCH, 0);
            toast(st.keys[3] ? "HOLD ON" : "HOLD OFF", 0, 0, 0, -1);
            mark_dirty();
            return;
        }
        if (btn == H1_BTN_OCTDN) {
            int bars = 1 << (st.seq[1] < 6 ? st.seq[1] : 2);
            st.tape[1] = (uint8_t)((st.tape[1] + 1) % bars);
            toast("IN BAR ", st.tape[1] + 1, 1, 0, -1);
            mark_dirty();
            return;
        }
        if (btn == H1_BTN_OCTUP) {
            int bars = 1 << (st.seq[1] < 6 ? st.seq[1] : 2);
            tape_len = (uint8_t)(((int)tape_len + 1) % (bars + 1)); //JSM
            st.tape[2] = tape_len ? (uint8_t)iclamp((int)st.tape[1] + (int)tape_len, 1, 32) : 0;
            toast(tape_len ? "LEN " : "OUT FULL", tape_len, tape_len != 0, 0, -1);
            mark_dirty();
            return;
        }
    }

    switch (btn) {
    case H1_BTN_FX:    open_page(PG_FX); break;
    case H1_BTN_SEL:   open_page(PG_MIX); break;
    case H1_BTN_ENV:   open_page(PG_ENV); break;
    case H1_BTN_LFO:   open_page(PG_LFO); break;
    case H1_BTN_ARP:   open_page(PG_ARP); break;
    case H1_BTN_PLAY:  eng_post(EV_PLAY, 0); break;
    case H1_BTN_OCTDN: octave((btn_held & (1u << H1_BTN_OCTUP)) ? 0 : -1); break;
    case H1_BTN_OCTUP: octave((btn_held & (1u << H1_BTN_OCTDN)) ? 0 : 1); break;
    default: break;
    }
}

/* ---- long-press holds & USB host ------------------------------------------------------ */
#define HOLD_RESET_ALGO 900u
#define HOLD_RESET_INST 2200u
#define HOLD_RESET_ALL  4000u
#define HOLD_EDIT       400u
#define HOLD_USB        1000u

static void (*usb_fn)(int on);
static int usb_host;
void h1_usb_host_fn(void (*fn)(int on)) { usb_fn = fn; }
int h1_usb_host(void) { return usb_host; }
const char *h1_toast_now(void) { return toast_t < 1.25f ? toast_s : ""; }

void h1_toast(const char *text)
{
    int i = 0;
    for (; text && *text && i < 16; text++) {
        char c = *text;
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.'))
            c = ' ';
        if (c == ' ' && (i == 0 || toast_s[i - 1] == ' ')) continue;
        toast_s[i++] = c;
    }
    while (i > 0 && toast_s[i - 1] == ' ') i--;
    toast_s[i] = 0;
    if (clear_all_asked) clear_all_answer = 1;
    clear_all_asked = 0;
    toast_t = 0.0f;
    toast_pips = -1;
}

static void holds(void)
{
    if ((btn_held & (1u << H1_BTN_EDIT)) && !btn_fired[H1_BTN_EDIT] && now - btn_ms[H1_BTN_EDIT] > HOLD_EDIT) {
        btn_fired[H1_BTN_EDIT] = 1;
        ring_show(RING_INST);
    }
    if ((btn_held & (1u << H1_BTN_REC)) && !btn_fired[H1_BTN_REC] && now - btn_ms[H1_BTN_REC] > 700u) {
        btn_fired[H1_BTN_REC] = 1;
        if (page == PG_ARP && st.ip[st.inst].arp[0] == ARP_RIFF) {
            eng_post(EV_RIFF_CLEAR, 0);
            toast("RIFF CLR", 0, 0, 0, -1);
        } else {
            eng_post(EV_CLEAR, 1 + st.track);
            toast("CLEARED T", st.track + 1, 1, 0, -1);
            btn_fired[H1_BTN_REC] = 2;
        }
    }
    if ((btn_held & (1u << H1_BTN_REC)) && btn_fired[H1_BTN_REC] == 2 && now - btn_ms[H1_BTN_REC] > 2800u) {
        btn_fired[H1_BTN_REC] = 1;               /* (the whole tape only once asked: a long hold is easy by accident) */
        toast("CLEAR ALL? TAP REC", 0, 0, 0, -1);
        clear_all_asked = 1;
    }
    if (clear_all_asked == 2 && now - clear_all_ms >= CLEAR_ALL_WAIT)
        clear_all_keep();
    if ((btn_held & (1u << H1_BTN_SEQ)) && !btn_fired[H1_BTN_SEQ] && now - btn_ms[H1_BTN_SEQ] > 550u) {
        btn_fired[H1_BTN_SEQ] = 1;
        set_seq_mode(st.seq[0] == SEQ_STEP ? SEQ_TAPE : SEQ_STEP);
        toast(O_SEQMODE[st.seq[0]], 0, 0, 0, -1);
    }
    if ((btn_held & (1u << H1_BTN_GLO)) && !btn_fired[H1_BTN_GLO] && now - btn_ms[H1_BTN_GLO] > 550u) {
        btn_fired[H1_BTN_GLO] = 1;
        st.seq[5] = (uint8_t)((st.seq[5] + 1u) % 3u);
        toast(O_CLICK[st.seq[5]], 0, 0, 0, -1);
        mark_dirty();
    }
    if ((btn_held & (1u << H1_BTN_SAVE)) && !btn_fired[H1_BTN_SAVE] && now - btn_ms[H1_BTN_SAVE] > HOLD_RESET_ALGO) {
        int i;
        btn_fired[H1_BTN_SAVE] = 1;
        knobs_default(st.inst, st.ip[st.inst].algo);
        for (i = 0; i < 4; i++) knob_touch[i] = 0.0f;
        dice_inst = -1;
        toast(insts[st.inst]->algo[st.ip[st.inst].algo], 0, 0, " RESET", -1);
        mark_dirty();
    }
    if ((btn_held & (1u << H1_BTN_SAVE)) && btn_fired[H1_BTN_SAVE] == 1 && now - btn_ms[H1_BTN_SAVE] > HOLD_RESET_INST) {
        int alg = st.ip[st.inst].algo, i;
        btn_fired[H1_BTN_SAVE] = 2;
        st.ip[st.inst] = insts[st.inst]->def;
        st.trk[st.track].algo = st.ip[st.inst].algo & 7;   /* (the steps played the old one until a snapshot) */
        knobs_default(st.inst, -1);
        vis.algo_prev = alg; vis.algo = st.ip[st.inst].algo; vis.algo_t = 0.0f;
        for (i = 0; i < 4; i++) knob_touch[i] = 0.0f;
        dice_inst = -1;
        toast(insts[st.inst]->name, 0, 0, " RESET", -1);
        mark_dirty();
    }
    if ((btn_held & (1u << H1_BTN_SAVE)) && btn_fired[H1_BTN_SAVE] == 2 && now - btn_ms[H1_BTN_SAVE] > HOLD_RESET_ALL) {
        int i;
        btn_fired[H1_BTN_SAVE] = 3;
        for (i = 0; i < NINST; i++) { st.ip[i] = insts[i]->def; knobs_default(i, -1); }
        dice_inst = -1;
        st.trk[st.track].algo = st.ip[st.inst].algo & 7;
        st.keys[0] = st.keys[1] = st.keys[2] = st.keys[3] = 0;
        st.glo[0] = 118; st.glo[1] = 0; st.glo[2] = 50; st.glo[3] = 100;
        st.octave = 0;
        eng_post(EV_UNLATCH, 0);
        toast("ALL RESET", 0, 0, 0, -1);
        mark_dirty();
    }
    if ((btn_held & (1u << H1_BTN_HOME)) && !btn_fired[H1_BTN_HOME] && now - btn_ms[H1_BTN_HOME] > 700u) {
        btn_fired[H1_BTN_HOME] = 1;
        eng_post(EV_PANIC, 0);
        toast("HUSH", 0, 0, 0, -1);
    }
    if ((btn_held & (1u << H1_BTN_OCTDN)) && (btn_held & (1u << H1_BTN_OCTUP)) &&
        !btn_fired[H1_BTN_OCTDN] && !btn_fired[H1_BTN_OCTUP] &&
        now - btn_ms[H1_BTN_OCTDN] > HOLD_USB && now - btn_ms[H1_BTN_OCTUP] > HOLD_USB) {
        btn_fired[H1_BTN_OCTDN] = btn_fired[H1_BTN_OCTUP] = 1;
        if (usb_fn) {
            usb_host = !usb_host;
            usb_fn(usb_host);
            toast(usb_host ? "USB HOST" : "USB COMPUTER", 0, 0, 0, -1);
        } else toast("NO USB HOST", 0, 0, 0, -1);
    }
}

/* ---- encoders ------------------------------------------------------------------------- */
static void set_algo(int a)
{
    InstParams *ip = &st.ip[st.inst];
    a = (a % NALGO + NALGO) % NALGO;
    if (a == ip->algo) return;
    vis.algo_prev = ip->algo;
    ip->algo = (uint8_t)a;
    st.trk[st.track].algo = (uint8_t)a;
    vis.algo = a;
    vis.algo_t = 0.0f;
    toast(insts[st.inst]->algo[a], 0, 0, 0, a);
    mark_dirty();
}

static void set_fx_type(int f)
{
    f = (f % NFX + NFX) % NFX;
    if (f == st.fx_type) return;
    st.fx_type = (uint8_t)f;
    sync_cols(PG_FX);
    toast(FX_NAMES[f], 0, 0, 0, f);
    mark_dirty();
}

static void set_tempo(int bpm)
{
    st.glo[0] = (uint8_t)iclamp(bpm, 40, 240);
    toast(0, st.glo[0], 1, " BPM", -1);
    mark_dirty();
}

static void set_knob(int i, int v)
{
    uint8_t *k = &KNOBS_NOW(st.inst)[i], was = *k;
    *k = (uint8_t)iclamp(v, 0, 100);
    knob_touch[i] = 0.0f;
    mark_dirty();
    if (*k != was) {
        eng_post_auto(i, *k);
        if (eng_midi_fn)
            eng_midi_fn(0xB0u, (uint8_t)(20 + i), (uint8_t)((*k * 127 + 50) / 100));
    }
}

static void set_arp_mode(int m)                 /* ALGORITHM on the SEQ / ARP page, or EDIT + ALGORITHM */
{
    if (m == ARP_SHAKE && st.ip[st.inst].arp[0] != ARP_SHAKE) subpage[PG_ARP] = 0;   /* SHAKE opens on 1/2 */
    st.ip[st.inst].arp[0] = (uint8_t)m;
    if (page == PG_ARP) sync_cols(PG_ARP);
    toast("SEQ: ", 0, 0, O_ARPMODE[m], -1);
    mark_dirty();
}

static APART void set_param(int i, int v)
{
    const ParamDef *p = page_param(page, i);
    uint8_t *q = page_value(page, i);
    v = iclamp(v, p->min, p->max);
    if (p->name[0]) active_col = i;              /* (an empty column is never the one in hand) */
    if (v == *q) return;
    if (page == PG_SEQ && st.seq[0] == SEQ_TAPE && subpage[PG_SEQ] == 1 && i == 0) { //JSM
        st.tape[1] = (uint8_t)v;
        if (tape_len > 0) {
            st.tape[2] = (uint8_t)iclamp((int)st.tape[1] + (int)tape_len, 1, 32);
        }
        mark_dirty();
        return;
    }
    if (page == PG_SEQ && st.seq[0] == SEQ_TAPE && subpage[PG_SEQ] == 1 && i == 1) { //JSM
        tape_len = (uint8_t)v;
        st.tape[2] = v == 0 ? 0 : (uint8_t)iclamp((int)st.tape[1] + v, 1, 32);
        mark_dirty();
        return;
    }
    if (page == PG_SEQ && st.seq[0] == SEQ_TAPE && subpage[PG_SEQ] == 2) { //JSM
        if (i == 0) {
            notes_sel = (uint8_t)v;
            notes_del = 0;
            sync_cols(PG_SEQ);
            mark_dirty();
            return;
        }
        if (i == 1) {
            int delta = v - (int)(*q);
            if (delta) eng_tape_note_edit(st.track & 3, notes_sel, delta, 0);
            sync_cols(PG_SEQ);
            mark_dirty();
            return;
        }
        if (i == 2) {
            uint8_t cur_v = 0;
            if (eng_tape_note_info(st.track & 3, notes_sel, 0, 0, &cur_v, 0)) {
                int delta = v - (int)cur_v;
                if (delta) eng_tape_note_edit(st.track & 3, notes_sel, 0, delta);
            }
            sync_cols(PG_SEQ);
            mark_dirty();
            return;
        }
        if (i == 3 && v != 0) {
            int total = 0;
            eng_tape_note_info(st.track & 3, 0, 0, 0, 0, &total);
            if (total > 0 && notes_sel < total) {
                eng_post_del(st.track & 3, notes_sel);
                if (notes_sel >= total - 1)
                    notes_sel = (uint8_t)(total > 1 ? total - 2 : 0);
            }
            notes_del = 0;
            sync_cols(PG_SEQ);
            mark_dirty();
            return;
        }
    }
    if (page == PG_ARP && i == 0 && v == ARP_SHAKE) subpage[PG_ARP] = 0;   /* SHAKE opens on 1/2 */
    *q = (uint8_t)v;
    if (page == PG_GLO && subpage[PG_GLO] == 1 && i == 3 && !v) eng_post(EV_UNLATCH, 0);
    if (page == PG_GLO && subpage[PG_GLO] == 2 && i == 0) {  /* a new mode: its type in its range */
        int tmax = v == CHORD_SCALE ? CHORD_NSCALE - 1 : (v == CHORD_SHAPE ? CHORD_NSHAPE - 1 : 0);
        if (st.chord[1] > tmax) st.chord[1] = (uint8_t)tmax;
        if (v != CHORD_OFF) chord_last = (uint8_t)v;
    }
    if (page == PG_ARP && i == 0) sync_cols(PG_ARP);
    mark_dirty();
}

static int step_cursor_live(void) { return page == PG_HOME && seq.mode == SEQ_STEP && seq.write; }

SMALL void h1_encoder(int enc, int d)
{
    if (!d || enc < 0 || enc >= H1_NENC) return;
    clear_all_keep();
    switch (enc) {
    case H1_ENC_PRESET:
        /*if (btn_held & (1u << H1_BTN_EDIT)) {
            btn_fired[H1_BTN_EDIT] = 1;
            set_track(st.track + d);
        } else {
            set_instrument(st.inst + d);
        }*/
        set_algo(st.ip[st.inst].algo + d); // JSM
        break;
    case H1_ENC_ALGO:
        if (page == PG_FX) set_fx_type(st.fx_type + d);
        else if (page == PG_ARP || (btn_held & (1u << H1_BTN_EDIT))) {
            if (btn_held & (1u << H1_BTN_EDIT)) btn_fired[H1_BTN_EDIT] = 1;
            set_arp_mode(((st.ip[st.inst].arp[0] + d) % NARPMODE + NARPMODE) % NARPMODE);
        } else {
            // set_algo(st.ip[st.inst].algo + d);
            set_track(st.track + d); // JSM
        }
        break;
    case H1_ENC_SELECT:
        if (btn_held & (1u << H1_BTN_EDIT)) {
            btn_fired[H1_BTN_EDIT] = 1;
            set_trk_vol(st.trk[st.track].vol + accel(enc, d));
        } else if (step_cursor_live()) {
            eng_post(EV_STEP_MOVE, iclamp(d, -100, 100) + 128);
        } else if (page == PG_HOME) {
            // set_tempo(st.glo[0] + accel(enc, d));
            set_instrument(st.inst + d); // JSM
        } else {
            int ns = page_nsub(page);
            if (ns > 1) {
                subpage[page] = (subpage[page] + (d > 0 ? 1 : -1) + ns) % ns;
                if (page == PG_SEQ && subpage[page] == 2) {
                    notes_trk = st.track & 3;
                    notes_sel = 0;
                    notes_del = 0;
                }
                page_t = 0.0f;
                sync_cols(page);
            } else if (page == PG_FX) {
                set_fx_type(st.fx_type + (d > 0 ? 1 : -1));
            } else {
                int np = ((page - 1 + (d > 0 ? 1 : -1)) % (NPAGE - 1) + (NPAGE - 1)) % (NPAGE - 1) + 1;
                page = np; page_t = 0.0f; sync_cols(np);
            }
        }
        break;
    default: {
        int i = enc - H1_ENC_K1;
        if (page == PG_MIX && subpage[PG_MIX] == 0 && (btn_held & (1u << H1_BTN_EDIT))) {
            btn_fired[H1_BTN_EDIT] = 1;
            st.trk[i].mute ^= 1u;
            toast(st.trk[i].mute & 1u ? "MUTE T" : "UNMUTE T", i + 1, 1, 0, -1);
            mark_dirty();
            return;
        }
        if (page == PG_HOME) { // JSM, show the synth parameter in big
            uint8_t val;
            int pip;
            set_knob(i, KNOBS_NOW(st.inst)[i] + accel(enc, d));
            val = KNOBS_NOW(st.inst)[i];
            pip = ((int)val * 7 + 50) / 100;
            toast(insts[st.inst]->knob[i], 0, 0, 0, pip);
            mark_dirty();
        }
        else {
            const ParamDef *p = page_param(page, i);
            int step = p->kind == PK_ENUM || (p->kind == PK_INT && p->max - p->min < 16) ? d : accel(enc, d);
            set_param(i, *page_value(page, i) + step);
        }
        break;
    }
    }
}

SMALL int h1_control(int enc, int *lo, int *hi, const char **name, const char **text)
{
    static char buf[H1_NENC][20];
    int v = 0, a = 0, b = 0;
    const char *nm = "", *tx = "";
    char *d;
    if (enc < 0 || enc >= H1_NENC) return 0;
    d = buf[enc]; d[0] = 0; tx = d;
    switch (enc) {
    case H1_ENC_PRESET:
        v = st.inst; b = NINST - 1; nm = "ENGINE"; tx = insts[st.inst]->name;
        break;
    case H1_ENC_ALGO:
        if (page == PG_FX) { v = st.fx_type; b = NFX - 1; nm = "FX"; tx = FX_NAMES[st.fx_type]; }
        else if (page == PG_ARP) {               /* (the encoder turns the SEQ mode here, so the slider does) */
            v = st.ip[st.inst].arp[0]; b = NARPMODE - 1; nm = "SEQ MODE"; tx = O_ARPMODE[v < NARPMODE ? v : 0];
        } else { v = st.ip[st.inst].algo; b = NALGO - 1; nm = "PRESET"; tx = insts[st.inst]->algo[v]; }
        break;
    case H1_ENC_SELECT:
        if ((btn_held & (1u << H1_BTN_EDIT)) && btn_fired[H1_BTN_EDIT]) {
            v = st.trk[st.track].vol; b = 100; nm = "TRK VOL"; put_i(d, v);
        } else if (page == PG_HOME) {
            v = st.glo[0]; a = 40; b = 240; nm = "TEMPO"; put_s(put_i(d, v), " BPM");
        } else {
            v = ui_subpage(); a = 0; b = page_nsub(page) - 1; nm = "SUBPAGE"; put_i(d, v + 1);
        }
        break;
    default: {
        int i = enc - H1_ENC_K1;
        if (page == PG_HOME) {
            v = KNOBS_NOW(st.inst)[i]; b = 100; nm = insts[st.inst]->knob[i]; put_i(d, v);
        } else {
            const ParamDef *p = page_param(page, i);
            v = *page_value(page, i); a = p->min; b = p->max; nm = p->name;
            value_text(page, i, d);
        }
        break;
    }
    }
    if (lo) *lo = a;
    if (hi) *hi = b;
    if (name) *name = nm;
    if (text) *text = tx;
    return v;
}

SMALL void h1_control_set(int enc, int v)
{
    if (enc < 0 || enc >= H1_NENC) return;
    clear_all_keep();
    switch (enc) {
    case H1_ENC_PRESET: set_instrument(iclamp(v, 0, NINST - 1)); break;
    case H1_ENC_ALGO:
        if (page == PG_FX) set_fx_type(iclamp(v, 0, NFX - 1));
        else if (page == PG_ARP) { if (v != st.ip[st.inst].arp[0]) set_arp_mode(iclamp(v, 0, NARPMODE - 1)); }
        else set_algo(iclamp(v, 0, NALGO - 1));
        break;
    case H1_ENC_SELECT:
        if (page == PG_HOME) set_tempo(iclamp(v, 40, 240));
        else {
            subpage[page] = iclamp(v, 0, page_nsub(page) - 1);
            if (page == PG_SEQ && subpage[page] == 2) {
                notes_trk = st.track & 3;
                notes_sel = 0;
                notes_del = 0;
            }
            sync_cols(page);
        }
        break;
    default:
        if (page == PG_HOME) set_knob(enc - H1_ENC_K1, v);
        else set_param(enc - H1_ENC_K1, v);
        break;
    }
}

/* ---- 8-Color x 32-Shade RGB565 Palette ------------------------------------------- */
static const uint8_t OP1_RGB[8][3] = { // JSM
    {240, 243, 248},   /* 0 GC_WHITE  (K3) */
    {205, 180, 219},//{ 59, 158, 255},   /* 1 GC_BLUE   (K1 - TE Blue) */
    {255, 200, 221},//{ 68, 227, 126},   /* 2 GC_GREEN  (K2 - TE Green) */
    {255, 175, 204},//{255, 107,  26},   /* 3 GC_ORANGE (K4 - TE Orange) */
    {189, 224, 254},//{255,  42,  75},   /* 4 GC_RED    (REC / Alert) */
    {255, 210,  74},   /* 5 GC_GOLD   (Playhead / Active) */
    { 42, 230, 224},   /* 6 GC_CYAN   (LFO / FX) */
    {115, 128, 148},   /* 7 GC_SLATE  (Grids / Chrome) */
};

static void build_palette(void)
{
    int b, i;
    f32 bright;
    if (pal_bright_seen == st.glo[3])
        return;
    pal_bright_seen = st.glo[3];
    bright = (f32)st.glo[3] * 0.01f;
    for (b = 0; b < 8; b++) {
        pal[b * 32] = 0;
        for (i = 1; i < 32; i++) {
            f32 x = (f32)i * (1.0f / 31.0f);
            f32 v = lerpf(x, sqrtfx(x), 0.25f) * bright;
            uint32_t r = (uint32_t)((f32)OP1_RGB[b][0] * v * (31.0f / 255.0f) + 0.5f);
            uint32_t g = (uint32_t)((f32)OP1_RGB[b][1] * v * (63.0f / 255.0f) + 0.5f);
            uint32_t bl = (uint32_t)((f32)OP1_RGB[b][2] * v * (31.0f / 255.0f) + 0.5f);
            if (r > 31u) r = 31u;
            if (g > 63u) g = 63u;
            if (bl > 31u) bl = 31u;
            pal[b * 32 + i] = (uint16_t)((r << 11) | (g << 5) | bl);
        }
    }
    force_full = 1;
}

/* ---- UI Chrome Drawing ----------------------------------------------------------- */
static void draw_topbar(const char *left_tag, const char *title, const char *right_tag)
{
    /* Dark top chrome bar */
    g_mode(G_PAINT);
    g_rect(0.0f, 0.0f, 240.0f, 26.0f, 0.0f);
    g_mode(G_MAX);

    /* Track pill on left colored with track color (T1..T4) */
    g_color(KCOL[st.track & 3]);
    g_box(6.0f, 5.0f, 22.0f, 15.0f, 1.4f, 0.9f);
    g_text(17.0f, 8.0f, left_tag, 9.0f, 1.4f, 1.0f, 1);

    /* Engine / Page title (auto-scaled so it never overlaps right status tag) */
    g_color(GC_WHITE);
    {
        f32 sz = 10.5f, tw = g_text_w(title, sz);
        if (tw > 128.0f && tw > 0.1f) sz *= 128.0f / tw;
        g_text(34.0f, 12.5f - sz * 0.5f, title, sz, maxf(1.3f, sz * 0.15f), 1.0f, 0);
    }

    /* Right status tag (BPM / FX / Algo) */
    g_color(GC_SLATE);
    g_text(224.0f, 8.0f, right_tag, 9.0f, 1.3f, 0.9f, 2);

    /* REC / PLAY indicator dot at far right */
    if (seq.loop_state == LOOP_REC || seq.loop_state == LOOP_DUB || (seq.loop_state == LOOP_ARMED && fractf(tsec * 2.0f) < 0.5f)) {
        g_color(GC_RED);
        g_disc(232.0f, 12.0f, 3.6f, 1.0f);
    } else if (seq.loop_state == LOOP_PLAY || seq.playing) {
        g_color(GC_GREEN);
        g_disc(232.0f, 12.0f, 3.0f, 0.95f);
    }
    g_color(GC_SLATE);
    g_line(0.0f, 26.0f, 240.0f, 26.0f, 1.0f, 0.35f);
    if (vis.level > 0.01f) {
        f32 mw = minf(120.0f, vis.level * 320.0f);
        g_color(KCOL[st.track & 3]);
        g_line(120.0f - mw, 26.0f, 120.0f + mw, 26.0f, 1.6f, minf(0.95f, 0.35f + vis.level * 2.2f));
    }
    g_color(GC_WHITE);
}

static void draw_hud(void)
{
    const InstDef *in = insts[st.inst];
    int i;
    for (i = 0; i < 4; i++) {
        f32 a = st.hud ? 0.94f : 1.0f - smooth01((knob_touch[i] - 0.9f) / 0.5f);
        f32 x0 = 8.0f + (f32)i * 58.0f, w = 50.0f, v = vk[i];
        if (a <= 0.01f) continue;
        g_mode(G_PAINT);
        g_rect(x0 - 3.0f, 204.0f, w + 6.0f, 31.0f, 0.0f);
        g_mode(G_MAX);
        g_dim(a);
        g_color(GC_SLATE);
        g_line(x0, 211.0f, x0 + w, 211.0f, 2.2f, 0.38f);
        g_color(KCOL[i]);
        g_line(x0, 211.0f, x0 + w * v, 211.0f, 4.2f, 1.0f);
        g_disc(x0 + w * v, 211.0f, 2.6f, 1.0f);
        fit_text(x0 + w * 0.5f, 219.0f, in->knob[i], 9.0f, w, 0.95f);
        g_dim(1.0f);
    }
    g_color(GC_WHITE);
}

static void draw_bottom_cols(int pg, f32 top_y)
{
    char buf[20];
    int i;
    for (i = 0; i < 4; i++) {
        const ParamDef *p = page_param(pg, i);
        f32 cx = 30.0f + (f32)i * 60.0f;
        int v = *page_value(pg, i);
        f32 tgt;
        if (!p->name[0]) continue;               /* an empty column (SHAKE 2/2) */
        tgt = p->max > p->min ? (f32)(v - p->min) / (f32)(p->max - p->min) : 0.0f;
        spring(&col_v[i], &col_vv[i], tgt, 26.0f, vis.dt);

        g_color(GC_SLATE);
        g_line(cx - 22.0f, top_y, cx + 22.0f, top_y, 2.0f, 0.35f);
        g_color(KCOL[i]);
        if (p->show_off < 0) {
            f32 mid = cx, px = (cx - 22.0f) + col_v[i] * 44.0f;
            g_line(mid, top_y, px, top_y, 4.0f, 1.0f);
            g_disc(px, top_y, 3.0f, 1.0f);
        } else {
            f32 px = (cx - 22.0f) + col_v[i] * 44.0f;
            g_line(cx - 22.0f, top_y, px, top_y, 4.0f, 1.0f);
            g_disc(px, top_y, 3.0f, 1.0f);
        }
        value_text(pg, i, buf);
        fit_text(cx, top_y + 7.0f, buf, 15.0f, 52.0f, i == active_col ? 1.0f : 1.2f); // JSM
        g_color(i == active_col ? KCOL[i] : GC_SLATE);
        fit_text(cx, top_y + 27.0f, p->name, 12.5f, 52.0f, i == active_col ? 1.0f : 1.2f); //JSM
    }
    g_color(GC_WHITE);
}

/* ---- Dedicated Animated Pages ---------------------------------------------------- */
static void draw_env_page(void)
{
    int sub = subpage[PG_ENV], i;
    const uint8_t *ev = sub == 1 ? st.ip[st.inst].menv : st.ip[st.inst].env;
    draw_topbar(O_TRK[st.track], sub == 1 ? "MOD ENV" : "AMP ENV", sub == 1 ? "2/2" : "1/2");

    g_color(GC_SLATE);
    g_box(16.0f, 38.0f, 208.0f, 126.0f, 1.0f, 0.25f);
    g_line(16.0f, 152.0f, 224.0f, 152.0f, 1.0f, 0.3f);

    if (sub == 0) {
        /* Classic 4-Color Animated ADSR Curve + Shaded Vertical Gradient Fill */
        f32 x0 = 26.0f, y0 = 152.0f, ytop = 52.0f, h = y0 - ytop;
        f32 xa = x0 + 6.0f + (f32)ev[0] * 0.46f;
        f32 xd = xa + 12.0f + (f32)ev[1] * 0.46f;
        f32 ys = y0 - ((f32)ev[2] * 0.01f) * h;
        f32 xs = xd + 34.0f;
        f32 xr = minf(216.0f, xs + 8.0f + (f32)ev[3] * 0.44f);
        f32 pts[10] = {x0, y0, xa, ytop, xd, ys, xs, ys, xr, y0};

        g_color(GC_CYAN);
        g_fillv(pts, 5, ytop, 0.30f, y0, 0.03f);
        g_color(GC_BLUE);   g_line(x0, y0, xa, ytop, 2.8f, 0.95f); g_disc(xa, ytop, 4.0f, 1.0f);
        g_color(GC_GREEN);  g_line(xa, ytop, xd, ys, 2.8f, 0.95f); g_disc(xd, ys, 4.0f, 1.0f);
        g_color(GC_WHITE);  g_line(xd, ys, xs, ys, 2.8f, 0.95f);   g_disc(xs, ys, 3.8f, 1.0f);
        g_color(GC_ORANGE); g_line(xs, ys, xr, y0, 2.8f, 0.95f);   g_disc(xr, y0, 4.0f, 1.0f);

        /* Live Gold voice playhead dot along envelope! */
        for (i = 0; i < NVOICE; i++) {
            if (vis.v[i].on) {
                f32 py = y0 - vis.v[i].env * h;
                f32 px = vis.v[i].gate ? lerpf(xa, xs, sat(vis.v[i].age * 1.5f)) : lerpf(xs, xr, 1.0f - vis.v[i].env);
                g_color(GC_GOLD);
                g_disc(clampf(px, x0, xr), clampf(py, ytop, y0), 4.5f, 1.0f);
                break;
            }
        }
    } else {
        /* Mod / Filter Exponential Decay Envelope + Shaded Fill + Punch Spike */
        f32 xy[2 * 50], depth = (f32)ev[0] * 0.01f, dec = 0.08f + (f32)ev[1] * 0.01f * 0.92f;
        f32 vel = (f32)ev[2] * 0.01f, punch = (f32)ev[3] * 0.01f;
        xy[0] = 26.0f;
        xy[1] = 152.0f;
        for (i = 0; i < 48; i++) {
            f32 u = (f32)i * (1.0f / 47.0f);
            f32 y = depth * exp2fx(-u * (5.5f - 4.2f * dec)) + punch * 0.3f * exp2fx(-u * 26.0f);
            xy[2 * (i + 1)]     = 26.0f + u * 188.0f;
            xy[2 * (i + 1) + 1] = 152.0f - sat(y) * 102.0f;
        }
        xy[2 * 49]     = 214.0f;
        xy[2 * 49 + 1] = 152.0f;

        g_color(GC_GREEN);
        g_fillv(xy, 50, 50.0f, 0.30f, 152.0f, 0.03f);
        g_poly(xy + 2, 48, 0, 2.8f, 0.95f);
        g_color(GC_BLUE);
        g_disc(26.0f, 152.0f - depth * 102.0f, 4.2f, 1.0f);
        g_color(GC_ORANGE);
        g_line(26.0f, 152.0f, 26.0f, 152.0f - sat(depth + punch * 0.3f) * 102.0f, 2.4f, 0.9f);
        g_color(GC_WHITE);
        g_line(26.0f, 152.0f - vel * 95.0f, 214.0f, 152.0f - vel * 95.0f, 1.2f, 0.35f);
        for (i = 0; i < NVOICE; i++) {
            if (vis.v[i].on && vis.v[i].menv > 0.01f) {
                f32 u = 1.0f - vis.v[i].menv;
                g_color(GC_GOLD);
                g_disc(26.0f + u * 188.0f, 152.0f - sat(vis.v[i].menv * depth) * 102.0f, 4.2f, 1.0f);
                break;
            }
        }
    }
    draw_bottom_cols(PG_ENV, 182.0f);
}

static void draw_fx_page(void)
{
    int ft = st.fx_type < NFX ? st.fx_type : 0, i;
    const uint8_t *p = st.fx_p[ft];
    f32 k0 = (f32)p[0] * 0.01f, k1 = (f32)p[1] * 0.01f, k2 = (f32)p[2] * 0.01f, k3 = (f32)p[3] * 0.01f;
    f32 lev = minf(1.0f, vis.level * 3.0f);
    f32 cx = 120.0f, cy = 100.0f;
    draw_topbar(O_TRK[st.track], FX_NAMES[ft], st.fx_on ? "FX ON" : "BYPASS");

    /* 8 FX selector pips */
    for (i = 0; i < NFX; i++) {
        g_color(i == ft ? GC_CYAN : GC_SLATE);
        g_disc(64.0f + (f32)i * 16.0f, 36.0f, i == ft ? 3.2f : 1.6f, i == ft ? 1.0f : 0.45f);
    }
    if (!st.fx_on) g_dim(0.35f);

    /* Bespoke animated visualizer per FX type */
    switch (ft) {
    case FX_PLATE:
        for (i = 0; i < 5; i++) {
            f32 r = 14.0f + (f32)i * (8.0f + 7.0f * k1) + 4.0f * sin_turns(tsec * (0.4f + k3) + (f32)i * 0.18f);
            g_color(KCOL[i & 3]);
            g_ring(cx, cy, r, 1.8f + 1.4f * k0, (1.0f - (f32)i * 0.14f) * (0.5f + 0.5f * lev));
        }
        break;
    case FX_DELAY:
        for (i = 0; i < 5; i++) {
            f32 x = 44.0f + (f32)i * 38.0f;
            f32 y = cy + ((i & 1) ? -14.0f : 14.0f) * (0.4f + 0.6f * k0);
            f32 r = (18.0f - (f32)i * (3.0f - 2.0f * k2)) * (0.7f + 0.4f * lev);
            g_color(KCOL[i & 3]);
            g_ring(x, y, maxf(3.0f, r), 2.0f, 0.9f - (f32)i * 0.14f);
        }
        break;
    case FX_FILTER: {
        f32 xy[2 * 50], xl = 30.0f + k0 * 175.0f, xh = 30.0f + k1 * 175.0f;
        xy[0] = 24.0f;
        xy[1] = 146.0f;
        for (i = 0; i < 48; i++) {
            f32 x = 24.0f + (f32)i * (192.0f / 47.0f);
            f32 lp = x < xl ? 1.0f : 1.0f / (1.0f + (x - xl) * (x - xl) * 0.004f);
            f32 hp = x > xh ? 1.0f : 1.0f / (1.0f + (xh - x) * (xh - x) * 0.004f);
            f32 pk = k2 * 28.0f / (1.0f + (x - xl) * (x - xl) * 0.02f);
            xy[2 * (i + 1)] = x;
            xy[2 * (i + 1) + 1] = clampf(144.0f - lp * hp * 48.0f - pk, 48.0f, 146.0f);
        }
        xy[2 * 49] = 216.0f;
        xy[2 * 49 + 1] = 146.0f;
        g_color(GC_CYAN);  g_fillv(xy, 50, 48.0f, 0.30f, 146.0f, 0.03f);
        g_color(GC_WHITE); g_poly(xy + 2, 48, 0, 2.4f, 0.95f);
        g_color(GC_BLUE);  g_disc(xl, 96.0f - k2 * 26.0f, 4.5f, 1.0f);
        g_color(GC_GREEN); g_disc(xh, 118.0f, 4.0f, 1.0f);
        break;
    }
    case FX_SHIFT:
        g_color(GC_CYAN);
        g_ring(cx, cy, 42.0f, 2.0f, 0.85f);
        for (i = 0; i < 8; i++) {
            f32 a = (f32)i * 0.125f + tsec * ((k0 - 0.5f) * 2.2f);
            g_color(KCOL[i & 3]);
            g_line(cx + cos_turns(a) * 14.0f, cy + sin_turns(a) * 14.0f,
                   cx + cos_turns(a) * (32.0f + 16.0f * k2), cy + sin_turns(a) * (32.0f + 16.0f * k2),
                   2.2f, 0.85f);
        }
        g_color(GC_ORANGE);
        g_disc(cx, cy, 6.0f + 10.0f * k3 * (0.5f + 0.5f * lev), 0.95f);
        break;
    case FX_CHORUS:
        for (i = 0; i < 3; i++) {
            f32 xy[2 * 40];
            int j;
            for (j = 0; j < 40; j++) {
                f32 u = (f32)j * (1.0f / 39.0f);
                xy[2 * j] = 26.0f + u * 188.0f;
                xy[2 * j + 1] = cy + (10.0f + 26.0f * k2) * sin_turns(u * 2.0f + tsec * (0.4f + 1.8f * k1) + (f32)i * 0.333f);
            }
            g_color(KCOL[i]);
            g_poly(xy, 40, 0, 2.2f, 0.88f);
        }
        break;
    default: {
        f32 xy[2 * 48];
        for (i = 0; i < 48; i++) {
            f32 u = (f32)i * (1.0f / 47.0f);
            f32 s = vis.scope[(vis.scope_pos + i * 6) & (SCOPE_N - 1)];
            xy[2 * i] = 24.0f + u * 192.0f;
            xy[2 * i + 1] = cy - clampf(s * (55.0f + 35.0f * k0), -44.0f, 44.0f);
        }
        g_color(GC_ORANGE);
        g_poly(xy, 48, 0, 2.4f, 0.95f);
        break;
    }
    }
    g_dim(1.0f);
    draw_bottom_cols(PG_FX, 182.0f);
}

static void draw_lfo_page(void)
{
    int sh = st.ip[st.inst].lfo[0], i;
    f32 xy[2 * 56], depth = (f32)st.ip[st.inst].lfo[2] * 0.01f;
    draw_topbar(O_TRK[st.track], "LFO", O_SHAPE[sh < 7 ? sh : 0]);
    g_color(GC_SLATE);
    g_box(18.0f, 44.0f, 204.0f, 114.0f, 1.0f, 0.28f);
    g_line(18.0f, 101.0f, 222.0f, 101.0f, 1.0f, 0.20f);
    for (i = 0; i < 56; i++) {
        f32 u = (f32)i * (1.0f / 55.0f), y = 0.0f;
        switch (sh) {
        case 0: y = sin_turns(u * 2.0f); break;
        case 1: y = 4.0f * absf(fractf(u * 2.0f) - 0.5f) - 1.0f; break;
        case 2: y = 2.0f * fractf(u * 2.0f) - 1.0f; break;
        case 3: y = fractf(u * 2.0f) < 0.5f ? 1.0f : -1.0f; break;
        case 4: y = sin_turns((f32)(i / 6) * 0.37f); break;
        default: y = 0.65f * sin_turns(u * 1.5f) + 0.35f * sin_turns(u * 3.7f); break;
        }
        xy[2 * i] = 24.0f + u * 192.0f;
        xy[2 * i + 1] = 101.0f - y * (12.0f + 34.0f * depth);
    }
    g_color(GC_CYAN);
    g_poly(xy, 56, 0, 2.5f, 0.95f);
    /* live phase ball */
    g_color(GC_GOLD);
    g_disc(120.0f, 101.0f - vis.lfo * (12.0f + 34.0f * depth), 4.8f, 1.0f);
    draw_bottom_cols(PG_LFO, 182.0f);
}

static void draw_mix_page(void)
{
    static const char *const SUBT[4] = {"MIX VOL", "MIX PAN", "FX SEND", "MASTER"};
    int sub = subpage[PG_MIX], i;
    char buf[16];
    draw_topbar(O_TRK[st.track], SUBT[sub & 3], sub == 3 ? "EQ+DRV" : "4-TRK");

    if (sub == 3) {
        /* Master 3-Band EQ Curve + Shaded Vertical Gradient + Tape Drive Gauge */
        f32 lo = ((f32)st.master[0] - 50.0f) * 0.02f;
        f32 md = ((f32)st.master[1] - 50.0f) * 0.02f;
        f32 hi = ((f32)st.master[2] - 50.0f) * 0.02f;
        f32 drv = (f32)st.master[3] * 0.01f;
        f32 xy[2 * 50];
        g_color(GC_SLATE);
        g_box(18.0f, 42.0f, 204.0f, 118.0f, 1.0f, 0.26f);
        g_line(18.0f, 101.0f, 222.0f, 101.0f, 1.0f, 0.25f);
        xy[0] = 24.0f;
        xy[1] = 154.0f;
        for (i = 0; i < 48; i++) {
            f32 u = (f32)i * (1.0f / 47.0f);
            f32 wl = maxf(0.0f, 1.0f - u * 2.2f);
            f32 wm = maxf(0.0f, 1.0f - absf(u - 0.5f) * 3.0f);
            f32 wh = maxf(0.0f, (u - 0.55f) * 2.2f);
            f32 y = 101.0f - (lo * wl + md * wm + hi * wh) * 38.0f;
            xy[2 * (i + 1)] = 24.0f + u * 192.0f;
            xy[2 * (i + 1) + 1] = clampf(y, 48.0f, 154.0f);
        }
        xy[2 * 49] = 216.0f;
        xy[2 * 49 + 1] = 154.0f;
        g_color(GC_CYAN);   g_fillv(xy, 50, 56.0f, 0.26f, 154.0f, 0.03f);
        g_color(GC_WHITE);  g_poly(xy + 2, 48, 0, 2.6f, 0.95f);
        g_color(GC_BLUE);   g_disc(44.0f,  101.0f - lo * 38.0f, 4.2f, 1.0f);
        g_color(GC_GREEN);  g_disc(120.0f, 101.0f - md * 38.0f, 4.2f, 1.0f);
        g_color(GC_WHITE);  g_disc(196.0f, 101.0f - hi * 38.0f, 4.2f, 1.0f);
        /* Drive bar along top */
        g_color(GC_ORANGE);
        g_line(26.0f, 52.0f, 26.0f + 188.0f * drv, 52.0f, 4.5f, 0.95f);
        draw_bottom_cols(PG_MIX, 182.0f);
        return;
    }

    /* 4-Track Vertical Mixer Faders + Live Peak VU Meters */
    for (i = 0; i < NTRACK; i++) {
        f32 cx = 34.0f + (f32)i * 57.0f;
        f32 top = 46.0f, bot = 162.0f, h = bot - top;
        int val = *page_value(PG_MIX, i);
        int muted = (st.trk[i].mute & 1u) != 0;
        f32 y = bot - ((f32)val * 0.01f) * h;

        /* Track slot frame */
        g_color(i == st.track ? KCOL[i] : GC_SLATE);
        g_box(cx - 21.0f, top - 6.0f, 42.0f, h + 14.0f, i == st.track ? 1.6f : 1.0f, i == st.track ? 0.55f : 0.22f);

        /* Live VU meter bar beside fader */
        if (!muted && vis.trk_lvl[i] > 0.01f) {
            f32 vh = sat(vis.trk_lvl[i]) * h;
            g_color(vis.trk_lvl[i] > 0.85f ? GC_RED : KCOL[i]);
            g_rect(cx + 8.0f, bot - vh, 6.0f, vh, 0.82f);
        }

        /* Fader track & thumb */
        g_color(GC_SLATE);
        g_line(cx - 5.0f, top, cx - 5.0f, bot, 2.0f, 0.38f);
        g_color(muted ? GC_RED : KCOL[i]);
        if (sub == 1) {
            f32 mid = (top + bot) * 0.5f;
            g_line(cx - 5.0f, mid, cx - 5.0f, y, 7.0f, 0.95f);
        } else {
            g_line(cx - 5.0f, bot, cx - 5.0f, y, 7.0f, muted ? 0.35f : 0.95f);
        }
        g_line(cx - 13.0f, y, cx + 3.0f, y, 3.0f, 1.0f);

        /* Label & value */
        if (muted) put_s(buf, "MUTE");
        else value_text(PG_MIX, i, buf);
        g_color(muted ? GC_RED : KCOL[i]);
        fit_text(cx, 178.0f, buf, 12.0f, 50.0f, 1.0f);
        g_color(GC_WHITE);
        fit_text(cx, 198.0f, insts[st.trk[i].inst < NINST ? st.trk[i].inst : 0]->name, 8.0f, 50.0f, 0.78f);
        g_color(GC_SLATE);
        fit_text(cx, 214.0f, O_TRK[i], 9.0f, 40.0f, i == st.track ? 1.0f : 0.5f);
    }
    g_color(GC_WHITE);
}

static void draw_tape_page(void)
{
    int sub = subpage[PG_SEQ], t, c, i;
    f32 spd_mul = st.tape[0] == TAPE_SPD_HALF ? 0.5f : (st.tape[0] == TAPE_SPD_2X ? 2.0f : (st.tape[0] == TAPE_SPD_REV ? -1.0f : 1.0f));
    int moving = (seq.loop_state == LOOP_PLAY || seq.loop_state == LOOP_REC || seq.loop_state == LOOP_DUB);
    int rec = (seq.loop_state == LOOP_REC || seq.loop_state == LOOP_DUB);

    if (st.seq[0] != SEQ_STEP && sub == 2) { // JSM
        static const char *const NNAME[12] = {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };
        int total = 0, start = 0, row, bt = seq_bar_ticks();
        draw_topbar(O_TRK[st.track & 3], "TAPE NOTES", "3/3");
        eng_tape_note_info(st.track & 3, 0, 0, 0, 0, &total);
        if (total <= 0) {
            notes_sel = 0;
            g_color(GC_SLATE);
            fit_text(120.0f, 96.0f, "NO NOTES", 11.0f, 180.0f, 0.8f);
        } else {
            if (notes_sel >= total) notes_sel = (uint8_t)(total - 1);
            if (total > 4) {
                start = (int)notes_sel - 1;
                if (start < 0) start = 0;
                if (start > total - 4) start = total - 4;
            }
            for (row = 0; row < 4; row++) {
                int idx = start + row;
                f32 y = 36.0f + (f32)row * 33.0f;
                if (idx >= total) break;
                {
                    uint16_t tk = 0;
                    uint8_t mn = 0, vl = 0;
                    char tbuf[16], nbuf[8], vbuf[8];
                    int meas, in_bar, beat, div, oct, sel = (idx == (int)notes_sel);
                    char *d;
                    eng_tape_note_info(st.track & 3, idx, &tk, &mn, &vl, 0);
                    meas = (bt > 0 ? (int)tk / bt : 0) + 1;
                    in_bar = bt > 0 ? (int)tk % bt : (int)tk;
                    beat = in_bar / 96;
                    div = in_bar % 96;
                    d = put_i(tbuf, meas);
                    *d++ = ':';
                    d = put_i(d, beat);
                    *d++ = ':';
                    put_i(d, div);

                    oct = ((int)mn / 12) - 1;
                    d = put_s(nbuf, NNAME[mn % 12]);
                    put_i(d, oct);
                    put_i(vbuf, vl);

                    if (sel) {
                        g_color(KCOL[st.track & 3]);
                        g_box(10.0f, y - 3.0f, 220.0f, 30.0f, 1.2f, 0.85f);
                    }
                    g_color(sel ? GC_WHITE : GC_SLATE);
                    g_text(16.0f, y + 5.0f, tbuf, 14.0f, 2.0f, sel ? 1.0f : 0.75f, 0);
                    g_color(sel ? KCOL[st.track & 3] : GC_WHITE);
                    g_text(122.0f, y + 5.0f, nbuf, 14.0f, 2.0f, sel ? 1.0f : 0.75f, 0);
                    g_color(GC_GOLD);
                    g_text(222.0f, y + 5.0f, vbuf, 14.0f, 2.0f, sel ? 1.0f : 0.75f, 2);
                }
            }
        }
        draw_bottom_cols(PG_SEQ, 182.0f);
        g_color(GC_WHITE);
        return;
    }

    if (moving) reel_ph = fractf(reel_ph + vis.dt * 0.42f * spd_mul + 2.0f);

    draw_topbar(O_TRK[st.track], st.seq[0] == SEQ_STEP ? "PATTERN" : (sub == 1 ? "TAPE EDIT" : "4-TRACK TAPE"),
                seq.clip_count ? "CLIP RDY" : O_SPEED[st.tape[0] & 3]);

    /* Iconic Spinning Twin Tape Reels! */
    for (i = 0; i < 2; i++) {
        f32 rx = i == 0 ? 64.0f : 176.0f, ry = 58.0f, rr = 22.0f;
        int s;
        g_color(rec ? GC_RED : GC_WHITE);
        g_ring(rx, ry, rr, 2.0f, 0.92f);
        g_color(KCOL[st.track & 3]);
        g_ring(rx, ry, 7.0f, 1.8f, 0.9f);
        for (s = 0; s < 3; s++) {
            f32 a = reel_ph + (f32)s * (1.0f / 3.0f);
            g_line(rx + cos_turns(a) * 8.0f, ry + sin_turns(a) * 8.0f,
                   rx + cos_turns(a) * (rr - 3.0f), ry + sin_turns(a) * (rr - 3.0f), 2.0f, 0.85f);
        }
    }
    /* Tape ribbon connecting reels */
    g_color(GC_SLATE);
    g_line(64.0f, 36.0f, 176.0f, 36.0f, 1.2f, 0.45f);
    g_line(64.0f, 80.0f, 176.0f, 80.0f, 1.2f, 0.45f);
    /* Speed / bar readout between reels */
    g_color(GC_GOLD);
    {
        char b[12];
        put_s(put_i(b, seq.bars), " BAR");
        fit_text(120.0f, 42.0f, b, 10.0f, 58.0f, 0.9f);
    }
    /* JSM background color In/Out box  */
    {
        f32 xi = 28.0f + seq.loop_in * 200.0f;
        f32 xo = 28.0f + seq.loop_out * 200.0f;
        f32 w = xo - xi;
        if (w > 0.5f) {
            g_color(GC_SLATE);
            g_rect(xi, 88.0f, w, 76.0f, 0.18f);
        }
        g_color(GC_CYAN);
        g_line(xi, 88.0f, xi, 164.0f, 1.4f, 0.7f);
        g_line(xo, 88.0f, xo, 164.0f, 1.4f, 0.7f);
    }
    { // JSM show space available
        int rem = eng_tape_ev_free();
        char rbuf[12];
        put_s(put_i(rbuf, rem), "");
        g_color(rem <= 8 ? GC_RED : (rem <= 32 ? GC_GOLD : GC_WHITE));
        fit_text(120.0f, 62.0f, rbuf, 10.0f, 58.0f, 0.9f);
        g_color(GC_WHITE);
    }
    /* 4 Horizontal Color-Coded Tape Track Lanes (T1 Blue, T2 Green, T3 White, T4 Orange) */
    for (t = 0; t < NTRACK; t++) {
        f32 y0 = 92.0f + (f32)t * 18.0f;
        int muted = (st.trk[t].mute & 1u) != 0;
        g_color(t == st.track ? KCOL[t] : GC_SLATE);
        g_text(10.0f, y0 + 2.0f, O_TRK[t], 8.0f, 1.2f, t == st.track ? 1.0f : 0.45f, 0);
        g_color(GC_SLATE);
        g_rect(28.0f, y0 + 6.0f, 200.0f, 2.0f, 0.25f);
        g_color(muted ? GC_SLATE : KCOL[t]);
        for (c = 0; c < 32; c++) {
            if (seq.trk_dens[t][c]) {
                f32 bh = clampf(4.0f + (f32)seq.trk_dens[t][c] * 1.4f, 4.0f, 12.0f);
                g_rect(28.0f + (f32)c * 6.25f, y0 + 7.0f - bh * 0.5f, 5.0f, bh, muted ? 0.3f : 0.9f);
            }
        }
    }
    /* Loop In/Out brackets + Vertical Gold Playhead */
    {
        f32 xi = 28.0f + seq.loop_in * 200.0f;
        f32 xo = 28.0f + seq.loop_out * 200.0f;
        f32 xp = 28.0f + seq.loop_pos * 200.0f;
        g_color(GC_CYAN);
        g_line(xi, 88.0f, xi, 164.0f, 1.4f, 0.7f);
        g_line(xo, 88.0f, xo, 164.0f, 1.4f, 0.7f);
        g_color(rec ? GC_RED : GC_GOLD);
        g_line(xp, 86.0f, xp, 166.0f, 2.2f, 1.0f);
    }

    draw_bottom_cols(PG_SEQ, 182.0f);
}

static void draw_arp_page(void)
{
    int mode = st.ip[st.inst].arp[0], i;
    draw_topbar(O_TRK[st.track], "SEQ / ARP",
                mode == ARP_SHAKE ? (arp_sub() ? "SHAKE 2/2" : "SHAKE 1/2") : O_ARPMODE[mode < NARPMODE ? mode : 0]);

    if (mode == ARP_SHAKE) {
        /* ICONIC SHAKE ROTATING HEXAGONAL GRAVITY CAGE! */
        f32 cx = 120.0f, cy = 102.0f;
        f32 cr = (0.58f + ((f32)st.shake[3] * 0.01f) * 0.32f) * 66.0f;
        f32 hex[12];
        for (i = 0; i < 6; i++) {
            f32 a = vis.tomb_angle + ((f32)i + 0.5f) * (1.0f / 6.0f);
            hex[2 * i]     = cx + cos_turns(a) * cr * 1.155f;
            hex[2 * i + 1] = cy + sin_turns(a) * cr * 1.155f;
        }
        g_color(GC_WHITE);
        g_poly(hex, 6, 1, 2.4f, 0.92f);
        for (i = 0; i < NSHAKE; i++) {
            if (!vis.balls[i].on) continue;
            f32 bx = cx + vis.balls[i].x * 66.0f;
            f32 by = cy + vis.balls[i].y * 66.0f;
            g_color(KCOL[i & 3]);
            g_disc(bx, by, 4.5f + 2.5f * vis.balls[i].hit, 0.95f);
            if (vis.balls[i].hit > 0.1f)
                g_ring(bx, by, 7.0f + (1.0f - vis.balls[i].hit) * 14.0f, 1.6f, vis.balls[i].hit);
        }
        if (!vis.balls[0].on && !vis.balls[1].on) {
            g_color(GC_SLATE);
            /* (two lines, inside the cage at its smallest: on one it ran across the walls) */
            fit_text(cx, cy - 11.0f, "PLAY KEYS", 9.0f, 2.0f * (cr - 8.0f), 0.7f);
            fit_text(cx, cy + 2.0f, "TO DROP", 9.0f, 2.0f * (cr - 8.0f), 0.7f);
        }
    } else if (mode == ARP_RIFF) {
        /* ICONIC RIFF STEP RIBBON! */
        int len = st.riff_len;
        g_color(GC_SLATE);
        g_box(16.0f, 44.0f, 208.0f, 114.0f, 1.0f, 0.28f);
        g_color(seq.riff_rec ? GC_RED : GC_GREEN);
        fit_text(120.0f, 52.0f, seq.riff_rec ? "REC: TAP NOTES (EDIT=REST)" : "HOLD KEY TO TRANSPOSE", 8.5f, 196.0f, 0.9f);
        if (len > 0) {
            int show = imin(len, 16);
            f32 step_w = 192.0f / (f32)show;
            for (i = 0; i < show; i++) {
                int sv = st.riff[i];
                f32 x = 24.0f + (f32)i * step_w + step_w * 0.5f;
                int active = (!seq.riff_rec && (seq.riff_at % len) == i);
                if (sv > 0) {
                    f32 ny = clampf(142.0f - ((f32)(sv - 48) * 1.8f), 74.0f, 146.0f);
                    g_color(active ? GC_GOLD : KCOL[i & 3]);
                    g_disc(x, ny, active ? 5.0f : 3.5f, 1.0f);
                } else {
                    g_color(GC_SLATE);
                    g_line(x - 3.0f, 110.0f, x + 3.0f, 110.0f, 1.6f, 0.5f);
                }
            }
        } else {
            g_color(GC_SLATE);
            fit_text(120.0f, 100.0f, "PRESS REC TO ENTER STEPS", 9.0f, 190.0f, 0.75f);
        }
    } else {
        /* Classic Arpeggiator visualizer */
        int j;
        g_color(GC_SLATE);
        g_box(18.0f, 44.0f, 204.0f, 114.0f, 1.0f, 0.26f);
        for (j = 0; j < 8; j++) {
            f32 x = 32.0f + (f32)j * 25.0f;
            f32 h = 24.0f + (f32)((mode == ARP_DOWN ? 7 - j : j) % 4) * 18.0f;
            g_color(KCOL[j & 3]);
            g_rect(x - 6.0f, 148.0f - h, 12.0f, h, mode == ARP_OFF ? 0.25f : 0.85f);
        }
    }
    draw_bottom_cols(PG_ARP, 182.0f);
}

static void draw_glo_page(void)
{
    int sub = subpage[PG_GLO], i;
    char buf[24];
    draw_topbar(O_TRK[st.track], sub == 2 ? "CHORD" : (sub == 1 ? "KEYS / SCALE" : "GLOBAL"),
                sub == 2 ? "3/3" : (sub == 1 ? "2/3" : "1/3"));
    if (sub == 0) {
        put_s(put_s(buf, "HOLD GLO: "), O_CLICK[st.seq[5] < 3 ? st.seq[5] : 0]);
        g_color(GC_SLATE);
        g_text(18.0f, 38.0f, buf, 8.5f, 1.2f, 0.8f, 0);
    }
    for (i = 0; i < 4; i++) {
        const ParamDef *p = page_param(PG_GLO, i);
        f32 cx = 30.0f + (f32)i * 60.0f, top = 62.0f, bot = 148.0f, h = bot - top;
        int v = *page_value(PG_GLO, i);
        f32 tgt = p->max > p->min ? (f32)(v - p->min) / (f32)(p->max - p->min) : 0.0f;
        f32 y;
        spring(&col_v[i], &col_vv[i], tgt, 26.0f, vis.dt);
        y = bot - col_v[i] * h;
        g_color(GC_SLATE);
        g_line(cx, top, cx, bot, 2.0f, 0.35f);
        g_color(KCOL[i]);
        if (p->show_off < 0) {
            f32 mid = (top + bot) * 0.5f;
            g_line(cx, mid, cx, y, 8.0f, 0.95f);
        } else {
            g_line(cx, bot, cx, y, 8.0f, 0.95f);
        }
        g_line(cx - 12.0f, y, cx + 12.0f, y, 3.0f, 1.0f);
        value_text(PG_GLO, i, buf);
        fit_text(cx, 166.0f, buf, 14.0f, 52.0f, 1.0f);
        g_color(GC_SLATE);
        fit_text(cx, 194.0f, p->name, 9.0f, 50.0f, 0.85f);
    }
    g_color(GC_WHITE);
}

/* ---- overlays: toast, name flash, volume ring, bottom sequencer bar ------------------- */
static void draw_toast(void) // JSM larger box
{
    f32 a = 1.0f - smooth01((toast_t - 0.95f) / 0.35f), y = 108.0f, w;
    f32 box_h = toast_pips >= 0 ? 50.0f : 34.0f;
    int i;
    if (a <= 0.01f || !toast_s[0]) return;
    w = g_text_w(toast_s, 20.0f);
    g_mode(G_PAINT);
    g_rect(120.0f - w * 0.5f - 14.0f, y - 8.0f, w + 28.0f, box_h + 2.0f, 0.0f);
    g_mode(G_MAX);
    g_dim(a);
    g_color(GC_GOLD);
    g_box(120.0f - w * 0.5f - 13.0f, y - 7.0f, w + 26.0f, box_h, 1.4f, 0.55f);
    g_color(GC_WHITE);
    g_text(120.0f, y, toast_s, 20.0f, 2.5f, 1.0f, 1);
    if (toast_pips >= 0) {
        for (i = 0; i < NALGO; i++) {
            f32 x = 120.0f + ((f32)i - 3.5f) * 14.0f;
            g_color(i == toast_pips ? GC_ORANGE : GC_SLATE);
            g_disc(x, y + 31.0f, i == toast_pips ? 4.5f : 2.2f, i == toast_pips ? 1.0f : 0.5f);
        }
    }
    g_dim(1.0f);
    g_color(GC_WHITE);
}

static void draw_name(void)
{
    const char *name = insts[st.inst]->name;
    f32 t = name_t, size, w, a, y;
    if (t > 0.72f) return;
    size = 58.0f;
    w = g_text_w(name, size);
    if (w > 200.0f) size *= 200.0f / w;
    a = t < 0.52f ? 1.0f : 1.0f - smooth01((t - 0.52f) / 0.2f);   /* (out in 0.2 s: browsing, the picture is the point) */
    y = 118.0f - size * 0.5f;
    g_mode(G_PAINT);
    g_rect(12.0f, y - 12.0f, 216.0f, size + 24.0f, 0.0f);
    g_mode(G_MAX);
    g_dim(a);
    g_color(KCOL[st.track & 3]);
    g_text(120.0f, y, name, size, maxf(2.8f, size * 0.13f), 1.0f, 1);
    g_dim(1.0f);
    g_color(GC_WHITE);
}

static void shade_disc(f32 cx, f32 cy, f32 r, f32 k)
{
    int k256 = (int)(sat(k) * 256.0f), y, x, y0 = imax(0, ifloor(cy - r)), y1 = imin(FB_H - 1, ifloor(cy + r) + 1);
    if (k256 >= 256) return;
    for (y = y0; y <= y1; y++) {
        f32 dy = (f32)y - cy, hw2 = r * r - dy * dy, hw;
        int xa, xb;
        uint8_t *row = &g_fb[y * FB_W];
        if (hw2 <= 0.0f) continue;
        hw = sqrtfx(hw2);
        xa = imax(0, ifloor(cx - hw + 0.5f));
        xb = imin(FB_W - 1, ifloor(cx + hw + 0.5f));
        for (x = xa; x <= xb; x++) {
            uint8_t px = row[x];
            if (px) {
                int lv = ((px & 31) * k256) >> 8;
                row[x] = lv > 0 ? (uint8_t)((px & 0xE0) | lv) : 0;
            }
        }
        if ((y & 15) == 0) G_POLL();
    }
}

static void draw_volume(f32 dt)
{
    const char *label = ring_kind == RING_INST ? "TRACK VOL" : "MASTER";
    const f32 cx = 120.0f, cy = 120.0f, t0 = 0.375f, sweep = 0.75f;
    f32 a, r, x, te;
    char num[8];
    if ((btn_held & (1u << H1_BTN_EDIT)) && btn_fired[H1_BTN_EDIT] == 1 && name_t >= 0.95f) {
        if (ring_t >= RING_STAYS + 0.4f) ring_show(RING_INST);
        else if (ring_kind == RING_INST) ring_t = 0.0f;
    }
    if (!ring_kind || ring_t >= RING_STAYS + 0.4f) return;
    spring(&ring_x, &ring_v, ring_value(ring_kind), 40.0f, dt);
    a = 1.0f - smooth01((ring_t - RING_STAYS) / 0.4f);
    if (a <= 0.01f) return;
    x = sat(ring_x);
    te = t0 + sweep * x;
    r = 56.0f - 4.0f * (1.0f - ease_out(ring_up / 0.14f));
    put_i(num, (int)(ring_value(ring_kind) * 100.0f + 0.5f));

    shade_disc(cx, cy, r + 16.0f, 1.0f - 0.88f * a);
    g_mode(G_MAX);
    g_dim(a);
    g_color(GC_SLATE);
    g_arc(cx, cy, r, t0, t0 + sweep, 2.4f, 0.4f);
    g_color(ring_kind == RING_INST ? KCOL[st.track & 3] : GC_ORANGE);
    if (x > 0.002f) g_arc(cx, cy, r, t0, te, 7.5f, 1.0f);
    g_disc(cx + r * cos_turns(te), cy + r * sin_turns(te), 6.0f, 1.0f);
    g_color(GC_WHITE);
    g_text(cx, cy - 18.0f, num, 28.0f, 3.6f, 1.0f, 1);
    g_color(GC_SLATE);
    g_text(cx, cy + 18.0f, label, 9.0f, 1.3f, 0.9f, 1);
    g_dim(1.0f);
    g_color(GC_WHITE);
}

static void draw_seq_mark(void)
{
    int s = seq.loop_state;
    if (seq.mode == SEQ_TAPE) {
        if (s == LOOP_EMPTY || s == LOOP_IDLE) return;
        g_color(s == LOOP_PLAY ? GC_GOLD : GC_RED);
        g_line(0.0f, 238.5f, 240.0f * seq.loop_pos, 238.5f, 2.2f, 0.95f);
        return;
    }
    /* Step notches along bottom */
    {
        int i, n = seq.nsteps;
        f32 pitch = n > 0 ? 232.0f / (f32)n : 10.0f;
        for (i = 0; i < n; i++) {
            int full = st.steps[i][0] != 0;
            int at = seq.playing && i == seq.at;
            f32 x = 4.0f + (f32)i * pitch;
            g_color(at ? GC_GOLD : (full ? KCOL[st.step_trk[i][0] & 3] : GC_SLATE));
            g_rect(x, at ? 233.0f : (full ? 235.0f : 237.0f), maxf(1.5f, pitch - 1.5f), at ? 6.0f : (full ? 4.0f : 2.0f), full || at ? 0.95f : 0.35f);
        }
    }
    g_color(GC_WHITE);
}

/* ---- main UI frame -------------------------------------------------------------------- */
void ui_frame(uint32_t now_ms)
{
    f32 dt;
    int i;
    now = now_ms;
    dt = clampf((f32)(now - last_ms) * 0.001f, 0.001f, 0.1f);   /* (to 100 ms: a busy jam draws at 15 to 20 frames
                                                                  * a second, and at 50 the pictures ran slow) */
    if (!frame_n) dt = 0.016f;
    last_ms = now;
    frame_n++;
    tsec += dt;
    holds();

    eng_snapshot(&vis);
    eng_seq_info(&seq);
    if (seq.write != seq_write_was) {
        seq_write_was = seq.write;
        toast(seq.write ? "STEP WRITE" : "WRITE OFF", 0, 0, 0, -1);
    }
    if (seq.riff_rec != riff_rec_was) {
        riff_rec_was = seq.riff_rec;
        toast(seq.riff_rec ? "RIFF REC" : "RIFF READY", 0, 0, 0, -1);
    }
    if (eng_loop_full() != loop_full_was) {
        loop_full_was = eng_loop_full();
        toast("TAPE FULL", 0, 0, 0, -1);
    }

    for (i = 0; i < 4; i++) {
        if (vis.fresh) { vk[i] = vis.k[i]; vkv[i] = 0.0f; }
        spring(&vk[i], &vkv[i], vis.k[i], 32.0f, dt);
        vis.k[i] = sat(vk[i]);
        knob_touch[i] += dt;
    }
    vis.t = tsec; vis.dt = dt; vis.rng = &vrng;
    vis.algo = st.ip[st.inst].algo;
    vis.algo_t = sat(vis.algo_t + dt / 0.35f);
    name_t += dt; page_t += dt; toast_t += dt; vol_t += dt; ring_t += dt; ring_up += dt;
    if (clear_all_asked) toast_t = 0.0f;         /* CLEAR ALL? stays up until it is answered */
    {   /* the engine wrote what the state saves (a step, a riff, a snapshot): saved as a setting is */
        static uint32_t saved_seen;
        uint32_t sn = eng_saved_n();
        if (sn != saved_seen) { saved_seen = sn; mark_dirty(); }
    }

    build_palette();
    g_clear();
    g_mode(G_MAX);
    g_color(GC_WHITE);
    g_dim(1.0f);
    g_origin(0.0f, 0.0f);

    if (page == PG_HOME) {
        char right[20];
        put_s(put_i(right, st.glo[0]), " BPM");
        {   /* the picture is the track in hand's: its level and its scope, not the whole jam's (the top bar's meter
             * keeps the mix) */
            f32 mix_level = vis.level;
            const f32 *mix_scope = vis.scope;
            vis.level = vis.own_level;
            if (vis.own_scope) vis.scope = vis.own_scope;
            g_dim(0.35f + 0.65f * ease_out(vis.enter / 0.14f));   /* (a new engine comes in as a page does) */
            insts[st.inst]->draw(&vis);
            g_dim(1.0f);
            vis.level = mix_level;
            vis.scope = mix_scope;
        }
        g_dim(1.0f); g_mode(G_MAX); g_origin(0.0f, 0.0f);
        {
            char hdr[28];
            put_s(put_s(put_s(hdr, insts[st.inst]->name), " "), insts[st.inst]->algo[st.ip[st.inst].algo & 7]);
            draw_topbar(O_TRK[st.track], hdr, right);
        }
        draw_hud();
        draw_seq_mark();
        draw_toast();
        draw_name();
    } else if (page == PG_ENV) {
        g_dim(0.35f + 0.65f * ease_out(page_t / 0.14f));
        draw_env_page();
        g_dim(1.0f);
        draw_seq_mark();
        draw_toast();
    } else if (page == PG_FX) {
        draw_fx_page();
        draw_seq_mark();
        draw_toast();
    } else if (page == PG_LFO) {
        draw_lfo_page();
        draw_seq_mark();
        draw_toast();
    } else if (page == PG_MIX) {
        draw_mix_page();
        draw_seq_mark();
        draw_toast();
    } else if (page == PG_SEQ) {
        draw_tape_page();
        draw_seq_mark();
        draw_toast();
    } else if (page == PG_ARP) {
        draw_arp_page();
        draw_seq_mark();
        draw_toast();
    } else {
        draw_glo_page();
        draw_seq_mark();
        draw_toast();
    }
    draw_volume(dt);
    vis.fresh = 0;
    vis.enter += dt;
}

SMALL void ui_init(void)
{
    int i;
    for (i = 0; i < NPAGE; i++) subpage[i] = 0;
    page = PG_HOME;
    vis.fresh = 1;
    vis.algo = vis.algo_prev = st.ip[st.inst].algo;
    vis.algo_t = 1.0f;
    name_t = 0.0f;
    force_full = 1;
    rq_n = rq_i = 0;
    pal_bright_seen = 0;
    dirty = 0;
    btn_held = 0;
    clear_all_asked = clear_all_answer = 0;
    ring_kind = 0;
    glob_held = master_raw = -1;
    ring_t = 9.0f;
    dice_inst = -1;
    notes_trk = notes_sel = notes_del = 0;
    h1_set_cur_snap(0); // JSM
}

/* ---- dirty-tile presentation (60 MHz SPI DMA) ----------------------------------------- */
#define TILES (FB_W / 8)
#define PRESENT_RECT 200
#define PRESENT_GAP 256

static uint32_t row_tiles(int y)
{
    const uint32_t *wa = &g_fb_w[y * (FB_W / 4)], *wb = &prev_w[y * (FB_W / 4)];
    uint32_t m = 0;
    int t, i;
    if (force_full) return (1u << TILES) - 1u;
    for (t = 0; t < TILES; t++, wa += 2, wb += 2) {
        const uint8_t *a, *b;
        if (wa[0] == wb[0] && wa[1] == wb[1]) continue;
        a = (const uint8_t *)wa;
        b = (const uint8_t *)wb;
        for (i = 0; i < 8; i++)
            if (a[i] != b[i] && pal[a[i]] != pal[b[i]]) {
                m |= 1u << t;
                break;
            }
    }
    return m;
}

void h1_present_begin(void)
{
    int y = 0;
    rq_n = rq_i = 0;
    rq_bytes = 0;
    while (y < FB_H) {
        uint32_t rm[20], all = row_tiles(y);
        int n = 0, y0 = y, t = 0;
        if (!all) { y++; continue; }
        rm[n++] = all;
        for (y++; y < FB_H && n < 20; y++) {
            uint32_t m = row_tiles(y);
            if (!m) break;
            rm[n++] = m;
            all |= m;
        }
        while (t < TILES) {
            uint32_t run;
            int t0, t1, r0 = 0, r1 = n - 1, yy, x0, x1;
            if (!(all >> t & 1u)) { t++; continue; }
            t0 = t1 = t;
            for (;;) {
                int u = t1 + 1;
                while (u < TILES && !(all >> u & 1u)) u++;
                if (u >= TILES) break;
                if ((u - t1 - 1) * 8 * n * 2 > PRESENT_GAP && rq_n < RQ_MAX - FB_H) break;
                t1 = u;
            }
            run = ((t1 - t0 + 1 >= 32 ? 0u : 1u << (t1 - t0 + 1)) - 1u) << t0;
            while (!(rm[r0] & run)) r0++;
            while (!(rm[r1] & run)) r1--;
            x0 = t0 * 8; x1 = t1 * 8 + 7;
            for (yy = y0 + r0; yy <= y0 + r1; yy++) {
                const uint32_t *src = &g_fb_w[(yy * FB_W + x0) / 4];
                uint32_t *dst = &prev_w[(yy * FB_W + x0) / 4];
                int k, nw = (x1 - x0 + 1) / 4;
                for (k = 0; k < nw; k++) dst[k] = src[k];
            }
            rq[rq_n][0] = (uint8_t)x0;
            rq[rq_n][1] = (uint8_t)(y0 + r0);
            rq[rq_n][2] = (uint8_t)(x1 - x0 + 1);
            rq[rq_n][3] = (uint8_t)(r1 - r0 + 1);
            rq_n++;
            rq_bytes += (uint32_t)((x1 - x0 + 1) * (r1 - r0 + 1) * 2 + PRESENT_RECT);
            t = t1 + 1;
        }
    }
    force_full = 0;
}

int h1_present_next(int *x, int *y, int *w, int *h)
{
    if (rq_i >= rq_n) return 0;
    *x = rq[rq_i][0]; *y = rq[rq_i][1]; *w = rq[rq_i][2]; *h = rq[rq_i][3];
    rq_i++;
    return 1;
}
int h1_present_left(void)      { return rq_n - rq_i; }
uint32_t h1_present_bytes(void){ return rq_bytes; }
int h1_present_rects(void)     { return rq_n; }
const uint8_t *h1_panel(void)  { return prev_fb; }
void h1_while_drawing(void (*fn)(void)) { g_poll_fn = fn; }

uint32_t h1_present(h1_blit_fn blit)
{
    int x, y, w, h;
    h1_present_begin();
    while (h1_present_next(&x, &y, &w, &h))
        if (blit) blit(x, y, w, h);
    return rq_bytes;
}

const uint8_t *h1_framebuffer(void) { return g_fb; }
const uint16_t *h1_palette(void)    { return pal; }

/* ---- LEDs & state dirty --------------------------------------------------------------- */
uint32_t h1_key_leds(void)
{
    /* JSM not distracting leds when playing
    uint32_t m;
    if (eng_seq_leds(&m, page == PG_SEQ, fractf(tsec * 2.2f) < 0.6f)) return m; 
    return eng_sounding_lo();*/
    return 0;
}

uint32_t h1_button_leds(void)
{
    static const int8_t PAGE_BTN[NPAGE] = {
        H1_BTN_HOME, H1_BTN_FX, H1_BTN_SEL, H1_BTN_ENV, H1_BTN_LFO, H1_BTN_GLO, H1_BTN_ARP, H1_BTN_SEQ
    };
    uint32_t m = 0;
    int s = seq.mode == SEQ_TAPE ? seq.loop_state : LOOP_EMPTY, blink = fractf(tsec * 2.5f) < 0.5f;
    if (page != PG_HOME) m |= 1u << PAGE_BTN[page];
    if (st.ip[st.inst].arp[0] && (page == PG_ARP || vis.beat < 0.25f)) m |= 1u << H1_BTN_ARP;
    if (st.hud) m |= 1u << H1_BTN_EDIT;
    if (st.fx_on) m |= 1u << H1_BTN_FX;
    if (s == LOOP_PLAY || s == LOOP_DUB || s == LOOP_REC) m |= 1u << H1_BTN_PLAY;
    if (s == LOOP_REC || s == LOOP_DUB || (s == LOOP_ARMED && blink) || seq.riff_rec) m |= 1u << H1_BTN_REC;
    if (seq.mode == SEQ_STEP) {
        if (seq.playing) m |= 1u << H1_BTN_PLAY;
        if (seq.write)   m |= 1u << H1_BTN_REC;
    }
    if (st.octave < 0) m |= 1u << H1_BTN_OCTDN;
    if (st.octave > 0) m |= 1u << H1_BTN_OCTUP;
    return m;
}

int h1_state_dirty(void) { return dirty && now - dirty_ms > 1500u; }
void h1_state_saved(void) { dirty = 0; }