// BeatDisplay.cpp
#include "BeatDisplay.h"

void BeatDisplay::paint(juce::Graphics& g)
{
    // Twelve distinct colours — full spectrum, one per beat pad
    static const juce::Colour kPad[12] = {
        juce::Colour(0xFFFF3355),  //  1 red
        juce::Colour(0xFFFF6633),  //  2 orange-red
        juce::Colour(0xFFFFAA00),  //  3 amber
        juce::Colour(0xFFFFEE33),  //  4 yellow
        juce::Colour(0xFF99FF33),  //  5 lime
        juce::Colour(0xFF33FF88),  //  6 green
        juce::Colour(0xFF00FFCC),  //  7 teal
        juce::Colour(0xFF33FFFF),  //  8 cyan
        juce::Colour(0xFF33BBFF),  //  9 sky blue
        juce::Colour(0xFF3366FF),  // 10 blue
        juce::Colour(0xFFAA33FF),  // 11 purple
        juce::Colour(0xFFFF33CC),  // 12 pink
    };

    auto b = getLocalBounds().toFloat().reduced(3.0f);

    // Dark panel background
    g.setColour(juce::Colour(0xFF050810));
    g.fillRoundedRectangle(b, 8.0f);
    g.setColour(juce::Colour(0xFF6EE7FF).withAlpha(0.08f));
    g.drawRoundedRectangle(b, 8.0f, 1.0f);

    b = b.reduced(4.0f, 3.0f);

    // ---- BPM header row -------------------------------------------
    auto headerRow = b.removeFromTop(15.0f);
    g.setFont(juce::Font(9.5f, juce::Font::bold));

    const auto aStr = deckA.bpm > 1.0 ? juce::String(juce::roundToInt(deckA.bpm)) : "---";
    const auto bStr = deckB.bpm > 1.0 ? juce::String(juce::roundToInt(deckB.bpm)) : "---";

    g.setColour(juce::Colour(0xFF6EE7FF).withAlpha(deckA.playing ? 0.90f : 0.30f));
    g.drawText("A  " + aStr, headerRow.removeFromLeft(headerRow.getWidth() * 0.5f).toNearestInt(),
               juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xFFFF4D6D).withAlpha(deckB.playing ? 0.90f : 0.30f));
    g.drawText(bStr + "  B", headerRow.toNearestInt(), juce::Justification::centredRight);

    b.removeFromTop(4.0f);

    // ---- Square 3×4 pad grid, centred in the remaining area ------
    constexpr int   cols = 3, rows = 4;
    constexpr float gap  = 4.0f;
    const float maxPadW = (b.getWidth()  - gap * (cols - 1)) / cols;
    const float maxPadH = (b.getHeight() - gap * (rows - 1)) / rows;
    const float padSize = juce::jmin(maxPadW, maxPadH);
    const float gridW   = cols * padSize + gap * (cols - 1);
    const float gridH   = rows * padSize + gap * (rows - 1);
    const float gridX   = b.getX() + (b.getWidth()  - gridW) * 0.5f;
    const float gridY   = b.getY() + (b.getHeight() - gridH) * 0.5f;

    for (int i = 0; i < 12; ++i)
    {
        const int   col  = i % cols;
        const int   row  = i / cols;
        const float x    = gridX + col * (padSize + gap);
        const float y    = gridY + row * (padSize + gap);
        const auto  pad  = juce::Rectangle<float>(x, y, padSize, padSize);
        const auto  col_ = kPad[i];

        const float flash = (i == activePad) ? padFlashBrightness : 0.0f;

        if (flash > 0.01f)
        {
            // Glow halo
            g.setColour(col_.withAlpha(flash * 0.35f));
            g.fillRoundedRectangle(pad.expanded(3.0f), 7.0f);

            // Bright filled pad
            g.setColour(col_.withAlpha(0.25f + flash * 0.65f));
            g.fillRoundedRectangle(pad, 6.0f);

            // Bright border
            g.setColour(col_.withAlpha(0.60f + flash * 0.40f));
            g.drawRoundedRectangle(pad, 6.0f, 1.8f);
        }
        else
        {
            // Inactive — dim but always visible
            g.setColour(col_.withAlpha(0.12f));
            g.fillRoundedRectangle(pad, 6.0f);
            g.setColour(col_.withAlpha(0.30f));
            g.drawRoundedRectangle(pad, 6.0f, 1.2f);
        }
    }
}

void BeatDisplay::mouseDown(const juce::MouseEvent& e)
{
    constexpr int   cols = 3, rows = 4;
    constexpr float gap  = 4.0f;

    auto b = getLocalBounds().toFloat().reduced(3.0f).reduced(4.0f, 3.0f);
    b.removeFromTop(19.0f);   // skip BPM header + spacer

    const float maxPadW = (b.getWidth()  - gap * (cols - 1)) / cols;
    const float maxPadH = (b.getHeight() - gap * (rows - 1)) / rows;
    const float padSize = juce::jmin(maxPadW, maxPadH);
    const float gridW   = cols * padSize + gap * (cols - 1);
    const float gridH   = rows * padSize + gap * (rows - 1);
    const float gridX   = b.getX() + (b.getWidth()  - gridW) * 0.5f;
    const float gridY   = b.getY() + (b.getHeight() - gridH) * 0.5f;

    const float mx = (float)e.x;
    const float my = (float)e.y;

    for (int i = 0; i < 12; ++i)
    {
        const int   col = i % cols;
        const int   row = i / cols;
        const float x   = gridX + col * (padSize + gap);
        const float y   = gridY + row * (padSize + gap);

        if (juce::Rectangle<float>(x, y, padSize, padSize).contains(mx, my))
        {
            activePad          = i;
            padFlashBrightness = 1.0f;
            repaint();
            if (onPadClicked) onPadClicked(i);
            return;
        }
    }
}

void BeatDisplay::mouseUp(const juce::MouseEvent& /*e*/)
{
    if (activePad >= 0 && onPadReleased)
        onPadReleased(activePad);
}
