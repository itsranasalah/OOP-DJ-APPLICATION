#pragma once
#include <JuceHeader.h>
#include "DeckEngine.h"
#include <atomic>

/**
 * MixerEngine
 * -----------
 * Mixes two DeckEngine instances using an equal-power crossfader and a master gain stage.
 * Also provides DJ cue (headphone) monitoring by blending cue and master signals.
 *
 * With 2 output channels the engine writes a phones blend (Cue/Master mix) to the
 * output so headphone cueing can be tested on speakers.
 * Extending to 4 output channels would allow separate speaker (master L/R)
 * and headphone (phones L/R) routing.
 */
class MixerEngine
{
public:
    /**
     * Prepares the mixer for audio streaming. Must be called before process().
     * @param sampleRate        Output sample rate in Hz.
     * @param samplesPerBlock   Expected audio block size.
     * @param numOutputChannels Number of output channels (typically 2).
     */
    void prepare(double sampleRate, int samplesPerBlock, int numOutputChannels);

    /** Releases internal mix and cue buffers. */
    void release();

    /**
     * Sets the crossfader position controlling the left/right deck balance.
     * Uses an equal-power (sin/cos) curve to avoid level dips at centre.
     * @param x01 Crossfader position in [0, 1]. 0 = full left, 0.5 = centre, 1 = full right.
     */
    void setCrossfader(float x01) { cross = juce::jlimit(0.0f, 1.0f, x01); }

    /**
     * Sets the master output gain applied after mixing both decks.
     * @param g01 Linear gain in [0, 1].
     */
    void setMasterGain(float g01) { master = juce::jlimit(0.0f, 1.0f, g01); }

    /**
     * Sets the phones (headphone) blend between cue and master signals.
     * @param v01 Mix ratio in [0, 1]. 0 = cue-only, 1 = master-only.
     */
    void setPhonesMix(float v01) { phonesMix = juce::jlimit(0.0f, 1.0f, v01); }

    /**
     * Sets the overall headphone output volume.
     * @param v01 Linear gain in [0, 1].
     */
    void setPhonesVolume(float v01) { phonesVol = juce::jlimit(0.0f, 1.0f, v01); }

    /** @return Current crossfader position in [0, 1]. */
    float getCrossfader() const { return cross; }

    /** @return Current master gain in [0, 1]. */
    float getMasterGain() const { return master; }

    /**
     * Renders one audio block from both decks into the output buffer.
     * Pulls post-fader audio from each deck, applies crossfader + master gain,
     * blends in the cue signal for headphone monitoring, and updates master meters.
     * @param left       Left deck engine (Deck A).
     * @param right      Right deck engine (Deck B).
     * @param out        Output buffer to write the final mix into.
     * @param numSamples Number of samples to render.
     */
    void process(DeckEngine& left, DeckEngine& right, juce::AudioBuffer<float>& out, int numSamples);

    /** @return Latest master RMS level in [0, 1.5] (thread-safe). */
    float getMasterRms() const { return masterRms.load(std::memory_order_relaxed); }

    /** @return Latest master peak level in [0, 1.5] (thread-safe). */
    float getMasterPeak() const { return masterPeak.load(std::memory_order_relaxed); }

    /** @return true if the master output clipped in the last audio block (thread-safe). */
    bool  getMasterClip() const { return masterClip.load(std::memory_order_relaxed); }

private:
    int numCh { 2 };
    float cross { 0.5f };
    float master { 0.9f };

    float phonesMix { 0.5f };
    float phonesVol { 0.85f };

    juce::AudioBuffer<float> mixBuffer;
    juce::AudioBuffer<float> cueBuffer;

    std::atomic<float> masterRms { 0.0f };
    std::atomic<float> masterPeak { 0.0f };
    std::atomic<bool>  masterClip { false };

    float leftMixGain() const
    {
        // Equal-power crossfade
        const float x = cross;
        return std::cos(x * juce::MathConstants<float>::halfPi);
    }

    float rightMixGain() const
    {
        const float x = cross;
        return std::sin(x * juce::MathConstants<float>::halfPi);
    }

    void updateMasterMetersFrom(const juce::AudioBuffer<float>& buf, int numSamples);
};
