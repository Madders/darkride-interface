#
#  make_system_sounds.py
#  tools/sounds
#
#  Created on 2026-10-18 for Darkride's system sounds.
#
#  The client's system sounds, made from one small palette so they belong together: soft sine "bloops" that glide
#  (the pop of a bubble), marimba-like mallet notes, and a small bright room. Each file kept the name, place and format
#  of the sound it replaced (no code changes) and that sound's loudness over its loudest 100 ms, measured from the old
#  files and written in SOUNDS below (the tablet's own button click was near full scale; it is 6 dB above a click).
#
#      python3 tools/sounds/make_system_sounds.py <out dir>        (the repo's root writes the files in place)
#
#  Needs numpy, and lame for the MP3; resampling uses macOS's afconvert.
#
#  Distributed under the MIT License.
#  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
#
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

import numpy as np

SR = 48000


def note(name):
    """'C6' -> Hz (A4 = 440)."""
    steps = {"C": -9, "C#": -8, "D": -7, "D#": -6, "E": -5, "F": -4, "F#": -3, "G": -2, "G#": -1, "A": 0, "A#": 1, "B": 2}
    return 440.0 * 2 ** ((steps[name[:-1]] + 12 * (int(name[-1]) - 4)) / 12)


def envelope(n, attack, decay):
    """A raised-cosine attack, then an exponential decay with time constant `decay` (seconds)."""
    t = np.arange(n) / SR
    env = np.exp(-t / decay)
    a = max(1, int(attack * SR))
    env[:a] *= 0.5 - 0.5 * np.cos(np.pi * np.arange(a) / a)
    return env


def mallet(freq, decay, length=None, attack=0.002):
    """A marimba-like tone: the fundamental, a quickly dying 4x partial for the mallet's knock, a soft octave."""
    n = int((length or max(0.05, 6 * decay)) * SR)
    t = np.arange(n) / SR
    tone = (np.sin(2 * np.pi * freq * t) + 0.35 * np.exp(-t / 0.03) * np.sin(2 * np.pi * 4 * freq * t)
            + 0.15 * np.sin(2 * np.pi * 2 * freq * t))
    return tone * envelope(n, attack, decay)


def bloop(f0, f1, length=0.07, attack=0.003, decay=0.035):
    """A bubbly pop: a sine gliding from f0 to f1 (exponentially, over the first 60 %) under a short envelope."""
    n = int(length * SR)
    t = np.arange(n) / SR
    f = f0 * (f1 / f0) ** np.minimum(t / (0.6 * length), 1.0)
    return np.sin(2 * np.pi * np.cumsum(f) / SR) * envelope(n, attack, decay)


def place(events, total):
    """Mix (start seconds, signal, pan -1..1, gain) into a stereo buffer `total` seconds long."""
    out = np.zeros((2, int(total * SR)))
    for start, sig, pan, gain in events:
        i = int(start * SR)
        j = min(out.shape[1], i + len(sig))
        out[0, i:j] += gain * np.sqrt(0.5 * (1 - pan)) * sig[:j - i]
        out[1, i:j] += gain * np.sqrt(0.5 * (1 + pan)) * sig[:j - i]
    return out


def comb(x, delay, feedback):
    y = np.copy(x)
    d = int(delay * SR)
    for i in range(d, len(y)):
        y[i] += feedback * y[i - d]
    return y


def allpass(x, delay, g=0.6):
    d = int(delay * SR)
    y = np.zeros_like(x)
    for i in range(len(x)):
        y[i] = -g * x[i] + (x[i - d] + g * y[i - d] if i >= d else 0.0)
    return y


def room(stereo, wet=0.12, feedback=0.72):
    """A small, bright room: four combs and two allpasses per side, the right side's delays a little longer."""
    out = np.copy(stereo)
    for ch, spread in ((0, 0.0), (1, 0.0013)):
        x = stereo[ch]
        r = sum(comb(x, d + spread, feedback) for d in (0.0297, 0.0371, 0.0411, 0.0437)) / 4
        out[ch] = x + wet * allpass(allpass(r, 0.005 + spread), 0.0017)
    return out


def soften(stereo, cutoff=9000.0):
    """A one-pole low-pass, to take the edge off the top."""
    a = np.exp(-2 * np.pi * cutoff / SR)
    out = np.copy(stereo)
    for ch in range(2):
        y = 0.0
        for i in range(out.shape[1]):
            y = (1 - a) * out[ch, i] + a * y
            out[ch, i] = y
    return out


def mix(events, total, wet=0.12):
    return soften(room(place(events, total), wet=wet))


# --- the sounds, by where they play ---

def connect():          # Audio.receivedFirstPacket: arriving in a world (tivoli-scripts connection-sounds.ts)
    return mix([(0.0, bloop(520, 1250), 0.0, 0.7), (0.06, mallet(note("C6"), 0.16, 0.6), -0.2, 1.0),
                (0.13, mallet(note("G6"), 0.26, 0.7), 0.2, 0.9)], 0.95)


def disconnect():       # Audio.disconnected: leaving
    return mix([(0.0, bloop(1150, 480), 0.0, 0.6), (0.07, mallet(note("G5"), 0.16, 0.6), 0.2, 0.9),
                (0.15, mallet(note("C5"), 0.30, 0.8), -0.2, 1.0)], 0.95)


def click():            # a button or checkbox pressed (TabletEnums.ButtonClick); the mini tablet's button-click.wav
    return mix([(0.0, bloop(650, 1000, 0.05, decay=0.02), 0.0, 0.8), (0.012, mallet(note("E6"), 0.035), 0.0, 0.7)],
               0.2, wet=0.06)


def hover():            # the pointer over a button (TabletEnums.ButtonHover); the mini tablet's button-hover.wav
    return mix([(0.0, bloop(1300, 1700, 0.035, attack=0.004, decay=0.012), 0.0, 1.0)], 0.1, wet=0.05)


def tablet_click():     # the tablet's and windows' own buttons (TabletRoot.qml, WindowRoot.qml)
    return mix([(0.0, bloop(600, 950, 0.05, decay=0.02), 0.0, 0.8), (0.012, mallet(note("D6"), 0.045), 0.0, 0.75)],
               0.22, wet=0.07)


def tablet_open():      # the tablet appears (TabletEnums.TabletOpen)
    return mix([(0.0, bloop(480, 1100, 0.08, decay=0.04), 0.0, 0.6), (0.05, mallet(note("G5"), 0.09), -0.15, 0.9),
                (0.11, mallet(note("D6"), 0.16), 0.15, 0.85)], 0.55)


def hands_in():         # a hand comes to the tablet (TabletEnums.TabletHandsIn)
    return mix([(0.0, bloop(700, 1150, 0.05, decay=0.02), 0.0, 1.0)], 0.12, wet=0.06)


def hands_out():        # a hand leaves the tablet (TabletEnums.TabletHandsOut)
    return mix([(0.0, bloop(1150, 700, 0.05, decay=0.02), 0.0, 1.0)], 0.12, wet=0.06)


def snapshot():         # the camera (Application's snapshot sound, snapshot.js, keyboardShortcuts.js): pop, pop, sparkle
    return mix([(0.0, bloop(900, 1500, 0.04, decay=0.015), -0.1, 0.9), (0.07, bloop(1300, 700, 0.05, decay=0.02), 0.1, 0.9),
                (0.12, mallet(note("C7"), 0.12), -0.2, 0.55), (0.165, mallet(note("E7"), 0.10), 0.2, 0.45)], 0.65)


def key_press():        # a key on the in-world keyboard (Keyboard.cpp)
    return mix([(0.0, bloop(900, 1100, 0.02, attack=0.001, decay=0.008), 0.0, 0.6),
                (0.0, mallet(note("A5"), 0.03), 0.0, 1.0)], 0.12, wet=0.04)


def bump():             # the avatar's collision sound, heard in the world (MyAvatar's default collisionSoundURL)
    return mix([(0.0, bloop(320, 190, 0.06, decay=0.025), 0.0, 1.0), (0.0, mallet(note("G3"), 0.05), 0.0, 0.6)],
               0.15, wet=0.0)


def chat_a():           # a chat line (chat.ts plays chat1 and chat2 in turn, at volume 0.05)
    return mix([(0.0, bloop(820, 1350, 0.045, decay=0.018), 0.0, 0.8), (0.01, mallet(note("G6"), 0.04), 0.0, 0.6)],
               0.15, wet=0.05)


def chat_b():
    return mix([(0.0, bloop(700, 1150, 0.045, decay=0.018), 0.0, 0.8), (0.01, mallet(note("E6"), 0.04), 0.0, 0.6)],
               0.15, wet=0.05)


def space_bubble():     # bubble.js (off in Tivoli's defaultScripts): the personal space bubble turns on
    return mix([(0.0, bloop(260, 820, 0.32, attack=0.01, decay=0.12), 0.0, 1.0),
                (0.12, mallet(note("C6"), 0.1), -0.2, 0.4), (0.17, mallet(note("G6"), 0.1), 0.0, 0.35),
                (0.22, mallet(note("C7"), 0.12), 0.2, 0.3)], 0.75, wet=0.15)


def rezzing():          # marketplaces.js (off in Tivoli's defaultScripts): an item appears
    return mix([(0.0, bloop(500, 1000, 0.06, decay=0.03), 0.0, 0.5)] +
               [(0.04 + 0.05 * k, mallet(note(n), 0.12 if k < 3 else 0.22), (-0.3, -0.1, 0.1, 0.3)[k], 0.8)
                for k, n in enumerate(["C6", "E6", "G6", "C7"])], 0.65, wet=0.15)


# (repo path, maker, loudness over the loudest 100 ms in dBFS, sample rate, channels)
SOUNDS = [
    ("interface/resources/sounds/hello.wav", connect, -25.81, 48000, 2),
    ("interface/resources/sounds/goodbye.wav", disconnect, -25.81, 48000, 2),
    ("interface/resources/sounds/ButtonClick.wav", click, -43.68, 48000, 2),
    ("interface/resources/sounds/ButtonHover.wav", hover, -48.01, 48000, 2),
    ("interface/resources/sounds/Gamemaster-Audio-button-click.wav", tablet_click, -37.68, 48000, 2),
    ("interface/resources/sounds/TabletOpen.wav", tablet_open, -43.61, 48000, 2),
    ("interface/resources/sounds/TabletHandsIn.wav", hands_in, -39.68, 48000, 2),
    ("interface/resources/sounds/TabletHandsOut.wav", hands_out, -38.72, 48000, 2),
    ("interface/resources/sounds/snapshot/snap.wav", snapshot, -28.18, 48000, 2),
    ("interface/resources/sounds/keyboardPress.mp3", key_press, -22.56, 44100, 2),
    ("interface/resources/sounds/Body_Hits_Impact.wav", bump, -42.91, 48000, 1),
    ("tivoli-scripts/scripts/src/assets/chat1.wav", chat_a, -31.10, 44100, 2),
    ("tivoli-scripts/scripts/src/assets/chat2.wav", chat_b, -35.52, 44100, 2),
    ("scripts/system/assets/sounds/button-click.wav", click, -43.68, 48000, 2),
    ("scripts/system/assets/sounds/button-hover.wav", hover, -48.01, 48000, 2),
    ("scripts/system/assets/sounds/bubble.wav", space_bubble, -10.83, 48000, 1),
    ("scripts/system/assets/sounds/rezzing.wav", rezzing, -28.58, 48000, 1),
]


def read_audio(path):
    """(stereo float array at its own rate, rate, channels) of a WAV or MP3 (an MP3 decoded by macOS's afconvert)."""
    path, tmp = Path(path), None
    if path.suffix == ".mp3":
        tmp = Path(tempfile.mkdtemp(dir=path.parent))
        subprocess.run(["afconvert", "-f", "WAVE", "-d", "LEI16", str(path), str(tmp / "decoded.wav")], check=True)
        path = tmp / "decoded.wav"
    with wave.open(str(path)) as w:
        ch, rate = w.getnchannels(), w.getframerate()
        x = np.frombuffer(w.readframes(w.getnframes()), "<i2").astype(float).reshape(-1, ch).T / 32768
    if tmp is not None:
        path.unlink()
        tmp.rmdir()
    return x, rate, ch


def loudness(x, rate):
    """The RMS over the loudest 100 ms (5 ms steps; a shorter sound counts as padded with silence), in dBFS."""
    win, hop = int(0.1 * rate), int(0.005 * rate)
    power = np.concatenate([np.mean(x ** 2, axis=0), np.zeros(win)])
    sums = np.concatenate([[0.0], np.cumsum(power)])
    best = max(sums[i + win] - sums[i] for i in range(0, len(power) - win + 1, hop)) / win
    return 10 * np.log10(best + 1e-20)


def write(stereo, path, rate, channels):
    """Write 16-bit PCM WAV at `rate` and `channels` (resampled by macOS's afconvert), or an MP3 through lame."""
    fade = int(0.01 * SR)
    stereo[:, -fade:] *= np.linspace(1, 0, fade)
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(".tmp.wav")
    data = stereo if channels == 2 else stereo.mean(axis=0, keepdims=True)
    with wave.open(str(tmp), "wb") as w:
        w.setnchannels(channels)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes((np.clip(data.T, -1, 1) * 32767).astype("<i2").tobytes())
    if path.suffix == ".mp3":
        subprocess.run(["lame", "--quiet", "--resample", str(rate / 1000), "-b", "128", str(tmp), str(path)], check=True)
        tmp.unlink()
    elif rate != SR:
        subprocess.run(["afconvert", "-f", "WAVE", "-d", f"LEI16@{rate}", "-r", "127", str(tmp), str(path)], check=True)
        tmp.unlink()
    else:
        tmp.replace(path)


def main():
    out = Path(sys.argv[1])
    for repo_path, make, goal, rate, channels in SOUNDS:
        stereo = make()
        stereo *= 10 ** ((goal - loudness(stereo, SR)) / 20)
        write(stereo, out / repo_path, rate, channels)
        n, nrate, nch = read_audio(out / repo_path)
        print(f"{repo_path:60s} {nch}ch {nrate} {n.shape[1] / nrate:.2f} s  loudest 100 ms {loudness(n, nrate):6.1f} dBFS"
              f" (goal {goal:.1f})  peak {20 * np.log10(np.abs(n).max()):6.1f}")


if __name__ == "__main__":
    main()
