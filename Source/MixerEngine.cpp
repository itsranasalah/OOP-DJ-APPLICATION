#include "MixerEngine.h"

void MixerEngine::prepare(double /*sampleRate*/, int samplesPerBlock, int numOutputChannels)
{
    numCh = juce::jmax(1, numOutputChannels);
    mixBuffer.setSize(numCh, samplesPerBlock);
    cueBuffer.setSize(numCh, samplesPerBlock);
}

void MixerEngine::release()
{
    mixBuffer.setSize(0, 0);
    cueBuffer.setSize(0, 0);
}

void MixerEngine::process(DeckEngine& left, DeckEngine& right, juce::AudioBuffer<float>& out, int numSamples)
{
    out.clear();
    cueBuffer.setSize(out.getNumChannels(), numSamples, false, false, true);
    cueBuffer.clear();

    // Process each deck exactly ONCE into separate buffers (calling processTo
    // advances the transport read position, so calling it twice would skip audio)
    juce::AudioBuffer<float> leftBuf(out.getNumChannels(), numSamples);
    juce::AudioBuffer<float> rightBuf(out.getNumChannels(), numSamples);
    leftBuf.clear();
    rightBuf.clear();

    left.processTo(leftBuf, numSamples);
    right.processTo(rightBuf, numSamples);

    const float gL = leftMixGain();
    const float gR = rightMixGain();

    for (int ch = 0; ch < out.getNumChannels(); ++ch)
    {
        out.addFrom(ch, 0, leftBuf, ch, 0, numSamples, gL);
        out.addFrom(ch, 0, rightBuf, ch, 0, numSamples, gR);
    }

    // Master gain
    out.applyGain(master);

    // Cue buffer (pre-fader tap from decks that have cue enabled)
    // Only blend cue into output if at least one deck has cue enabled,
    // otherwise the phones mix was silently attenuating the master output.
    left.processCueTo(cueBuffer, numSamples);
    right.processCueTo(cueBuffer, numSamples);

    // Check if any cue signal is actually present
    float cueEnergy = 0.0f;
    for (int ch = 0; ch < cueBuffer.getNumChannels(); ++ch)
        cueEnergy += cueBuffer.getMagnitude(ch, 0, numSamples);

    if (cueEnergy > 0.0001f)
    {
        // Cue is active: blend cue into the output at phonesVol level
        const float mix = phonesMix;
        const float vol = phonesVol;

        for (int ch = 0; ch < out.getNumChannels(); ++ch)
        {
            auto* w = out.getWritePointer(ch);
            auto* c = cueBuffer.getReadPointer(ch);

            for (int i = 0; i < numSamples; ++i)
                w[i] = (1.0f - mix) * vol * c[i] + mix * w[i];
        }
    }
    // If no cue active: master output passes through untouched at full level

    updateMasterMetersFrom(out, numSamples);
}

void MixerEngine::updateMasterMetersFrom(const juce::AudioBuffer<float>& buf, int numSamples)
{
    float rms = 0.0f;
    float peak = 0.0f;

    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        rms = juce::jmax(rms, buf.getRMSLevel(ch, 0, numSamples));
        peak = juce::jmax(peak, buf.getMagnitude(ch, 0, numSamples));
    }

    masterRms.store(rms, std::memory_order_relaxed);
    masterPeak.store(peak, std::memory_order_relaxed);
    masterClip.store(peak >= 0.99f, std::memory_order_relaxed);
}