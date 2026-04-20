#include "TrackStateStore.h"

TrackStateStore::TrackStateStore() {}

juce::File TrackStateStore::getStoreFile()
{
    // Stored in the OS user application data directory so it persists across runs
    // e.g. on Windows: C:\Users\<user>\AppData\Roaming\DJApp\track_states.json
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("DJApp")
        .getChildFile("track_states.json");
}

void TrackStateStore::loadFromDisk()
{
    store.clear();

    const auto file = getStoreFile();
    if (!file.existsAsFile()) return;

    const auto text = file.loadFileAsString();
    if (text.isEmpty()) return;

    // Parse JSON
    const auto json = juce::JSON::parse(text);
    const auto* obj = json.getDynamicObject();
    if (obj == nullptr) return;

    for (auto& entry : obj->getProperties())
    {
        const juce::String path = entry.name.toString();
        const auto* trackObj = entry.value.getDynamicObject();
        if (trackObj == nullptr) continue;

        TrackState state;

        // Load hot cues
        const auto cueArray = trackObj->getProperty("hotCues");
        if (const auto* arr = cueArray.getArray())
        {
            for (int i = 0; i < juce::jmin(8, arr->size()); ++i)
            {
                const auto* cueObj = arr->getReference(i).getDynamicObject();
                if (cueObj == nullptr) continue;

                const bool isSet = (bool)cueObj->getProperty("set");
                const double secs = (double)cueObj->getProperty("seconds");

                state.hotCueSet[i]     = isSet;
                state.hotCueSeconds[i] = isSet ? secs : -1.0;
            }
        }

        // Load EQ
        state.eqLow  = (float)(double)trackObj->getProperty("eqLow");
        state.eqMid  = (float)(double)trackObj->getProperty("eqMid");
        state.eqHigh = (float)(double)trackObj->getProperty("eqHigh");

        // Guard against corrupt/missing EQ values — default to neutral
        auto guardEq = [](float v) { return (v >= 0.0f && v <= 1.0f) ? v : 0.5f; };
        state.eqLow  = guardEq(state.eqLow);
        state.eqMid  = guardEq(state.eqMid);
        state.eqHigh = guardEq(state.eqHigh);

        // Load loop points (default to -1 if missing/zero)
        state.loopInSeconds  = (double)trackObj->getProperty("loopIn");
        state.loopOutSeconds = (double)trackObj->getProperty("loopOut");
        state.loopEnabled    = (bool)  trackObj->getProperty("loopEnabled");
        if (state.loopInSeconds  == 0.0) state.loopInSeconds  = -1.0;
        if (state.loopOutSeconds == 0.0) state.loopOutSeconds = -1.0;

        // Load speed ratio (default to 1.0 if missing/zero)
        state.speedRatio = (double)trackObj->getProperty("speedRatio");
        if (state.speedRatio <= 0.0) state.speedRatio = 1.0;

        store[path] = state;
    }
}

void TrackStateStore::saveToDisk() const
{
    auto root = std::make_unique<juce::DynamicObject>();

    for (const auto& entry : store)
    {
        auto trackObj = std::make_unique<juce::DynamicObject>();

        // Save hot cues as an array
        juce::Array<juce::var> cueArray;
        for (int i = 0; i < 8; ++i)
        {
            auto cueObj = std::make_unique<juce::DynamicObject>();
            cueObj->setProperty("set",     entry.second.hotCueSet[i]);
            cueObj->setProperty("seconds", entry.second.hotCueSet[i]
                                           ? entry.second.hotCueSeconds[i]
                                           : 0.0);
            cueArray.add(juce::var(cueObj.release()));
        }
        trackObj->setProperty("hotCues", cueArray);

        // Save EQ
        trackObj->setProperty("eqLow",  (double)entry.second.eqLow);
        trackObj->setProperty("eqMid",  (double)entry.second.eqMid);
        trackObj->setProperty("eqHigh", (double)entry.second.eqHigh);

        // Save loop points
        trackObj->setProperty("loopIn",      entry.second.loopInSeconds);
        trackObj->setProperty("loopOut",     entry.second.loopOutSeconds);
        trackObj->setProperty("loopEnabled", entry.second.loopEnabled);

        // Save speed ratio
        trackObj->setProperty("speedRatio",  entry.second.speedRatio);

        root->setProperty(entry.first, juce::var(trackObj.release()));
    }

    const juce::var jsonVar(root.release());
    const auto text = juce::JSON::toString(jsonVar, true); // pretty-print

    const auto file = getStoreFile();
    file.getParentDirectory().createDirectory(); // ensure DJApp/ folder exists
    file.replaceWithText(text);
}

void TrackStateStore::saveTrack(const juce::String& filePath, const TrackState& state)
{
    store[filePath] = state;
}

bool TrackStateStore::loadTrack(const juce::String& filePath, TrackState& state) const
{
    const auto it = store.find(filePath);
    if (it == store.end())
    {
        state = TrackState{}; // return defaults
        return false;
    }

    state = it->second;
    return true;
}
