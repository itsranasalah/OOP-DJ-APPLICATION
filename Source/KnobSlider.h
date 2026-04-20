// KnobSlider.h
#pragma once

#include <JuceHeader.h>

//============================================================
/**
 * KnobSlider
 * ----------
 * Rotary knob slider with a fully custom drag handler.
 *
 * JUCE's built-in rotary drag modes call enableUnboundedMouseMovement()
 * internally, which forces the cursor to NoCursor at the platform level and
 * cannot be overridden from outside. The only fix is to never let JUCE's
 * mouseDown run for left-clicks — we compute value changes from raw screen
 * deltas ourselves, so the cursor is never touched.
 *
 * Dragging up or right increases the value; down or left decreases it.
 * 200 px of travel = one full sweep of the knob's range.
 * Right-click / popup-menu events are forwarded to the parent as normal.
 */
class KnobSlider : public juce::Slider
{
public:
    KnobSlider() = default;

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            juce::Slider::mouseDown(e);   // keep right-click context menu
            return;
        }
        dragStartX     = e.getScreenX();
        dragStartY     = e.getScreenY();
        dragStartValue = getValue();
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) return;

        const double range = getMaximum() - getMinimum();
        const double dy    = dragStartY - e.getScreenY();   // up   = positive
        const double dx    = e.getScreenX() - dragStartX;  // right = positive
        const double delta = ((dy + dx) / 200.0) * range;

        setValue(juce::jlimit(getMinimum(), getMaximum(), dragStartValue + delta),
                 juce::sendNotificationSync);
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
            juce::Slider::mouseUp(e);
    }

private:
    int    dragStartX     = 0;
    int    dragStartY     = 0;
    double dragStartValue = 0.0;
};
