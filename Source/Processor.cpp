#include "Processor.h"
#include "Editor.h"
#include "FactoryPresets.h"
#include "SessionState.h"
const juce::String &OptoProcessor::hostId(const char *key) {
    // JUCE hashes parameter strings into VST3 IDs. Seven printable base-31 digits
    // give deterministic preimages of the compatible FourCC numeric IDs.
    static const auto ids = [] {
        std::map<juce::String, juce::String> result;
        const std::array<std::pair<const char *, uint32_t>, 14> compatibleIds{
            {{"bypass", 1296202096u},
             {"bank", 1113681515u},
             {"Powr", 1349482354u},
             {"R_37", 1381970743u},
             {"Mode", 1299145829u},
             {"Rcov", 1382248310u},
             {"Agin", 1097296238u},
             {"Gain", 1197566318u},
             {"Peak", 1348821355u},
             {"Driv", 1148348790u},
             {"DrWt", 1148344180u},
             {"Metr", 1298494578u},
             {"GRMeter", 1299466066u},
             {"VUMeter", 1299469909u}}};
        uint32_t baseline = 0;
        for (int i = 0; i < 7; ++i)
            baseline = baseline * 31u + 65u;
        for (const auto &entry : compatibleIds) {
            auto n = entry.second - baseline;
            char text[8]{};
            for (int i = 6; i >= 0; --i) {
                text[i] = static_cast<char>(65u + n % 31u);
                n /= 31u;
            }
            juce::String id(text);
            jassert(static_cast<uint32_t>(id.hashCode()) == entry.second);
            result.emplace(entry.first, id);
        }
        return result;
    }();
    return ids.at(key);
}
const std::array<ParameterSpec, 10> &OptoProcessor::parameterSpecs() {
    static const std::array<ParameterSpec, 10> specs{{{"Powr", "Power", 1, 2},
                                                      {"R_37", "R37", 1, 0},
                                                      {"Mode", "Mode", 0, 2},
                                                      {"Rcov", "Recovery", .75f, 0},
                                                      {"Agin", "Aging", 0, 0},
                                                      {"Gain", "Gain", .24f, 0},
                                                      {"Peak", "Peak Reduction", 0, 0},
                                                      {"Driv", "Drive", .25f, 0},
                                                      {"DrWt", "Mix", 1, 0},
                                                      {"Metr", "Meter Mode", .5f, 3}}};
    return specs;
}
juce::AudioProcessorValueTreeState::ParameterLayout OptoProcessor::makeLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{hostId("bypass"), 1},
                                                          "Master Bypass", false));
    result.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{hostId("bank"), 1}, "Bank", juce::StringArray{"A", "B", "C", "D"}, 0,
        juce::AudioParameterChoiceAttributes().withMeta(true)));
    for (const auto &p : parameterSpecs()) {
        const juce::ParameterID id{hostId(p.id), 1};
        if (juce::String(p.id) == "Mode")
            result.add(std::make_unique<juce::AudioParameterChoice>(
                id, p.name, juce::StringArray{"Compress", "Limit"}, 0));
        else if (p.steps == 2)
            result.add(std::make_unique<juce::AudioParameterBool>(id, p.name, p.initial > .5f));
        else if (p.steps == 3)
            result.add(std::make_unique<juce::AudioParameterChoice>(
                id, p.name, juce::StringArray{"Input", "Reduction", "Output"}, 1));
        else {
            const juce::String key = p.id;
            auto attrs = juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                [key](float value, int) {
                    if (key == "R_37" && value <= 0)
                        return juce::String("HF Focus");
                    if (key == "R_37" && value >= 100)
                        return juce::String("Flat");
                    if (key == "Rcov" && value <= 0)
                        return juce::String("Slow");
                    if (key == "Rcov" && value >= 100)
                        return juce::String("Fast");
                    if (key == "Agin" && value <= 0)
                        return juce::String("New");
                    if (key == "Agin" && value >= 100)
                        return juce::String("Old");
                    if (key == "Driv" && value <= 0)
                        return juce::String("Clean");
                    if (key == "Driv" && value >= 100)
                        return juce::String("Dirty");
                    if (key == "DrWt" && value <= 0)
                        return juce::String("Dry");
                    if (key == "DrWt" && value >= 100)
                        return juce::String("Wet");
                    return juce::String(value, 1) +
                           (key == "Rcov" || key == "Agin" || key == "Driv" || key == "DrWt" ? " %"
                                                                                             : "");
                });
            result.add(std::make_unique<juce::AudioParameterFloat>(
                id, p.name, juce::NormalisableRange<float>(0, 100), p.initial * 100, attrs));
        }
    }
    result.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{hostId("GRMeter"), 1}, "GR Meter", juce::NormalisableRange<float>(0, 1),
        1, juce::AudioParameterFloatAttributes().withAutomatable(false)));
    result.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{hostId("VUMeter"), 1}, "VU Meter", juce::NormalisableRange<float>(0, 1),
        0, juce::AudioParameterFloatAttributes().withAutomatable(false)));
    return result;
}
// Keep the serialized tree identifier stable for existing sessions.
OptoProcessor::OptoProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, &undoManager, "OptoA2A", makeLayout()) {
    for (auto &bank : banks)
        for (int i = 0; i < 10; ++i)
            bank[static_cast<size_t>(i)] = parameterSpecs()[static_cast<size_t>(i)].initial;
    state.addParameterListener(hostId("bank"), this);
}
OptoProcessor::~OptoProcessor() { state.removeParameterListener(hostId("bank"), this); }
void OptoProcessor::parameterChanged(const juce::String &, float value) {
    selectBank(juce::roundToInt(value));
}
void OptoProcessor::selectBank(int bank) {
    bank = juce::jlimit(0, 3, bank);
    if (bank == activeBank)
        return;
    for (int i = 0; i < 10; ++i)
        banks[static_cast<size_t>(activeBank)][static_cast<size_t>(i)] = normalized(i);
    activeBank = bank;
    for (int i = 0; i < 10; ++i)
        setNormalized(i, banks[static_cast<size_t>(bank)][static_cast<size_t>(i)]);
    auto *p = parameter("bank");
    if (juce::roundToInt(p->getValue() * 3) != bank)
        p->setValueNotifyingHost(static_cast<float>(bank) / 3.f);
}
void OptoProcessor::loadFactoryPreset(int index) {
    index = juce::jlimit(0, static_cast<int>(factoryPresets.size()) - 1, index);
    activePreset = index;
    undoManager.beginNewTransaction("Load preset");
    for (int i = 0; i < 10; ++i)
        setNormalized(i, factoryPresets[static_cast<size_t>(index)].values[static_cast<size_t>(i)]);
    updateHostDisplay();
}
const juce::String OptoProcessor::getProgramName(int index) {
    return factoryPresets[static_cast<size_t>(juce::jlimit(0, 84, index))].name;
}
float OptoProcessor::normalized(int i) const {
    return parameter(parameterSpecs()[static_cast<size_t>(i)].id)->getValue();
}
void OptoProcessor::setNormalized(int i, float v) {
    parameter(parameterSpecs()[static_cast<size_t>(i)].id)
        ->setValueNotifyingHost(juce::jlimit(0.f, 1.f, v));
}
void OptoProcessor::prepareToPlay(double sr, int) {
    model.prepare(sr, getTotalNumInputChannels());
    setLatencySamples(32);
}
void OptoProcessor::releaseResources() { model.reset(); }
bool OptoProcessor::isBusesLayoutSupported(const BusesLayout &b) const {
    return b.getMainInputChannelSet() == b.getMainOutputChannelSet() &&
           (b.getMainOutputChannelSet() == juce::AudioChannelSet::mono() ||
            b.getMainOutputChannelSet() == juce::AudioChannelSet::stereo());
}
template <typename T> void OptoProcessor::process(juce::AudioBuffer<T> &buffer) {
    juce::ScopedNoDenormals noDenormals;
    if (buffer.getNumSamples() == 0)
        return;
    for (int i = 0; i < 10; ++i)
        model.setParameter(i, i == 0 && getBypassParameter()->getValue() > .5f ? 0 : normalized(i));
    auto inLevel = 0.0;
    for (int c = 0; c < buffer.getNumChannels(); ++c)
        inLevel = std::max(inLevel,
                           static_cast<double>(buffer.getRMSLevel(c, 0, buffer.getNumSamples())));
    std::array<T *, 2> channels{};
    for (int c = 0; c < buffer.getNumChannels(); ++c)
        channels[static_cast<size_t>(c)] = buffer.getWritePointer(c);
    auto **ptr = channels.data();
    model.process(ptr, ptr, buffer.getNumChannels(), buffer.getNumSamples());
    auto outLevel = 0.0;
    for (int c = 0; c < buffer.getNumChannels(); ++c)
        outLevel = std::max(outLevel,
                            static_cast<double>(buffer.getRMSLevel(c, 0, buffer.getNumSamples())));
    inputDb.store(static_cast<float>(juce::Decibels::gainToDecibels(inLevel, -80.0)));
    outputDb.store(static_cast<float>(juce::Decibels::gainToDecibels(outLevel, -80.0)));
    reductionDb.store(static_cast<float>(model.meterGainReduction()));
    needlePosition.store(static_cast<float>(model.meterVuNormalized()));
    parameter("GRMeter")->setValue(static_cast<float>(model.meterGainReductionNormalized()));
    parameter("VUMeter")->setValue(static_cast<float>(model.meterVuNormalized()));
}
void OptoProcessor::processBlock(juce::AudioBuffer<float> &b, juce::MidiBuffer &) { process(b); }
void OptoProcessor::processBlock(juce::AudioBuffer<double> &b, juce::MidiBuffer &) { process(b); }
juce::AudioProcessorEditor *OptoProcessor::createEditor() { return new OptoEditor(*this); }
void OptoProcessor::getStateInformation(juce::MemoryBlock &dest) {
    for (int i = 0; i < 10; ++i)
        banks[static_cast<size_t>(activeBank)][static_cast<size_t>(i)] = normalized(i);
    auto tree = state.copyState();
    tree.setProperty("activeBank", activeBank, nullptr);
    tree.setProperty("activePreset", activePreset, nullptr);
    for (int b = 0; b < 4; ++b)
        for (int i = 0; i < 10; ++i)
            tree.setProperty("bank" + juce::String(b) + "_" + juce::String(i),
                             banks[static_cast<size_t>(b)][static_cast<size_t>(i)], nullptr);
    auto xml = tree.createXml();
    copyXmlToBinary(*xml, dest);
}
void OptoProcessor::setStateInformation(const void *data, int size) {
    if (size <= 0 || size > 1024 * 1024)
        return;
    auto xml = getXmlFromBinary(data, size);
    if (xml && xml->hasTagName(state.state.getType())) {
        auto tree = juce::ValueTree::fromXml(*xml);
        activeBank = juce::jlimit(0, 3, static_cast<int>(tree.getProperty("activeBank", 0)));
        activePreset = juce::jlimit(0, 84, static_cast<int>(tree.getProperty("activePreset", 0)));
        for (int b = 0; b < 4; ++b)
            for (int i = 0; i < 10; ++i)
                banks[static_cast<size_t>(b)][static_cast<size_t>(i)] = static_cast<float>(
                    tree.getProperty("bank" + juce::String(b) + "_" + juce::String(i),
                                     parameterSpecs()[static_cast<size_t>(i)].initial));
        state.replaceState(tree);
        return;
    }
    juce::MemoryBlock component;
    if (xml && xml->hasTagName("VST3PluginState")) {
        auto *child = xml->getChildByName("IComponent");
        if (child == nullptr)
            return;
        auto text = child->getAllSubText();
        auto prefix = text.upToFirstOccurrenceOf(".", false, false);
        auto length = prefix.getIntValue();
        if ((length != 128 && length != 464) || prefix.containsAnyOf("-+ ") ||
            text.length() != prefix.length() + 1 + (length * 8 + 5) / 6)
            return;
        const auto digits = text.fromFirstOccurrenceOf(".", false, false);
        if (!digits.containsOnly(
                ".ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+"))
            return;
        if (!component.fromBase64Encoding(text))
            return;
        data = component.getData();
        size = static_cast<int>(component.getSize());
    }
    auto imported = sessionState::parse(data, static_cast<size_t>(size));
    if (!imported)
        return;
    banks = imported->banks;
    activeBank = imported->activeBank;
    activePreset = 0;
    parameter("bypass")->setValueNotifyingHost(imported->bypass);
    parameter("bank")->setValueNotifyingHost(static_cast<float>(activeBank) / 3.f);
    for (int i = 0; i < 10; ++i)
        setNormalized(i, imported->parameters[static_cast<size_t>(i)]);
}
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() { return new OptoProcessor(); }
