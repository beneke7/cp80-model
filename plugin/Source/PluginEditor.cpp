#include "PluginEditor.h"
#include "PluginProcessor.h"

CP80PluginEditor::CP80PluginEditor(CP80PluginProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(760, 250);
    setResizable(true, false);

    title.setText("CP-80  ELECTRIC GRAND", juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions().withHeight(20.0f).withStyle("Bold")));
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title);

    patch.setText("PATCH OUT   ·   TONE / TREMOLO   ·   PATCH IN", juce::dontSendNotification);
    patch.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
    patch.setColour(juce::Label::textColourId, juce::Colour(0xff9da6b5));
    patch.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(patch);

    for (auto* slider : { &volume, &bass, &middle, &treble, &depth, &speed }) {
        slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
        slider->setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffd2a85e));
        slider->setColour(juce::Slider::thumbColourId, juce::Colours::white);
        addAndMakeVisible(*slider);
    }

    brilliance.addItem("Low", 1);
    brilliance.addItem("Medium", 2);
    brilliance.addItem("High", 3);
    addAndMakeVisible(brilliance);
    addAndMakeVisible(tremolo);

    const std::array<const char*, 8> names {
        "VOLUME", "BASS", "MIDDLE", "TREBLE", "BRILLIANCE", "TREMOLO", "DEPTH", "SPEED"
    };
    for (size_t i = 0; i < labels.size(); ++i) {
        labels[i].setText(names[i], juce::dontSendNotification);
        labels[i].setJustificationType(juce::Justification::centred);
        labels[i].setColour(juce::Label::textColourId, juce::Colour(0xffc2c9d3));
        labels[i].setFont(juce::Font(juce::FontOptions().withHeight(10.0f).withStyle("Bold")));
        addAndMakeVisible(labels[i]);
    }

    auto& state = processor.parameters;
    volumeAttachment = std::make_unique<SliderAttachment>(state, "volume", volume);
    bassAttachment = std::make_unique<SliderAttachment>(state, "bass", bass);
    middleAttachment = std::make_unique<SliderAttachment>(state, "middle", middle);
    trebleAttachment = std::make_unique<SliderAttachment>(state, "treble", treble);
    depthAttachment = std::make_unique<SliderAttachment>(state, "depth", depth);
    speedAttachment = std::make_unique<SliderAttachment>(state, "speed", speed);
    brillianceAttachment = std::make_unique<ComboAttachment>(state, "brilliance", brilliance);
    tremoloAttachment = std::make_unique<ButtonAttachment>(state, "tremolo", tremolo);
}

void CP80PluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff14171b));
    g.setColour(juce::Colour(0xff222831));
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(8.0f), 8.0f);
    g.setColour(juce::Colour(0xff0e1013));
    g.fillRoundedRectangle(juce::Rectangle<float>(18.0f, 58.0f,
                                                   float(getWidth() - 36), 166.0f), 5.0f);
    g.setColour(juce::Colour(0xffd2a85e));
    g.fillRect(18.0f, 56.0f, float(getWidth() - 36), 2.0f);
}

void CP80PluginEditor::resized()
{
    title.setBounds(24, 16, 300, 30);
    patch.setBounds(getWidth() - 390, 18, 360, 24);

    const int y = 78;
    const int h = 122;
    const int start = 30;
    const int gap = 118;
    const int w = 96;
    const std::array<juce::Slider*, 6> sliders { &volume, &bass, &middle, &treble, &depth, &speed };
    for (int i = 0; i < int(sliders.size()); ++i) {
        const int x = start + i * gap;
        sliders[size_t(i)]->setBounds(x, y, w, h);
        labels[size_t(i)].setBounds(x, 64, w, 16);
    }

    brilliance.setBounds(602, 96, 126, 26);
    labels[4].setBounds(602, 76, 126, 16);
    tremolo.setBounds(602, 142, 126, 26);
    labels[5].setBounds(602, 124, 126, 16);
    labels[6].setBounds(502, 64, 88, 16);
    labels[7].setBounds(502, 184, 88, 16);
}
