// WaveformView.cpp
#include "WaveformView.h"

// One colour per hot-cue slot (indices 0-7 → buttons 1-8)
static const juce::Colour kHotCueColours[8] = {
    juce::Colour(0xFF4488FF),  // 1 – blue
    juce::Colour(0xFFFF4444),  // 2 – red
    juce::Colour(0xFF44FF88),  // 3 – green
    juce::Colour(0xFFFFDD44),  // 4 – yellow
    juce::Colour(0xFFFF8844),  // 5 – orange
    juce::Colour(0xFFAA44FF),  // 6 – purple
    juce::Colour(0xFF44DDDD),  // 7 – teal
    juce::Colour(0xFFFF44AA),  // 8 – pink
};

WaveformView::WaveformView(juce::AudioFormatManager& fm, juce::AudioThumbnailCache& cache)
    : thumbnail(512, fm, cache)
{
    thumbnail.addChangeListener(this);
}

void WaveformView::setFile(const juce::File& file)
{
    currentFile = file;
    thumbnail.clear();
    if (file.existsAsFile())
        thumbnail.setSource(new juce::FileInputSource(file));
    repaint();
}

void WaveformView::clear()
{
    currentFile = {};
    thumbnail.clear();
    playheadSeconds = 0.0;
    repaint();
}

void WaveformView::setPlayheadPositionSeconds(double seconds)
{
    playheadSeconds = seconds;
    repaint();
}

void WaveformView::setZoomWindowSeconds(double seconds)
{
    zoomWindowSeconds = juce::jmax(0.0, seconds);
    repaint();
}

void WaveformView::setLoopRegion(double inSec, double outSec, bool enabled)
{
    loopInSec  = inSec;
    loopOutSec = outSec;
    loopActive = enabled;
    repaint();
}

void WaveformView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF0B1020).withAlpha(0.75f));

    auto bounds = getLocalBounds().toFloat().reduced(6.0f);
    g.setColour(waveColour.withAlpha(0.18f));
    g.drawRoundedRectangle(bounds, 12.0f, 1.0f);

    const auto totalLen = thumbnail.getTotalLength();

    if (totalLen <= 0.0)
    {
        g.setColour(juce::Colours::white.withAlpha(0.45f));
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawText("Drop / Load a track", bounds.toNearestInt(), juce::Justification::centred);
        return;
    }

    double viewStart = 0.0, viewEnd = totalLen;
    if (zoomWindowSeconds > 0.0 && zoomWindowSeconds < totalLen)
    {
        const double half = zoomWindowSeconds * 0.5;
        viewStart = juce::jlimit(0.0, juce::jmax(0.0, totalLen - zoomWindowSeconds), playheadSeconds - half);
        viewEnd = juce::jlimit(zoomWindowSeconds, totalLen, viewStart + zoomWindowSeconds);
    }

    g.setColour(waveColour.withAlpha(0.75f));
    thumbnail.drawChannel(g, bounds.toNearestInt(), viewStart, viewEnd, 0, 1.0f);

    const double viewLen = viewEnd - viewStart;

    // ---- Loop region overlay ----
    if (loopInSec >= 0.0 && loopOutSec > loopInSec && viewLen > 0.0)
    {
        const float lx = bounds.getX()
            + bounds.getWidth() * (float)juce::jlimit(0.0, 1.0, (loopInSec - viewStart) / viewLen);
        const float rx = bounds.getX()
            + bounds.getWidth() * (float)juce::jlimit(0.0, 1.0, (loopOutSec - viewStart) / viewLen);

        if (rx > lx)
        {
            auto loopRect = juce::Rectangle<float>(lx, bounds.getY(), rx - lx, bounds.getHeight());

            // Filled overlay
            g.setColour(loopActive
                ? juce::Colour(0xFF00FF99).withAlpha(0.15f)
                : juce::Colour(0xFFFFFF88).withAlpha(0.08f));
            g.fillRect(loopRect);

            // Bracket lines
            g.setColour(loopActive
                ? juce::Colour(0xFF00FF99).withAlpha(0.80f)
                : juce::Colour(0xFFFFFF88).withAlpha(0.45f));
            g.drawLine(lx, bounds.getY(), lx, bounds.getBottom(), 2.0f);
            g.drawLine(rx, bounds.getY(), rx, bounds.getBottom(), 2.0f);

            // Top/bottom bracket caps
            const float capW = juce::jmin(6.0f, (rx - lx) * 0.3f);
            g.drawLine(lx, bounds.getY(), lx + capW, bounds.getY(), 2.0f);
            g.drawLine(lx, bounds.getBottom(), lx + capW, bounds.getBottom(), 2.0f);
            g.drawLine(rx, bounds.getY(), rx - capW, bounds.getY(), 2.0f);
            g.drawLine(rx, bounds.getBottom(), rx - capW, bounds.getBottom(), 2.0f);
        }
    }

    // ---- Hot-cue markers ----
    for (int i = 0; i < 8; ++i)
    {
        if (!hotCueSet[i] || hotCueSecs[i] < 0.0) continue;
        if (hotCueSecs[i] < viewStart || hotCueSecs[i] > viewEnd) continue;

        const float cx = bounds.getX()
            + bounds.getWidth() * (float)((hotCueSecs[i] - viewStart) / viewLen);
        const auto col = kHotCueColours[i];

        // Vertical line
        g.setColour(col.withAlpha(0.80f));
        g.drawLine(cx, bounds.getY(), cx, bounds.getBottom(), 1.5f);

        // Filled flag triangle at the top
        juce::Path flag;
        flag.addTriangle(cx,         bounds.getY(),
                         cx + 9.0f,  bounds.getY() + 5.5f,
                         cx,         bounds.getY() + 11.0f);
        g.setColour(col);
        g.fillPath(flag);

        // Cue number inside the flag
        g.setColour(juce::Colours::black.withAlpha(0.85f));
        g.setFont(juce::Font(7.5f, juce::Font::bold));
        g.drawText(juce::String(i + 1),
            juce::Rectangle<float>(cx + 1.0f, bounds.getY() + 0.5f, 8.0f, 10.0f).toNearestInt(),
            juce::Justification::centred);
    }

    // ---- Playhead ----
    const double rel = viewLen > 0.0 ? (playheadSeconds - viewStart) / viewLen : 0.0;
    const float x = bounds.getX() + bounds.getWidth() * (float)juce::jlimit(0.0, 1.0, rel);

    g.setColour(juce::Colour(0xFFFFFF88));
    g.drawLine(x, bounds.getY(), x, bounds.getBottom(), 2.0f);

    g.setColour(juce::Colours::white.withAlpha(0.25f));
    g.setFont(11.0f);
    auto hint = bounds;
    g.drawText("Scroll = zoom, click = seek",
        hint.removeFromBottom(14.0f).toNearestInt(),
        juce::Justification::centredRight);

    // Drag-and-drop hover highlight
    if (dragHover)
    {
        g.setColour(juce::Colour(0xFF6EE7FF).withAlpha(0.25f));
        g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), 10.0f);
        g.setColour(juce::Colour(0xFF6EE7FF).withAlpha(0.70f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), 10.0f, 2.0f);
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText("Drop to load", getLocalBounds(), juce::Justification::centred);
    }
}

void WaveformView::mouseDown(const juce::MouseEvent& e)
{
    const auto totalLen = thumbnail.getTotalLength();
    if (totalLen <= 0.0) return;

    auto bounds = getLocalBounds().toFloat().reduced(6.0f);
    if (!bounds.contains(e.position)) return;

    double viewStart = 0.0, viewEnd = totalLen;
    if (zoomWindowSeconds > 0.0 && zoomWindowSeconds < totalLen)
    {
        const double half = zoomWindowSeconds * 0.5;
        viewStart = juce::jlimit(0.0, juce::jmax(0.0, totalLen - zoomWindowSeconds), playheadSeconds - half);
        viewEnd = juce::jlimit(zoomWindowSeconds, totalLen, viewStart + zoomWindowSeconds);
    }

    const double t = juce::jlimit(0.0, 1.0,
        (double)((e.position.x - bounds.getX()) / bounds.getWidth()));
    if (onSeek) onSeek(viewStart + t * (viewEnd - viewStart));
}

void WaveformView::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const double totalLen = thumbnail.getTotalLength();
    if (totalLen <= 0.0) return;

    const double factor = (wheel.deltaY > 0.0f) ? 0.8 : 1.25;
    double z = (zoomWindowSeconds <= 0.0) ? juce::jmin(20.0, totalLen) : zoomWindowSeconds;
    z = juce::jlimit(5.0, totalLen, z * factor);
    if (z >= totalLen * 0.95) z = 0.0;
    setZoomWindowSeconds(z);
}

void WaveformView::setHotCues(const std::array<double, 8>& secs, const std::array<bool, 8>& isSet)
{
    if (hotCueSecs == secs && hotCueSet == isSet) return;
    hotCueSecs = secs;
    hotCueSet  = isSet;
    repaint();
}

void WaveformView::changeListenerCallback(juce::ChangeBroadcaster*) { repaint(); }
