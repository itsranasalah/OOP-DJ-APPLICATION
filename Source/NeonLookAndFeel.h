// NeonLookAndFeel.h
#pragma once

#include <JuceHeader.h>

//============================================================
/**
 * NeonLookAndFeel
 * ---------------
 * Custom JUCE LookAndFeel that renders all UI controls in a neon/dark DJ aesthetic
 * using pure programmatic drawing — no image assets required.
 *
 * Overrides button backgrounds, button text, rotary knobs, and linear sliders.
 */
class NeonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NeonLookAndFeel();

    /**
     * Draws the background of a TextButton with a rounded dark rectangle
     * and a neon cyan border that brightens on hover/press or when toggled on.
     * @param g                 Graphics context to draw into.
     * @param button            The button being painted.
     * @param backgroundColour  Base fill colour (from the button's colour ID).
     * @param isMouseOverButton True when the cursor is hovering over the button.
     * @param isButtonDown      True while the button is being held down.
     */
    void drawButtonBackground(juce::Graphics& g,
        juce::Button& button,
        const juce::Colour& backgroundColour,
        bool isMouseOverButton,
        bool isButtonDown) override;

    /**
     * Draws the label text on a TextButton in bold white at 12.5pt,
     * centred within the button bounds with a 6px inset.
     * @param g                 Graphics context to draw into.
     * @param button            The button whose text is being drawn.
     * @param isMouseOverButton True when the cursor is hovering.
     * @param isButtonDown      True while the button is held down.
     */
    void drawButtonText(juce::Graphics& g,
        juce::TextButton& button,
        bool isMouseOverButton,
        bool isButtonDown) override;

    /**
     * Draws a rotary (knob) slider as a neon dial: outer ring, dark inner circle,
     * and a glowing needle indicating the current value position.
     * @param g                    Graphics context.
     * @param x,y,width,height     Bounding box for the knob.
     * @param sliderPosProportional Normalised knob position in [0, 1].
     * @param rotaryStartAngle     Start angle in radians.
     * @param rotaryEndAngle       End angle in radians.
     * @param slider               The Slider component being drawn.
     */
    void drawRotarySlider(juce::Graphics& g,
        int x, int y, int width, int height,
        float sliderPosProportional,
        float rotaryStartAngle,
        float rotaryEndAngle,
        juce::Slider& slider) override;

    /**
     * Draws a linear slider (horizontal or vertical) with a rounded dark track,
     * a filled neon fill region, and a circular thumb clamped within the track bounds.
     * @param g              Graphics context.
     * @param x,y,width,height Bounding box.
     * @param sliderPos      Pixel position of the thumb along the track.
     * @param minSliderPos   Pixel position corresponding to the minimum value.
     * @param maxSliderPos   Pixel position corresponding to the maximum value.
     * @param style          Slider style (LinearHorizontal or LinearVertical).
     * @param slider         The Slider component being drawn.
     */
    void drawLinearSlider(juce::Graphics& g,
        int x, int y, int width, int height,
        float sliderPos,
        float minSliderPos,
        float maxSliderPos,
        const juce::Slider::SliderStyle style,
        juce::Slider& slider) override;
};
