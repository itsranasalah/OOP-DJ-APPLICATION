// LevelMeter.cpp
#include "LevelMeter.h"

LevelMeter::LevelMeter() { startTimerHz(30); }

void LevelMeter::setLevel(float newLevel)
{
    level.store(juce::jlimit(0.0f, 1.5f, newLevel), std::memory_order_relaxed);
}

void LevelMeter::timerCallback()
{
    smoothLevel = 0.85f * smoothLevel + 0.15f * level.load(std::memory_order_relaxed);
    repaint();
}

void LevelMeter::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF05060A).withAlpha(0.9f));

    auto r = getLocalBounds().toFloat().reduced(3.0f);
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawRoundedRectangle(r, 6.0f, 1.0f);

    const int segments = 12;
    const float segH = r.getHeight() / (float)segments;

    for (int i = 0; i < segments; ++i)
    {
        auto seg = r.removeFromBottom(segH).reduced(2.0f, 1.0f);
        const float threshold = (float)(i + 1) / (float)segments;

        if (smoothLevel >= threshold)
        {
            juce::Colour c = juce::Colour(0xFF00FF99);
            if (threshold > 0.75f) c = juce::Colour(0xFFFFD36E);
            if (threshold > 0.92f) c = juce::Colour(0xFFFF4D6D);
            g.setColour(c.withAlpha(0.85f));
        }
        else
        {
            g.setColour(juce::Colours::white.withAlpha(0.06f));
        }
        g.fillRoundedRectangle(seg, 3.0f);
    }
}
