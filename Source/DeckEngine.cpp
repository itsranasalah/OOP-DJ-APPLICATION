#include "DeckEngine.h"

namespace
{
    double estimateBpmFromMono(const float* x, int n, double fs)
    {
        if (n < 2000) return 0.0;

        // simple energy envelope + autocorrelation
        double mean = 0.0;
        for (int i = 0; i < n; ++i) mean += x[i];
        mean /= (double)n;

        std::vector<float> y(n), env(n);
        for (int i = 0; i < n; ++i) y[i] = x[i] - (float)mean;

        const int win = (int)juce::jmax(1.0, fs * 0.05); // 50ms
        float s = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            s += std::abs(y[i]);
            if (i - win >= 0) s -= std::abs(y[i - win]);
            env[i] = s / (float)win;
        }

        const double minBpm = 70.0, maxBpm = 180.0;
        const int maxLag = (int)std::round(fs * 60.0 / minBpm);
        const int minLag = (int)std::round(fs * 60.0 / maxBpm);

        double best = 0.0;
        int bestLag = 0;

        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            double c = 0.0;
            for (int i = 0; i + lag < n; ++i)
                c += (double)env[i] * (double)env[i + lag];

            if (c > best)
            {
                best = c;
                bestLag = lag;
            }
        }

        if (bestLag <= 0) return 0.0;
        return 60.0 * fs / (double)bestLag;
    }
}

DeckEngine::DeckEngine(juce::AudioFormatManager& fm)
    : formatManager(fm)
{
    hotCueSeconds.fill(0.0);
    hotCueSet.fill(false);

    readAheadThread.startThread();

    reverbParams.roomSize = 0.35f;
    reverbParams.damping = 0.55f;
    reverbParams.wetLevel = 0.0f;
    reverbParams.dryLevel = 1.0f;
    reverb.setParameters(reverbParams);
}

DeckEngine::~DeckEngine()
{
    release();
    readAheadThread.stopThread(1000);
}

void DeckEngine::prepare(double sr, int samplesPerBlock, int numOutputChannels)
{
    sampleRate = sr;
    blockSize = samplesPerBlock;
    numCh = juce::jlimit(1, 2, numOutputChannels);

    workBuffer.setSize(numCh, blockSize);
    preFaderBuffer.setSize(numCh, blockSize);

    // echo buffer: 2 seconds max
    delayBuffer.setSize(numCh, (int)(sampleRate * 2.0));
    delayBuffer.clear();
    delayWritePos = 0;
    delaySamples = (int)(sampleRate * 0.35); // 350ms

    transport.prepareToPlay(blockSize, sampleRate);

    // if resampler exists, prepare it too
    if (resampler)
        resampler->prepareToPlay(blockSize, sampleRate);

    updateFilters();
}

void DeckEngine::release()
{
    transport.stop();
    transport.setSource(nullptr);
    resampler.reset();
    readerSource.reset();
    loaded = false;
}

bool DeckEngine::loadFile(const juce::File& file, double bpmHint)
{
    if (!file.existsAsFile()) return false;

    unload();

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (!reader) return false;

    // Prefer the embedded track title from metadata tags (ID3v2 TIT2, Vorbis TITLE, WAV Title, etc.)
    // Fall back to the filename without extension if no title tag is present.
    {
        auto meta = [&](const juce::String& key) -> juce::String
        {
            return reader->metadataValues.getValue(key, {});
        };

        juce::String title = meta("title");          // most JUCE readers (lowercase)
        if (title.isEmpty()) title = meta("TITLE");  // FLAC / Vorbis comments
        if (title.isEmpty()) title = meta("TIT2");   // raw ID3v2 frame name
        if (title.isEmpty()) title = meta("Title");  // WAV / AIFF INFO chunk

        // Some encoders store "Artist - Title" inside the title tag itself.
        // Strip any leading "Artist - " prefix so only the pure track title is shown.
        if (title.isNotEmpty())
        {
            juce::String artist = meta("artist");
            if (artist.isEmpty()) artist = meta("ARTIST");
            if (artist.isEmpty()) artist = meta("TPE1");
            if (artist.isEmpty()) artist = meta("Author");

            if (artist.isNotEmpty())
            {
                const juce::String prefix = artist.trim() + " - ";
                if (title.startsWith(prefix))
                    title = title.substring(prefix.length());
            }
        }

        trackName = title.isNotEmpty() ? title.trim()
                                       : file.getFileNameWithoutExtension();
    }

    currentFile = file;

    // Store the file sample rate so transport can resample to device rate correctly
    const double fileSampleRate = reader->sampleRate;

    readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader.release(), true);

    // Pass the FILE sample rate here — AudioTransportSource uses this to know
    // the source rate and resamples internally to match the device rate.
    // Passing device sampleRate here was causing speed errors when they differed.
    transport.setSource(readerSource.get(), 32768, &readAheadThread, fileSampleRate, numCh);

    // Reset speed to 1.0 (normal) on every load — no carryover from previous file
    speedRatio = 1.0;

    // Resampler wraps transport for user pitch/speed control only.
    // Ratio 1.0 = no additional pitch change on top of the transport resampling.
    resampler = std::make_unique<juce::ResamplingAudioSource>(&transport, false, numCh);
    resampler->setResamplingRatio(1.0);
    resampler->prepareToPlay(blockSize, sampleRate);

    loaded = true;

    // Use cached BPM from library if available; avoids re-reading the audio file
    estimatedBpm = (bpmHint > 0.0) ? bpmHint : estimateBpmFromFile(file);

    return true;
}

void DeckEngine::unload()
{
    stop();
    transport.setSource(nullptr);
    resampler.reset();
    readerSource.reset();
    loaded = false;
    currentFile = {};
    trackName = {};
    estimatedBpm = 0.0;
    clearLoop();
    hotCueSet.fill(false);

    // Clear echo delay buffer so previous track's tail doesn't bleed into next track
    delayBuffer.clear();
    delayWritePos = 0;
}

void DeckEngine::play() { if (loaded) transport.start(); }
void DeckEngine::pause() { transport.stop(); }
void DeckEngine::stop() { transport.stop(); transport.setPosition(0.0); }

void DeckEngine::setPositionSeconds(double seconds)
{
    transport.setPosition(juce::jlimit(0.0, transport.getLengthInSeconds(), seconds));
}

void DeckEngine::setSpeedRatio(double ratio)
{
    speedRatio = juce::jlimit(0.5, 2.0, ratio);
    if (resampler)
        resampler->setResamplingRatio(speedRatio);
}

void DeckEngine::setChannelFader(float linear01) { channelFader = clamp01(linear01); }
void DeckEngine::setGainTrimDb(float db) { gainTrimDb = juce::jlimit(-12.0f, 12.0f, db); }

void DeckEngine::setEqLow(float v01) { eqLow01 = clamp01(v01); updateFilters(); }
void DeckEngine::setEqMid(float v01) { eqMid01 = clamp01(v01); updateFilters(); }
void DeckEngine::setEqHigh(float v01) { eqHigh01 = clamp01(v01); updateFilters(); }
void DeckEngine::setFilter(float v01) { filter01 = clamp01(v01); updateFilters(); }

void DeckEngine::setEchoEnabled(bool enabled) { echoEnabled = enabled; }
void DeckEngine::setEchoAmount(float v01) { echoAmt01 = clamp01(v01); }

void DeckEngine::setReverbEnabled(bool enabled)
{
    reverbEnabled = enabled;
    setReverbAmount(reverbAmt01);
}

void DeckEngine::setReverbAmount(float v01)
{
    reverbAmt01 = clamp01(v01);
    // wetLevel/dryLevel are now managed inside processTo for proper crossfading.
    // Just store the amount — no need to update params here.
}

bool DeckEngine::hasHotCue(int index) const
{
    if (index < 0 || index >= kMaxHotCues) return false;
    return hotCueSet[(size_t)index];
}

void DeckEngine::setHotCue(int index)
{
    if (index < 0 || index >= kMaxHotCues) return;
    hotCueSeconds[(size_t)index] = getPositionSeconds();
    hotCueSet[(size_t)index] = true;
}

void DeckEngine::clearHotCue(int index)
{
    if (index < 0 || index >= kMaxHotCues) return;
    hotCueSet[(size_t)index] = false;
    hotCueSeconds[(size_t)index] = 0.0;
}

void DeckEngine::jumpToHotCue(int index)
{
    if (!hasHotCue(index)) return;
    setPositionSeconds(hotCueSeconds[(size_t)index]);
}

void DeckEngine::setLoopIn()
{
    loopInSeconds = getPositionSeconds();
    if (loopOutSeconds >= 0.0 && loopOutSeconds <= loopInSeconds)
        loopOutSeconds = -1.0;
}

void DeckEngine::setLoopOut()
{
    loopOutSeconds = getPositionSeconds();
    // Only clear if loop-out is strictly before loop-in (not equal — equal is caught by hasLoopPoints)
    if (loopInSeconds >= 0.0 && loopOutSeconds < loopInSeconds)
        loopOutSeconds = -1.0;
}

void DeckEngine::clearLoop()
{
    loopEnabled = false;
    loopInSeconds = -1.0;
    loopOutSeconds = -1.0;
}

void DeckEngine::setLoopEnabled(bool enabled)
{
    loopEnabled = enabled && hasLoopPoints();
}

void DeckEngine::updateFilters()
{
    const float lowDb = map01ToDb12(eqLow01);
    const float midDb = map01ToDb12(eqMid01);
    const float highDb = map01ToDb12(eqHigh01);

    const float lowGain = dbToGain(lowDb);
    const float midGain = dbToGain(midDb);
    const float highGain = dbToGain(highDb);

    auto lowC = juce::IIRCoefficients::makeLowShelf(sampleRate, 200.0, 0.707, lowGain);
    auto midC = juce::IIRCoefficients::makePeakFilter(sampleRate, 1000.0, 0.8, midGain);
    auto highC = juce::IIRCoefficients::makeHighShelf(sampleRate, 6000.0, 0.707, highGain);

    // DJ filter: center off
    juce::IIRCoefficients djC;
    const float v = filter01;

    if (std::abs(v - 0.5f) < 0.02f)
    {
        djC = juce::IIRCoefficients::makeLowPass(sampleRate, 20000.0);
    }
    else if (v < 0.5f)
    {
        const float t = (0.5f - v) / 0.5f;
        const float fc = juce::jmap(t, 40.0f, 2000.0f);
        djC = juce::IIRCoefficients::makeHighPass(sampleRate, fc);
    }
    else
    {
        const float t = (v - 0.5f) / 0.5f;
        const float fc = juce::jmap(t, 18000.0f, 400.0f);
        djC = juce::IIRCoefficients::makeLowPass(sampleRate, fc);
    }

    for (int ch = 0; ch < 2; ++ch)
    {
        lowF[ch].setCoefficients(lowC);
        midF[ch].setCoefficients(midC);
        highF[ch].setCoefficients(highC);
        djF[ch].setCoefficients(djC);
    }
}

void DeckEngine::processTo(juce::AudioBuffer<float>& outBuffer, int numSamples)
{
    if (!loaded || !resampler) return;

    workBuffer.setSize(outBuffer.getNumChannels(), numSamples, false, false, true);
    workBuffer.clear();

    juce::AudioSourceChannelInfo info(&workBuffer, 0, numSamples);
    resampler->getNextAudioBlock(info);

    // looping (best effort)
    if (loopEnabled && hasLoopPoints())
    {
        const double pos = transport.getCurrentPosition();
        if (pos >= loopOutSeconds)
            transport.setPosition(loopInSeconds);
    }

    // pre-fader trim
    const float trim = dbToGain(gainTrimDb);
    workBuffer.applyGain(trim);

    // EQ + filter
    for (int ch = 0; ch < workBuffer.getNumChannels(); ++ch)
    {
        lowF[ch].processSamples(workBuffer.getWritePointer(ch), numSamples);
        midF[ch].processSamples(workBuffer.getWritePointer(ch), numSamples);
        highF[ch].processSamples(workBuffer.getWritePointer(ch), numSamples);
        djF[ch].processSamples(workBuffer.getWritePointer(ch), numSamples);
    }

    // Echo (simple delay)
    // Always feed the delay buffer so it is primed when echo is switched on.
    {
        const float wet = echoEnabled ? (0.6f * echoAmt01) : 0.0f;
        const float fb = echoEnabled ? (0.25f + 0.45f * echoAmt01) : 0.0f;

        for (int ch = 0; ch < workBuffer.getNumChannels(); ++ch)
        {
            auto* w = workBuffer.getWritePointer(ch);
            auto* d = delayBuffer.getWritePointer(ch);
            int localWrite = delayWritePos;

            for (int i = 0; i < numSamples; ++i)
            {
                const int readPos = (localWrite - delaySamples + delayBuffer.getNumSamples())
                    % delayBuffer.getNumSamples();
                const float delayed = d[readPos];
                const float x = w[i];

                // Mix echo into output only when enabled
                w[i] = x + wet * delayed;

                // Always write into the delay buffer so it stays primed
                d[localWrite] = x + delayed * fb;
                localWrite = (localWrite + 1) % delayBuffer.getNumSamples();
            }
        }
        delayWritePos = (delayWritePos + numSamples) % delayBuffer.getNumSamples();
    }

    // Reverb — proper wet/dry crossfade so knob controls the effect blend,
    // not just volume. At 0: fully dry. At 1: heavy wet mix.
    if (reverbEnabled && reverbAmt01 > 0.001f)
    {
        // Save dry signal before reverb processes it in-place
        juce::AudioBuffer<float> dryBuf(workBuffer.getNumChannels(), numSamples);
        for (int ch = 0; ch < workBuffer.getNumChannels(); ++ch)
            dryBuf.copyFrom(ch, 0, workBuffer, ch, 0, numSamples);

        float* L = workBuffer.getWritePointer(0);
        float* R = workBuffer.getNumChannels() > 1 ? workBuffer.getWritePointer(1) : L;

        // Set reverb to full wet for processing, then blend manually
        reverbParams.wetLevel = 1.0f;
        reverbParams.dryLevel = 0.0f;
        reverb.setParameters(reverbParams);
        reverb.processStereo(L, R, numSamples);

        // Crossfade: dry * (1 - amt) + wet * amt
        const float wetGain = reverbAmt01 * 0.85f;
        const float dryGain = 1.0f - reverbAmt01 * 0.5f;
        for (int ch = 0; ch < workBuffer.getNumChannels(); ++ch)
        {
            auto* w = workBuffer.getWritePointer(ch);
            const auto* d = dryBuf.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
                w[i] = dryGain * d[i] + wetGain * w[i];
        }
    }

    // meters (post-strip, pre-fader)
    updateMetersFrom(workBuffer, numSamples);

    // store pre-fader tap for cue
    preFaderBuffer.setSize(workBuffer.getNumChannels(), numSamples, false, false, true);
    preFaderBuffer.makeCopyOf(workBuffer, true);

    // channel fader
    workBuffer.applyGain(channelFader);

    // sum to output
    for (int ch = 0; ch < outBuffer.getNumChannels(); ++ch)
        outBuffer.addFrom(ch, 0, workBuffer, ch, 0, numSamples);
}

void DeckEngine::processCueTo(juce::AudioBuffer<float>& cueOut, int numSamples)
{
    if (!cueEnabled || !loaded) return;

    const int chs = juce::jmin(cueOut.getNumChannels(), preFaderBuffer.getNumChannels());
    const int n = juce::jmin(numSamples, preFaderBuffer.getNumSamples());

    for (int ch = 0; ch < chs; ++ch)
        cueOut.addFrom(ch, 0, preFaderBuffer, ch, 0, n);
}

void DeckEngine::updateMetersFrom(const juce::AudioBuffer<float>& buf, int numSamples)
{
    float rms = 0.0f;
    float peak = 0.0f;

    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        rms = juce::jmax(rms, buf.getRMSLevel(ch, 0, numSamples));
        peak = juce::jmax(peak, buf.getMagnitude(ch, 0, numSamples));
    }

    rmsLevel.store(rms, std::memory_order_relaxed);
    peakLevel.store(peak, std::memory_order_relaxed);
    clipping.store(peak >= 0.99f, std::memory_order_relaxed);
}

double DeckEngine::estimateBpmFromFile(const juce::File& file) const
{
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (!reader) return 0.0;

    const double srcSR = reader->sampleRate;
    const int srcCh = (int)reader->numChannels;

    const int maxSeconds = 15;
    const int totalSamples = (int)juce::jmin<juce::int64>(reader->lengthInSamples, (juce::int64)(srcSR * maxSeconds));

    if (totalSamples < (int)(srcSR * 4.0)) return 0.0;

    juce::AudioBuffer<float> tmp(srcCh, totalSamples);
    reader->read(&tmp, 0, totalSamples, 0, true, true);

    std::vector<float> mono(totalSamples);
    for (int i = 0; i < totalSamples; ++i)
    {
        float s = 0.0f;
        for (int ch = 0; ch < srcCh; ++ch) s += tmp.getSample(ch, i);
        mono[i] = s / (float)srcCh;
    }

    const double targetFs = 200.0;
    const int stride = (int)juce::jmax(1.0, std::floor(srcSR / targetFs));
    const int n = totalSamples / stride;

    std::vector<float> ds(n);
    for (int i = 0; i < n; ++i) ds[i] = mono[i * stride];

    const double bpm = estimateBpmFromMono(ds.data(), n, srcSR / (double)stride);
    if (bpm < 50.0 || bpm > 220.0) return 0.0;
    return bpm;
}