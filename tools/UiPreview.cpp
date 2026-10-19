#include "FactoryPresets.h"
#include "Processor.h"
#include <iostream>
#include <juce_gui_extra/juce_gui_extra.h>
int main(int argc, char **argv) {
    juce::ScopedJuceInitialiser_GUI initialise;
    if (argc > 3 && juce::String(argv[1]) == "--check-rename") {
        juce::VST3PluginFormat format;
        auto load = [&](const juce::String &path) {
            juce::OwnedArray<juce::PluginDescription> descriptions;
            format.findAllTypesForFile(descriptions, path);
            juce::String error;
            return descriptions.isEmpty()
                       ? std::unique_ptr<juce::AudioPluginInstance>{}
                       : format.createInstanceFromDescription(*descriptions[0], 48000, 512, error);
        };
        auto previous = load(argv[2]), renamed = load(argv[3]);
        if (!previous || !renamed) {
            std::cerr << "Cannot load compatibility pair\n";
            return 1;
        }
        auto &before = previous->getParameters();
        auto &after = renamed->getParameters();
        previous->prepareToPlay(48000, 512);
        renamed->prepareToPlay(48000, 512);
        auto flush = [](juce::AudioPluginInstance &instance) {
            // Flush the host's controller queue, then synchronize its parameter cache
            // without relying on a running desktop message loop.
            juce::MemoryBlock state;
            instance.getStateInformation(state);
            juce::AudioBuffer<float> buffer(2, 512);
            buffer.clear();
            juce::MidiBuffer midi;
            instance.processBlock(buffer, midi);
            instance.getStateInformation(state);
            instance.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        };
        if (before.size() != 14 || after.size() != 14)
            return 1;
        for (int i = 0; i < 14; ++i) {
            auto *a = dynamic_cast<juce::HostedAudioProcessorParameter *>(before[i]);
            auto *b = dynamic_cast<juce::HostedAudioProcessorParameter *>(after[i]);
            if (!a || !b || a->getParameterID() != b->getParameterID())
                return 1;
        }
        before[7]->setValueNotifyingHost(.81f);
        before[6]->setValueNotifyingHost(.45f);
        flush(*previous);
        before[1]->setValueNotifyingHost(1.f / 3.f);
        flush(*previous);
        before[7]->setValueNotifyingHost(.19f);
        before[6]->setValueNotifyingHost(.8f);
        flush(*previous);
        juce::MemoryBlock saved;
        previous->getStateInformation(saved);
        renamed->setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
        std::cout << "Restored active bank: " << after[7]->getValue() << ", "
                  << after[6]->getValue() << "\n";
        if (std::abs(after[7]->getValue() - .19f) > 1e-6f ||
            std::abs(after[6]->getValue() - .8f) > 1e-6f)
            return 1;
        after[1]->setValueNotifyingHost(0);
        flush(*renamed);
        std::cout << "Restored inactive bank: " << after[7]->getValue() << ", "
                  << after[6]->getValue() << "\n";
        if (std::abs(after[7]->getValue() - .81f) > 1e-6f ||
            std::abs(after[6]->getValue() - .45f) > 1e-6f)
            return 1;
        std::cout << "Renamed plugin recalls previous session and inactive banks; all parameter "
                     "IDs match\n";
        return 0;
    }
    if (argc > 2 && juce::String(argv[1]) == "--inspect-vst3") {
        juce::VST3PluginFormat format;
        juce::OwnedArray<juce::PluginDescription> descriptions;
        format.findAllTypesForFile(descriptions, argv[2]);
        if (descriptions.isEmpty()) {
            std::cerr << "No VST3 plugin found\n";
            return 1;
        }
        juce::String error;
        auto instance = format.createInstanceFromDescription(*descriptions[0], 48000, 512, error);
        if (!instance) {
            std::cerr << error << "\n";
            return 1;
        }
        instance->prepareToPlay(48000, 512);
        std::cout << "latency " << instance->getLatencySamples() << "\n";
        for (auto *parameter : instance->getParameters()) {
            auto *hosted = dynamic_cast<juce::HostedAudioProcessorParameter *>(parameter);
            std::cout << parameter->getName(100) << " "
                      << (hosted ? hosted->getParameterID() : juce::String("unknown")) << " "
                      << parameter->getValue() << "\n";
        }
        return 0;
    }
    OptoProcessor processor;
    processor.prepareToPlay(48000, 512);
    if (argc > 1 && juce::String(argv[1]) == "--self-test") {
        auto check = [](bool okay, const char *description) {
            if (!okay) {
                std::cerr << description << " failed\n";
                std::exit(1);
            }
        };
        check(processor.getParameters().size() == 14, "14 hosted parameters");
        check(processor.parameter("bank")->isMetaParameter(), "Bank meta-parameter flag");
        check(processor.getNumPrograms() == 1, "No synthetic VST3 program parameter");
        check(factoryPresets.size() == 85, "85 factory presets");
        check(processor.getLatencySamples() == 32, "Reported latency");
        const std::array<uint32_t, 14> expectedIds{
            {1296202096u, 1113681515u, 1349482354u, 1381970743u, 1299145829u, 1382248310u,
             1097296238u, 1197566318u, 1348821355u, 1148348790u, 1148344180u, 1298494578u,
             1299466066u, 1299469909u}};
        for (int i = 0; i < processor.getParameters().size(); ++i) {
            auto *parameter =
                dynamic_cast<juce::AudioProcessorParameterWithID *>(processor.getParameters()[i]);
            check(parameter != nullptr, "Hosted parameter identity");
            check(juce::VST3ClientExtensions::convertJuceParameterId(parameter->paramID) ==
                      expectedIds[static_cast<size_t>(i)],
                  "Stable VST3 parameter ID");
        }
        for (int i = 0; i < 10; ++i)
            check(std::abs(processor.normalized(i) -
                           processor.parameterSpecs()[static_cast<size_t>(i)].initial) < 1.e-6f,
                  "Default normalized value");
        processor.setNormalized(5, .81f);
        processor.selectBank(1);
        check(std::abs(processor.normalized(5) - .24f) < 1.e-6f, "Independent comparison bank");
        processor.setNormalized(5, .19f);
        processor.selectBank(0);
        check(std::abs(processor.normalized(5) - .81f) < 1.e-6f, "Comparison bank recall");
        juce::MemoryBlock saved;
        check(!processor.isDarkMode(), "Light theme by default");
        {
            std::unique_ptr<juce::AudioProcessorEditor> view(processor.createEditor());
            auto *toggle = dynamic_cast<juce::TextButton *>(view->findChildWithID("themeToggle"));
            check(toggle != nullptr, "Theme toggle exists");
            auto light = view->createComponentSnapshot(view->getLocalBounds());
            toggle->onClick();
            check(processor.isDarkMode(), "Theme toggle enables dark mode");
            auto dark = view->createComponentSnapshot(view->getLocalBounds());
            check(light.getPixelAt(0, 0) != dark.getPixelAt(0, 0), "Theme changes panel colour");
            processor.getStateInformation(saved);
            toggle->onClick();
            check(!processor.isDarkMode(), "Theme toggle restores light mode");
        }
        processor.loadFactoryPreset(20);
        processor.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
        check(processor.isDarkMode(), "Theme state round trip");
        {
            std::unique_ptr<juce::AudioProcessorEditor> view(processor.createEditor());
            auto *toggle = dynamic_cast<juce::TextButton *>(view->findChildWithID("themeToggle"));
            check(toggle != nullptr && toggle->getButtonText() == "Light mode",
                  "Reopened editor restores dark theme");
        }
        check(std::abs(processor.normalized(5) - .81f) < 1.e-6f, "State round trip");
        processor.selectBank(1);
        check(std::abs(processor.normalized(5) - .19f) < 1.e-6f, "Inactive bank state round trip");
        processor.loadFactoryPreset(84);
        check(processor.currentFactoryPreset() == 84, "Factory preset selection");
        if (argc > 2) {
            juce::MemoryBlock fixture;
            check(juce::File(argv[2]).loadFileAsData(fixture), "Session state fixture");
            processor.setNormalized(5, .9f);
            processor.setStateInformation(fixture.getData(), static_cast<int>(fixture.getSize()));
            check(std::abs(processor.normalized(5) - .24f) < 1.e-6f, "Component state import");
            processor.setNormalized(5, .81f);
            const std::array<char, 4> malformed{{'F', 's', 'C', 'k'}};
            processor.setStateInformation(malformed.data(), 4);
            check(std::abs(processor.normalized(5) - .81f) < 1.e-6f, "Malformed state ignored");
        }
        for (int channelCount : {1, 2}) {
            auto layout = processor.getBusesLayout();
            layout.inputBuses.set(0, channelCount == 1 ? juce::AudioChannelSet::mono()
                                                       : juce::AudioChannelSet::stereo());
            layout.outputBuses.set(0, layout.inputBuses[0]);
            check(processor.setBusesLayout(layout), "Mono/stereo layout");
            processor.loadFactoryPreset(0);
            processor.prepareToPlay(48000, 512);
            processor.getBypassParameter()->setValueNotifyingHost(1);
            juce::MidiBuffer midi;
            juce::AudioBuffer<float> f(channelCount, 1024);
            f.clear();
            for (int ch = 0; ch < channelCount; ++ch)
                f.setSample(ch, 0, 1);
            processor.processBlock(f, midi);
            check(std::abs(f.getSample(0, 0)) < 1.e-9f, "Bypass keeps delay");
            for (int ch = 0; ch < channelCount; ++ch)
                for (int i = 0; i < f.getNumSamples(); ++i)
                    check(std::isfinite(f.getSample(ch, i)), "Finite float processing");
            processor.getBypassParameter()->setValueNotifyingHost(0);
            processor.prepareToPlay(48000, 512);
            juce::AudioBuffer<double> d(channelCount, 1024);
            d.clear();
            for (int ch = 0; ch < channelCount; ++ch)
                d.setSample(ch, 0, .5);
            processor.processBlock(d, midi);
            for (int ch = 0; ch < channelCount; ++ch)
                for (int i = 0; i < d.getNumSamples(); ++i)
                    check(std::isfinite(d.getSample(ch, i)), "Finite double processing");
        }
        std::cout << "Plugin IDs, presets, state, banks, mono/stereo, float/double and bypass "
                     "checks passed\n";
        return 0;
    }
    if (argc > 2 && juce::String(argv[2]) == "--dark")
        processor.setDarkMode(true);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    editor->setSize(1120, 620);
    auto image = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.f);
    juce::File target(argc > 1 ? juce::String(argv[1])
                               : juce::File::getCurrentWorkingDirectory()
                                     .getChildFile("ui-preview.png")
                                     .getFullPathName());
    target.deleteFile();
    auto output = target.createOutputStream();
    if (!output || !juce::PNGImageFormat().writeImageToStream(image, *output)) {
        std::cerr << "Could not write preview\n";
        return 1;
    }
    std::cout << target.getFullPathName() << "\n";
    return 0;
}
