# Aplus, a fork of A22
- Third page on th SEQ pannel, lists the notes on the track and you can delete them one by one to fix mistakes
- Proof of concept: adding the COZY instrument
- One instrument per track in tape. algo and instrument are still recorded on the track but ignored at playback. Makes things easier.

## Original doc from now on
Custom firmware for the M-VAVE FM-1: an FM workstation. Fourteen synth and drum engines, each with eight
algorithms and four live knobs, drawn in colour vector graphics. Eight selectable effects, a
four-track tape with knob automation, a mixer, Riff and Shake sequencers, an arpeggiator, a step
sequencer and eight patch snapshots. Almost everything is synthesised: KIT's 909, CRISP, LOFI and WORLD kits
layer in short CC0 recordings of real drums (h1's PLUNK hits; see [docs/SAMPLES.md](docs/SAMPLES.md)), SKINS'
two kits are made of them, and CYCLE plays single cycles of recorded instruments from Adventure Kid Waveforms (CC0; see
[docs/WAVETABLES.md](docs/WAVETABLES.md)).

A22 is derived from [h1](https://github.com/jakehandy/h1) by Jake Handy and built on fm1-base.

**[The guide and website](https://abektes.github.io/a22/)** · **[Play it in your browser](https://abektes.github.io/a22/sim/)** ·
**[Download](https://github.com/abektes/a22/releases/latest)** · **[☕ Buy me a coffee](https://buymeacoffee.com/abektes)**

<p>
<img src="docs/img/a22/fm4.png" width="120" alt="FM-4"> <img src="docs/img/a22/electric.png" width="120" alt="ELECTRIC"> <img src="docs/img/a22/stack.png" width="120" alt="STACK"> <img src="docs/img/a22/analog.png" width="120" alt="ANALOG"> <img src="docs/img/a22/shaper.png" width="120" alt="SHAPER">
<img src="docs/img/a22/wire.png" width="120" alt="WIRE"> <img src="docs/img/a22/haze.png" width="120" alt="HAZE"> <img src="docs/img/a22/mono.png" width="120" alt="MONO"> <img src="docs/img/a22/kit.png" width="120" alt="KIT"> <img src="docs/img/a22/perc.png" width="120" alt="PERC">
<img src="docs/img/a22/wave.png" width="120" alt="WAVE"> <img src="docs/img/a22/chip.png" width="120" alt="CHIP"> <img src="docs/img/a22/cycle.png" width="120" alt="CYCLE"> <img src="docs/img/a22/skins.png" width="120" alt="SKINS">
</p>

> **A22 is early.** It has been installed and run on one FM-1 (every engine, effect and the tape driven over
> USB, with no crash), but not yet played at length by many people. Install it only if you are ready to go
> back to stock or another firmware if it misbehaves (see [If something goes wrong](#if-something-goes-wrong)).

## Engines

PRESETS changes the engine, ALGORITHM picks one of its eight algorithms, and the four knobs shape the
sound and the picture together. Each algorithm keeps its own knob settings.

| Engine | Sound | Algorithms | Knobs |
|---|---|---|---|
| **FM-4** | four-operator stereo FM with a filter after it | KEYS, BASS, BELL, PAD, BRASS, PLUCK, ORGAN, ALIEN | RATIO, DEPTH, FEEDBK, FILTER |
| **ELECTRIC** | electric pianos and keys | RHODES, WURLI, DX-EP, CLAV, ORGAN, FELT, VIBES, TAPE | BARK, TINE, TREM, WARMTH |
| **STACK** | stacked chords and wide pads | CLOUD, CHORD, JUNO, GLASS, ORGAN, DRONE, CHOIR, DREAM | SPREAD, CHORD, DRIFT, GLOW |
| **ANALOG** | virtual analog with thru-zero FM and a ladder filter | DUAL, TZFM, SUPER, SUB, SYNC, ACID, BRASS, CHORD | SHAPE, TZFM, CUTOFF, RESO |
| **SHAPER** | phase distortion and wavefolding | CZ-SAW, CZ-RES, BUCHLA, PLUCK, METRIC, CYBER, GLASSY, CRUSH | WARP, FOLD, BIAS, TONE |
| **WIRE** | plucked and bowed strings, physically modelled | NYLON, STEEL, HARP, KOTO, CLAV, BASS, SITAR, BOWED | DAMP, PLUCK, BODY, TENSION |
| **HAZE** | evolving ambient textures | AETHER, VOX, SOLAR, ICE, MIST, ABYSS, PRISM, GHOST | MORPH, FORMANT, DRIFT, SHIMMER |
| **MONO** | monophonic leads and basses, with glide | ACID, REESE, SH-LEAD, ZAP-BASS, GROWL, SUB-808, LASER, HOOVER | FM-BITE, CUTOFF, GLIDE, SUB-DRV |
| **KIT** | drum kits, a drum on every note of the octave | 808, 909, CYCLES, CRISP, LOFI, MODULAR, METAL, WORLD | TUNE, DECAY, PUNCH, FM-GRIT |
| **PERC** | tuned percussion: eight modes a note, and sympathetic strings in SPACE's top | HANDPAN, MARIMBA, KALIMBA, TABLA, WOOD, GAMELAN, STEEL, GLITCH | PITCH, MALLET, RING, SPACE |
| **WAVE** | wavetables: eight banks of fifteen waves, morphing, band-limited per octave | HARMON, ORGAN, VOX, DIGI, PULSE, SYNC, GLASS, GROWL | WAVE, SCAN, SPREAD, FILTER |
| **CHIP** | 8-bit console voices: pulses, stepped triangle, noise, arpeggios | PULSE, TRI, NOISE, ARP, BASS, LEAD, DRUM, DUET | DUTY, SWEEP, ARP, CRUSH |
| **CYCLE** | WAVE's player on single cycles of recorded instruments ([AKWF](docs/WAVETABLES.md), CC0): four banks, each smooth and stepped | STRING, REED, KEYS, VOICE, STEP-STR, STEP-RD, STEP-KEY, STEP-VOX | WAVE, SCAN, SPREAD, FILTER |
| **SKINS** | two recorded drum kits ([Virtuosity Drums and VCSL](docs/SAMPLES.md), CC0), a drum on every note of the octave: an acoustic set and a marching and orchestral kit, each dry, in a room, tight and vintage | STUDIO, LIVE, TIGHT, VINTAGE, ORCH, HALL, MARCH, SHELLAC | TUNE, DECAY, TONE, ROOM |

## Effects

One effect at a time, chosen on the FX page with ALGORITHM. PLATE, DELAY, SHIFT, CHORUS and TANK are sends: each
track sets how much of it it sends. FILTER, LOFI and THUMP work on the whole mix, whatever the sends are.

| Effect | Knobs |
|---|---|
| **PLATE** (plate reverb) | MIX, DECAY, DAMP, MOD |
| **DELAY** (tape ping-pong) | MIX, TIME, FEEDBK, TAPE |
| **FILTER** (resonant filter with an envelope follower) | LPF, HPF, RESO, ENV |
| **SHIFT** (frequency shifter and comb) | FREQ, DELAY, FEEDBK, MIX |
| **CHORUS** | MIX, RATE, DEPTH, WIDTH |
| **TANK** (spring reverb) | MIX, DECAY, TWANG, TONE |
| **LOFI** (lo-fi) | CRUSH, RATE, BAND, GRIT |
| **THUMP** (drive and transient) | PUNCH, DRIVE, BASS, TONE |

The master bus has its own three-band EQ and drive (the MIX page's last subpage).

## Pages

<p>
<img src="docs/img/a22/fx.png" width="180" alt="The FX page"> <img src="docs/img/a22/mix.png" width="180" alt="The MIX page"> <img src="docs/img/a22/tape.png" width="180" alt="The TAPE page">
</p>

Each page's four columns are the four knobs. SELECT steps through a page's subpages.

| Page | Button | Columns |
|---|---|---|
| FX | FX | The effect's four knobs |
| MIX | SEL | Track volume, pan, FX send (one column a track); master LOW, MID, HIGH, DRIVE |
| ENV | ENV | ATTACK, DECAY, SUSTAIN, RELEASE; the mod envelope's DEPTH, DECAY, VEL, PUNCH |
| LFO | LFO | SHAPE, RATE, DEPTH, TO (pitch, a knob, level or pan) |
| GLOBAL | GLO | TEMPO, SWING, TUNE, BRIGHT; KEYS: ROOT, SCALE, GLIDE, HOLD; CHORD: MODE, TYPE, VOICING, STRUM |
| SEQ | ARP | MODE (OFF, RIFF, SHAKE, UP, DOWN, UP/DOWN, RANDOM, CHORD), RATE, RANGE, GATE. In SHAKE: MODE, GRAV, SPIN, BOUNCE; MODE, CAGE |
| TAPE | SEQ | TRACK (T1 to T4), BARS (1 to 32), SPEED (0.5X, 1X, 2X, REV), TRICK (BRAKE, stutters); edit: IN BAR, OUT BAR, SNAP, PITCH |

## The tape

Four tracks, T1 to T4. Each track keeps its own engine, algorithm, knobs, volume, pan, send and mute.
The tape is 1 to 32 bars and holds 2,048 events (1,024 notes) across all four tracks, knob moves included.
When it is full the screen says **TAPE FULL**.

- **REC** arms the track in hand and the first key starts the take. REC again closes it and the tape plays
  on. REC while it plays overdubs.
- **PLAY** starts and stops the tape.
- Knobs turned while recording are recorded too, and play back as automation.
- SPEED plays the tape at half speed, double speed or backwards. TRICK adds a tape brake or stutters.
- **EDIT + REC** cuts the track in hand's notes (between the loop's in and out bars) to a clipboard, and
  **EDIT + PLAY** pastes them at the current bar.
- **EDIT + OCT −** and **EDIT + OCT +** move the loop's in and out bars.
- **Hold REC** clears the track in hand. Keep holding (about three seconds) and it asks CLEAR ALL?, until
  it is answered: tap REC within three seconds of letting go to clear the whole tape. Anything else (another
  button, a key, an encoder, or waiting) keeps it.

The tape and every setting are saved by themselves and come back after a restart.

**STEP.** Hold SEQ to swap the tape for the step sequencer. REC then turns step writing on and off, PLAY
runs the pattern, SELECT moves the step cursor and a tap of EDIT leaves a rest.

## Riff and Shake

Both are modes of the SEQ page (ARP button, then ALGORITHM, or hold EDIT and turn ALGORITHM anywhere).

- **RIFF:** on the SEQ page, REC starts recording a sequence. Play its notes one at a time, tap EDIT for
  a rest, and REC again to finish. Then hold any key and the sequence plays from it, transposed. Hold
  REC on the page to clear it.
- **SHAKE:** every key drops a ball into a spinning cage, and the ball plays its note each time it
  bounces. GRAV, SPIN, BOUNCE and CAGE shape the cage: the first three on the page, CAGE on its second
  subpage (SELECT). KNOB 1 stays MODE in SHAKE, as in every mode.

## Chord memory

One key plays a chord. Its settings are GLO's third subpage, CHORD (GLO, then SELECT twice):

- **MODE:** OFF; MEMORY, a chord you play in yourself; SCALE, chords built from the KEYS page's root and
  scale (with CHROM, the major scale on the root); SHAPE, a fixed chord.
- **TYPE:** in SCALE, TRIAD, 7TH, SUS2, SUS4, ADD9 or 6TH. In SHAPE, MAJ, MIN, SUS4, 7, MIN7, MAJ7, POWER or OCT.
- **VOICING:** CLOSE; OPEN (the middle note an octave up); DROP2 (the second note from the top an octave
  down); +BASS (the root again an octave below).
- **STRUM:** 0 plays the notes together; up to 60 ms between each, from the bottom up.

**Hold the chord's keys and press EDIT + GLO** to store it as MEMORY's chord (up to six notes). Each key then
plays that shape from itself. EDIT + GLO with no keys held turns chords off, and on again.

The tape, the step sequencer, the arpeggiator and MIDI out all get the chord's notes, so what was recorded
plays back as heard. A key lets go of the chord it started even if the settings changed while it was held, and
two chords sharing a note hold it until both are let go. Notes coming in over MIDI are not made into chords.
MONO is monophonic and plays the chord's root alone. A chord takes a voice a note, out of eight.

## Hotkeys

| Control | What it does |
|---|---|
| PRESETS | Engine. **Hold EDIT:** the track in hand, T1 to T4 |
| ALGORITHM | One of the engine's eight algorithms. On the FX page: the effect. On the SEQ page, or with EDIT held: the sequencer mode |
| KNOB 1–4 | Home screen: the engine's four knobs. On a page: its four columns. On the MIX volume subpage with EDIT held: mute T1 to T4 |
| SELECT | Home screen: tempo. On a page: the next subpage. Writing steps: the cursor. **Hold EDIT:** the track's volume |
| MASTER | Volume of everything. **Hold EDIT:** the track's volume |
| FX SEL ENV LFO GLO ARP SEQ | Open that page. HOME goes back |
| EDIT | Pins the knob strip on the home screen. Writing steps or Riff: a rest |
| EDIT + FX | Effect on or bypassed |
| EDIT + SEL | Mute the track in hand |
| EDIT + ARP | Hold (latch) on and off |
| EDIT + GLO | With keys held: store them as the chord. With none: chords off, or on again |
| EDIT + LFO | DICE: roll the four knobs of the algorithm in hand, near where that algorithm sits |
| EDIT + ENV | Back to the knobs from before the last roll, and again to the roll (compare them) |
| SAVE + white key 1–8 | Load that snapshot, or save to it if it is empty. **SAVE + EDIT + key** saves over it. The keys are the eight white keys from the lowest F |
| hold SAVE | Reset the algorithm's knobs. Longer (about two seconds): reset the engine. Four seconds: reset every engine and the GLOBAL and KEYS pages |
| hold SEQ | Swap TAPE and STEP |
| hold GLO | Metronome: CLICK REC, CLICK ON, CLICK OFF |
| hold HOME | Silence everything (and send CC 123) |
| OCT − / + | Octave, two each way. Both together: the middle octave |
| hold OCT − and OCT + | USB host for a keyboard, or back to the computer |

A button with a hold does its tap when you let go.

## USB audio and MIDI

Plug the FM-1 into a computer or a phone with a USB cable that carries data. It shows up as one device
with two parts.

**Audio.** An audio input, like a USB microphone: the whole mix at the MASTER volume. Nothing plays back
through it.

**MIDI in,** over USB or the TRS jack, on any channel: notes play the engine in hand at their velocity
(softer is quieter and darker), CC 20 to 23 turn the four knobs, and CC 123 stops every note.

**MIDI out,** over USB only, on channel 1: the notes of the keys, the arpeggiator and the tape, and
CC 20 to 23 as the knobs turn. A note that came in over MIDI isn't sent back out.

**A USB keyboard, with no computer.** Hold **OCT −** and **OCT +** together for a second and the USB port
turns host for a USB-MIDI keyboard. The keyboard needs its own power or a powered hub. Hold both again to
go back. It's never kept, so the FM-1 always starts as a USB device.

## Install

Releases are on [GitHub](https://github.com/abektes/a22/releases), and A22 is listed on
[The Firmware Hub](https://thefirmwarehub.com), which offers each new release a few hours after it is out.

### Before you install

- **Back up first.** Every firmware keeps its work in the FM-1's flash in its own way. Save what the
  firmware you are leaving has made with its own backup. The Firmware Hub's notes on this are
  [here](https://thefirmwarehub.com/kb/#data).
- Charge the FM-1 and use a USB cable that carries data.
- A22 is community firmware with no warranty, and it has nothing to do with M-VAVE. You install it at
  your own risk.

### Get the package

Download `a22-X.Y.Z.fwsc` from the [latest release](https://github.com/abektes/a22/releases/latest) (a release
X.Y.Z answers `FM-1_4122XXYYZZ`). Between releases, every push to `main` builds a development package: open
the latest successful run under **Actions**, download the `a22-dev-build` artifact and unzip it for
`a22.fwsc` (identity `FM-1_412200`). The `a22-release-test` artifact is the next release built as it will be.

### From a terminal

From a clone of this repo (`git clone --recurse-submodules`), with the FM-1 on USB:

```
pip3 install mido python-rtmidi
python3 base/tools/fm1_install.py a22.fwsc
```

It works from the stock firmware (V15) and from other custom firmwares, asks before it writes, and checks
what the FM-1 answers afterwards. If the FM-1 runs something older than V15, update it with M-VAVE's
updater first.

## Leave A22 again

A22's package carries fm1-base's update loader, so other installers can replace it:

- **Another custom firmware:** use its own installer, or its card on
  [The Firmware Hub](https://thefirmwarehub.com).
- **Back to stock:** press **Back to official V15** in the Hub's
  [Official firmware](https://thefirmwarehub.com/#official) box.

A22 only saves its own settings. Leaving it doesn't bring back what an earlier firmware saved.

## If something goes wrong

These come from fm1-base, which A22 is built on:

- **An install stopped halfway:** plug the FM-1 back in and install again. The loader picks up where it
  stopped.
- **A22 doesn't start:** after two failed starts it comes up in SAFE MODE. There is no sound, but USB
  works, so A22, another firmware or V15 can be installed.
- **Nothing reaches it:** hold **REC** while you switch it on. That is the chip's own update mode, and the
  Hub's [rescue page](https://thefirmwarehub.com/tools/rescue/) takes it from there. The chip's
  bootloader is in ROM and can't be overwritten.

## Compatibility

- **M-VAVE FM-1 only.** Not the SMK-37.
- **Both flash chips:** the original Puya P25Q80 (JEDEC ID `0x856014`) and the UC25HQ80 (`0xB36014`).
- USB audio out, USB MIDI in and out, or USB host for a keyboard. MIDI in over the TRS jack. No MIDI out
  over TRS and no Bluetooth.
- A screen reader ([fm1-reader](https://github.com/charlesvestal/fm1-reader)) hears the engine, the
  algorithm, the page and each control's value as it changes.
- Package identity `FM-1_4122`. A release X.Y.Z answers `FM-1_4122XXYYZZ`.
- The firmware's image is 406 KB of its 582 KB flash slot and uses 440 KB of the chip's 475 KB of SRAM.

## For developers

```
core/    the whole firmware: portable C99, no libc, no malloc
app/     the device: fm1-base's app, a thin pass-through to core/
base/    fm1-base (a git submodule, with felucca-bsp inside it)
port/    other builds of the same core: host (pictures, WAVs), rfm1 (browser simulators)
tests/   the device app in fm1-base's host simulator, the core alone, and the crash hunt
```

- `./build.sh` builds `app/build/a22.fwsc`, a development build with identity `FM-1_412200`. It needs
  JieLi's toolchain and the AC79 SDK (see `base/README.md`). `./build.sh --release X.Y.Z` builds the
  release package.
- `sh tests/run_tests.sh` builds the device app for the host and runs `tests/basic.txt`,
  `tests/usbhost.txt` and `tests/looppower.txt` in it, then on the core alone: `tests/a22_test.c`
  (two cores, the tape, snapshots, varispeed, Riff, Shake), `tests/basics.c` (the limiter, velocity,
  MIDI out), `tests/mixer.c`, `tests/looper.c`, `tests/echo.c`, `tests/cores.c` and `tests/lines.c`. It
  needs only a C compiler and Pillow.
- `sh tests/run_fuzz.sh` is the crash hunt: the core under the address and undefined-behaviour checkers,
  fed any saved state and any panel input.
- The chip's second core renders half the voices. `FM1B_CPU2=0 ./build.sh` builds the firmware without
  it: see "Two cores" in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
- `tools/build_host.sh` builds the core alone as `build/h1host`, a script runner that writes pictures
  and WAVs. `tools/build_rfm1.sh` builds `dist/a22.rfm1` for `.rfm1` hosts such as
  [fm1-browser-simulator](https://github.com/jakehandy/fm1-browser-simulator).
- CI (`.github/workflows/ci.yml`) runs the tests and the crash hunt and builds the firmware on every push
  to `main` and every pull request. A tag `vX.Y.Z` runs `.github/workflows/release.yml`, which builds,
  tests, attests and publishes the release through the shared
  [fm1-build](https://github.com/zvenson/fm1-build) builder.
- `firmware.json` is written by the build from `app/fm1.toml`.

`docs/ARCHITECTURE.md` and `docs/RESEARCH.md` come from h1 and describe its instruments, but the
platform, the two cores and the hardware notes still apply.

## On a computer

A22's synth core also runs without the FM-1, in
[a22-soundtrack](https://github.com/abektes/a22-soundtrack): an agent skill that scores videos offline,
with music from A22's engines and sound effects timed to the frame. Its demos play at
[abektes.github.io/a22-soundtrack](https://abektes.github.io/a22-soundtrack/).

## Support

A22 is free and stays free. If it gave you a good evening, you can
[buy me a coffee](https://buymeacoffee.com/abektes).

## Licence

A22 is [MIT](LICENSE), as h1 is.

The firmware package also contains [fm1-base](https://github.com/charlesvestal/fm1-base) by Charles
Vestal and [felucca-bsp](https://github.com/hugelton/felucca-bsp), which come from
[Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita, Hügelton Instruments. Both are MPL-2.0,
unchanged, and their source is the `base` submodule at the commit a release was built from. Their
notices are in `base/LICENSING.md` and `base/bsp/NOTICE`.

CYCLE's wavetables are made from [Adventure Kid Waveforms](https://github.com/KristofferKarlAxelEkstrand/AKWF-FREE)
by Kristoffer Ekstrand, CC0 1.0 ([docs/WAVETABLES.md](docs/WAVETABLES.md)).

M-VAVE and FM-1 are trademarks of their owners. A22 is not affiliated with M-VAVE.
