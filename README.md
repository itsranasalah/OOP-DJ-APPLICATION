# 🎧 Retro DJ

A fully custom two-deck DJ application built in **C++17** using the **JUCE 7** framework, developed as the CM2005 Object-Oriented Programming coursework submission.

![Retro DJ Screenshot](docs/screenshot.png)

---

## Features

### Dual Deck Playback
- Load MP3 and WAV files via file browser or drag-and-drop from the library
- Play, pause, and stop with transport controls
- Speed control from −50% to +50% via `ResamplingAudioSource` (pitch-correct)
- Real-time waveform display with **click-to-seek** and **scroll-to-zoom**
- Animated vinyl record spinning at 33⅓ RPM with embedded album art (ID3v2 APIC)
- Elapsed / total time display

### Music Library
- Persistent track library saved to `%APPDATA%\DJApp\music_library.json`
- Displays track name, duration, and detected BPM
- Real-time search filtering by name or BPM
- Drag rows directly onto either deck to load
- Per-row Load A / Load B buttons

### Hot Cues (R3)
- 8 hot cue slots per deck, each a unique colour
- Left-click unset slot → sets cue at current position
- Left-click set slot → jumps to stored position
- Right-click → clears slot
- Coloured flag markers displayed on the waveform
- All cue positions persisted to `%APPDATA%\DJApp\track_states.json` and restored on next load

### Three-Band EQ + DJ Filter (R4)
| Knob | Type | Frequency | Q |
|------|------|-----------|---|
| LOW  | Low-shelf  | 200 Hz  | 0.707 |
| MID  | Peaking bell | 1 kHz | 0.8 |
| HIGH | High-shelf | 6 kHz   | 0.707 |
| FILT | Resonant sweep | 40 Hz – 18 kHz | — |

- **ECHO** — 350 ms circular delay buffer with feedback
- **REVERB** — `juce::Reverb` with manual wet/dry crossfade
- Both effects have independent amount knobs

### Mixer
- Equal-power crossfader: `cos(x·π/2)` / `sin(x·π/2)` curve — no level dip at centre
- Master volume fader
- Headphone cue monitoring with CUE/MIX and VOL controls
- 12-segment RMS level meters on every channel

### BPM Detection & Sync (R5)
- Autocorrelation algorithm on the first 15 seconds of audio
- 50 ms sliding energy envelope → autocorrelation search across 70–180 BPM lags
- Formula: `BPM = 60 × sampleRate ÷ bestLag`
- Result cached in the library; re-analysis skipped on subsequent loads
- **SYNC** button: sets speed ratio to `otherDeckBPM ÷ thisDeckBPM` in one click

### Beat Display & Drum Pads
- 12 colour-coded drum pads in a 3×4 grid
- Each pad triggers an embedded drum sample (kick, snare, hi-hat, percussion)
- Samples compiled into the binary via JUCE BinaryData
- Press = play looped, release = stop
- Visual flash feedback with per-frame brightness decay
- Live BPM comparison: shows difference between decks (or SYNC if within 1 BPM)

### Loop Controls
- Set IN and OUT points at any playback position
- LOOP toggle for seamless looping between the two points
- Loop region visualised on the waveform
- Loop state persisted per track

---

## Architecture

```
Source/
├── Main.cpp                  # JUCE application entry point
├── MainComponent.h/.cpp      # Root UI + audio device, owns everything
├── DeckUI.h                  # Plain widget struct for one deck (no logic)
├── DeckEngine.h/.cpp         # Full audio chain: transport → EQ → effects → metering
├── MixerEngine.h/.cpp        # Crossfader, master gain, headphone cue blend
├── TrackStateStore.h/.cpp    # JSON persistence for hot cues, EQ, loop, speed
├── MusicLibrary.h/.cpp       # Library table, search, JSON persistence
├── WaveformView.h/.cpp       # AudioThumbnail waveform, seek, zoom, markers
├── VinylView.h/.cpp          # Spinning vinyl + ID3v2 album art parser
├── BeatDisplay.h/.cpp        # Drum pads, BPM display, sample playback
├── LevelMeter.h/.cpp         # RMS level meter with traffic-light segments
├── NeonLookAndFeel.h/.cpp    # Custom JUCE LookAndFeel (dark neon theme)
└── KnobSlider.h              # Custom rotary knob with fixed-cursor drag
```

### Audio Processing Chain (per deck)
```
File → AudioFormatReaderSource
     → AudioTransportSource   (play / pause / seek)
     → ResamplingAudioSource  (speed ratio 0.5×–2.0×)
     → Gain trim (dB)
     → IIR EQ (low / mid / high / sweep filter)
     → Echo delay (circular buffer, 350 ms)
     → Reverb (juce::Reverb, wet/dry blend)
     → RMS metering (std::atomic)
     → Pre-fader tap (cue monitoring)
     → Channel fader
     → MixerEngine (crossfader + master)
```

---

## Build Requirements

| Requirement | Version |
|-------------|---------|
| JUCE | 7.x |
| C++ Standard | C++17 |
| Projucer | 7.x |
| Visual Studio | 2022 (Windows) |
| Xcode | 14+ (macOS) |

### Steps

1. Clone the repository
2. Open `DJAPPLICATION.jucer` in Projucer
3. Set your JUCE path in Projucer → Global Paths
4. Save and open the generated IDE project
5. Build in **Release** mode

---

## Persistence

All data is stored in the OS user application data directory:

| File | Contents |
|------|----------|
| `%APPDATA%\DJApp\music_library.json` | Track list (name, path, duration, BPM) |
| `%APPDATA%\DJApp\track_states.json` | Per-track hot cues, EQ, loop points, speed ratio |

---

## Controls Reference

### Per Deck
| Control | Function |
|---------|----------|
| LOAD | Open file browser |
| PLAY / PAUSE | Toggle transport |
| STOP | Stop and return to start |
| SYNC | Match BPM to other deck |
| RESET | Restore all controls to defaults |
| Speed slider | Playback rate −50% to +50% |
| LOW / MID / HIGH | EQ shelf/peak filters |
| FILT | Resonant sweep filter |
| ECHO + AMT | 350 ms delay effect |
| REVERB + AMT | Convolution reverb |
| Pads 1–8 | Hot cue set / jump / clear |
| CLR ALL | Clear all 8 hot cues |
| IN / OUT / LOOP | Loop point controls |
| VOL | Channel fader |
| TRIM | Input gain ±12 dB |

### Mixer
| Control | Function |
|---------|----------|
| Crossfader | Equal-power A/B blend |
| MASTER | Output level |
| CUE/MIX | Headphone cue vs master blend |
| VOL (phones) | Headphone level |

### Waveform
| Interaction | Action |
|-------------|--------|
| Left-click | Seek to position |
| Scroll up | Zoom in |
| Scroll down | Zoom out |
| Drag from library | Load track |

---

## Tech Highlights

- **Thread safety** — all audio-to-UI data transferred via `std::atomic<float>` / `std::atomic<bool>`
- **Equal-power crossfader** — `cos/sin` curve maintains constant power across full throw
- **ID3v2 binary parser** — custom frame-walker extracts APIC album art without external libraries
- **Autocorrelation BPM** — energy envelope + lag search; runs once per file, result cached
- **Circular echo buffer** — fixed 2-second buffer with configurable tap at 350 ms
- **Custom knob** — suppresses JUCE's unbounded mouse movement; delta-based drag, cursor never hidden

---

## License

This project was developed for academic coursework at the University of London. Not licensed for commercial use.
