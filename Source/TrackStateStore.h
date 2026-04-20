#pragma once
#include <JuceHeader.h>

/**
 * TrackStateStore
 * ---------------
 * Persists per-track state (hot cues + EQ settings) to a JSON file on disk.
 *
 * State is keyed by the track's absolute file path so each track remembers
 * its own settings across application restarts.
 *
 * Usage:
 *   - Call loadFromDisk() once at startup.
 *   - Call saveTrack() whenever a track is unloaded or the app closes.
 *   - Call loadTrack() after a new track is loaded to restore its state.
 *   - Call saveToDisk() on app shutdown (or periodically).
 */
class TrackStateStore
{
public:
    /** Per-track data structure */
    struct TrackState
    {
        // Hot cues: up to 8 time positions in seconds, -1 = unset
        std::array<double, 8> hotCueSeconds{};
        std::array<bool,   8> hotCueSet{};

        // EQ values: 0..1 with 0.5 = neutral
        float eqLow  { 0.5f };
        float eqMid  { 0.5f };
        float eqHigh { 0.5f };

        // Loop points (-1 = not set)
        double loopInSeconds  { -1.0 };
        double loopOutSeconds { -1.0 };
        bool   loopEnabled    { false };

        // Playback speed ratio (1.0 = normal)
        double speedRatio { 1.0 };

        TrackState()
        {
            hotCueSeconds.fill(-1.0);
            hotCueSet.fill(false);
        }
    };

    TrackStateStore();

    /** Load all track states from the JSON file on disk. Call once at startup. */
    void loadFromDisk();

    /** Save all track states to the JSON file on disk. Call on shutdown. */
    void saveToDisk() const;

    /**
     * Save the current state for a specific track.
     * @param filePath  Absolute path to the audio file (used as key).
     * @param state     The TrackState to store.
     */
    void saveTrack(const juce::String& filePath, const TrackState& state);

    /**
     * Load the saved state for a specific track.
     * @param filePath  Absolute path to the audio file.
     * @param state     Output: populated with saved values, or defaults if not found.
     * @return true if a saved state was found, false if defaults were used.
     */
    bool loadTrack(const juce::String& filePath, TrackState& state) const;

    /** Returns the file used for persistence (in the user app data directory). */
    static juce::File getStoreFile();

private:
    // In-memory map: file path -> TrackState
    std::map<juce::String, TrackState> store;
};
