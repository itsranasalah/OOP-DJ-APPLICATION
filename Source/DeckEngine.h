#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <array>

/**
 * DeckEngine
 * ----------
 * Self-contained audio playback engine for one DJ deck.
 *
 * Features:
 *  - Transport: AudioTransportSource with positionable play/pause/seek.
 *  - Speed control: ResamplingAudioSource wrapping the transport (0.5×–2.0× ratio).
 *  - Three-band EQ + DJ resonant filter using juce::IIRFilter.
 *  - Echo (delay line) and Reverb (juce::Reverb) insert effects.
 *  - Up to 8 hot cue points that can be set, jumped to, or cleared.
 *  - Loop in/out points with enable/disable toggle.
 *  - Post-fader RMS/peak metering (thread-safe atomics).
 *  - Best-effort BPM estimation from the loaded file's metadata.
 */
class DeckEngine
{
public:
    /** Maximum number of hot cue slots supported per deck. */
    static constexpr int kMaxHotCues = 8;

    /**
     * Constructs a DeckEngine that will use the given format manager to open audio files.
     * @param fm Shared AudioFormatManager (must outlive this DeckEngine).
     */
    explicit DeckEngine(juce::AudioFormatManager& fm);
    ~DeckEngine();

    /**
     * Prepares the engine for audio streaming. Must be called before processTo().
     * @param sampleRate       Output sample rate in Hz.
     * @param samplesPerBlock  Expected audio block size.
     * @param numOutputChannels Number of output channels (typically 2).
     */
    void prepare(double sampleRate, int samplesPerBlock, int numOutputChannels);

    /** Releases all audio resources. Call when audio streaming stops. */
    void release();

    /**
     * Loads an audio file into the deck and estimates its BPM.
     * @param file     The audio file to load.
     * @param bpmHint  Optional pre-computed BPM (e.g. from library cache). If > 0,
     *                 the expensive audio-based BPM analysis is skipped.
     * @return true if the file was opened successfully.
     */
    bool loadFile(const juce::File& file, double bpmHint = 0.0);

    /** Unloads the current track and resets the transport. */
    void unload();

    /** @return true if a track is currently loaded. */
    bool isLoaded() const { return loaded; }

    /** @return The display name of the currently loaded track (filename without extension). */
    juce::String getTrackName() const { return trackName; }

    /** @return The absolute file path of the currently loaded track, or empty string if none. */
    juce::String getCurrentFilePath() const { return currentFile.getFullPathName(); }

    /** Starts or resumes playback from the current position. */
    void play();

    /** Pauses playback without resetting the position. */
    void pause();

    /** Stops playback and resets the position to 0. */
    void stop();

    /** @return true if the deck is currently playing. */
    bool isPlaying() const { return transport.isPlaying(); }

    /**
     * Seeks to a specific time position.
     * @param seconds Target position in seconds.
     */
    void setPositionSeconds(double seconds);

    /** @return Current playback position in seconds. */
    double getPositionSeconds() const { return transport.getCurrentPosition(); }

    /** @return Total track duration in seconds, or 0 if no track is loaded. */
    double getLengthSeconds() const { return transport.getLengthInSeconds(); }

    /**
     * Sets the playback speed as a ratio relative to normal speed.
     * @param ratio Speed multiplier clamped to [0.5, 2.0]. 1.0 = normal speed.
     */
    void setSpeedRatio(double ratio);

    /** @return Current speed ratio (1.0 = normal). */
    double getSpeedRatio() const { return speedRatio; }

    /**
     * Sets the post-EQ channel fader level applied before the mixer.
     * @param linear01 Linear gain in [0, 1].
     */
    void setChannelFader(float linear01);

    /** @return Current channel fader value in [0, 1]. */
    float getChannelFader() const { return channelFader; }

    /**
     * Applies an input gain trim in decibels before the EQ stage.
     * @param db Gain in dB, clamped to [-12, +12].
     */
    void setGainTrimDb(float db);

    /** @return Current gain trim in dB. */
    float getGainTrimDb() const { return gainTrimDb; }

    // ---- Three-band EQ + DJ Filter ----
    // All take a normalised value in [0, 1] where 0.5 = neutral (unity gain / no filter).

    /** Sets the low-shelf EQ gain. 0 = cut, 0.5 = flat, 1 = boost. */
    void setEqLow(float v01);

    /** Sets the mid-peak EQ gain. 0 = cut, 0.5 = flat, 1 = boost. */
    void setEqMid(float v01);

    /** Sets the high-shelf EQ gain. 0 = cut, 0.5 = flat, 1 = boost. */
    void setEqHigh(float v01);

    /** Sets the DJ resonant filter sweep. 0 = low-pass, 0.5 = open, 1 = high-pass. */
    void setFilter(float v01);

    // ---- Effects ----

    /**
     * Enables or disables the echo (delay) effect.
     * @param enabled true to activate the echo insert.
     */
    void setEchoEnabled(bool enabled);

    /**
     * Sets the echo/delay wet mix level.
     * @param v01 Wet amount in [0, 1].
     */
    void setEchoAmount(float v01);

    /**
     * Enables or disables the reverb effect.
     * @param enabled true to activate the reverb insert.
     */
    void setReverbEnabled(bool enabled);

    /**
     * Sets the reverb room size / wet mix level.
     * @param v01 Reverb amount in [0, 1].
     */
    void setReverbAmount(float v01);

    // ---- Cue monitoring ----

    /**
     * Enables or disables pre-fader cue monitoring for this deck.
     * @param enabled true to route this deck to the cue (headphone) bus.
     */
    void setCueEnabled(bool enabled) { cueEnabled = enabled; }

    /** @return true if cue monitoring is active. */
    bool isCueEnabled() const { return cueEnabled; }

    // ---- Hot cues ----

    /**
     * @param index Slot index in [0, kMaxHotCues).
     * @return true if a hot cue is set at the given slot.
     */
    bool hasHotCue(int index) const;

    /**
     * Records the current playback position as a hot cue.
     * @param index Slot index in [0, kMaxHotCues).
     */
    void setHotCue(int index);

    /**
     * Clears the hot cue at the given slot.
     * @param index Slot index in [0, kMaxHotCues).
     */
    void clearHotCue(int index);

    /**
     * Seeks the deck to the stored hot cue position.
     * @param index Slot index in [0, kMaxHotCues). Does nothing if not set.
     */
    void jumpToHotCue(int index);

    /**
     * Returns the stored time in seconds for a hot cue, or -1 if not set.
     * Used when saving state to TrackStateStore (R3D persistence).
     */
    double getHotCueSeconds(int index) const
    {
        if (index < 0 || index >= kMaxHotCues) return -1.0;
        return hotCueSet[(size_t)index] ? hotCueSeconds[(size_t)index] : -1.0;
    }

    /**
     * Directly assigns a hot cue at a specific time in seconds.
     * Used when restoring persisted state from TrackStateStore (R3D persistence).
     * @param index  Hot cue slot (0..kMaxHotCues-1)
     * @param seconds  Time position to store
     */
    void setHotCueAt(int index, double seconds)
    {
        if (index < 0 || index >= kMaxHotCues) return;
        hotCueSeconds[(size_t)index] = seconds;
        hotCueSet[(size_t)index] = true;
    }

    // ---- Looping ----

    /** Marks the current playback position as the loop-in (start) point. */
    void setLoopIn();

    /** Marks the current playback position as the loop-out (end) point. */
    void setLoopOut();

    /** Clears both loop-in and loop-out points. */
    void clearLoop();

    /**
     * Enables or disables looping between the stored in/out points.
     * Has no effect if loop points have not been set.
     * @param enabled true to activate looping.
     */
    void setLoopEnabled(bool enabled);

    /** @return true if loop playback is currently active. */
    bool isLoopEnabled() const { return loopEnabled; }

    /** @return true if both loop-in and loop-out points are set and valid. */
    bool hasLoopPoints() const { return loopInSeconds >= 0.0 && loopOutSeconds > loopInSeconds; }

    /** @return Current loop-in time in seconds, or -1 if not set. */
    double getLoopInSeconds()  const { return loopInSeconds; }

    /** @return Current loop-out time in seconds, or -1 if not set. */
    double getLoopOutSeconds() const { return loopOutSeconds; }

    /**
     * Directly assigns the loop-in point (used when restoring persisted state).
     * @param seconds  Time in seconds to set as loop-in.
     */
    void setLoopInAt(double seconds)  { loopInSeconds  = seconds; }

    /**
     * Directly assigns the loop-out point (used when restoring persisted state).
     * @param seconds  Time in seconds to set as loop-out.
     */
    void setLoopOutAt(double seconds) { loopOutSeconds = seconds; }

    // ---- BPM ----

    /**
     * @return Best-effort BPM estimate for the loaded track, or 0 if unavailable.
     *         Derived from file metadata tags (e.g. ID3 TBPM) or a basic analysis.
     */
    double getEstimatedBpm() const { return estimatedBpm; }

    // ---- Audio processing ----

    /**
     * Renders post-fader audio into the output buffer (called by MixerEngine).
     * Applies speed, EQ, filter, echo, reverb, trim, and fader in order.
     * @param outBuffer  Buffer to write rendered audio into.
     * @param numSamples Number of samples to render.
     */
    void processTo(juce::AudioBuffer<float>& outBuffer, int numSamples);

    /**
     * Renders pre-fader audio for cue monitoring (headphone mix).
     * Same processing chain as processTo() but bypasses the channel fader.
     * @param cueBuffer  Buffer to write cue audio into.
     * @param numSamples Number of samples to render.
     */
    void processCueTo(juce::AudioBuffer<float>& cueBuffer, int numSamples);

    // ---- Metering (thread-safe) ----

    /** @return Latest post-fader RMS level in [0, 1.5]. Updated each audio block. */
    float getRmsLevel() const { return rmsLevel.load(std::memory_order_relaxed); }

    /** @return Latest post-fader peak level in [0, 1.5]. Updated each audio block. */
    float getPeakLevel() const { return peakLevel.load(std::memory_order_relaxed); }

    /** @return true if the output signal exceeded 0 dBFS in the last audio block. */
    bool  getClipping() const { return clipping.load(std::memory_order_relaxed); }

private:
    juce::AudioFormatManager& formatManager;

    juce::AudioTransportSource transport;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;

    // speed control: resampler wraps transport
    std::unique_ptr<juce::ResamplingAudioSource> resampler;

    juce::TimeSliceThread readAheadThread{ "DeckReadAhead" };

    juce::File currentFile;
    bool loaded{ false };
    juce::String trackName;

    double sampleRate{ 44100.0 };
    int blockSize{ 512 };
    int numCh{ 2 };

    double speedRatio{ 1.0 };

    float channelFader{ 0.85f };
    float gainTrimDb{ 0.0f };

    float eqLow01{ 0.5f }, eqMid01{ 0.5f }, eqHigh01{ 0.5f };
    float filter01{ 0.5f };

    bool echoEnabled{ false };
    float echoAmt01{ 0.0f };

    bool reverbEnabled{ false };
    float reverbAmt01{ 0.0f };

    bool cueEnabled{ false };

    // Hot cues
    std::array<double, kMaxHotCues> hotCueSeconds{};
    std::array<bool, kMaxHotCues> hotCueSet{};

    // Loop
    bool loopEnabled{ false };
    double loopInSeconds{ -1.0 };
    double loopOutSeconds{ -1.0 };

    // BPM estimate
    double estimatedBpm{ 0.0 };

    // Classic filters per-channel
    juce::IIRFilter lowF[2], midF[2], highF[2], djF[2];
    void updateFilters();

    // Reverb (classic)
    juce::Reverb reverb;
    juce::Reverb::Parameters reverbParams;

    // Echo delay buffer
    juce::AudioBuffer<float> delayBuffer;
    int delayWritePos{ 0 };
    int delaySamples{ 0 };

    // Work buffers
    juce::AudioBuffer<float> workBuffer;
    juce::AudioBuffer<float> preFaderBuffer;

    // Meters
    std::atomic<float> rmsLevel{ 0.0f };
    std::atomic<float> peakLevel{ 0.0f };
    std::atomic<bool>  clipping{ false };
    void updateMetersFrom(const juce::AudioBuffer<float>& buf, int numSamples);

    // Helpers
    static float clamp01(float v) { return juce::jlimit(0.0f, 1.0f, v); }
    static float dbToGain(float db) { return juce::Decibels::decibelsToGain(db); }
    static float map01ToDb12(float v01) { return (v01 - 0.5f) * 24.0f; } // -12..+12

    double estimateBpmFromFile(const juce::File& file) const;
};