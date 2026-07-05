#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "plugin/PluginProcessor.h"
#include "state/FactoryPresets.h"

namespace solar {

// Message-thread owner of the preset system (plan §state): factory presets
// are code-built full-rig trees; user presets are .s42n XML snapshots in
// ~/Library/Application Support/Solar42N/Presets. Every load goes through the
// processor's click-free fade. Loading a preset keeps the CURRENT unit serial
// ("a patch always sounds like *your* unit") — the serial only changes via
// the DAW-state round-trip or an explicit Swap Unit.
class PresetManager : public juce::ChangeBroadcaster
{
public:
    explicit PresetManager(Solar42NProcessor& p) : proc_(p)
    {
        userDir().createDirectory();
        migrateLegacyPresets();
    }

    static constexpr const char* kExtension = ".s42n";

    // ---- factory
    int numFactory() const { return (int) factory::presets().size(); }
    juce::String factoryName(int i) const { return factory::presets()[(size_t) i].name; }
    juce::String factoryHint(int i) const { return factory::presets()[(size_t) i].hint; }

    void loadFactory(int i)
    {
        if (i < 0 || i >= numFactory())
            return;
        auto tree = proc_.defaultState().createCopy();
        factory::presets()[(size_t) i].build(tree);
        apply(tree, factoryName(i), true);
    }

    // ---- user (.s42n files)
    // On macOS userApplicationDataDirectory is ~/Library, NOT
    // ~/Library/Application Support — descend explicitly so presets live in
    // the conventional per-user location, outside any build tree.
    juce::File userDir() const
    {
        auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
#if JUCE_MAC
        base = base.getChildFile("Application Support");
#endif
        return base.getChildFile("Solar42N").getChildFile("Presets");
    }

    juce::StringArray userPresetNames() const
    {
        juce::StringArray names;
        for (const auto& f : userDir().findChildFiles(juce::File::findFiles, false,
                                                      "*" + juce::String(kExtension)))
            names.add(f.getFileNameWithoutExtension());
        names.sortNatural();
        return names;
    }

    bool saveUser(const juce::String& name)
    {
        const auto file = userDir().getChildFile(
            juce::File::createLegalFileName(name) + kExtension);
        const auto xml = proc_.currentFullState().createXml();
        if (xml == nullptr || !file.replaceWithText(xml->toString()))
            return false;
        current_ = name;
        sendChangeMessage();
        return true;
    }

    bool loadUser(const juce::String& name)
    {
        const auto file = userDir().getChildFile(
            juce::File::createLegalFileName(name) + kExtension);
        if (const auto xml = juce::parseXML(file))
        {
            apply(juce::ValueTree::fromXml(*xml), name, true);
            return true;
        }
        return false;
    }

    // ---- navigation: factory list then user list, wrapping.
    void loadNext(int direction)
    {
        juce::StringArray all;
        for (int i = 0; i < numFactory(); ++i)
            all.add(factoryName(i));
        all.addArray(userPresetNames());
        if (all.isEmpty())
            return;

        int idx = all.indexOf(current_);
        idx = (idx < 0 ? (direction > 0 ? 0 : all.size() - 1)
                       : (idx + direction + all.size()) % all.size());
        if (idx < numFactory())
            loadFactory(idx);
        else
            loadUser(all[idx]);
    }

    // ---- the digital "swap hardware units" convenience: new tolerance serial,
    // same patch. Goes through the same faded load.
    void swapUnitSerial()
    {
        auto tree = proc_.currentFullState();
        auto tol = tree.getOrCreateChildWithName("TOLERANCES", nullptr);
        tol.setProperty("serial",
                        juce::String::toHexString(juce::Random::getSystemRandom().nextInt64()),
                        nullptr);
        apply(tree, current_, false);
    }

    juce::String currentName() const { return current_; }

private:
    // Pre-2026-07-05 builds resolved userDir() to ~/Library/Solar42N/Presets
    // (missed the "Application Support" hop). Adopt anything saved there:
    // move if the name is free, otherwise leave the file untouched.
    void migrateLegacyPresets()
    {
#if JUCE_MAC
        const auto legacy =
            juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile("Solar42N")
                .getChildFile("Presets");
        if (legacy == userDir() || !legacy.isDirectory())
            return;
        for (const auto& f : legacy.findChildFiles(juce::File::findFiles, false,
                                                   "*" + juce::String(kExtension)))
        {
            const auto dest = userDir().getChildFile(f.getFileName());
            if (!dest.existsAsFile())
                f.moveFileTo(dest);
        }
        if (legacy.getNumberOfChildFiles(juce::File::findFilesAndDirectories) == 0
            && legacy.deleteFile())
            legacy.getParentDirectory().deleteFile(); // only succeeds when empty
#endif
    }

    void apply(juce::ValueTree tree, const juce::String& name, bool keepCurrentSerial)
    {
        if (keepCurrentSerial)
        {
            auto tol = tree.getOrCreateChildWithName("TOLERANCES", nullptr);
            tol.setProperty("serial",
                            juce::String::toHexString((juce::int64) proc_.unitSerial()),
                            nullptr);
        }
        proc_.requestStateLoad(tree);
        current_ = name;
        sendChangeMessage();
    }

    Solar42NProcessor& proc_;
    juce::String current_ { "Init" };
};

} // namespace solar
