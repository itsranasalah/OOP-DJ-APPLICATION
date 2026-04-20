// MusicLibrary.h
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <memory>
#include <functional>

/**
 * MusicLibrary
 * ------------
 * A JUCE Component that displays a searchable, scrollable table of audio tracks
 * and allows the user to load them into Deck A or Deck B.
 *
 * Features:
 *  - Add audio files via a file chooser (Add button).
 *  - Displays track name, duration, and estimated BPM for each entry.
 *  - Double-click a row or use Load A / Load B buttons to send a track to a deck.
 *  - Persists the library list to a JSON file across application restarts.
 *  - Prevents duplicate entries (same absolute file path).
 */
class MusicLibrary : public juce::Component,
    public juce::TableListBoxModel
{
public:
    /**
     * Represents one entry in the music library table.
     */
    struct TrackEntry
    {
        juce::String filePath;      ///< Absolute path to the audio file.
        juce::String fileName;      ///< Display name (filename without extension).
        juce::String duration;      ///< Formatted duration string (e.g. "3:42").
        juce::String bpm;           ///< BPM string (e.g. "128" or "–" if unknown).

        double durationSeconds = 0.0; ///< Raw duration in seconds.
        double bpmValue        = 0.0; ///< Raw BPM value (0 if unavailable).
    };

public:
    /**
     * Constructs the MusicLibrary component with the supplied format manager.
     * @param fm Shared AudioFormatManager used to read metadata from audio files.
     */
    explicit MusicLibrary(juce::AudioFormatManager& fm);
    ~MusicLibrary() override;

    // ---- Component overrides ----

    /** Lays out the title label, toolbar buttons, and track table. */
    void resized() override;

    /** Paints the library panel background. */
    void paint(juce::Graphics& g) override;

    // ---- TableListBoxModel overrides ----

    /** @return The total number of tracks currently in the library. */
    int getNumRows() override;

    /**
     * Paints the background of a single table row (selected / unselected).
     * @param g           Graphics context.
     * @param rowNumber   Zero-based row index.
     * @param width,height Row dimensions in pixels.
     * @param rowIsSelected true if this row is currently selected.
     */
    void paintRowBackground(juce::Graphics& g, int rowNumber,
        int width, int height, bool rowIsSelected) override;

    /**
     * Paints the content of one cell in the library table.
     * Column IDs: 1 = name, 2 = duration, 3 = BPM.
     * @param g           Graphics context.
     * @param rowNumber   Zero-based row index.
     * @param columnId    Column identifier (1–3).
     * @param width,height Cell dimensions in pixels.
     * @param rowIsSelected true if this row is currently selected.
     */
    void paintCell(juce::Graphics& g, int rowNumber, int columnId,
        int width, int height, bool rowIsSelected) override;

    /**
     * Handles a double-click on a table row: loads the track into Deck A by default
     * and fires the onLoadToDeck callback with deckIndex = 0.
     * @param rowNumber Zero-based row index.
     * @param columnId  Column that was double-clicked.
     * @param e         The originating mouse event.
     */
    void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& e) override;

    /**
     * Provides custom components for table cells (Load A / Load B buttons per row).
     * @param rowNumber                Zero-based row index.
     * @param columnId                 Column identifier.
     * @param isRowSelected            true if this row is selected.
     * @param existingComponentToUpdate Previously returned component to reuse, or nullptr.
     * @return Pointer to the component for this cell, or nullptr for text-only cells.
     */
    juce::Component* refreshComponentForCell(int rowNumber, int columnId,
        bool isRowSelected,
        juce::Component* existingComponentToUpdate) override;

    /**
     * Returns a drag description for the selected rows, enabling drag-and-drop
     * from the library table to deck waveforms.
     * @param selectedRows The set of currently selected row indices.
     * @return A var containing the file path of the first selected track.
     */
    juce::var getDragSourceDescription(const juce::SparseSet<int>& selectedRows) override;

    // ---- Public helpers ----

    /**
     * @return The absolute file path of the currently selected row,
     *         or an empty string if no row is selected.
     */
    juce::String getSelectedFilePath() const;

    /**
     * Loads the track at the given row index into the specified deck
     * by firing the onLoadToDeck callback.
     * @param rowNumber Zero-based index into the tracks list.
     * @param deck      0 = Deck A (left), 1 = Deck B (right).
     */
    void loadRowToDeck(int rowNumber, int deck);

    // ---- Persistence ----

    /**
     * Loads the saved library list from the JSON file on disk.
     * Should be called once at application startup.
     */
    void loadFromDisk();

    /**
     * Saves the current library list to a JSON file on disk.
     * Should be called on application shutdown.
     */
    void saveToDisk() const;

    /**
     * Returns the cached BPM for a track already in the library.
     * @param filePath  Absolute file path to look up.
     * @return Cached BPM value, or 0.0 if not found in the library.
     */
    double getCachedBpm(const juce::String& filePath) const;

    // ---- Callback ----

    /**
     * Callback fired when a track should be loaded into a deck.
     * Signature: void(int deckIndex, const juce::String& filePath)
     *   - deckIndex: 0 = Deck A, 1 = Deck B.
     *   - filePath:  Absolute path to the audio file to load.
     */
    std::function<void(int, const juce::String&)> onLoadToDeck;

    /**
     * Called when a track is removed from the library.
     * Signature: void(const juce::String& filePath)
     *   - filePath: Absolute path of the track that was removed.
     */
    std::function<void(const juce::String&)> onTrackRemoved;

private:
    // ---- Internal actions ----

    /** Opens a file chooser and adds selected audio files to the library. */
    void addFiles();

    /**
     * Reads metadata from an audio file and appends a new TrackEntry to the list.
     * @param file The audio file to add.
     */
    void addEntry(const juce::File& file);

    /**
     * Checks whether a file is already present in the library.
     * @param path Absolute file path to check.
     * @return true if the path is already in the library.
     */
    bool alreadyInLibrary(const juce::String& path) const;

    // ---- Utilities ----

    /**
     * Formats a duration in seconds as a "M:SS" string.
     * @param seconds Duration to format.
     * @return Formatted string, e.g. "3:42".
     */
    static juce::String formatDuration(double seconds);

    /**
     * Estimates the BPM of an audio file from its metadata tags.
     * @param file The audio file to analyse.
     * @return BPM value, or 0.0 if it could not be determined.
     */
    double estimateBpm(const juce::File& file) const;

    /** @return The JSON file path used for library persistence. */
    static juce::File getLibraryFile();

private:
    juce::AudioFormatManager& formatManager;

    juce::Label titleLabel;

    juce::TextButton addButton{ "Add" };       ///< Opens file chooser to add tracks.
    juce::TextButton loadAButton{ "Load A" };  ///< Loads selected track into Deck A.
    juce::TextButton loadBButton{ "Load B" };  ///< Loads selected track into Deck B.
    juce::TextButton removeButton{ "Remove" }; ///< Removes the selected track from the library.

    juce::TextEditor searchBox;                ///< Search/filter box for library tracks.

    juce::TableListBox table{ "MusicLibraryTable", this };

    std::vector<TrackEntry> tracks;
    std::vector<int> filteredIndices;  ///< Indices into tracks[] matching current search query.
    std::unique_ptr<juce::FileChooser> chooser;

    /** Rebuilds filteredIndices based on the current search query. */
    void updateFilter();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MusicLibrary)
};
