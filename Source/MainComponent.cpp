// MainComponent.cpp
#include "MainComponent.h"
#include <BinaryData.h>

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

//============================================================
// Helper: format seconds as M:SS
juce::String MainComponent::formatTime(double seconds)
{
    if (seconds < 0.0) seconds = 0.0;
    int totalSec = (int)seconds;
    int min = totalSec / 60;
    int sec = totalSec % 60;
    return juce::String(min) + ":" + juce::String(sec).paddedLeft('0', 2);
}

//============================================================
// MainComponent
MainComponent::MainComponent()
{
    setLookAndFeel(&neonLnf);
    formatManager.registerBasicFormats();

    // Title
    appTitle.setText("RETRO DJ", juce::dontSendNotification);
    appTitle.setJustificationType(juce::Justification::centred);
    appTitle.setFont(juce::Font(22.0f, juce::Font::bold));
    appTitle.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.92f));
    addAndMakeVisible(appTitle);

    // Decks
    setupDeck(leftDeckUI, deckEngine[Left], "DECK A", leftAccent);
    setupDeck(rightDeckUI, deckEngine[Right], "DECK B", rightAccent);

    // Mixer labels + controls
    mixerTitle.setText("MIXER", juce::dontSendNotification);
    mixerTitle.setJustificationType(juce::Justification::centred);
    mixerTitle.setFont(juce::Font(13.0f, juce::Font::bold));
    mixerTitle.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.78f));
    addAndMakeVisible(mixerTitle);

    crossfaderLabel.setText("CROSSFADER", juce::dontSendNotification);
    crossfaderLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    crossfaderLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.70f));
    addAndMakeVisible(crossfaderLabel);

    crossfader.setSliderStyle(juce::Slider::LinearHorizontal);
    crossfader.setRange(0.0, 1.0, 0.001);
    crossfader.setValue(0.5);
    crossfader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    crossfader.addListener(this);
    addAndMakeVisible(crossfader);

    // Phones (headphone) section
    phonesLabel.setText("PHONES", juce::dontSendNotification);
    phonesLabel.setJustificationType(juce::Justification::centred);
    phonesLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    phonesLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.70f));
    addAndMakeVisible(phonesLabel);

    phonesMixLabel.setText("CUE/MIX", juce::dontSendNotification);
    phonesMixLabel.setJustificationType(juce::Justification::centred);
    phonesMixLabel.setFont(juce::Font(10.0f));
    phonesMixLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.55f));
    addAndMakeVisible(phonesMixLabel);

    phonesMix.setSliderStyle(juce::Slider::LinearHorizontal);
    phonesMix.setRange(0.0, 1.0, 0.001);
    phonesMix.setValue(0.5);   // 50 % cue / 50 % master
    phonesMix.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    phonesMix.addListener(this);
    addAndMakeVisible(phonesMix);

    phonesVolLabel.setText("VOL", juce::dontSendNotification);
    phonesVolLabel.setJustificationType(juce::Justification::centred);
    phonesVolLabel.setFont(juce::Font(10.0f));
    phonesVolLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.55f));
    addAndMakeVisible(phonesVolLabel);

    phonesVol.setSliderStyle(juce::Slider::LinearHorizontal);
    phonesVol.setRange(0.0, 1.0, 0.001);
    phonesVol.setValue(0.85);
    phonesVol.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    phonesVol.addListener(this);
    addAndMakeVisible(phonesVol);

    addAndMakeVisible(beatDisplay);
    beatDisplay.onPadClicked  = [this](int padIndex) { beatPlayers[padIndex].trigger(); };
    beatDisplay.onPadReleased = [this](int padIndex) { beatPlayers[padIndex].stop(); };

    masterLabel.setText("MASTER", juce::dontSendNotification);
    masterLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    masterLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.70f));
    addAndMakeVisible(masterLabel);

    masterVolume.setSliderStyle(juce::Slider::LinearVertical);
    masterVolume.setRange(0.0, 1.0, 0.001);
    masterVolume.setValue(0.9);
    masterVolume.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    masterVolume.addListener(this);
    addAndMakeVisible(masterVolume);

    addAndMakeVisible(masterMeter);

    auto setupKnob = [&](juce::Slider& s)
    {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.addListener(this);
        addAndMakeVisible(s);
    };

    // R2: Music library — wire the callback
    musicLibrary.onLoadToDeck = [this](int deck, const juce::String& path)
        {
            loadFilePathForDeck(deck, path);
        };

    musicLibrary.onTrackRemoved = [this](const juce::String& removedPath)
        {
            for (int d = 0; d < 2; ++d)
            {
                if (deckEngine[d].getCurrentFilePath() == removedPath)
                {
                    deckEngine[d].unload();
                    auto& ui = (d == Left) ? leftDeckUI : rightDeckUI;
                    ui.waveform.clear();
                    ui.vinyl.clear();
                    ui.waveform.setPlayheadPositionSeconds(0.0);
                    ui.trackName.setText("No track", juce::dontSendNotification);
                }
            }
        };

    addAndMakeVisible(musicLibrary);

    // Audio setup
    setAudioChannels(0, 2);
    startTimerHz(30);

    // Persistence: load track states and library on startup
    trackStore.loadFromDisk();
    musicLibrary.loadFromDisk();

    // Size the window to fit the user's actual screen.
    {
        const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
        const int screenH   = display ? display->userArea.getHeight() : 900;
        const int screenW   = display ? display->userArea.getWidth()  : 1280;
        setSize(juce::jmin(1280, screenW),
                juce::jmin(900,  screenH - 60));
    }
}

MainComponent::~MainComponent()
{
    auto saveIfLoaded = [&](int deckIndex, DeckUI& ui)
        {
            auto& eng = deckEngine[deckIndex];
            if (!eng.isLoaded()) return;

            TrackStateStore::TrackState state;
            for (int i = 0; i < DeckEngine::kMaxHotCues; ++i)
            {
                state.hotCueSet[i] = eng.hasHotCue(i);
                state.hotCueSeconds[i] = eng.getHotCueSeconds(i);
            }
            state.eqLow  = (float)ui.eqLow.getValue();
            state.eqMid  = (float)ui.eqMid.getValue();
            state.eqHigh = (float)ui.eqHigh.getValue();

            // Persist loop points
            state.loopInSeconds  = eng.getLoopInSeconds();
            state.loopOutSeconds = eng.getLoopOutSeconds();
            state.loopEnabled    = eng.isLoopEnabled();

            // Persist speed ratio
            state.speedRatio = eng.getSpeedRatio();

            trackStore.saveTrack(eng.getCurrentFilePath(), state);
        };

    saveIfLoaded(Left, leftDeckUI);
    saveIfLoaded(Right, rightDeckUI);
    trackStore.saveToDisk();

    musicLibrary.saveToDisk();

    shutdownAudio();
    setLookAndFeel(nullptr);
}

void MainComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient bg(
        juce::Colour(0xFF0B1020), bounds.getCentreX(), 0,
        juce::Colour(0xFF020305), bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill(bg);
    g.fillAll();

    juce::ColourGradient glow(
        juce::Colour(0xFF6EE7FF).withAlpha(0.08f), bounds.getCentreX(), bounds.getY(),
        juce::Colour(0xFF00FF99).withAlpha(0.04f), bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill(glow);
    g.fillRect(bounds);

    g.setColour(juce::Colours::black.withAlpha(0.06f));
    for (int y = 0; y < getHeight(); y += 3)
        g.drawLine(0.0f, (float)y, (float)getWidth(), (float)y);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(16);

    appTitle.setBounds(area.removeFromTop(40));

    // Reserve bottom 160px for the music library
    auto libraryArea = area.removeFromBottom(160).reduced(4);
    musicLibrary.setBounds(libraryArea);

    area.removeFromBottom(4);

    // Decks + mixer in the remaining space
    const int mixerW = 220;
    const int deckW = (area.getWidth() - mixerW) / 2;

    auto leftArea = area.removeFromLeft(deckW).reduced(6);
    auto mixerArea = area.removeFromLeft(mixerW).reduced(6);
    auto rightArea = area.reduced(6);

    layoutDeck(leftDeckUI, leftArea);
    layoutDeck(rightDeckUI, rightArea);

    mixerTitle.setBounds(mixerArea.removeFromTop(22));

    masterLabel.setBounds(mixerArea.removeFromTop(18));
    {
        auto masterRow = mixerArea.removeFromTop(200);
        masterMeter.setBounds(masterRow.removeFromLeft(22).reduced(2, 4));
        {
            auto mv = masterRow.reduced(0, 4);
            const int mvW = juce::jmin(mv.getWidth(), 90);
            masterVolume.setBounds(mv.withSizeKeepingCentre(mvW, mv.getHeight()));
        }
    }

    mixerArea.removeFromTop(6);
    crossfaderLabel.setBounds(mixerArea.removeFromTop(16));
    crossfader.setBounds(mixerArea.removeFromTop(28).reduced(2, 0));

    mixerArea.removeFromTop(6);
    phonesLabel.setBounds(mixerArea.removeFromTop(16));
    phonesMixLabel.setBounds(mixerArea.removeFromTop(14));
    phonesMix.setBounds(mixerArea.removeFromTop(22).reduced(2, 0));
    phonesVolLabel.setBounds(mixerArea.removeFromTop(14));
    phonesVol.setBounds(mixerArea.removeFromTop(22).reduced(2, 0));

    mixerArea.removeFromTop(6);
    beatDisplay.setBounds(mixerArea);
}

void MainComponent::setupDeck(DeckUI& deck, DeckEngine& engine,
    const juce::String& titleText, juce::Colour accent)
{
    deck.title.setText(titleText, juce::dontSendNotification);
    deck.title.setJustificationType(juce::Justification::centred);
    deck.title.setFont(juce::Font(14.0f, juce::Font::bold));
    deck.title.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.86f));
    addAndMakeVisible(deck.title);

    deck.trackName.setText("No track", juce::dontSendNotification);
    deck.trackName.setJustificationType(juce::Justification::centred);
    deck.trackName.setFont(juce::Font(11.0f));
    deck.trackName.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.65f));
    addAndMakeVisible(deck.trackName);

    // Time display label
    deck.timeDisplay.setText("0:00 / 0:00", juce::dontSendNotification);
    deck.timeDisplay.setJustificationType(juce::Justification::centred);
    deck.timeDisplay.setFont(juce::Font(11.0f, juce::Font::bold));
    deck.timeDisplay.setColour(juce::Label::textColourId, accent.withAlpha(0.80f));
    addAndMakeVisible(deck.timeDisplay);

    deck.waveform.setWaveColour(accent);
    deck.waveform.setOnSeek([&engine, &deck](double seconds)
        {
            engine.setPositionSeconds(seconds);
            deck.waveform.setPlayheadPositionSeconds(seconds);
        });

    // Drag-and-drop: load a track dropped onto the waveform
    const int deckIdx = (&deck == &leftDeckUI) ? Left : Right;
    deck.waveform.onFileDrop = [this, deckIdx](const juce::String& path)
        {
            loadFilePathForDeck(deckIdx, path);
        };

    addAndMakeVisible(deck.waveform);

    addAndMakeVisible(deck.vinyl);
    addAndMakeVisible(deck.meter);

    for (auto* b : { &deck.loadButton, &deck.cueButton,
                     &deck.playButton, &deck.stopButton,
                     &deck.echoButton, &deck.reverbButton,
                     &deck.hot1, &deck.hot2, &deck.hot3, &deck.hot4,
                     &deck.hot5, &deck.hot6, &deck.hot7, &deck.hot8,
                     &deck.clearHots,
                     &deck.loopIn, &deck.loopOut, &deck.loopToggle,
                     &deck.syncButton, &deck.resetButton })
    {
        b->addListener(this);
        addAndMakeVisible(*b);
    }

    deck.cueButton.setClickingTogglesState(true);
    deck.echoButton.setClickingTogglesState(true);
    deck.reverbButton.setClickingTogglesState(true);
    deck.loopToggle.setClickingTogglesState(true);

    deck.hotLabel.setText("HOT CUES", juce::dontSendNotification);
    deck.hotLabel.setFont(juce::Font(10.5f, juce::Font::bold));
    deck.hotLabel.setJustificationType(juce::Justification::centredLeft);
    deck.hotLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.65f));
    addAndMakeVisible(deck.hotLabel);

    auto setupKnob = [&](juce::Slider& s)
    {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.addListener(this);
        addAndMakeVisible(s);
    };

    deck.speedLabel.setText("SPEED", juce::dontSendNotification);
    deck.speedLabel.setFont(juce::Font(10.5f, juce::Font::bold));
    deck.speedLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.65f));
    addAndMakeVisible(deck.speedLabel);

    deck.speed.setSliderStyle(juce::Slider::LinearHorizontal);
    deck.speed.setRange(-50.0, 50.0, 1.0);
    deck.speed.setValue(0.0);
    deck.speed.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 18);
    deck.speed.setNumDecimalPlacesToDisplay(0);
    deck.speed.addListener(this);
    addAndMakeVisible(deck.speed);

    deck.gainTrimLabel.setText("TRIM", juce::dontSendNotification);
    deck.gainTrimLabel.setFont(juce::Font(10.5f, juce::Font::bold));
    deck.gainTrimLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.65f));
    addAndMakeVisible(deck.gainTrimLabel);
    deck.gainTrim.setRange(-12.0, 12.0, 0.1);
    deck.gainTrim.setValue(0.0);
    setupKnob(deck.gainTrim);

    deck.volumeLabel.setText("VOL", juce::dontSendNotification);
    deck.volumeLabel.setFont(juce::Font(10.5f, juce::Font::bold));
    deck.volumeLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.65f));
    addAndMakeVisible(deck.volumeLabel);

    deck.volume.setSliderStyle(juce::Slider::LinearVertical);
    deck.volume.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    deck.volume.setRange(0.0, 1.0, 0.001);
    deck.volume.setValue(0.85);
    deck.volume.addListener(this);
    addAndMakeVisible(deck.volume);

    auto setupEqLabel = [&](juce::Label& lab, const juce::String& t)
        {
            lab.setText(t, juce::dontSendNotification);
            lab.setJustificationType(juce::Justification::centred);
            lab.setFont(juce::Font(10.0f, juce::Font::bold));
            lab.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.60f));
            addAndMakeVisible(lab);
        };

    setupEqLabel(deck.eqLowLabel, "LOW");
    setupEqLabel(deck.eqMidLabel, "MID");
    setupEqLabel(deck.eqHighLabel, "HIGH");
    setupEqLabel(deck.filterLabel, "FILT");

    for (auto* s : { &deck.eqLow, &deck.eqMid, &deck.eqHigh, &deck.filter })
    {
        s->setRange(0.0, 1.0, 0.001);
        s->setValue(0.5);
        setupKnob(*s);
    }

    deck.echoLabel.setText("AMT", juce::dontSendNotification);
    deck.echoLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    deck.echoLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.60f));
    deck.echoLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(deck.echoLabel);
    deck.echoAmt.setRange(0.0, 1.0, 0.001);
    deck.echoAmt.setValue(0.0);
    setupKnob(deck.echoAmt);

    deck.reverbLabel.setText("AMT", juce::dontSendNotification);
    deck.reverbLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    deck.reverbLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.60f));
    deck.reverbLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(deck.reverbLabel);
    deck.reverbAmt.setRange(0.0, 1.0, 0.001);
    deck.reverbAmt.setValue(0.0);
    setupKnob(deck.reverbAmt);

    deck.playButton.setClickingTogglesState(true);
    deck.playButton.setToggleState(false, juce::dontSendNotification);
    deck.stopButton.setClickingTogglesState(false);

    updateHotCueLook(deck, engine);
}

void MainComponent::layoutDeck(DeckUI& deck, juce::Rectangle<int> area)
{
    auto r = area;

    deck.title.setBounds(r.removeFromTop(22));
    deck.trackName.setBounds(r.removeFromTop(16));
    deck.timeDisplay.setBounds(r.removeFromTop(16));

    // LOAD and CUE buttons are hidden — loading is handled via the music library
    deck.loadButton.setVisible(false);
    deck.cueButton.setVisible(false);

    // Waveform
    deck.waveform.setBounds(r.removeFromTop(75).reduced(4));

    // ── Fader column: reserve from the full remaining height ─────────────
    const int faderColW = 52;
    auto faderCol = r.removeFromRight(faderColW);

    deck.gainTrimLabel.setBounds(faderCol.removeFromTop(14));
    {
        const int trimSz = juce::jmin(faderCol.getWidth(), 44);
        auto trimArea = faderCol.removeFromTop(trimSz + 6);
        int s = juce::jmin(trimArea.getWidth(), trimArea.getHeight());
        deck.gainTrim.setBounds(juce::Rectangle<int>(0, 0, s, s).withCentre(trimArea.reduced(2).getCentre()));
    }
    deck.volumeLabel.setBounds(faderCol.removeFromTop(14));
    deck.volume.setBounds(faderCol.reduced(6, 2));

    // ── Vinyl + level meter ──────────────────────────────────────────────
    const int vinylSize = juce::jmin(r.getWidth() - 28, 170);
    auto vinylRow = r.removeFromTop(vinylSize + 8);
    deck.vinyl.setBounds(vinylRow.removeFromLeft(vinylRow.getWidth() - 24).reduced(4));
    deck.meter.setBounds(vinylRow.reduced(3, 6));

    // ── Transport buttons ────────────────────────────────────────────────
    auto btnRow = r.removeFromTop(26);
    const int btnW = btnRow.getWidth() / 4;
    deck.playButton.setBounds(btnRow.removeFromLeft(btnW).reduced(2, 1));
    deck.stopButton.setBounds(btnRow.removeFromLeft(btnW).reduced(2, 1));
    deck.syncButton.setBounds(btnRow.removeFromLeft(btnW).reduced(2, 1));
    deck.resetButton.setBounds(btnRow.reduced(2, 1));

    // ── Speed slider ─────────────────────────────────────────────────────
    auto speedRow = r.removeFromTop(22);
    deck.speedLabel.setBounds(speedRow.removeFromLeft(48));
    deck.speed.setBounds(speedRow.reduced(2, 2));

    // ── EQ knobs | FX | HOT CUES | Loop ─────────────────────────────────
    // Strategy: claim the loop row from the BOTTOM first so it is always
    // the last row regardless of how much space the rows above consume.
    auto knobsArea = r;

    auto loopRow = knobsArea.removeFromBottom(26);
    {
        const int lW = loopRow.getWidth() / 3;
        deck.loopIn.setBounds(loopRow.removeFromLeft(lW).reduced(2));
        deck.loopOut.setBounds(loopRow.removeFromLeft(lW).reduced(2));
        deck.loopToggle.setBounds(loopRow.reduced(2));
    }

    // Divide remaining: EQ 33% | FX 33% | HOT label + pads take the rest
    const int totalK    = knobsArea.getHeight();
    const int eqH       = juce::jmax(44, totalK * 33 / 100);
    const int fxH       = juce::jmax(44, totalK * 33 / 100);
    const int hotLabelH = 14;
    const int hotPadH   = juce::jmax(20, (totalK - eqH - fxH - hotLabelH) / 2);

    auto layoutKnobCell = [&](juce::Rectangle<int> cell, juce::Label& lbl, juce::Slider& knob)
        {
            cell = cell.reduced(3);
            lbl.setBounds(cell.removeFromTop(14));
            int s = juce::jmin(cell.getWidth(), cell.getHeight());
            knob.setBounds(juce::Rectangle<int>(0, 0, s, s).withCentre(cell.getCentre()));
        };

    // EQ row
    auto row1 = knobsArea.removeFromTop(eqH);
    const int cellW = row1.getWidth() / 4;
    layoutKnobCell(row1.removeFromLeft(cellW), deck.eqLowLabel,  deck.eqLow);
    layoutKnobCell(row1.removeFromLeft(cellW), deck.eqMidLabel,  deck.eqMid);
    layoutKnobCell(row1.removeFromLeft(cellW), deck.eqHighLabel, deck.eqHigh);
    layoutKnobCell(row1.removeFromLeft(cellW), deck.filterLabel, deck.filter);

    // FX row
    auto row2 = knobsArea.removeFromTop(fxH);
    const int fxW = row2.getWidth() / 2;

    auto eBox = row2.removeFromLeft(fxW).reduced(3);
    deck.echoButton.setBounds(eBox.removeFromTop(22).reduced(1));
    eBox.removeFromTop(3);
    deck.echoLabel.setBounds({});
    {
        int s = juce::jmin(eBox.getWidth(), eBox.getHeight());
        deck.echoAmt.setBounds(juce::Rectangle<int>(0, 0, s, s).withCentre(eBox.reduced(2).getCentre()));
    }

    auto rBox = row2.reduced(3);
    deck.reverbButton.setBounds(rBox.removeFromTop(22).reduced(1));
    rBox.removeFromTop(3);
    deck.reverbLabel.setBounds({});
    {
        int s = juce::jmin(rBox.getWidth(), rBox.getHeight());
        deck.reverbAmt.setBounds(juce::Rectangle<int>(0, 0, s, s).withCentre(rBox.reduced(2).getCentre()));
    }

    // HOT CUES label + two pad rows
    deck.hotLabel.setBounds(knobsArea.removeFromTop(hotLabelH));
    {
        const int padW = knobsArea.getWidth() / 4;

        auto hotRow1 = knobsArea.removeFromTop(hotPadH);
        deck.hot1.setBounds(hotRow1.removeFromLeft(padW).reduced(2));
        deck.hot2.setBounds(hotRow1.removeFromLeft(padW).reduced(2));
        deck.hot3.setBounds(hotRow1.removeFromLeft(padW).reduced(2));
        deck.hot4.setBounds(hotRow1.reduced(2));

        auto hotRow2 = knobsArea.removeFromTop(hotPadH);
        const int padW2 = hotRow2.getWidth() / 5;
        deck.hot5.setBounds(hotRow2.removeFromLeft(padW2).reduced(2));
        deck.hot6.setBounds(hotRow2.removeFromLeft(padW2).reduced(2));
        deck.hot7.setBounds(hotRow2.removeFromLeft(padW2).reduced(2));
        deck.hot8.setBounds(hotRow2.removeFromLeft(padW2).reduced(2));
        deck.clearHots.setBounds(hotRow2.reduced(2));
    }

    // Loop row is already placed at the bottom (claimed above)
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    for (int d = 0; d < 2; ++d)
        deckEngine[d].prepare(sampleRate, samplesPerBlockExpected, 2);

    mixerEngine.prepare(sampleRate, samplesPerBlockExpected, 2);

    // Load all 12 embedded beat files from binary resources
    const char* beatData[12] = {
        BinaryData::beat1_mp3,  BinaryData::beat2_mp3,  BinaryData::beat3_mp3,
        BinaryData::beat4_mp3,  BinaryData::beat5_mp3,  BinaryData::beat6_mp3,
        BinaryData::beat7_mp3,  BinaryData::beat8_mp3,  BinaryData::beat9_mp3,
        BinaryData::beat10_mp3, BinaryData::beat11_mp3, BinaryData::beat12_mp3,
    };
    const int beatSize[12] = {
        BinaryData::beat1_mp3Size,  BinaryData::beat2_mp3Size,  BinaryData::beat3_mp3Size,
        BinaryData::beat4_mp3Size,  BinaryData::beat5_mp3Size,  BinaryData::beat6_mp3Size,
        BinaryData::beat7_mp3Size,  BinaryData::beat8_mp3Size,  BinaryData::beat9_mp3Size,
        BinaryData::beat10_mp3Size, BinaryData::beat11_mp3Size, BinaryData::beat12_mp3Size,
    };
    for (int i = 0; i < 12; ++i)
        beatPlayers[i].load(beatData[i], (size_t)beatSize[i], formatManager);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (bufferToFill.buffer == nullptr) return;

    mixerEngine.process(deckEngine[Left], deckEngine[Right],
        *bufferToFill.buffer, bufferToFill.numSamples);

    for (auto& player : beatPlayers)
        player.addToBuffer(*bufferToFill.buffer, bufferToFill.numSamples);
}

void MainComponent::releaseResources()
{
    for (int d = 0; d < 2; ++d) deckEngine[d].release();
    mixerEngine.release();
}

static DeckEngine& otherDeck(DeckEngine* decks, int deckIndex)
{
    return decks[deckIndex == 0 ? 1 : 0];
}

void MainComponent::buttonClicked(juce::Button* button)
{
    auto handleDeckButtons = [&](int deckIndex, DeckUI& ui) -> bool
        {
            auto& eng = deckEngine[deckIndex];

            if (button == &ui.loadButton)
            {
                loadFileForDeck(deckIndex);
                return true;
            }

            if (button == &ui.cueButton)
            {
                eng.setCueEnabled(ui.cueButton.getToggleState());
                return true;
            }

            if (button == &ui.playButton)
            {
                if (ui.playButton.getToggleState()) eng.play();
                else eng.pause();
                return true;
            }

            if (button == &ui.stopButton)
            {
                stopDeck(deckIndex);
                ui.playButton.setToggleState(false, juce::dontSendNotification);
                return true;
            }

            if (button == &ui.echoButton) { eng.setEchoEnabled(ui.echoButton.getToggleState()); return true; }

            if (button == &ui.reverbButton)
            {
                eng.setReverbEnabled(ui.reverbButton.getToggleState());
                eng.setReverbAmount((float)ui.reverbAmt.getValue());
                return true;
            }

            auto hotClick = [&](int idx, juce::TextButton& b) -> bool
                {
                    if (button != &b) return false;

                    const auto mods = juce::ModifierKeys::getCurrentModifiers();

                    if (mods.isRightButtonDown())
                        eng.clearHotCue(idx);                   // right-click  → clear
                    else if (mods.isShiftDown())
                        eng.setHotCue(idx);                     // shift+click  → update/overwrite
                    else if (!eng.hasHotCue(idx))
                        eng.setHotCue(idx);                     // left-click on unset → set
                    else
                        eng.jumpToHotCue(idx);                  // left-click on set   → jump

                    updateHotCueLook(ui, eng);
                    return true;
                };

            if (hotClick(0, ui.hot1)) return true;
            if (hotClick(1, ui.hot2)) return true;
            if (hotClick(2, ui.hot3)) return true;
            if (hotClick(3, ui.hot4)) return true;
            if (hotClick(4, ui.hot5)) return true;
            if (hotClick(5, ui.hot6)) return true;
            if (hotClick(6, ui.hot7)) return true;
            if (hotClick(7, ui.hot8)) return true;

            if (button == &ui.clearHots)
            {
                for (int i = 0; i < DeckEngine::kMaxHotCues; ++i)
                    eng.clearHotCue(i);
                updateHotCueLook(ui, eng);
                return true;
            }

            if (button == &ui.loopIn) { eng.setLoopIn(); return true; }
            if (button == &ui.loopOut) { eng.setLoopOut(); return true; }

            if (button == &ui.loopToggle)
            {
                eng.setLoopEnabled(ui.loopToggle.getToggleState());
                return true;
            }

            if (button == &ui.syncButton)
            {
                auto& other = otherDeck(deckEngine, deckIndex);
                const double a = eng.getEstimatedBpm();
                const double b = other.getEstimatedBpm();

                if (a > 0.0 && b > 0.0)
                {
                    eng.setSpeedRatio(juce::jlimit(0.5, 2.0, b / a));
                    const double display = (eng.getSpeedRatio() - 1.0) / 0.5 * 50.0;
                    ui.speed.setValue(display, juce::dontSendNotification);
                }
                return true;
            }

            if (button == &ui.resetButton)
            {
                ui.speed.setValue(0.0, juce::sendNotification);
                ui.gainTrim.setValue(0.0, juce::sendNotification);
                ui.volume.setValue(0.85, juce::sendNotification);
                ui.eqLow.setValue(0.5, juce::sendNotification);
                ui.eqMid.setValue(0.5, juce::sendNotification);
                ui.eqHigh.setValue(0.5, juce::sendNotification);
                ui.filter.setValue(0.5, juce::sendNotification);
                ui.echoAmt.setValue(0.0, juce::sendNotification);
                ui.reverbAmt.setValue(0.0, juce::sendNotification);
                ui.echoButton.setToggleState(false, juce::sendNotification);
                ui.reverbButton.setToggleState(false, juce::sendNotification);
                return true;
            }

            return false;
        };

    if (handleDeckButtons(Left, leftDeckUI))  return;
    if (handleDeckButtons(Right, rightDeckUI)) return;
}

void MainComponent::sliderValueChanged(juce::Slider* slider)
{
    auto handleDeckSliders = [&](int deckIndex, DeckUI& ui) -> bool
        {
            auto& eng = deckEngine[deckIndex];

            if (slider == &ui.speed)
            {
                const double ratio = 1.0 + (ui.speed.getValue() / 50.0) * 0.5;
                eng.setSpeedRatio(juce::jlimit(0.5, 2.0, ratio));
                return true;
            }

            if (slider == &ui.gainTrim) { eng.setGainTrimDb((float)ui.gainTrim.getValue()); return true; }
            if (slider == &ui.volume) { eng.setChannelFader((float)ui.volume.getValue()); return true; }
            if (slider == &ui.eqLow) { eng.setEqLow((float)ui.eqLow.getValue()); return true; }
            if (slider == &ui.eqMid) { eng.setEqMid((float)ui.eqMid.getValue()); return true; }
            if (slider == &ui.eqHigh) { eng.setEqHigh((float)ui.eqHigh.getValue()); return true; }
            if (slider == &ui.filter) { eng.setFilter((float)ui.filter.getValue()); return true; }
            if (slider == &ui.echoAmt) { eng.setEchoAmount((float)ui.echoAmt.getValue()); return true; }
            if (slider == &ui.reverbAmt) { eng.setReverbAmount((float)ui.reverbAmt.getValue()); return true; }

            return false;
        };

    if (handleDeckSliders(Left, leftDeckUI))  return;
    if (handleDeckSliders(Right, rightDeckUI)) return;

    if (slider == &crossfader)        mixerEngine.setCrossfader((float)crossfader.getValue());
    else if (slider == &masterVolume) mixerEngine.setMasterGain((float)masterVolume.getValue());
    else if (slider == &phonesMix)    mixerEngine.setPhonesMix((float)phonesMix.getValue());
    else if (slider == &phonesVol)    mixerEngine.setPhonesVolume((float)phonesVol.getValue());
}

//==============================================================================
// Shared helper: load a file path into a deck and restore persisted state
void MainComponent::loadFilePathForDeck(int deck, const juce::String& path)
{
    const juce::File file(path);
    if (!file.existsAsFile()) return;

    // Use cached BPM from library if available — skips expensive audio analysis
    const double cachedBpm = musicLibrary.getCachedBpm(file.getFullPathName());

    if (deckEngine[deck].loadFile(file, cachedBpm))
    {
        auto& ui = (deck == Left) ? leftDeckUI : rightDeckUI;
        auto& eng = deckEngine[deck];

        ui.waveform.setFile(file);
        ui.vinyl.setFile(file);

        // Reset PLAY button to stopped state for the newly loaded track
        ui.playButton.setButtonText("PLAY");
        ui.playButton.setToggleState(false, juce::dontSendNotification);

        const double bpm = eng.getEstimatedBpm();
        ui.trackName.setText(
            bpm > 0.0 ? eng.getTrackName() + " (" + juce::String((int)bpm) + " BPM)"
            : eng.getTrackName(),
            juce::dontSendNotification);

        // Restore persisted state (hot cues, EQ, loop, speed)
        double restoredSpeedSliderVal = 0.0;

        TrackStateStore::TrackState state;
        if (trackStore.loadTrack(file.getFullPathName(), state))
        {
            for (int i = 0; i < DeckEngine::kMaxHotCues; ++i)
                if (state.hotCueSet[i])
                    eng.setHotCueAt(i, state.hotCueSeconds[i]);

            ui.eqLow.setValue(state.eqLow, juce::sendNotification);
            ui.eqMid.setValue(state.eqMid, juce::sendNotification);
            ui.eqHigh.setValue(state.eqHigh, juce::sendNotification);

            // Restore loop points
            if (state.loopInSeconds >= 0.0 && state.loopOutSeconds > state.loopInSeconds)
            {
                eng.setLoopInAt(state.loopInSeconds);
                eng.setLoopOutAt(state.loopOutSeconds);
                if (state.loopEnabled)
                    eng.setLoopEnabled(true);
            }

            // Restore speed
            if (state.speedRatio > 0.0 && state.speedRatio != 1.0)
                restoredSpeedSliderVal = (state.speedRatio - 1.0) * 100.0;
        }

        ui.speed.setValue(restoredSpeedSliderVal, juce::sendNotification);

        updateHotCueLook(ui, eng);
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Load Failed",
            "Could not open \"" + file.getFileName() + "\".\n"
            "The file may be in an unsupported format or could not be read.",
            "OK");
    }
}

void MainComponent::loadFileForDeck(int deck)
{
    const auto wildcard = formatManager.getWildcardForAllFormats();
    chooser = std::make_unique<juce::FileChooser>("Select an audio file...", juce::File{}, wildcard);

    const auto fileFlags = juce::FileBrowserComponent::openMode
        | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync(fileFlags, [this, deck](const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file.existsAsFile())
                loadFilePathForDeck(deck, file.getFullPathName());
        });
}

void MainComponent::startDeck(int deck) { deckEngine[deck].play(); }

void MainComponent::stopDeck(int deck)
{
    deckEngine[deck].stop();
}

void MainComponent::updateHotCueLook(DeckUI& deck, DeckEngine& engine)
{
    juce::TextButton* btns[8] = {
        &deck.hot1, &deck.hot2, &deck.hot3, &deck.hot4,
        &deck.hot5, &deck.hot6, &deck.hot7, &deck.hot8
    };

    std::array<double, 8> secs;
    std::array<bool,   8> isSet;

    for (int i = 0; i < 8; ++i)
    {
        isSet[i] = engine.hasHotCue(i);
        secs[i]  = engine.getHotCueSeconds(i);

        btns[i]->setColour(juce::TextButton::buttonColourId,
                           isSet[i] ? kHotCueColours[i].withAlpha(0.28f)
                                    : juce::Colour(0xFF101A2B));
    }

    deck.waveform.setHotCues(secs, isSet);
}

void MainComponent::timerCallback()
{
    for (int d = 0; d < 2; ++d)
    {
        auto& ui  = (d == Left) ? leftDeckUI : rightDeckUI;
        auto& eng = deckEngine[d];

        const double pos     = eng.getPositionSeconds();
        const double total   = eng.getLengthSeconds();
        const bool   playing = eng.isPlaying();

        ui.waveform.setPlayheadPositionSeconds(pos);
        ui.meter.setLevel(eng.getRmsLevel());
        ui.vinyl.setPlaying(playing);

        // Update time display: elapsed / total
        ui.timeDisplay.setText(formatTime(pos) + " / " + formatTime(total),
                               juce::dontSendNotification);

        // Update loop region on waveform
        ui.waveform.setLoopRegion(eng.getLoopInSeconds(),
                                  eng.getLoopOutSeconds(),
                                  eng.isLoopEnabled());

        // Keep PLAY button in sync with engine state
        ui.playButton.setButtonText(playing ? "PAUSE" : "PLAY");
        ui.playButton.setToggleState(playing, juce::dontSendNotification);
    }

    masterMeter.setLevel(mixerEngine.getMasterRms());

    // Beat display — push fresh BPM + position data every frame
    beatDisplay.update(
        { deckEngine[Left].getEstimatedBpm(),
          deckEngine[Left].getPositionSeconds(),
          deckEngine[Left].isPlaying() },
        { deckEngine[Right].getEstimatedBpm(),
          deckEngine[Right].getPositionSeconds(),
          deckEngine[Right].isPlaying() });
}
