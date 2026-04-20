// VinylView.h
#pragma once

#include <JuceHeader.h>

//============================================================
/**
 * VinylView
 * ---------
 * Renders an animated vinyl record that spins at 33⅓ RPM while a track plays.
 * The centre label area shows the track's embedded album art (ID3v2 APIC) if
 * available, or a neon default label otherwise.
 *
 * Rotation is driven by an internal 30 fps timer activated via setPlaying().
 */
class VinylView : public juce::Component,
    private juce::Timer
{
public:
    /** Constructs the VinylView and starts its internal animation timer. */
    VinylView();
    ~VinylView() override;

    /**
     * Loads album art from the given audio file and resets the rotation angle.
     * Currently supports MP3 files with ID3v2 APIC frames (JPEG/PNG embedded art).
     * @param file The audio file to read album art from.
     */
    void setFile(const juce::File& file);

    /** Clears the album art and resets the rotation angle. */
    void clear();

    /**
     * Controls whether the record spins.
     * @param shouldPlay Pass true to spin, false to pause rotation.
     */
    void setPlaying(bool shouldPlay);

    /** Paints the vinyl record, grooves, label circle, and spindle hole. */
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;

    /**
     * Attempts to parse ID3v2 tags in an MP3 file and extract the
     * first embedded APIC (Attached Picture) frame as a JUCE Image.
     * @param file The MP3 file to read.
     * @return A valid Image if album art was found, or an invalid Image otherwise.
     */
    static juce::Image tryLoadAlbumArt(const juce::File& file);

    juce::Image albumArt;
    float angle = 0.0f;
    bool playing = false;
};
