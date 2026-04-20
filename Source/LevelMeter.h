// LevelMeter.h
#pragma once

#include <JuceHeader.h>
#include <atomic>

//============================================================
/**
 * LevelMeter
 * ----------
 * A vertical segmented RMS level meter with exponential smoothing,
 * colour-coded green / amber / red from bottom to top.
 * Repaints itself at 30 fps via an internal timer.
 */
class LevelMeter : public juce::Component,
    private juce::Timer
{
public:
    /** Constructs the meter and starts the 30 fps repaint timer. */
    LevelMeter();

    /**
     * Sets the latest RMS level to display (thread-safe).
     * @param newLevel Linear RMS level in [0, 1.5] — values above 1.0 indicate clipping.
     */
    void setLevel(float newLevel);

    /** Paints the segmented bar, colouring each segment based on the smoothed level. */
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;

    std::atomic<float> level{ 0.0f };
    float smoothLevel = 0.0f;
};
