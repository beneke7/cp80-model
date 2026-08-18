#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>

class CP80PluginProcessor;

namespace cp80ui {
// Two finishes. PAPER is the 1978 brochure: ink on cream, the photograph supplying
// the only colour. PLATE is the instrument's own control panel, black and lit.
// Flip kPaperFinish to A/B them in a host; there is no user-facing control.
constexpr bool kPaperFinish = true;

struct Theme {
    juce::Colour page;      // behind the photograph
    juce::Colour strip;     // control area
    juce::Colour text;      // captions, title
    juce::Colour dim;       // secondary text, knob rings
    juce::Colour arc;       // value arc
    juce::Colour pointer;   // knob pointer, switch cap
    juce::Colour rule;      // divider above the strip
    float        ruleHeight;
};

const Theme& theme();

juce::Font condensed(float height, bool bold);
juce::Font display(float height);

// The hardware uses one slide-switch part twice: BRILLIANCE (3-way) and
// TREMOLO (2-way).  So does this.
class SlideSwitch final : public juce::Component {
public:
    SlideSwitch(juce::StringArray positionLabels, bool topIsHighest);

    int  position() const { return index; }
    void setPosition(int newIndex, juce::NotificationType);
    std::function<void(int)> onChange;

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;

private:
    int positionAt(juce::Point<int>) const;

    juce::StringArray labels;
    bool  highestAtTop;
    int   index = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlideSwitch)
};

class KnobStyle final : public juce::LookAndFeel_V4 {
public:
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h,
                          float pos, float startAngle, float endAngle,
                          juce::Slider&) override;
};
} // namespace cp80ui

class CP80PluginEditor final : public juce::AudioProcessorEditor,
                               private juce::Timer {
public:
    explicit CP80PluginEditor(CP80PluginProcessor&);
    ~CP80PluginEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

private:
    static constexpr int kWidth = 860;
    static constexpr int kPlateHeight = 570;
    static constexpr int kStripHeight = 152;

    struct Knob {
        juce::Slider slider;
        juce::Label  caption;
        bool         detented = false;
        juce::String name;
        juce::String suffix;   // dB / % / Hz, from the parameter
    };

    void layoutStrip(juce::Rectangle<int> strip);
    void applyPlateVisibility();
    juce::Rectangle<int> arrowBounds() const;
    // The switches have no attachment class, so poll for host automation.
    void timerCallback() override;

    juce::RangedAudioParameter* brillianceParam = nullptr;
    juce::RangedAudioParameter* tremoloParam = nullptr;

    CP80PluginProcessor& processor;
    cp80ui::KnobStyle knobStyle;

    juce::Image plate;
    bool plateVisible = true;
    bool arrowHot = false;

    std::array<Knob, 6> knobs;             // volume bass middle treble depth speed
    cp80ui::SlideSwitch brilliance { { "HIGH", "MED", "LOW" }, true };
    cp80ui::SlideSwitch tremolo { { "ON", "OFF" }, true };
    juce::Label brillianceCaption, tremoloCaption;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::array<std::unique_ptr<SliderAttachment>, 6> attachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CP80PluginEditor)
};
