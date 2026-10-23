#!/usr/bin/env python3
"""
Builds the player-hurt sound effects.

Two tools share the work, each doing what it is actually good at:

  numpy  - synthesises the raw impact layers. Envelope shape is the whole sound
           here, and a sample-exact exponential decay is far easier to state in
           code than to draw with a DAW's linear fades.
  REAPER - runs the processing chain and the pitch variants through its own
           DSP (saturation, transient shaping, EQ, filters) and renders 24-bit
           output. Pitch shifting goes through elastique3, which is a lot
           cleaner than anything hand-rolled.

Run with the system Python:

    python Scripts/build_hurt_sfx.py

Output:
    Content/HurtSFX/*.wav          finished effects (auto-imported by the editor)
    Audio/reaper/*.rpp             the projects, openable in REAPER
    Audio/reaper/src/*.wav         the synthesised source layer

The files are mono on purpose: a hurt cue usually hangs off a character, and a
stereo asset cannot be spatialised or attenuated properly. Render a stereo pair
only if the sound is meant to sit flat on the listener.
"""

import os
import subprocess
import sys
import wave

import numpy as np
from scipy.io import wavfile

import generate_hit_sfx as hitfx

REAPER_EXE = r"D:\REAPER\reaper.exe"
SR = 44100

_HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(_HERE, ".."))

SRC_DIR = os.path.join(ROOT, "Audio", "reaper", "src")
PROJ_DIR = os.path.join(ROOT, "Audio", "reaper")
OUT_DIR = os.path.join(ROOT, "Content", "HurtSFX")

SOURCE_NAME = "hurt_source"

# name, pitch (semitones), saturation %, attack %, sustain %, low dB, mid dB, high dB, output dB
VARIANTS = (
    ("Hurt_01", 0.0, 28, 50, -18, 0, 2, 2, -10),
    ("Hurt_02", -2.5, 40, 58, -24, 1, 3, 1, -12),
    ("Hurt_03", 1.5, 20, 42, -12, -1, 1, 3, -10),
)


# --------------------------------------------------------------------------- #
# source
# --------------------------------------------------------------------------- #

def synth_hurt_source(rng):
    duration = 0.36
    n = int(duration * SR)
    t = np.arange(n) / SR
    out = np.zeros(n)

    # Sub: the weight underneath the hit. Deliberately restrained. An earlier
    # take ran this at 0.50 with a 0.115 s decay and ended up with 57% of its
    # energy below 80 Hz, which reads as a muffled boom instead of an impact.
    phase = 2.0 * np.pi * (46.0 * t + 62.0 * 0.024 * (1.0 - np.exp(-t / 0.024)))
    out += 0.17 * np.sin(phase) * np.exp(-t / 0.090)

    # Thud: the chest cavity resonance. This is what makes the hit read as
    # landing on a body rather than a drum -- the sub layer alone cannot supply
    # this band, and without it the whole thing sounds thin.
    thud = hitfx.filtered_noise(rng, n, 80, 320)
    out += 0.58 * thud * np.exp(-t / 0.055)

    # Body: low-passed noise carrying the bulk of the thud. The corner sits at
    # 2 kHz so it still has some edge after the punch layer is mixed in.
    body = hitfx.filtered_noise(rng, n, 0, 2000, mode="low")
    out += 0.55 * body * np.exp(-t / 0.070)

    # Punch: the meat of the sound. It lives in the band the ear is most
    # sensitive to, so it carries the perceived hit while most of the raw
    # energy stays out of the sub range.
    punch = hitfx.filtered_noise(rng, n, 190, 800)
    out += 0.62 * punch * np.exp(-t / 0.038)

    # Crack: the hard core. Without this the layers below just smear together
    # and it stops reading as something solid being struck.
    crack = hitfx.filtered_noise(rng, n, 900, 2800)
    out += 0.66 * crack * np.exp(-t / 0.021)

    # Tear: a short bright rasp on contact. Low in energy but high in
    # salience, which is why it survived the rework at a larger share.
    tear = hitfx.filtered_noise(rng, n, 2200, 7500)
    out += 0.48 * tear * np.exp(-t / 0.013)

    # Contact transient.
    click_n = int(0.0030 * SR)
    click = hitfx.filtered_noise(rng, click_n, 1500, 12000)
    click *= np.exp(-np.arange(click_n) / (0.0008 * SR))
    out[:click_n] += 0.45 * click

    # Tinnitus: a very quiet high sine that lingers after the impact. This one
    # detail is what separates "I got hit" from "something else got hit".
    ring = np.sin(2.0 * np.pi * 3800.0 * t) * np.exp(-t / 0.16)
    ring *= np.minimum(t / 0.020, 1.0)
    out += 0.022 * ring

    return hitfx.saturate(out, 1.05)


def write_wav(path, samples, peak_db=-1.0):
    x = np.asarray(samples, dtype=np.float64)
    x -= np.mean(x)
    x /= np.max(np.abs(x))

    x = hitfx.fade(x, 0.0004, 0.010)
    x *= hitfx.db_to_lin(peak_db)

    wavfile.write(path, SR, np.clip(np.round(x * 32767.0), -32768, 32767).astype(np.int16))


# --------------------------------------------------------------------------- #
# reaper project
# --------------------------------------------------------------------------- #

TRACK = """  <TRACK
    NAME "{name}"
    VOLPAN {volume} 0 1 -1
    <FXCHAIN
      WNDRECT 0 0 100 100
      SHOW 0
      LASTSEL 0
      DOCKED 0
      BYPASS 0 0 0
      <JS loser/Saturation ""
        {sat}
      >
      <JS loser/TransientController ""
        {attack} {sustain} 0
      >
      <JS loser/3BandEQ ""
        {low} 180 {mid} 2500 {high} {out_db}
      >
      <JS sstillwell/hpflpf ""
        {hpf} {lpf} 0
      >
    >
    <ITEM
      POSITION {position}
      SNAPOFFS 0
      LENGTH {length}
      LOOP 0
      ALLTAKES 0
      FADEIN 1 0 0 1 0 0 0
      FADEOUT 1 0 0 1 0 0 0
      MUTE 0 0
      SEL 0
      IGUID {{{guid_item}}}
      IID 1
      NAME "{name}"
      VOLPAN {volume} 0 1 -1
      SOFFS 0
      PLAYRATE 1 1 {pitch} -1 0 0.0025
      CHANMODE 0
      GUID {{{guid_take}}}
      <SOURCE WAVE
        FILE "{source}"
      >
    >
  >
"""

PROJECT = """<REAPER_PROJECT 0.1 "7.82/win64" 1729700000
  RIPPLE 0
  GROUPOVERRIDE 0 0 0
  SAMPLERATE 44100 0 0
  TEMPO 120 4 4
  PLAYRATE 1 1 0 -1 0 0.0025
  MASTER_NCH 2 2
  MASTER_VOLUME 1 0 -1 -1 -1
  RENDER_FILE "{out_dir}"
  RENDER_PATTERN "{name}"
  RENDER_1X 0
  RENDER_RANGE 1 0 0 18 0
  RENDER_RESAMPLE 3 0 1
  RENDER_ADDTOPROJ 0
  RENDER_STEMS 0
  RENDER_DITHER 0
  TIMELOCKMODE 1
  TEMPOENVLOCKMODE 1
  ITEMMIX 0
  DEFPITCHMODE 589824 0
  TAKELANE 1
{body}
{weight}
>
"""


def build_project(path, name, source, duration, pitch, sat, attack, sustain, low, mid, high, out_db, tag):
    """Two tracks from one source: the hit itself, plus a detuned layer under it.

    The lower layer is what gives the impact weight without making it longer --
    it is the same transient, just tuned down and tucked underneath. Its filter
    corner sits much lower, because everything above is already covered by the
    body track and doubling it just muddies the attack.
    """
    body = TRACK.format(
        name="Body", volume="1 0 1 -1",
        sat=sat, attack=attack, sustain=sustain, low=low, mid=mid, high=high,
        out_db=out_db, hpf=30, lpf=18000,
        position="0", length="%.4f" % duration, pitch=pitch,
        guid_item="%s-0001-4000-8000-000000000001" % tag,
        guid_take="%s-0002-4000-8000-000000000002" % tag,
        source=source)

    weight = TRACK.format(
        name="Weight", volume="0.20 0 1 -1",
        sat="%.0f" % (sat + 15), attack="%.0f" % (attack - 10), sustain=sustain,
        low="%.0f" % (low + 2), mid=mid, high="%.0f" % (high - 2),
        out_db="%.0f" % (out_db - 7), hpf=25, lpf=7000,
        position="0.008", length="%.4f" % duration, pitch=pitch - 5.0,
        guid_item="%s-0003-4000-8000-000000000003" % tag,
        guid_take="%s-0004-4000-8000-000000000004" % tag,
        source=source)

    text = PROJECT.format(
        out_dir=OUT_DIR, name=name, body=body.rstrip("\n"), weight=weight.rstrip("\n"))

    with open(path, "w", encoding="ascii", newline="\r\n") as handle:
        handle.write(text)


# --------------------------------------------------------------------------- #
# render
# --------------------------------------------------------------------------- #

def render(project_path):
    result = subprocess.run(
        [REAPER_EXE, "-renderproject", project_path, "-nosplash"],
        capture_output=True, timeout=180)
    return result.returncode


def to_mono(path):
    """Collapse the rendered file to mono in place.

    REAPER ignores both MASTER_NCH and RENDER_CHANNELS when rendering from the
    command line, so the fold happens on the raw PCM instead. A mono source
    panned centre produces two identical channels, so taking one is a lossless
    byte slice rather than a downmix -- and it keeps the original 24-bit
    samples untouched, which a scipy round-trip would not.
    """
    with wave.open(path, "rb") as handle:
        channels = handle.getnchannels()
        width = handle.getsampwidth()
        rate = handle.getframerate()
        frames = handle.readframes(handle.getnframes())

    if channels == 1:
        return False

    stride = channels * width
    raw = np.frombuffer(frames, dtype=np.uint8).reshape(-1, stride)[:, :width].tobytes()

    with wave.open(path, "wb") as handle:
        handle.setnchannels(1)
        handle.setsampwidth(width)
        handle.setframerate(rate)
        handle.writeframes(raw)

    return True


def measure(path):
    with wave.open(path, "rb") as handle:
        duration = handle.getnframes() / float(handle.getframerate())
        rate = handle.getframerate()

    _, pcm = wavfile.read(path)
    full_scale = float(np.iinfo(pcm.dtype).max) if pcm.dtype != np.int16 else 32767.0
    peak = np.max(np.abs(pcm.astype(np.float64))) / full_scale
    return duration, rate, peak


def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except AttributeError:
        pass

    for path in (SRC_DIR, PROJ_DIR, OUT_DIR):
        os.makedirs(path, exist_ok=True)

    source = os.path.join(SRC_DIR, SOURCE_NAME + ".wav")
    write_wav(source, synth_hurt_source(np.random.default_rng(20261023)))
    print("source : " + source)

    duration = 0.36

    for index, (name, pitch, sat, attack, sustain, low, mid, high, out_db) in enumerate(VARIANTS):
        project = os.path.join(PROJ_DIR, name.lower() + ".rpp")
        build_project(project, name, source, duration, pitch, sat, attack, sustain,
                      low, mid, high, out_db, "%08X" % (0xA1000000 + index))

        target = os.path.join(OUT_DIR, name + ".wav")
        if os.path.exists(target):
            os.remove(target)

        code = render(project)
        if code != 0 or not os.path.exists(target):
            print("{:<10} FAILED (exit {}, output {})".format(
                name, code, "missing" if not os.path.exists(target) else "present"))
            continue

        to_mono(target)
        length, rate, peak = measure(target)
        print("{:<10}{:>6.2f}s{:>7.0f}Hz  peak {:>6.1f} dBFS  pitch {:+.1f} st".format(
            name, length, rate, 20.0 * np.log10(peak + 1e-12), pitch))


if __name__ == "__main__":
    main()
