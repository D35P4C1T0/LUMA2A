#pragma once
#include "Processor.h"

class OptoEditor final : public juce::AudioProcessorEditor, private juce::Timer {
  public:
    explicit OptoEditor(OptoProcessor &);
    ~OptoEditor() override;
    void paint(juce::Graphics &) override;
    void resized() override;

  private:
    class Control;
    struct Palette {
        juce::Colour ink{0xff20201e}, muted{0xff777770}, accent{0xffde392b};
        juce::Colour panel{0xfff2f1eb}, paper{0xfffaf9f5}, rule{0xffcecec5};
        juce::Colour knob{0xff20201e}, knobRing{0x33faf9f5};
    };
    class FlatLook final : public juce::LookAndFeel_V4 {
      public:
        Palette colours;

      private:
        void drawButtonBackground(juce::Graphics &, juce::Button &, const juce::Colour &, bool,
                                  bool) override;
        juce::Font getTextButtonFont(juce::TextButton &, int) override;
        juce::Font getComboBoxFont(juce::ComboBox &) override;
        void drawComboBox(juce::Graphics &, int, int, bool, int, int, int, int,
                          juce::ComboBox &) override;
    } look;
    OptoProcessor &engine;
    std::vector<std::unique_ptr<Control>> controls;
    std::vector<std::unique_ptr<juce::TextButton>> buttons;
    juce::ComboBox preset;
    juce::TextButton themeButton;
    juce::TooltipWindow tooltip{this, 650};
    std::array<float, 10> copied{};
    bool hasCopy = false;
    bool darkMode = false;
    float needle = .67f;
    void timerCallback() override;
    void drawMeter(juce::Graphics &);
    void applyTheme();
};
