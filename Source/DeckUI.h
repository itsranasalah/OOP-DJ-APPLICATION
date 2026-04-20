// DeckUI.h
#pragma once

#include <JuceHeader.h>
#include "WaveformView.h"
#include "VinylView.h"
#include "LevelMeter.h"
#include "KnobSlider.h"

//============================================================
/**
 * DeckUI
 * ------
 * Plain aggregate struct holding every UI widget that belongs to one deck:
 * waveform display, vinyl visualiser, level meter, transport buttons,
 * EQ/FX knobs, hot cue pads, loop controls, speed and volume faders.
 *
 * MainComponent owns two instances (left and right) and passes their
 * individual widgets to JUCE's component tree via addAndMakeVisible().
 */
struct DeckUI
{
    juce::Label title, trackName;
    juce::Label timeDisplay;        ///< Elapsed / remaining time readout.

    WaveformView waveform;  ///< Scrollable waveform thumbnail with seek support.
    VinylView vinyl;        ///< Spinning vinyl record with album art.
    LevelMeter meter;       ///< Vertical RMS level meter.

    juce::TextButton loadButton{ "LOAD" };   ///< Opens a file chooser to load a track into this deck.
    juce::TextButton cueButton{ "CUE" };     ///< Toggles pre-fader headphone cue monitoring for this deck.

    juce::TextButton playButton{ "PLAY" };   ///< Toggles playback (play / pause).
    juce::TextButton stopButton{ "STOP" };   ///< Stops playback and resets to start.
    juce::TextButton syncButton{ "SYNC" };   ///< Syncs this deck's BPM to the other deck.
    juce::TextButton resetButton{ "RESET" }; ///< Resets all controls to default values.

    juce::TextButton echoButton{ "ECHO" };     ///< Toggles the echo/delay effect.
    juce::TextButton reverbButton{ "REVERB" }; ///< Toggles the reverb effect.

    juce::Label speedLabel;
    juce::Slider speed;      ///< Pitch/tempo slider (-50% to +50%), LinearHorizontal.

    juce::Label gainTrimLabel;
    KnobSlider gainTrim;     ///< Input gain trim knob (-12 dB to +12 dB), rotary.

    juce::Label volumeLabel;
    juce::Slider volume;     ///< Channel fader (0–1), LinearVertical.

    juce::Label eqLowLabel, eqMidLabel, eqHighLabel, filterLabel;
    KnobSlider eqLow, eqMid, eqHigh, filter; ///< Three-band EQ + DJ filter knobs (0–1, 0.5 = neutral).

    juce::Label echoLabel, reverbLabel;
    KnobSlider echoAmt, reverbAmt; ///< Effect depth knobs (0–1), rotary.

    juce::Label hotLabel;
    /** Hot cue pad buttons 1–8. Left-click sets or jumps; Shift/right-click clears. */
    juce::TextButton hot1{ "1" }, hot2{ "2" }, hot3{ "3" }, hot4{ "4" };
    juce::TextButton hot5{ "5" }, hot6{ "6" }, hot7{ "7" }, hot8{ "8" };
    juce::TextButton clearHots{ "CLR ALL" }; ///< Clears all 8 hot cues at once.

    juce::TextButton loopIn{ "IN" }, loopOut{ "OUT" }, loopToggle{ "LOOP" };

    /**
     * Constructs DeckUI, initialising the WaveformView with shared format
     * manager and thumbnail cache instances owned by MainComponent.
     */
    DeckUI(juce::AudioFormatManager& fm, juce::AudioThumbnailCache& cache)
        : waveform(fm, cache)
    {
    }
};
