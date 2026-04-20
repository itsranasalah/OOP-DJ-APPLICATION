// NeonLookAndFeel.cpp
#include "NeonLookAndFeel.h"

NeonLookAndFeel::NeonLookAndFeel()
{
    setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF101A2B));
    setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.92f));
    setColour(juce::Slider::thumbColourId, juce::Colour(0xFF6EE7FF));
    setColour(juce::Slider::trackColourId, juce::Colour(0xFF6EE7FF).withAlpha(0.55f));
    setColour(juce::Slider::backgroundColourId, juce::Colour(0xFF070A12).withAlpha(0.65f));
}

void NeonLookAndFeel::drawButtonBackground(juce::Graphics& g,
    juce::Button& button,
    const juce::Colour& backgroundColour,
    bool isMouseOverButton,
    bool isButtonDown)
{
    auto r = button.getLocalBounds().toFloat().reduced(1.0f);
    auto base = backgroundColour;

    if (button.getToggleState())
        base = juce::Colour(0xFF6EE7FF).withAlpha(0.22f);

    if (isButtonDown)           base = base.darker(0.25f);
    else if (isMouseOverButton) base = base.brighter(0.12f);

    g.setColour(base);
    g.fillRoundedRectangle(r, 10.0f);

    // Cue buttons have a bright identity colour set on buttonColourId; use it for the
    // border so each pad glows in its own hue. Other buttons keep the default cyan border.
    const bool isCueColoured = backgroundColour.getBrightness() > 0.40f;
    const auto borderCol = isCueColoured
                               ? backgroundColour.withAlpha(0.85f)
                               : juce::Colour(0xFF6EE7FF).withAlpha(button.getToggleState() ? 0.65f : 0.18f);
    g.setColour(borderCol);
    g.drawRoundedRectangle(r, 10.0f, 1.5f);
}

void NeonLookAndFeel::drawButtonText(juce::Graphics& g,
    juce::TextButton& button,
    bool /*isMouseOverButton*/,
    bool /*isButtonDown*/)
{
    g.setColour(button.findColour(juce::TextButton::textColourOffId));
    g.setFont(juce::Font(12.5f, juce::Font::bold));
    g.drawFittedText(button.getButtonText(),
        button.getLocalBounds().reduced(6),
        juce::Justification::centred, 1);
}

void NeonLookAndFeel::drawRotarySlider(juce::Graphics& g,
    int x, int y, int width, int height,
    float sliderPosProportional,
    float rotaryStartAngle,
    float rotaryEndAngle,
    juce::Slider& /*slider*/)
{
    auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height).reduced(3.0f);
    auto centre = bounds.getCentre();
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;

    g.setColour(juce::Colour(0xFF6EE7FF).withAlpha(0.12f));
    g.fillEllipse(bounds);
    g.setColour(juce::Colour(0xFF6EE7FF).withAlpha(0.25f));
    g.drawEllipse(bounds, 1.5f);

    auto inner = bounds.reduced(radius * 0.22f);
    g.setColour(juce::Colour(0xFF0B1020).withAlpha(0.90f));
    g.fillEllipse(inner);
    g.setColour(juce::Colour(0xFF6EE7FF).withAlpha(0.22f));
    g.drawEllipse(inner, 1.0f);

    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    juce::Path needle;
    needle.addRectangle(-1.6f, -radius * 0.82f, 3.2f, radius * 0.82f);
    g.setColour(juce::Colour(0xFF6EE7FF).withAlpha(0.85f));
    g.fillPath(needle, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));

    g.setColour(juce::Colour(0xFF6EE7FF).withAlpha(0.35f));
    g.fillEllipse(centre.x - 4.0f, centre.y - 4.0f, 8.0f, 8.0f);
}

void NeonLookAndFeel::drawLinearSlider(juce::Graphics& g,
    int x, int y, int width, int height,
    float sliderPos,
    float /*minSliderPos*/,
    float /*maxSliderPos*/,
    const juce::Slider::SliderStyle style,
    juce::Slider& slider)
{
    // Vertical faders get tighter inset so compact deck faders still have room for segments
    const float inset = (style == juce::Slider::LinearVertical) ? 2.0f : 4.0f;
    auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height).reduced(inset);

    if (style == juce::Slider::LinearVertical)
    {
        // ── Segmented LED fader ──────────────────────────────────────────
        // Dark background panel
        g.setColour(juce::Colour(0xFF05060A).withAlpha(0.92f));
        g.fillRoundedRectangle(bounds, 8.0f);
        g.setColour(juce::Colour(0xFF6EE7FF).withAlpha(0.18f));
        g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

        // Adaptive segment count: ensure each segment is at least 6 px tall
        // so they stay visible on compact deck faders (~50 px) and look great
        // on the taller master fader (~200 px).
        const int   numSegments = juce::jlimit(4, 22, (int)(bounds.getHeight() / 6));
        const float segH        = bounds.getHeight() / (float)numSegments;
        const float vGap        = juce::jmin(1.5f, segH * 0.22f);  // proportional gap
        const float proportion  = juce::jlimit(0.0f, 1.0f,
            (bounds.getBottom() - sliderPos) / bounds.getHeight());
        const int   numLit      = juce::roundToInt(proportion * (float)numSegments);

        for (int i = 0; i < numSegments; ++i)
        {
            // i = 0 is the bottommost segment
            auto seg = juce::Rectangle<float>(
                bounds.getX(),
                bounds.getBottom() - (float)(i + 1) * segH,
                bounds.getWidth(),
                segH).reduced(2.0f, vGap);

            if (i < numLit)
            {
                // Traffic-light gradient: green → amber → red (same as LevelMeter)
                const float t = (float)i / (float)(numSegments - 1);
                juce::Colour c;
                if      (t < 0.60f) c = juce::Colour(0xFF00FF99);  // green
                else if (t < 0.85f) c = juce::Colour(0xFFFFD36E);  // amber
                else                c = juce::Colour(0xFFFF4D6D);  // red
                g.setColour(c.withAlpha(0.88f));
            }
            else
            {
                g.setColour(juce::Colour(0xFF1A2035).withAlpha(0.90f));
            }
            g.fillRoundedRectangle(seg, 3.0f);
        }

        // Thin white thumb line so the drag position is always visible
        const float thumbY = juce::jlimit(bounds.getY() + 2.0f, bounds.getBottom() - 2.0f, sliderPos);
        g.setColour(juce::Colours::white.withAlpha(0.80f));
        g.fillRoundedRectangle(bounds.getX() + 2.0f, thumbY - 2.5f,
                               bounds.getWidth() - 4.0f, 5.0f, 2.5f);
    }
    else
    {
        // ── Horizontal fader (crossfader, speed) — keep original style ──
        g.setColour(juce::Colour(0xFF0B1020).withAlpha(0.7f));
        g.fillRoundedRectangle(bounds, 8.0f);
        g.setColour(juce::Colour(0xFF6EE7FF).withAlpha(0.28f));
        g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

        auto fill = bounds;
        fill.setWidth(sliderPos - bounds.getX());
        g.setColour(juce::Colour(0xFF6EE7FF).withAlpha(0.65f));
        g.fillRoundedRectangle(fill, 8.0f);

        const float thumbX = juce::jlimit(bounds.getX() + 7.0f, bounds.getRight() - 7.0f, sliderPos);
        g.setColour(slider.findColour(juce::Slider::thumbColourId));
        g.fillEllipse(thumbX - 7.0f, bounds.getCentreY() - 7.0f, 14.0f, 14.0f);
    }
}
