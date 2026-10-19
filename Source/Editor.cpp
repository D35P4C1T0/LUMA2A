#include "Editor.h"
#include "FactoryPresets.h"
namespace {
constexpr int canvasWidth = 1120, canvasHeight = 620;
juce::Font swiss(float height, bool bold = false) {
    return juce::Font(
        juce::FontOptions("Helvetica Neue", height, bold ? juce::Font::bold : juce::Font::plain));
}
constexpr float pi = juce::MathConstants<float>::pi;
juce::Point<float> polar(juce::Point<float> c, float radius, float angle) {
    return {c.x + radius * std::sin(angle), c.y - radius * std::cos(angle)};
}
void text(juce::Graphics &g, const juce::String &value, juce::Rectangle<float> bounds, float size,
          juce::Colour colour, juce::Justification align = juce::Justification::centred) {
    g.setColour(colour);
    g.setFont(swiss(size));
    g.drawText(value, bounds, align);
}
} // namespace
void OptoEditor::FlatLook::drawButtonBackground(juce::Graphics &g, juce::Button &b,
                                                const juce::Colour &, bool over, bool down) {
    const auto accent = colours.accent;
    if (over || down) {
        g.setColour(accent.withAlpha(down ? .14f : .07f));
        g.fillRect(b.getLocalBounds());
    }
    if (b.findColour(juce::TextButton::textColourOffId) == accent) {
        g.setColour(accent);
        g.fillRect(0, b.getHeight() - 2, b.getWidth(), 2);
    }
}
juce::Font OptoEditor::FlatLook::getTextButtonFont(juce::TextButton &, int height) {
    return swiss(static_cast<float>(height) * .4f, true);
}
juce::Font OptoEditor::FlatLook::getComboBoxFont(juce::ComboBox &) { return swiss(14); }
void OptoEditor::FlatLook::drawComboBox(juce::Graphics &g, int w, int h, bool, int, int, int, int,
                                        juce::ComboBox &) {
    const auto &[ink, muted, accent, panel, paper, rule, knob, knobRing] = colours;
    g.setColour(paper);
    g.fillRect(0, 0, w, h);
    g.setColour(ink);
    g.drawRect(0, 0, w, h, 1);
    juce::Path arrow;
    arrow.addTriangle(static_cast<float>(w) - 19.f, h * .45f, static_cast<float>(w) - 11.f,
                      h * .45f, static_cast<float>(w) - 15.f, h * .6f);
    g.setColour(accent);
    g.fillPath(arrow);
}
class OptoEditor::Control final : public juce::Component, public juce::SettableTooltipClient {
  public:
    Control(OptoEditor &editor, OptoProcessor &p, int i, juce::Rectangle<int> rectangle)
        : layout(rectangle), owner(editor), engine(p), index(i) {
        setTooltip(p.parameterSpecs()[static_cast<size_t>(i)].name);
        setWantsKeyboardFocus(true);
        setTitle(p.parameterSpecs()[static_cast<size_t>(i)].name);
    }
    void paint(juce::Graphics &g) override {
        const auto &[ink, muted, accent, panel, paper, rule, knob, knobRing] = owner.look.colours;
        g.addTransform(juce::AffineTransform::scale(
            static_cast<float>(getWidth()) / static_cast<float>(layout.getWidth()),
            static_cast<float>(getHeight()) / static_cast<float>(layout.getHeight())));
        const float w = static_cast<float>(layout.getWidth()),
                    h = static_cast<float>(layout.getHeight()), value = engine.normalized(index);
        if (index == 0 || index == 2 || index == 9) {
            const int count = index == 9 ? 3 : 2;
            const float segment = w / static_cast<float>(count);
            const int selected =
                juce::jlimit(0, count - 1, juce::roundToInt(value * static_cast<float>(count - 1)));
            const juce::StringArray labels = index == 9
                                                 ? juce::StringArray{"INPUT", "REDUCTION", "OUTPUT"}
                                             : index == 2 ? juce::StringArray{"COMPRESS", "LIMIT"}
                                                          : juce::StringArray{"OFF", "POWER"};
            for (int i = 0; i < count; ++i) {
                auto r = juce::Rectangle<float>(i * segment, 0, segment - 3, h);
                g.setColour(i == selected ? ink : paper);
                g.fillRect(r);
                g.setColour(ink);
                g.drawRect(r, 1.f);
                text(g, labels[i], r, 10, i == selected ? paper : muted);
            }
            return;
        }
        const bool large = index == 5 || index == 6;
        const int number = index == 5   ? 1
                           : index == 6 ? 2
                           : index == 1 ? 3
                           : index == 3 ? 4
                           : index == 4 ? 5
                           : index == 7 ? 6
                                        : 7;
        const auto label =
            index == 1 ? juce::String("HF FOCUS")
                       : juce::String(engine.parameterSpecs()[static_cast<size_t>(index)].name)
                             .toUpperCase();
        text(g, "0" + juce::String(number) + " / " + label, {0, 0, w, 22}, large ? 13.f : 11.f, ink,
             juce::Justification::centredLeft);
        const float radius = large ? 63.f : 39.f;
        const juce::Point<float> centre(w * .5f, large ? 112.f : 79.f);
        for (int i = 0; i <= 10; ++i) {
            const float angle = -pi * .75f + pi * 1.5f * static_cast<float>(i) / 10.f;
            const auto a = polar(centre, radius + 10, angle), b = polar(centre, radius + 16, angle);
            g.setColour(ink.withAlpha(i % 5 == 0 ? .85f : .45f));
            g.drawLine({a, b}, i % 5 == 0 ? 2.f : 1.f);
            if (large && i % 2 == 0) {
                auto n = polar(centre, radius + 29, angle);
                text(g, juce::String(i * 10), {n.x - 14, n.y - 8, 28, 16}, 10, muted);
            }
        }
        juce::Path track;
        track.addCentredArc(centre.x, centre.y, radius + 5, radius + 5, 0, -pi * .75f,
                            -pi * .75f + pi * 1.5f * value, true);
        g.setColour(accent);
        g.strokePath(track, juce::PathStrokeType(2.f));
        g.setColour(knob);
        g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2, radius * 2);
        g.setColour(knobRing);
        g.drawEllipse(centre.x - radius + 4, centre.y - radius + 4, (radius - 4) * 2,
                      (radius - 4) * 2, .6f);
        const float angle = -pi * .75f + value * pi * 1.5f;
        g.setColour(accent);
        g.drawLine({polar(centre, radius * .50f, angle), polar(centre, radius * .83f, angle)},
                   large ? 3.f : 2.5f);
        auto display = engine.parameter(engine.parameterSpecs()[static_cast<size_t>(index)].id)
                           ->getCurrentValueAsText();
        text(g, display, {0, h - 30, w, 28}, large ? 26.f : 19.f, ink,
             juce::Justification::centredLeft);
        g.setColour(rule);
        g.drawLine(0, h - 1, w, h - 1, 1);
    }
    void mouseDown(const juce::MouseEvent &e) override {
        start = engine.normalized(index);
        origin = e.position;
        engine.undoManager.beginNewTransaction();
        parameter()->beginChangeGesture();
        if (index == 0 || index == 2)
            engine.setNormalized(index, start < .5f ? 1.f : 0.f);
        else if (index == 9) {
            const auto part = juce::jlimit(0, 2, static_cast<int>(e.position.x * 3 / getWidth()));
            engine.setNormalized(index, part * .5f);
        }
        repaint();
    }
    void mouseDrag(const juce::MouseEvent &e) override {
        if (index != 0 && index != 2 && index != 9) {
            engine.setNormalized(index,
                                 start + (origin.y - e.position.y + e.position.x - origin.x) *
                                             (e.mods.isShiftDown() ? .0005f : .004f));
            repaint();
        }
    }
    void mouseUp(const juce::MouseEvent &) override { parameter()->endChangeGesture(); }
    void mouseDoubleClick(const juce::MouseEvent &e) override {
        if (index == 0 || index == 2 || index == 9)
            return;
        if (e.position.y < 24.f * getHeight() / layout.getHeight() ||
            e.position.y > getHeight() - 28.f * getHeight() / layout.getHeight()) {
            auto *dialog = new juce::AlertWindow(
                "Set value", engine.parameterSpecs()[static_cast<size_t>(index)].name,
                juce::MessageBoxIconType::NoIcon);
            const auto &colours = owner.look.colours;
            dialog->setColour(juce::AlertWindow::backgroundColourId, colours.paper);
            dialog->setColour(juce::AlertWindow::textColourId, colours.ink);
            dialog->setColour(juce::AlertWindow::outlineColourId, colours.rule);
            dialog->addTextEditor("value", juce::String(engine.normalized(index) * 100, 1));
            auto *entry = dialog->getTextEditor("value");
            entry->setColour(juce::TextEditor::backgroundColourId, colours.panel);
            entry->setColour(juce::TextEditor::textColourId, colours.ink);
            entry->setColour(juce::TextEditor::highlightColourId, colours.accent);
            entry->setColour(juce::TextEditor::highlightedTextColourId, colours.paper);
            entry->setColour(juce::TextEditor::focusedOutlineColourId, colours.accent);
            entry->setColour(juce::CaretComponent::caretColourId, colours.ink);
            dialog->addButton("Apply", 1, juce::KeyPress(juce::KeyPress::returnKey));
            dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
            for (auto *child : dialog->getChildren())
                if (auto *button = dynamic_cast<juce::TextButton *>(child)) {
                    button->setColour(juce::TextButton::buttonColourId, colours.panel);
                    button->setColour(juce::TextButton::textColourOffId, colours.ink);
                }
            juce::Component::SafePointer<Control> safe(this);
            dialog->enterModalState(
                true, juce::ModalCallbackFunction::create([safe, dialog](int result) {
                    if (safe && result == 1) {
                        safe->parameter()->beginChangeGesture();
                        safe->engine.setNormalized(
                            safe->index,
                            dialog->getTextEditorContents("value").getFloatValue() * .01f);
                        safe->parameter()->endChangeGesture();
                        safe->repaint();
                    }
                }),
                true);
        } else {
            parameter()->beginChangeGesture();
            engine.setNormalized(index,
                                 engine.parameterSpecs()[static_cast<size_t>(index)].initial);
            parameter()->endChangeGesture();
            repaint();
        }
    }
    void mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &wheel) override {
        parameter()->beginChangeGesture();
        engine.setNormalized(index, engine.normalized(index) + wheel.deltaY * .1f);
        parameter()->endChangeGesture();
        repaint();
    }
    bool keyPressed(const juce::KeyPress &key) override {
        float direction = 0;
        if (key == juce::KeyPress::upKey || key == juce::KeyPress::rightKey)
            direction = 1;
        else if (key == juce::KeyPress::downKey || key == juce::KeyPress::leftKey)
            direction = -1;
        else
            return false;
        parameter()->beginChangeGesture();
        engine.setNormalized(index, engine.normalized(index) +
                                        direction * (index == 9                 ? .5f
                                                     : index == 0 || index == 2 ? 1.f
                                                                                : .01f));
        parameter()->endChangeGesture();
        repaint();
        return true;
    }
    juce::Rectangle<int> layout;

  private:
    juce::RangedAudioParameter *parameter() {
        return engine.parameter(engine.parameterSpecs()[static_cast<size_t>(index)].id);
    }
    OptoEditor &owner;
    OptoProcessor &engine;
    int index;
    float start = 0;
    juce::Point<float> origin;
};
OptoEditor::OptoEditor(OptoProcessor &p) : AudioProcessorEditor(p), engine(p) {
    setLookAndFeel(&look);
    auto add = [&](int index, juce::Rectangle<int> rectangle) {
        auto c = std::make_unique<Control>(*this, engine, index, rectangle);
        addAndMakeVisible(*c);
        controls.push_back(std::move(c));
    };
    add(5, {105, 105, 190, 240});
    add(6, {825, 105, 190, 240});
    add(1, {100, 405, 120, 165});
    add(3, {300, 405, 120, 165});
    add(4, {500, 405, 120, 165});
    add(7, {700, 405, 120, 165});
    add(8, {900, 405, 120, 165});
    add(2, {100, 355, 200, 30});
    add(9, {395, 355, 330, 30});
    add(0, {820, 355, 200, 30});
    const juce::StringArray names{"Undo", "Redo", "A",     "B",     "C",
                                  "D",    "Copy", "Paste", "Reset", "Bypass"};
    for (int i = 0; i < names.size(); ++i) {
        auto button = std::make_unique<juce::TextButton>(names[i]);
        button->setLookAndFeel(&look);
        button->onClick = [this, i] {
            engine.undoManager.beginNewTransaction();
            if (i == 0)
                engine.undoManager.undo();
            else if (i == 1)
                engine.undoManager.redo();
            else if (i >= 2 && i <= 5)
                engine.selectBank(i - 2);
            else if (i == 6) {
                for (int j = 0; j < 10; ++j)
                    copied[static_cast<size_t>(j)] = engine.normalized(j);
                hasCopy = true;
            } else if (i == 7 && hasCopy) {
                for (int j = 0; j < 10; ++j)
                    engine.setNormalized(j, copied[static_cast<size_t>(j)]);
            } else if (i == 8)
                engine.loadFactoryPreset(0);
            else if (i == 9) {
                auto *param = engine.getBypassParameter();
                param->beginChangeGesture();
                param->setValueNotifyingHost(param->getValue() > .5f ? 0.f : 1.f);
                param->endChangeGesture();
            }
            repaint();
        };
        addAndMakeVisible(*button);
        buttons.push_back(std::move(button));
    }
    for (size_t i = 0; i < factoryPresets.size(); ++i)
        preset.addItem(factoryPresets[i].name, static_cast<int>(i) + 1);
    preset.setSelectedId(engine.currentFactoryPreset() + 1, juce::dontSendNotification);
    preset.onChange = [this] {
        if (preset.getSelectedId() > 0)
            engine.loadFactoryPreset(preset.getSelectedId() - 1);
    };
    preset.setLookAndFeel(&look);
    preset.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(preset);
    themeButton.setComponentID("themeToggle");
    themeButton.setTitle("Colour theme");
    themeButton.onClick = [this] {
        engine.setDarkMode(!engine.isDarkMode());
        engine.updateHostDisplay(
            juce::AudioProcessorListener::ChangeDetails().withNonParameterStateChanged(true));
        applyTheme();
    };
    addAndMakeVisible(themeButton);
    applyTheme();
    setResizable(true, false);
    getConstrainer()->setFixedAspectRatio(static_cast<double>(canvasWidth) / canvasHeight);
    setResizeLimits(840, 465, 1680, 930);
    setSize(canvasWidth, canvasHeight);
    timerCallback();
    startTimerHz(30);
}
OptoEditor::~OptoEditor() {
    stopTimer();
    preset.setLookAndFeel(nullptr);
    for (auto &b : buttons)
        b->setLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}
void OptoEditor::applyTheme() {
    darkMode = engine.isDarkMode();
    look.colours = darkMode
        ? Palette{juce::Colour(0xffeeeee8), juce::Colour(0xffa7a9a2), juce::Colour(0xffff6958),
                  juce::Colour(0xff202220), juce::Colour(0xff292c29), juce::Colour(0xff484c46),
                  juce::Colour(0xff141614), juce::Colour(0xff555b53)}
        : Palette{};
    const auto &[ink, muted, accent, panel, paper, rule, knob, knobRing] = look.colours;
    look.setColour(juce::PopupMenu::backgroundColourId, paper);
    look.setColour(juce::PopupMenu::textColourId, ink);
    look.setColour(juce::PopupMenu::highlightedBackgroundColourId, ink);
    look.setColour(juce::PopupMenu::highlightedTextColourId, paper);
    look.setColour(juce::TooltipWindow::backgroundColourId, paper);
    look.setColour(juce::TooltipWindow::textColourId, ink);
    look.setColour(juce::TooltipWindow::outlineColourId, rule);
    preset.setColour(juce::ComboBox::backgroundColourId, paper);
    preset.setColour(juce::ComboBox::textColourId, ink);
    for (auto &button : buttons)
        button->setColour(juce::TextButton::textColourOffId, muted);
    themeButton.setButtonText(darkMode ? "Light mode" : "Dark mode");
    themeButton.setTooltip(darkMode ? "Switch to light mode" : "Switch to dark mode");
    themeButton.setColour(juce::TextButton::textColourOffId, muted);
    sendLookAndFeelChange();
    repaint();
}
void OptoEditor::drawMeter(juce::Graphics &g) {
    const auto &[ink, muted, accent, panel, paper, rule, knob, knobRing] = look.colours;
    const juce::Rectangle<float> shell(350, 105, 420, 237), face(362, 117, 396, 213);
    g.setColour(paper);
    g.fillRect(shell);
    g.setColour(ink);
    g.drawRect(shell, 1.f);
    g.setColour(rule);
    g.drawLine(face.getX(), face.getBottom(), face.getRight(), face.getBottom(), 1.f);
    const juce::Point<float> pivot(560, 333);
    const float radius = 180;
    const auto dark = ink, red = accent;
    const auto angle = [](float position) { return (-54.f + 108.f * position) * pi / 180.f; };
    // Positions share the processor's calibrated nonlinear VU scale.
    const std::array<float, 11> values{
        {.04f, .165f, .262f, .341f, .45f, .517f, .597f, .67f, .77f, .87f, 1.f}};
    const std::array<const char *, 11> labels{
        {"-20", "-10", "-7", "-5", "-3", "-2", "-1", "0", "+1", "+2", "+3"}};
    juce::Path arc;
    arc.addCentredArc(pivot.x, pivot.y, radius, radius, 0, angle(0), angle(1), true);
    g.setColour(dark);
    g.strokePath(arc, juce::PathStrokeType(1));
    for (size_t i = 0; i < values.size(); ++i) {
        const auto a = angle(values[i]);
        g.setColour(i > 7 ? red : dark);
        g.drawLine({polar(pivot, radius - 11, a), polar(pivot, radius + 3, a)},
                   i == 7 ? 2.f : 1.2f);
        auto p = polar(pivot, radius + 18, a);
        text(g, labels[i], {p.x - 13, p.y - 8, 26, 16}, 11, i > 7 ? red : dark);
    }
    // Secondary linear amplitude markings.
    for (int percent : {0, 20, 40, 60, 80, 100}) {
        float p = 0.f;
        if (percent > 0) {
            const float db = 20.f * std::log10(static_cast<float>(percent) * .01f);
            const std::array<float, 11> levels{{-20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3}};
            for (size_t i = 0; i + 1 < levels.size(); ++i)
                if (db >= levels[i] && db <= levels[i + 1]) {
                    p = values[i] + (values[i + 1] - values[i]) * (db - levels[i]) /
                                        (levels[i + 1] - levels[i]);
                    break;
                }
        }
        auto t = polar(pivot, radius - 25, angle(p));
        text(g, juce::String(percent), {t.x - 12, t.y - 6, 24, 12}, 8, dark.withAlpha(.7f));
    }
    text(g, "VU", {505, 225, 110, 35}, 32, dark);
    text(g, "LEVEL / dB", {485, 269, 150, 16}, 9, muted);
    g.saveState();
    g.reduceClipRegion(face.toNearestInt().reduced(2));
    const float a = angle(juce::jlimit(0.f, 1.f, needle));
    g.setColour(accent);
    g.drawLine({pivot, polar(pivot, radius - 3, a)}, 1.8f);
    g.fillEllipse(pivot.x - 4, pivot.y - 4, 8, 8);
    g.restoreState();
}
void OptoEditor::paint(juce::Graphics &g) {
    const auto &[ink, muted, accent, panel, paper, rule, knob, knobRing] = look.colours;
    g.fillAll(panel);
    g.addTransform(juce::AffineTransform::scale(static_cast<float>(getWidth()) / canvasWidth));
    g.setColour(ink);
    g.setFont(swiss(37, true));
    g.drawText("Luma2A", 32, 14, 280, 48, juce::Justification::centredLeft);
    text(g, "OPTICAL DYNAMICS / 01", {34, 60, 270, 14}, 9, muted, juce::Justification::centredLeft);
    g.setColour(accent);
    g.fillRect(32, 86, 24, 3);
    g.setColour(ink);
    g.fillRect(56, 86, 1032, 1);
    g.setColour(rule);
    g.drawLine(326, 105, 326, 377, 1);
    g.drawLine(794, 105, 794, 377, 1);
    g.drawLine(32, 391, 1088, 391, 1);
    g.drawLine(32, 589, 1088, 589, 1);
    for (float x : {260.f, 460.f, 660.f, 860.f})
        g.drawLine(x, 412, x, 570, 1);
    drawMeter(g);
    text(g, "OPTO CELL / TUBE DRIVE", {32, 596, 330, 14}, 9, muted,
         juce::Justification::centredLeft);
    text(g, "SHIFT  Fine   /   DOUBLE-CLICK  Reset or type", {690, 596, 398, 14}, 9, muted,
         juce::Justification::centredRight);
}
void OptoEditor::resized() {
    const float scale = static_cast<float>(getWidth()) / canvasWidth;
    for (auto &c : controls)
        c->setBounds((c->layout.toFloat() * scale).toNearestInt());
    preset.setBounds((juce::Rectangle<float>(326, 26, 262, 32) * scale).toNearestInt());
    themeButton.setBounds((juce::Rectangle<float>(976, 58, 112, 26) * scale).toNearestInt());
    const std::array<juce::Rectangle<float>, 10> bounds{{{605, 27, 42, 30},
                                                         {650, 27, 42, 30},
                                                         {708, 27, 25, 30},
                                                         {736, 27, 25, 30},
                                                         {764, 27, 25, 30},
                                                         {792, 27, 25, 30},
                                                         {835, 27, 42, 30},
                                                         {880, 27, 42, 30},
                                                         {936, 27, 44, 30},
                                                         {1000, 27, 88, 30}}};
    for (size_t i = 0; i < buttons.size(); ++i)
        buttons[i]->setBounds((bounds[i] * scale).toNearestInt());
}
void OptoEditor::timerCallback() {
    if (darkMode != engine.isDarkMode())
        applyTheme();
    const auto &[ink, muted, accent, panel, paper, rule, knob, knobRing] = look.colours;
    needle = engine.needlePosition.load();
    preset.setSelectedId(engine.currentFactoryPreset() + 1, juce::dontSendNotification);
    for (int i = 0; i < 4; ++i)
        buttons[static_cast<size_t>(i + 2)]->setColour(juce::TextButton::textColourOffId,
                                                       engine.currentBank() == i ? accent : muted);
    buttons[9]->setColour(juce::TextButton::textColourOffId,
                          engine.getBypassParameter()->getValue() > .5f ? accent : muted);
    repaint();
    for (auto &c : controls)
        c->repaint();
}
