// WaveformView.h
#pragma once

#include <JuceHeader.h>
#include <array>
#include <functional>

//============================================================
/**
 * WaveformView
 * ------------
 * Renders an audio waveform thumbnail with a scrolling playhead, zoom support,
 * and click-to-seek functionality.
 *
 * - Mouse scroll: zoom in/out around the playhead.
 * - Mouse click: seeks the deck to the clicked position via the onSeek callback.
 */
class WaveformView : public juce::Component,
    public juce::DragAndDropTarget,
    private juce::ChangeListener
{
public:
    /** Callback fired when a track file path is dropped onto this waveform. */
    std::function<void(const juce::String&)> onFileDrop;

    // ---- DragAndDropTarget ----
    bool isInterestedInDragSource(const SourceDetails& details) override
    {
        return details.description.isString();
    }

    void itemDropped(const SourceDetails& details) override
    {
        dragHover = false;
        repaint();
        if (onFileDrop && details.description.isString())
            onFileDrop(details.description.toString());
    }

    void itemDragEnter(const SourceDetails&) override { dragHover = true;  repaint(); }
    void itemDragExit(const SourceDetails&) override  { dragHover = false; repaint(); }

    /**
     * Constructs a WaveformView using the supplied format manager and thumbnail cache.
     * @param fm    AudioFormatManager used to decode audio for the thumbnail.
     * @param cache Shared thumbnail cache to avoid re-decoding the same file.
     */
    WaveformView(juce::AudioFormatManager& fm, juce::AudioThumbnailCache& cache);

    /**
     * Loads and displays the waveform for the given audio file.
     * Resets the playhead to position 0.
     * @param file The audio file to display.
     */
    void setFile(const juce::File& file);

    /** Clears the displayed waveform and resets the playhead. */
    void clear();

    /**
     * Updates the playhead position drawn on the waveform.
     * @param seconds Playback position in seconds.
     */
    void setPlayheadPositionSeconds(double seconds);

    /**
     * Sets the visible zoom window duration centred around the playhead.
     * Pass 0.0 to show the full track.
     * @param seconds Duration of the visible window in seconds.
     */
    void setZoomWindowSeconds(double seconds);

    /**
     * Sets the colour used to draw the waveform.
     * @param c The new waveform colour.
     */
    void setWaveColour(juce::Colour c) { waveColour = c; repaint(); }

    /**
     * Registers a callback invoked when the user clicks to seek.
     * @param fn Function receiving the target position in seconds.
     */
    void setOnSeek(std::function<void(double)> fn) { onSeek = std::move(fn); }

    /**
     * Updates the hot-cue marker positions drawn on the waveform.
     * @param secs  Time position (seconds) for each of the 8 slots.
     * @param isSet Whether each slot has a cue stored.
     */
    void setHotCues(const std::array<double, 8>& secs, const std::array<bool, 8>& isSet);

    /**
     * Updates the loop region drawn on the waveform.
     * @param inSec   Loop-in time in seconds (-1 if not set).
     * @param outSec  Loop-out time in seconds (-1 if not set).
     * @param enabled Whether the loop is currently active.
     */
    void setLoopRegion(double inSec, double outSec, bool enabled);

    /** Paints the waveform, playhead line, and zoom hint text. */
    void paint(juce::Graphics& g) override;

    /** Handles a click to seek: maps the click x-position to a time in seconds. */
    void mouseDown(const juce::MouseEvent& e) override;

    /** Handles scroll wheel to zoom in/out the visible window. */
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;

    juce::AudioThumbnail thumbnail;
    juce::File currentFile;

    double playheadSeconds = 0.0;
    double zoomWindowSeconds = 0.0;

    juce::Colour waveColour{ juce::Colour(0xFF6EE7FF) };
    std::function<void(double)> onSeek;

    std::array<double, 8> hotCueSecs{};
    std::array<bool,   8> hotCueSet{};

    double loopInSec  = -1.0;
    double loopOutSec = -1.0;
    bool   loopActive = false;
    bool   dragHover  = false;
};
