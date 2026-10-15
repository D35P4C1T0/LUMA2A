#pragma once
#include "Model.h"
#include <array>
#include <juce_audio_utils/juce_audio_utils.h>
struct ParameterSpec {
    const char *id;
    const char *name;
    float initial;
    int steps;
};
class OptoProcessor final : public juce::AudioProcessor,
                            private juce::AudioProcessorValueTreeState::Listener {
  public:
    OptoProcessor();
    ~OptoProcessor() override;
    juce::AudioProcessorParameter *getBypassParameter() const override {
        return parameter("bypass");
    }
    void selectBank(int);
    int currentBank() const { return activeBank; }
    static const std::array<ParameterSpec, 10> &parameterSpecs();
    static const juce::String &hostId(const char *readableKey);
    juce::RangedAudioParameter *parameter(const char *key) const {
        return state.getParameter(hostId(key));
    }
    juce::UndoManager undoManager{10000, 32};
    juce::AudioProcessorValueTreeState state;
    float normalized(int index) const;
    void setNormalized(int index, float value);
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    void processBlock(juce::AudioBuffer<double> &, juce::MidiBuffer &) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }
    bool isBusesLayoutSupported(const BusesLayout &) const override;
    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Luma2A"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    int currentFactoryPreset() const { return activePreset; }
    void loadFactoryPreset(int);
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String &) override {}
    void getStateInformation(juce::MemoryBlock &) override;
    void setStateInformation(const void *, int) override;
    std::atomic<float> inputDb{-60}, outputDb{-60}, reductionDb{0}, needlePosition{.67f};

  private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    template <typename T> void process(juce::AudioBuffer<T> &);
    void parameterChanged(const juce::String &, float) override;
    std::array<std::array<float, 10>, 4> banks{};
    int activeBank = 0;
    int activePreset = 0;
    Model model;
};
