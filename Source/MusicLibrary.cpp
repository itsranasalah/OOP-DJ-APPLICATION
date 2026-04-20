// MusicLibrary.cpp
#include "MusicLibrary.h"
#include <cmath>

//==============================================================================
// A small button component embedded in the table's last column for deck loading
class DeckLoadCell : public juce::Component
{
public:
    DeckLoadCell(MusicLibrary& libraryOwner, int rowIn)
        : owner(libraryOwner), row(rowIn)
    {
        deckABtn.setButtonText("A");
        deckBBtn.setButtonText("B");

        // No fake MouseEvent, just do the actual action
        deckABtn.onClick = [this] { owner.loadRowToDeck(row, 0); };
        deckBBtn.onClick = [this] { owner.loadRowToDeck(row, 1); };

        addAndMakeVisible(deckABtn);
        addAndMakeVisible(deckBBtn);
    }

    void setRow(int newRow) { row = newRow; }

    void resized() override
    {
        auto r = getLocalBounds().reduced(2);
        auto half = r.getWidth() / 2;
        deckABtn.setBounds(r.removeFromLeft(half).reduced(1));
        deckBBtn.setBounds(r.reduced(1));
    }

private:
    MusicLibrary& owner;
    int row = -1;
    juce::TextButton deckABtn, deckBBtn;
};

//==============================================================================
MusicLibrary::MusicLibrary(juce::AudioFormatManager& fm)
    : formatManager(fm)
{
    // Title
    titleLabel.setText("MUSIC LIBRARY", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(13.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.85f));
    addAndMakeVisible(titleLabel);

    // Buttons
    addAndMakeVisible(addButton);
    addAndMakeVisible(loadAButton);
    addAndMakeVisible(loadBButton);
    addAndMakeVisible(removeButton);

    addButton.onClick = [this] { addFiles(); };

    loadAButton.onClick = [this]
        {
            const int row = table.getSelectedRow();
            if (row >= 0 && row < (int)filteredIndices.size())
                loadRowToDeck(filteredIndices[(size_t)row], 0);
        };

    loadBButton.onClick = [this]
        {
            const int row = table.getSelectedRow();
            if (row >= 0 && row < (int)filteredIndices.size())
                loadRowToDeck(filteredIndices[(size_t)row], 1);
        };

    removeButton.onClick = [this]
        {
            const int row = table.getSelectedRow();
            if (row >= 0 && row < (int)filteredIndices.size())
            {
                const int realIdx = filteredIndices[(size_t)row];
                const juce::String removedPath = tracks[(size_t)realIdx].filePath;
                tracks.erase(tracks.begin() + realIdx);
                updateFilter();
                saveToDisk();
                if (onTrackRemoved) onTrackRemoved(removedPath);
            }
        };

    // Search box
    searchBox.setTextToShowWhenEmpty("Search tracks...", juce::Colours::white.withAlpha(0.35f));
    searchBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF101A2B));
    searchBox.setColour(juce::TextEditor::textColourId, juce::Colours::white.withAlpha(0.90f));
    searchBox.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xFF6EE7FF).withAlpha(0.25f));
    searchBox.setFont(juce::Font(12.0f));
    searchBox.onTextChange = [this] { updateFilter(); };
    addAndMakeVisible(searchBox);

    // Table setup
    table.setModel(this);
    table.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xFF080C14));
    table.setColour(juce::ListBox::outlineColourId, juce::Colour(0xFF6EE7FF).withAlpha(0.15f));
    table.setOutlineThickness(1);
    table.setRowHeight(24);

    auto& header = table.getHeader();
    header.addColumn("Track", 1, 200, 80, 600, juce::TableHeaderComponent::defaultFlags);
    header.addColumn("Duration", 2, 70, 60, 120, juce::TableHeaderComponent::defaultFlags);
    header.addColumn("BPM", 3, 70, 60, 120, juce::TableHeaderComponent::defaultFlags);
    header.addColumn("Load", 4, 90, 90, 90, juce::TableHeaderComponent::notResizable);
    header.setStretchToFitActive(true);

    addAndMakeVisible(table);

    // Load saved library
    loadFromDisk();
}

MusicLibrary::~MusicLibrary()
{
    saveToDisk();
}

//==============================================================================
void MusicLibrary::resized()
{
    auto r = getLocalBounds().reduced(4);

    titleLabel.setBounds(r.removeFromTop(20));
    r.removeFromTop(2);

    // Button row
    auto btnRow = r.removeFromTop(26);
    addButton.setBounds(btnRow.removeFromLeft(90).reduced(2));
    loadAButton.setBounds(btnRow.removeFromLeft(80).reduced(2));
    loadBButton.setBounds(btnRow.removeFromLeft(80).reduced(2));
    removeButton.setBounds(btnRow.removeFromLeft(80).reduced(2));
    searchBox.setBounds(btnRow.reduced(2));

    r.removeFromTop(2);
    table.setBounds(r);
}

void MusicLibrary::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF060910).withAlpha(0.95f));

    g.setColour(juce::Colour(0xFF6EE7FF).withAlpha(0.12f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 8.0f, 1.0f);
}

//==============================================================================
// TableListBoxModel
int MusicLibrary::getNumRows()
{
    return (int)filteredIndices.size();
}

void MusicLibrary::paintRowBackground(juce::Graphics& g, int rowNumber,
    int /*width*/, int /*height*/, bool rowIsSelected)
{
    if (rowIsSelected)
        g.fillAll(juce::Colour(0xFF6EE7FF).withAlpha(0.18f));
    else if (rowNumber % 2 == 0)
        g.fillAll(juce::Colour(0xFF0A0F1A).withAlpha(0.6f));
    else
        g.fillAll(juce::Colour(0xFF060910).withAlpha(0.6f));
}

void MusicLibrary::paintCell(juce::Graphics& g, int rowNumber, int columnId,
    int width, int height, bool /*rowIsSelected*/)
{
    if (rowNumber < 0 || rowNumber >= (int)filteredIndices.size()) return;
    if (columnId == 4) return; // handled by refreshComponentForCell

    const auto& entry = tracks[(size_t)filteredIndices[(size_t)rowNumber]];

    juce::String text;
    switch (columnId)
    {
    case 1: text = entry.fileName; break;
    case 2: text = entry.duration; break;
    case 3: text = entry.bpm;      break;
    default: break;
    }

    g.setColour(juce::Colours::white.withAlpha(0.82f));
    g.setFont(juce::Font(12.0f));
    g.drawFittedText(text,
        juce::Rectangle<int>(4, 0, width - 8, height),
        juce::Justification::centredLeft, 1);
}

void MusicLibrary::cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& /*e*/)
{
    if (columnId == 4) return;
    if (rowNumber < 0 || rowNumber >= (int)filteredIndices.size()) return;
    loadRowToDeck(filteredIndices[(size_t)rowNumber], 0);
}

juce::Component* MusicLibrary::refreshComponentForCell(int rowNumber, int columnId,
    bool /*isRowSelected*/,
    juce::Component* existingComponentToUpdate)
{
    if (columnId != 4)
    {
        delete existingComponentToUpdate;
        return nullptr;
    }

    const int realRow = (rowNumber >= 0 && rowNumber < (int)filteredIndices.size())
                        ? filteredIndices[(size_t)rowNumber] : rowNumber;

    auto* cell = dynamic_cast<DeckLoadCell*>(existingComponentToUpdate);
    if (cell == nullptr)
        cell = new DeckLoadCell(*this, realRow);
    else
        cell->setRow(realRow);

    return cell;
}

//==============================================================================
juce::var MusicLibrary::getDragSourceDescription(const juce::SparseSet<int>& selectedRows)
{
    if (selectedRows.size() == 0) return {};
    const int row = selectedRows[0];
    if (row >= 0 && row < (int)filteredIndices.size())
        return tracks[(size_t)filteredIndices[(size_t)row]].filePath;
    return {};
}

juce::String MusicLibrary::getSelectedFilePath() const
{
    const int row = table.getSelectedRow();
    if (row >= 0 && row < (int)filteredIndices.size())
        return tracks[(size_t)filteredIndices[(size_t)row]].filePath;
    return {};
}

double MusicLibrary::getCachedBpm(const juce::String& filePath) const
{
    for (const auto& t : tracks)
        if (t.filePath == filePath)
            return t.bpmValue;
    return 0.0;
}

void MusicLibrary::loadRowToDeck(int rowNumber, int deck)
{
    if (rowNumber < 0 || rowNumber >= (int)tracks.size()) return;
    if (!onLoadToDeck) return;

    onLoadToDeck(deck, tracks[(size_t)rowNumber].filePath);
}

//==============================================================================
void MusicLibrary::updateFilter()
{
    filteredIndices.clear();
    const auto query = searchBox.getText().trim().toLowerCase();

    for (int i = 0; i < (int)tracks.size(); ++i)
    {
        if (query.isEmpty()
            || tracks[(size_t)i].fileName.toLowerCase().containsIgnoreCase(query)
            || tracks[(size_t)i].bpm.containsIgnoreCase(query))
        {
            filteredIndices.push_back(i);
        }
    }

    table.updateContent();
    table.repaint();
}

void MusicLibrary::addFiles()
{
    const auto wildcard = formatManager.getWildcardForAllFormats();
    chooser = std::make_unique<juce::FileChooser>("Add tracks to library...",
        juce::File{}, wildcard);

    const auto fileFlags = juce::FileBrowserComponent::openMode
        | juce::FileBrowserComponent::canSelectFiles
        | juce::FileBrowserComponent::canSelectMultipleItems;

    chooser->launchAsync(fileFlags, [this](const juce::FileChooser& fc)
        {
            for (const auto& file : fc.getResults())
                addEntry(file);

            updateFilter();
            saveToDisk();
        });
}

void MusicLibrary::addEntry(const juce::File& file)
{
    if (!file.existsAsFile()) return;
    if (alreadyInLibrary(file.getFullPathName())) return;

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (!reader) return;

    TrackEntry entry;
    entry.filePath = file.getFullPathName();
    entry.fileName = file.getFileNameWithoutExtension();

    entry.durationSeconds = (double)reader->lengthInSamples / reader->sampleRate;
    entry.duration = formatDuration(entry.durationSeconds);

    // BPM estimation
    entry.bpmValue = estimateBpm(file);
    entry.bpm = (entry.bpmValue > 0.0)
        ? juce::String((int)std::round(entry.bpmValue))
        : "---";

    tracks.push_back(std::move(entry));
}

bool MusicLibrary::alreadyInLibrary(const juce::String& path) const
{
    for (const auto& t : tracks)
        if (t.filePath == path) return true;
    return false;
}

juce::String MusicLibrary::formatDuration(double seconds)
{
    const int totalSecs = (int)seconds;
    const int mins = totalSecs / 60;
    const int secs = totalSecs % 60;
    return juce::String(mins) + ":" + juce::String(secs).paddedLeft('0', 2);
}

double MusicLibrary::estimateBpm(const juce::File& file) const
{
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (!reader) return 0.0;

    const double srcSR = reader->sampleRate;
    const int    srcCh = (int)reader->numChannels;
    const int    maxSec = 15;

    const int totalSamples = (int)juce::jmin<juce::int64>(
        reader->lengthInSamples,
        (juce::int64)(srcSR * maxSec));

    if (totalSamples < (int)(srcSR * 4.0)) return 0.0;

    juce::AudioBuffer<float> tmp(srcCh, totalSamples);
    reader->read(&tmp, 0, totalSamples, 0, true, true);

    // Mix to mono
    std::vector<float> mono((size_t)totalSamples);
    for (int i = 0; i < totalSamples; ++i)
    {
        float s = 0.0f;
        for (int ch = 0; ch < srcCh; ++ch) s += tmp.getSample(ch, i);
        mono[(size_t)i] = s / (float)srcCh;
    }

    // Downsample to ~200 Hz
    const int stride = (int)juce::jmax(1.0, std::floor(srcSR / 200.0));
    const int n = totalSamples / stride;
    std::vector<float> ds((size_t)n);
    for (int i = 0; i < n; ++i) ds[(size_t)i] = mono[(size_t)(i * stride)];

    // Energy envelope
    const double fs = srcSR / (double)stride;
    const int win = (int)juce::jmax(1.0, fs * 0.05);
    std::vector<float> env((size_t)n);
    float envSum = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        envSum += std::abs(ds[(size_t)i]);
        if (i - win >= 0) envSum -= std::abs(ds[(size_t)(i - win)]);
        env[(size_t)i] = envSum / (float)win;
    }

    // Autocorrelation in BPM range
    const int maxLag = (int)std::round(fs * 60.0 / 70.0);
    const int minLag = (int)std::round(fs * 60.0 / 180.0);

    double best = 0.0;
    int bestLag = 0;
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double c = 0.0;
        for (int i = 0; i + lag < n; ++i)
            c += (double)env[(size_t)i] * (double)env[(size_t)(i + lag)];
        if (c > best) { best = c; bestLag = lag; }
    }

    if (bestLag <= 0) return 0.0;
    const double bpm = 60.0 * fs / (double)bestLag;
    return (bpm >= 50.0 && bpm <= 220.0) ? bpm : 0.0;
}

//==============================================================================
juce::File MusicLibrary::getLibraryFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("DJApp")
        .getChildFile("library.json");
}

void MusicLibrary::loadFromDisk()
{
    tracks.clear();

    const auto file = getLibraryFile();
    if (!file.existsAsFile()) return;

    const auto json = juce::JSON::parse(file.loadFileAsString());
    const auto* arr = json.getArray();
    if (arr == nullptr) return;

    int missingCount = 0;

    for (const auto& item : *arr)
    {
        const auto* obj = item.getDynamicObject();
        if (obj == nullptr) continue;

        const juce::String path = obj->getProperty("path").toString();
        if (path.isEmpty()) continue;

        const juce::File audioFile(path);
        if (!audioFile.existsAsFile())
        {
            ++missingCount;
            continue;
        }

        TrackEntry entry;
        entry.filePath = path;
        entry.fileName = obj->getProperty("name").toString();
        entry.duration = obj->getProperty("duration").toString();
        entry.bpm = obj->getProperty("bpm").toString();
        entry.bpmValue = (double)obj->getProperty("bpmValue");
        entry.durationSeconds = (double)obj->getProperty("durationSeconds");

        tracks.push_back(std::move(entry));
    }

    updateFilter();

    // Warn user about missing files (post async so the UI is fully initialised first)
    if (missingCount > 0)
    {
        const juce::String msg = juce::String(missingCount)
            + (missingCount == 1 ? " track was" : " tracks were")
            + " removed from the library because the file"
            + (missingCount == 1 ? " could" : "s could")
            + " not be found on disk.";

        juce::MessageManager::callAsync([msg]()
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::InfoIcon,
                    "Library Updated",
                    msg,
                    "OK");
            });
    }
}

void MusicLibrary::saveToDisk() const
{
    juce::Array<juce::var> arr;

    for (const auto& entry : tracks)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("path", entry.filePath);
        obj->setProperty("name", entry.fileName);
        obj->setProperty("duration", entry.duration);
        obj->setProperty("bpm", entry.bpm);
        obj->setProperty("bpmValue", entry.bpmValue);
        obj->setProperty("durationSeconds", entry.durationSeconds);
        arr.add(juce::var(obj.release()));
    }

    const auto file = getLibraryFile();
    file.getParentDirectory().createDirectory();
    file.replaceWithText(juce::JSON::toString(juce::var(arr), true));
}