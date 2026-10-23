#!/usr/bin/env python3
"""
Procedural melee hit sounds for MAL.

Synthesises a small impact / whoosh library from nothing but numpy and scipy --
no sample pack, no licensing, no third-party assets.

Run with the *system* Python (needs numpy + scipy), NOT the Unreal editor's
embedded Python:

    python Scripts/generate_hit_sfx.py

Output (44.1 kHz, 16-bit, mono):
    Content/ProceduralAudio/*.wav

Import them into the editor from the Content Browser afterwards.
"""

import os

import numpy as np
from scipy import signal
from scipy.io import wavfile

SR = 44100

_HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.normpath(os.path.join(_HERE, "..", "Content", "ProceduralAudio"))


# --------------------------------------------------------------------------- #
# helpers
# --------------------------------------------------------------------------- #

def db_to_lin(db):
    return 10.0 ** (db / 20.0)


def filtered_noise(rng, n, lo, hi, mode="band", order=2):
    """White noise through a Butterworth filter, applied zero-phase.

    Filtering in both directions matters here: a one-way filter adds group
    delay, which smears the very attack transient that makes a hit sound like
    a hit.
    """
    pad = 1024
    x = rng.standard_normal(n + 2 * pad)

    nyquist = SR * 0.5
    hi = min(float(hi), nyquist * 0.98)
    lo = max(float(lo), 5.0)

    if mode == "band":
        if lo >= hi:
            lo = hi * 0.5
        sos = signal.butter(order, [lo, hi], btype="bandpass", fs=SR, output="sos")
    elif mode == "high":
        sos = signal.butter(order, lo, btype="highpass", fs=SR, output="sos")
    else:
        sos = signal.butter(order, hi, btype="lowpass", fs=SR, output="sos")

    return signal.sosfiltfilt(sos, x)[pad:pad + n]


def fade(x, fade_in=0.0004, fade_out=0.008):
    """Ramp the ends to zero so the WAV never starts or stops on a click."""
    n = len(x)
    env = np.ones(n, dtype=np.float64)

    fi = min(int(fade_in * SR), n)
    fo = min(int(fade_out * SR), n)
    if fi > 0:
        env[:fi] *= np.linspace(0.0, 1.0, fi)
    if fo > 0:
        env[n - fo:] *= np.linspace(1.0, 0.0, fo)

    return x * env


def saturate(x, drive):
    """Soft clip. Rounds the peaks instead of squaring them off."""
    if drive <= 0.0:
        return x
    return np.tanh(x * drive) / np.tanh(drive)


# --------------------------------------------------------------------------- #
# voices
# --------------------------------------------------------------------------- #

def make_metal_hit(rng, dur=0.55, f0=600.0, tau=0.11, drive=1.15):
    """Blade on armour: inharmonic plate ring + contact crack + low thump."""
    n = int(dur * SR)
    t = np.arange(n) / SR
    out = np.zeros(n)

    # A struck plate rings at inharmonic ratios, not on a harmonic series.
    # These ratios are deliberately non-integer; that is what reads as metal.
    # (ratio, amplitude, decay-time scale -- high partials always die first)
    partials = (
        (1.00, 1.00, 1.00),
        (2.31, 0.58, 0.80),
        (3.83, 0.46, 0.68),
        (5.44, 0.38, 0.57),
        (7.09, 0.30, 0.48),
        (9.21, 0.24, 0.40),
        (13.40, 0.22, 0.30),  # bright "tink" that dies almost instantly
        (17.80, 0.14, 0.20),
    )

    # The plate gets pushed in on contact and springs back, so the pitch glides
    # up from slightly flat. Integrated so the phase stays continuous.
    glide_depth, glide_tau = 0.04, 0.016
    glide = glide_depth * glide_tau * (1.0 - np.exp(-t / glide_tau))

    for ratio, amp, tau_scale in partials:
        freq = f0 * ratio
        if freq > SR * 0.45:
            continue
        phase = 2.0 * np.pi * freq * (t - glide)
        out += amp * np.exp(-t / (tau * tau_scale)) * np.sin(phase)

    # Contact transient: a few milliseconds of broadband noise the instant the
    # steel meets. Without it the ring starts too cleanly to be believable.
    burst_n = int(0.006 * SR)
    burst = filtered_noise(rng, burst_n, 1400, 11000)
    burst *= np.exp(-np.arange(burst_n) / (0.0018 * SR))
    out[:burst_n] += 1.25 * burst

    # High sheen: the fine dust of high-frequency vibration that clings to the
    # partials for a moment. Leave it out and the ring reads as struck wood.
    sheen = filtered_noise(rng, n, 2500, 10000)
    out += 0.42 * sheen * np.exp(-t / 0.055)

    # Weight. Pure ringing sounds like a bell, not like something was hit. Kept
    # modest so it does not drag the tone back down into "thud" territory.
    out += 0.12 * np.sin(2.0 * np.pi * 92.0 * t) * np.exp(-t / 0.05)

    return saturate(out, drive)


def make_flesh_hit(rng, dur=0.32, thump_f0=72.0, tau=0.06):
    """Blade on a soft body: low mass thump + meaty low-passed smear."""
    n = int(dur * SR)
    t = np.arange(n) / SR
    out = np.zeros(n)

    # Mass, not pitch: low sines with a fast decay read as weight rather than
    # as a note.
    out += 0.48 * np.sin(2.0 * np.pi * thump_f0 * t) * np.exp(-t / tau)
    out += 0.26 * np.sin(2.0 * np.pi * thump_f0 * 1.62 * t + 0.6) * np.exp(-t / (tau * 0.7))

    # The meaty part. It rings out a little longer than the thump so the hit
    # does not collapse into a kick drum.
    smear = filtered_noise(rng, n, 0, 1600, mode="low")
    out += 1.05 * smear * np.exp(-t / (tau * 0.9))

    # Short mid resonances give the impact a body cavity instead of a click.
    out += 0.35 * np.sin(2.0 * np.pi * 305.0 * t) * np.exp(-t / 0.030)
    out += 0.16 * np.sin(2.0 * np.pi * 620.0 * t) * np.exp(-t / 0.020)

    # Contact click: very short, quiet, high enough to sit above the body.
    click_n = int(0.0028 * SR)
    click = filtered_noise(rng, click_n, 800, 5000)
    click *= np.exp(-np.arange(click_n) / (0.0008 * SR))
    out[:click_n] += 0.32 * click

    return saturate(out, 1.05)


def make_stone_hit(rng, dur=0.6, thump_f0=58.0):
    """Blade on rock/stone armour: brittle crack + trickling debris grains."""
    n = int(dur * SR)
    t = np.arange(n) / SR
    out = np.zeros(n)

    # Far less sub than a flesh hit: stone shatters, it does not thud.
    out += 0.52 * np.sin(2.0 * np.pi * thump_f0 * t) * np.exp(-t / 0.075)
    out += 0.18 * np.sin(2.0 * np.pi * thump_f0 * 2.65 * t) * np.exp(-t / 0.04)

    # The initial crack: wide band noise that dies fast. Brittle, not boomy.
    crack = filtered_noise(rng, n, 700, 7000)
    out += 1.10 * crack * np.exp(-t / 0.050)

    # Grit rides above the crack -- the surface breaking up, not just shaking.
    grit = filtered_noise(rng, n, 2200, 11000)
    out += 0.20 * grit * np.exp(-t / 0.030)

    # Debris. Scattered grains keep trickling after the crack -- randomly
    # placed, randomly tuned, randomly scaled. This is the layer that keeps two
    # hits of the same sound from feeling like the same hit.
    for _ in range(int(rng.integers(9, 14))):
        g_start = int(rng.integers(int(0.003 * SR), int(dur * 0.5 * SR)))
        g_n = int(rng.integers(int(0.006 * SR), int(0.018 * SR)))
        g_lo = float(rng.integers(2400, 4200))
        g_hi = g_lo + float(rng.integers(1500, 3500))

        grain = filtered_noise(rng, g_n, g_lo, g_hi)
        grain *= np.exp(-np.arange(g_n) / (rng.uniform(0.002, 0.006) * SR))
        grain *= rng.uniform(0.15, 0.40)

        g_end = min(g_start + g_n, n)
        out[g_start:g_end] += grain[:g_end - g_start]

    return saturate(out, 1.1)


def make_whoosh(rng, dur=0.32, tilt=0.35):
    """Swung blade through air. Use it for whiffs, not for hits."""
    n = int(dur * SR)
    t = np.arange(n) / SR
    x = t / dur

    # A band of noise that swells and falls. The envelope peaks before the
    # middle so it reads as a swipe rather than a hum.
    body = filtered_noise(rng, n, 240, 3000)
    env = np.sin(np.pi * np.clip(x, 0.0, 1.0)) ** 1.7
    env *= 1.0 - tilt * x

    # A quieter low band underneath gives the blade physical size.
    low = filtered_noise(rng, n, 90, 420)

    return body * env + 0.45 * low * env ** 1.5


def make_crit(rng, dur=0.85, f0=760.0):
    """Critical hit: the metal voice plus a sub boom under it."""
    metal = make_metal_hit(rng, dur=dur, f0=f0, tau=0.15, drive=0.0)
    n = len(metal)
    t = np.arange(n) / SR

    # Pitch sweeps down from 114 Hz to 44 Hz so the boom lands with the impact
    # instead of arriving after the ring has already faded.
    phase = 2.0 * np.pi * (44.0 * t + 70.0 * 0.025 * (1.0 - np.exp(-t / 0.025)))
    sub = np.sin(phase) * np.exp(-t / 0.22)

    mix = metal + 0.55 * sub

    return saturate(mix, 1.4)


# --------------------------------------------------------------------------- #
# library
# --------------------------------------------------------------------------- #

# name, target peak dBFS, builder
JOBS = (
    ("Metal_Hit_01", -1.0, lambda r: make_metal_hit(r, f0=585.0, tau=0.115)),
    ("Metal_Hit_02", -1.0, lambda r: make_metal_hit(r, f0=690.0, tau=0.095)),
    ("Metal_Hit_03", -1.5, lambda r: make_metal_hit(r, f0=505.0, tau=0.140)),

    ("Flesh_Hit_01", -1.5, lambda r: make_flesh_hit(r, thump_f0=68.0, tau=0.062)),
    ("Flesh_Hit_02", -1.5, lambda r: make_flesh_hit(r, thump_f0=86.0, tau=0.052)),
    ("Flesh_Hit_03", -2.0, lambda r: make_flesh_hit(r, thump_f0=57.0, tau=0.078)),

    ("Stone_Hit_01", -1.5, lambda r: make_stone_hit(r, thump_f0=54.0)),
    ("Stone_Hit_02", -1.5, lambda r: make_stone_hit(r, thump_f0=66.0)),
    ("Stone_Hit_03", -2.0, lambda r: make_stone_hit(r, thump_f0=47.0)),

    ("Whoosh_01", -6.0, lambda r: make_whoosh(r, dur=0.30)),
    ("Whoosh_02", -6.0, lambda r: make_whoosh(r, dur=0.38, tilt=0.20)),
    ("Whoosh_03", -7.0, lambda r: make_whoosh(r, dur=0.24, tilt=0.50)),

    ("Crit_Hit_01", -0.5, lambda r: make_crit(r, f0=780.0)),
    ("Crit_Hit_02", -0.5, lambda r: make_crit(r, f0=640.0)),
)


# --------------------------------------------------------------------------- #
# output
# --------------------------------------------------------------------------- #

def write_wav(path, samples, peak_db):
    x = np.asarray(samples, dtype=np.float64)
    x -= np.mean(x)

    peak = np.max(np.abs(x))
    if peak <= 1e-9:
        raise ValueError("silent buffer for " + path)

    x /= peak
    x = fade(x)
    # Normalise again after the fades so the requested peak is exact.
    x *= db_to_lin(peak_db)

    pcm = np.clip(np.round(x * 32767.0), -32768.0, 32767.0).astype(np.int16)
    wavfile.write(path, SR, pcm)

    rms_db = 20.0 * np.log10(np.sqrt(np.mean(x ** 2)) + 1e-12)
    return len(x) / SR, rms_db, os.path.getsize(path)


def main():
    os.makedirs(OUT_DIR, exist_ok=True)

    print("output: " + OUT_DIR)
    print("{:<15}{:>9}{:>10}{:>10}{:>9}".format("file", "length", "peak", "rms", "size"))
    print("-" * 53)

    total_bytes = 0
    for index, (name, peak_db, builder) in enumerate(JOBS):
        # A fixed but distinct seed per file keeps the output reproducible.
        rng = np.random.default_rng(20261023 + index * 977)
        length, rms_db, size = write_wav(
            os.path.join(OUT_DIR, name + ".wav"), builder(rng), peak_db
        )
        total_bytes += size
        print("{:<15}{:>8.2f}s{:>9.1f}dB{:>9.1f}dB{:>7.0f}KB".format(
            name, length, peak_db, rms_db, size / 1024.0))

    print("-" * 53)
    print("{} files, {:.0f} KB total".format(len(JOBS), total_bytes / 1024.0))


if __name__ == "__main__":
    main()
