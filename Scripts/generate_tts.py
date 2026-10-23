#!/usr/bin/env python3
"""
Offline text-to-speech for MAL voice lines.

Renders through the Chinese voices that ship with Windows, using SAPI5. No
cloud service, no API key, nothing to buy.

The default line lives in this file rather than being passed on the command
line so it never has to survive a console code page round-trip:

    python Scripts/generate_tts.py
    python Scripts/generate_tts.py --name Greeting --voice Kangkang --variants 2

Output (16 kHz / 16-bit / mono, the native rate of the SAPI Chinese models):
    Content/VoiceLines/*.wav

The editor's Auto Reimport picks the files up as SoundWaves by itself.
"""

import argparse
import os
import sys
import wave
import winreg

import numpy as np
from scipy.io import wavfile

try:
    import win32com.client.dynamic as com
except ImportError:
    sys.exit("pywin32 is required: pip install pywin32")


DEFAULT_TEXT = "蚁人去哪了？"
DEFAULT_VOICE = "Huihui"
DEFAULT_NAME = "AntMan_Where"

_HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.normpath(os.path.join(_HERE, "..", "Content", "VoiceLines"))

# Windows ships extra Chinese voices to OneCore (Narrator, WinRT) that never
# show up in the SAPI5 catalogue. Pulling the token straight out of the
# registry and handing it to SpVoice reaches them -- that is the only way to
# get the male voice on a stock install.
ONECORE_VOICES_KEY = r"SOFTWARE\Microsoft\Speech_OneCore\Voices\Tokens"
ONECORE_VOICES_MOUNT = "HKEY_LOCAL_MACHINE"

# SpeechVoiceSpeakFlags
SVSF_IS_XML = 8
# SpeechAudioFormatType
SAFT_16KHZ_16BIT_MONO = 18
# SpeechStreamFileMode
SSFM_CREATE_FOR_WRITE = 3

# One take per row: (suffix, SAPI rate -10..10, SSML pitch shift)
TAKES = (
    ("01", 0, "0%"),
    ("02", 2, "6%"),
    ("03", -2, "-6%"),
)


def find_onecore_token(wanted):
    """Build an SpObjectToken for a OneCore-only voice, or return None."""
    try:
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, ONECORE_VOICES_KEY) as key:
            names = []
            index = 0
            while True:
                try:
                    names.append(winreg.EnumKey(key, index))
                except OSError:
                    break
                index += 1
    except OSError:
        return None

    for name in names:
        if wanted.lower() not in name.lower():
            continue
        try:
            token = com.Dispatch("SAPI.SpObjectToken")
            token.SetId(ONECORE_VOICES_MOUNT + "\\" + ONECORE_VOICES_KEY + "\\" + name, "")
            return token
        except Exception:
            return None

    return None


def pick_voice(voice, wanted):
    """Select the first installed voice whose description contains `wanted`."""
    tokens = voice.GetVoices()
    installed = []

    for index in range(tokens.Count):
        token = tokens.Item(index)
        description = token.GetDescription()
        installed.append(description)

        if wanted.lower() in description.lower():
            voice.Voice = token
            return description

    # Not in the SAPI5 catalogue -- the OneCore store may still have it.
    token = find_onecore_token(wanted)
    if token is not None:
        try:
            voice.Voice = token
            return voice.Voice.GetDescription()
        except Exception:
            pass

    raise RuntimeError("voice '{}' not found; installed: {}".format(
        wanted, "; ".join(installed)))


def render(voice, text, path, rate, pitch):
    """Speak `text` straight into a WAV file."""
    fmt = com.Dispatch("SAPI.SpAudioFormat")
    fmt.Type = SAFT_16KHZ_16BIT_MONO

    stream = com.Dispatch("SAPI.SpFileStream")
    stream.Format = fmt
    stream.Open(path, SSFM_CREATE_FOR_WRITE)

    voice.Rate = rate
    voice.AudioOutputStream = stream

    ssml = (
        "<speak version='1.0' xmlns='http://www.w3.org/2001/10/synthesis' xml:lang='zh-CN'>"
        "<prosody pitch='{pitch}'>{text}</prosody>"
        "</speak>"
    ).format(pitch=pitch, text=text)

    voice.Speak(ssml, SVSF_IS_XML)

    # Close before the voice is reassigned on the next take, otherwise the next
    # Speak() can land in a stream that is already shutting down.
    voice.AudioOutputStream = None
    stream.Close()


def polish(path, target_db=-3.0):
    """Lift the take to a predictable peak and ramp the ends.

    SAPI leaves a few dB of headroom unused and starts on a hard sample; both
    are audible as a quiet line and an opening click.
    """
    sample_rate, pcm = wavfile.read(path)
    x = pcm.astype(np.float64)

    peak = np.max(np.abs(x))
    if peak <= 0.0:
        raise RuntimeError("silent rendering: " + path)

    x *= (10.0 ** (target_db / 20.0)) * 32767.0 / peak

    fade_in = min(int(0.003 * sample_rate), len(x))
    fade_out = min(int(0.010 * sample_rate), len(x))
    if fade_in > 0:
        x[:fade_in] *= np.linspace(0.0, 1.0, fade_in)
    if fade_out > 0:
        x[-fade_out:] *= np.linspace(1.0, 0.0, fade_out)

    wavfile.write(path, sample_rate, np.clip(np.round(x), -32768, 32767).astype(np.int16))

    with wave.open(path, "rb") as handle:
        return handle.getnframes() / float(handle.getframerate()), sample_rate


def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except AttributeError:
        pass

    parser = argparse.ArgumentParser(description="Render a voice line with a Windows SAPI5 voice.")
    parser.add_argument("--text", default=DEFAULT_TEXT)
    parser.add_argument("--name", default=DEFAULT_NAME)
    parser.add_argument("--voice", default=DEFAULT_VOICE)
    parser.add_argument("--variants", type=int, default=len(TAKES))
    parser.add_argument("--out", default=OUT_DIR)
    args = parser.parse_args()

    os.makedirs(args.out, exist_ok=True)

    voice = com.Dispatch("SAPI.SpVoice")
    description = pick_voice(voice, args.voice)

    print("voice  : " + description)
    print("text   : {} characters".format(len(args.text)))
    print("output : " + args.out)
    print()

    takes = TAKES[:max(1, min(args.variants, len(TAKES)))]
    for suffix, rate, pitch in takes:
        path = os.path.join(args.out, "{}_{}.wav".format(args.name, suffix))
        render(voice, args.text, path, rate, pitch)
        duration, sample_rate = polish(path)
        print("{:<22}{:>6.2f}s{:>8.0f}Hz{:>9.0f} KB".format(
            os.path.basename(path), duration, sample_rate, os.path.getsize(path) / 1024.0))


if __name__ == "__main__":
    main()
