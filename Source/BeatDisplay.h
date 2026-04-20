// BeatDisplay.h
#pragma once

#include <JuceHeader.h>
#include <functional>

//============================================================
/**
 * BeatDisplay
 * -----------
 * Visualises the beat timing of both decks in the mixer column.
 *
 * For each deck it shows:
 *  - A pulsing circle that flashes on every beat (size / brightness driven
 *    by the beat phase computed from the playhead position and BPM).
 *  - A thin progress bar that sweeps across one beat interval.
 *  - A large BPM number.
 *
 * Between the two decks a centre label shows either "SYNC" (green, when both
 * BPMs are within 1 BPM of each other) or the numeric difference otherwise.
 */
class BeatDisplay : public juce::Component
{
public:
    struct DeckBeat
    {
        double bpm     = 0.0;
        double pos     = 0.0;   ///< Current playhead position in seconds.
        bool   playing = false;
    };

    BeatDisplay() = default;

    /** Call from the 30-fps timer to push new deck state and trigger a repaint. */
    void update(const DeckBeat& a, const DeckBeat& b)
    {
        deckA = a;
        deckB = b;
        if (padFlashBrightness > 0.01f)
            padFlashBrightness *= 0.75f;
        else
            padFlashBrightness = 0.0f;
        repaint();
    }

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp  (const juce::MouseEvent& e) override;

    std::function<void(int)> onPadClicked;
    std::function<void(int)> onPadReleased;

private:
    DeckBeat deckA, deckB;
    int   activePad          = -1;
    float padFlashBrightness = 0.0f;
};
