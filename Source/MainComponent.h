// MainComponent.h
#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include <functional>

#include "DeckEngine.h"
#include "MixerEngine.h"
#include "TrackStateStore.h"
#include "MusicLibrary.h"
#include "NeonLookAndFeel.h"
#include "DeckUI.h"
#include "BeatDisplay.h"

//============================================================
/**
 * MainComponent
 * -------------
 * The root audio + UI component for the Retro DJ application.
 *
 * Responsibilities:
 *  - Owns two DeckEngine instances and routes their output through MixerEngine.
 *  - Owns two DeckUI structs and lays them out alongside a central mixer panel
 *    and a bottom music library panel.
 *  - Applies the NeonLookAndFeel to all child controls.
 *  - Persists track state (hot cues, EQ) via TrackStateStore on shutdown.
 *  - Updates waveform playheads, level meters, and vinyl rotation at 30 fps.
 */
class MainComponent : public juce::AudioAppComponent,
    public juce::Button::Listener,
    public juce::Slider::Listener,
    public juce::DragAndDropContainer,
    private juce::Timer
{
public:
    /** Constructs the component, registers audio formats, sets up all UI controls,
     *  initialises the audio engine, and restores persisted track/library state. */
    MainComponent();

    /** Saves track states and library to disk, shuts down audio, and clears the LookAndFeel. */
    ~MainComponent() override;

    /** Paints the dark gradient background with a neon glow and scanline overlay. */
    void paint(juce::Graphics& g) override;

    /** Lays out the two deck panels, central mixer column, and bottom library panel. */
    void resized() override;

    // ---- AudioAppComponent ----

    /**
     * Called by JUCE before audio streaming begins.
     * Prepares both deck engines and the mixer engine for the given sample rate and block size.
     * @param samplesPerBlockExpected Expected audio block size.
     * @param sampleRate              Sample rate in Hz.
     */
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;

    /**
     * Called on the audio thread to fill each output buffer.
     * Pulls audio from both deck engines through the mixer and writes to the output buffer.
     * @param bufferToFill Output buffer info supplied by JUCE.
     */
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

    /** Releases all audio resources held by the deck and mixer engines. */
    void releaseResources() override;

    // ---- Listeners ----

    /**
     * Handles all button clicks for both decks (transport, FX, hot cues, loops, sync, reset).
     * @param button Pointer to the button that was clicked.
     */
    void buttonClicked(juce::Button* button) override;

    /**
     * Handles slider value changes for both decks (speed, EQ, volume, echo/reverb)
     * and the mixer (crossfader, master, phones).
     * @param slider Pointer to the slider whose value changed.
     */
    void sliderValueChanged(juce::Slider* slider) override;

private:
    enum { Left = 0, Right = 1 };

    /** Fired at 30 fps: updates waveform playheads, level meters, and vinyl spin state. */
    void timerCallback() override;

    /**
     * Initialises all widgets in a DeckUI and registers listeners.
     * Also configures toggle states for play, cue, echo, reverb, and loop buttons.
     * @param deck      The DeckUI struct to configure.
     * @param engine    The DeckEngine that this deck's controls will drive.
     * @param titleText Display name shown above the waveform ("DECK A" / "DECK B").
     * @param accent    Accent colour applied to the waveform and related highlights.
     */
    void setupDeck(DeckUI& deck, DeckEngine& engine, const juce::String& titleText, juce::Colour accent);

    /**
     * Positions all widgets within a DeckUI to fit the given area.
     * Allocates rows top-to-bottom for: title, track name, waveform, vinyl,
     * transport buttons, speed slider, EQ knobs, FX controls, hot cue pads,
     * loop buttons, and the gain trim + volume fader column on the right.
     * @param deck The DeckUI whose widget bounds will be set.
     * @param area Available pixel rectangle for the deck.
     */
    void layoutDeck(DeckUI& deck, juce::Rectangle<int> area);

    /**
     * Opens an async file chooser for the user to pick an audio file,
     * then loads it into the specified deck.
     * @param deck 0 = left deck (A), 1 = right deck (B).
     */
    void loadFileForDeck(int deck);

    /**
     * Loads an audio file by path into a deck engine, updates the waveform
     * and vinyl displays, restores any persisted hot cues and EQ state,
     * and updates the track name label (with BPM if available).
     * @param deck 0 = left deck (A), 1 = right deck (B).
     * @param path Absolute path to the audio file.
     */
    void loadFilePathForDeck(int deck, const juce::String& path);

    /**
     * Starts playback on the specified deck.
     * @param deck 0 = left, 1 = right.
     */
    void startDeck(int deck);

    /**
     * Stops playback on the specified deck and resets the waveform playhead to 0.
     * @param deck 0 = left, 1 = right.
     */
    void stopDeck(int deck);

    /**
     * Refreshes hot-cue button colours and waveform markers for one deck.
     * Call after any cue set/clear/load operation.
     */
    void updateHotCueLook(DeckUI& deck, DeckEngine& engine);

    /**
     * Formats a time in seconds as M:SS string.
     * @param seconds Time to format.
     * @return Formatted string, e.g. "2:05".
     */
    static juce::String formatTime(double seconds);

private:
    NeonLookAndFeel neonLnf;

    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache{ 64 };
    std::unique_ptr<juce::FileChooser> chooser;

    DeckEngine deckEngine[2]{ DeckEngine(formatManager), DeckEngine(formatManager) };
    MixerEngine mixerEngine;

    TrackStateStore trackStore;

    DeckUI leftDeckUI{ formatManager, thumbnailCache };
    DeckUI rightDeckUI{ formatManager, thumbnailCache };

    juce::Label appTitle;

    // Mixer UI
    juce::Label mixerTitle;
    juce::Label crossfaderLabel;
    juce::Slider crossfader;

    juce::Label masterLabel;
    juce::Slider masterVolume;
    LevelMeter masterMeter;

    juce::Label phonesLabel;
    juce::Label phonesMixLabel;
    juce::Slider phonesMix;     ///< Cue/Master blend for headphone output (0 = cue-only, 1 = master-only).
    juce::Label phonesVolLabel;
    juce::Slider phonesVol;     ///< Headphone output volume (0–1).

    // Library
    MusicLibrary musicLibrary{ formatManager };

    BeatDisplay beatDisplay;

    struct BeatPlayer
    {
        juce::AudioBuffer<float> buffer;
        std::atomic<int>         readPos { -1 };

        void load(const void* data, size_t dataSize, juce::AudioFormatManager& fm)
        {
            auto stream = std::make_unique<juce::MemoryInputStream>(data, dataSize, false);
            std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(std::move(stream)));
            if (!reader) return;
            const int n = (int)reader->lengthInSamples;
            buffer.setSize(2, n);
            buffer.clear();
            reader->read(&buffer, 0, n, 0, true, true);
        }

        void trigger()
        {
            looping.store(true,  std::memory_order_relaxed);
            readPos.store(0,     std::memory_order_relaxed);
        }

        void stop()
        {
            looping.store(false, std::memory_order_relaxed);
            readPos.store(-1,    std::memory_order_relaxed);
        }

        void addToBuffer(juce::AudioBuffer<float>& out, int numSamples)
        {
            int pos = readPos.load(std::memory_order_relaxed);
            if (pos < 0 || buffer.getNumSamples() == 0) return;

            const bool loop = looping.load(std::memory_order_relaxed);
            int remaining   = numSamples;
            int outOffset   = 0;

            while (remaining > 0)
            {
                const int toRead = juce::jmin(remaining, buffer.getNumSamples() - pos);
                for (int ch = 0; ch < out.getNumChannels(); ++ch)
                {
                    const int srcCh = juce::jmin(ch, buffer.getNumChannels() - 1);
                    out.addFrom(ch, outOffset, buffer, srcCh, pos, toRead, 1.0f);
                }
                pos       += toRead;
                outOffset += toRead;
                remaining -= toRead;

                if (pos >= buffer.getNumSamples())
                {
                    if (loop) pos = 0;   // seamlessly restart for short clips
                    else      { readPos.store(-1, std::memory_order_relaxed); return; }
                }
            }
            readPos.store(pos, std::memory_order_relaxed);
        }

    private:
        std::atomic<bool> looping { false };
    };
    BeatPlayer beatPlayers[12];

    // Accent colours
    juce::Colour leftAccent{ juce::Colour(0xFF6EE7FF) };
    juce::Colour rightAccent{ juce::Colour(0xFFFF4D6D) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
