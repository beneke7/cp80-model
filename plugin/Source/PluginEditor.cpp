#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <BinaryData.h>

namespace cp80ui {

const Theme& theme()
{
    // #FFFFE6 is the brochure paper; the photograph's own background is #FFFFEB,
    // so the plate dissolves into the page instead of sitting in a frame.
    static const Theme paper {
        juce::Colour(0xffffffe6), juce::Colour(0xffffffe6), juce::Colour(0xff1a1a18),
        juce::Colour(0xff1a1a18).withAlpha(0.42f), juce::Colour(0xffc8462f),
        juce::Colour(0xff1a1a18), juce::Colour(0xff1a1a18), 1.5f
    };
    static const Theme plate {
        juce::Colour(0xffffffeb), juce::Colour(0xff101110), juce::Colour(0xffffffeb),
        juce::Colour(0xffffffeb).withAlpha(0.38f), juce::Colour(0xfff5b245),
        juce::Colour(0xffdb5841), juce::Colour(0xffdb5841), 3.0f
    };
    return kPaperFinish ? paper : plate;
}

namespace {
// Ask for the specified faces; fall back to whatever the machine actually has.
juce::String pick(const juce::StringArray& wanted)
{
    static const juce::StringArray installed = juce::Font::findAllTypefaceNames();
    for (const auto& name : wanted)
        if (installed.contains(name))
            return name;
    return juce::Font::getDefaultSansSerifFontName();
}
} // namespace

juce::Font condensed(float height, bool bold)
{
    static const juce::String face = pick({ "Helvetica Neue Condensed Bold",
                                            "HelveticaNeue-CondensedBold",
                                            "Univers Condensed", "Helvetica Neue",
                                            "Helvetica", "Arial Narrow" });
    return juce::Font(juce::FontOptions().withName(face).withHeight(height)
                          .withStyle(bold ? "Bold" : "Regular"));
}

juce::Font display(float height)
{
    static const juce::String face = pick({ "Futura PT Extra Bold", "FuturaPT-ExtraBold",
                                            "Futura Bold", "Futura",
                                            "Helvetica Neue Condensed Bold", "Helvetica" });
    return juce::Font(juce::FontOptions().withName(face).withHeight(height).withStyle("Bold"));
}

// ------------------------------------------------------------------ SlideSwitch
SlideSwitch::SlideSwitch(juce::StringArray positionLabels, bool topIsHighest)
    : labels(std::move(positionLabels)), highestAtTop(topIsHighest)
{
}

void SlideSwitch::setPosition(int newIndex, juce::NotificationType notify)
{
    newIndex = juce::jlimit(0, labels.size() - 1, newIndex);
    if (newIndex == index)
        return;
    index = newIndex;
    repaint();
    if (notify != juce::dontSendNotification && onChange)
        onChange(index);
}

int SlideSwitch::positionAt(juce::Point<int> p) const
{
    const float step = float(getHeight()) / float(labels.size());
    return juce::jlimit(0, labels.size() - 1, int(float(p.y) / juce::jmax(step, 1.0f)));
}

void SlideSwitch::mouseDown(const juce::MouseEvent& e) { setPosition(positionAt(e.getPosition()), juce::sendNotification); }
void SlideSwitch::mouseDrag(const juce::MouseEvent& e) { setPosition(positionAt(e.getPosition()), juce::sendNotification); }

void SlideSwitch::paint(juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    const float step = area.getHeight() / float(labels.size());
    const float trackW = 13.0f;
    const auto track = juce::Rectangle<float>(area.getX(), area.getY(), trackW, area.getHeight());

    g.setColour(theme().dim.withAlpha(0.22f));
    g.fillRoundedRectangle(track, trackW * 0.5f);

    // the travelling cap
    const auto cap = juce::Rectangle<float>(track.getX(), area.getY() + float(index) * step,
                                            trackW, step).reduced(1.5f, 2.0f);
    g.setColour(theme().arc);
    g.fillRoundedRectangle(cap, cap.getWidth() * 0.5f);

    for (int i = 0; i < labels.size(); ++i) {
        g.setColour(i == index ? theme().text : theme().dim);
        g.setFont(condensed(10.5f, true));
        g.drawText(labels[i],
                   juce::Rectangle<float>(track.getRight() + 6.0f, area.getY() + float(i) * step,
                                          area.getWidth() - trackW - 6.0f, step),
                   juce::Justification::centredLeft, false);
    }
}

// ------------------------------------------------------------------ KnobStyle
void KnobStyle::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                 float pos, float startAngle, float endAngle,
                                 juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int>(x, y, w, h).toFloat().reduced(3.0f);
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    g.setColour(theme().dim.withAlpha(0.9f));
    g.drawEllipse(juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre), 1.4f);

    // value arc; bipolar controls sweep out from twelve o'clock
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float origin = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    juce::Path arc;
    arc.addCentredArc(centre.x, centre.y, radius - 3.0f, radius - 3.0f, 0.0f,
                      juce::jmin(origin, angle), juce::jmax(origin, angle), true);
    g.setColour(theme().arc);
    g.strokePath(arc, juce::PathStrokeType(2.6f, juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));

    g.setColour(theme().pointer);
    juce::Path pointer;
    pointer.addRoundedRectangle(-1.3f, -radius + 5.0f, 2.6f, radius * 0.62f, 1.3f);
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre));
}
} // namespace cp80ui

// ------------------------------------------------------------------ Editor
namespace {
constexpr const char* kNames[6] { "VOLUME", "BASS", "MIDDLE", "TREBLE", "DEPTH", "SPEED" };
constexpr const char* kIds[6] { "volume", "bass", "middle", "treble", "depth", "speed" };

constexpr float kCaptionFont = 11.0f;
constexpr float kReadoutFont = 13.0f;

// One decimal, always.  The parameters carry intervals of 0.001 to 0.1, so the
// host's own text would read "0.800" next to "5.5" and look ragged.
juce::String readout(const juce::Slider& s, const juce::String& suffix)
{
    const juce::String text(s.getValue(), 1);
    return suffix.isEmpty() ? text : text + " " + suffix;
}
} // namespace

CP80PluginEditor::CP80PluginEditor(CP80PluginProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    plate = juce::ImageCache::getFromMemory(BinaryData::cp80_png, BinaryData::cp80_pngSize);

    auto& state = processor.parameters;
    for (size_t i = 0; i < knobs.size(); ++i) {
        auto& k = knobs[i];
        k.name = kNames[i];
        k.detented = (i >= 1 && i <= 3);          // BASS / MIDDLE / TREBLE detent at flat
        if (auto* param = state.getParameter(kIds[i]))
            k.suffix = param->getLabel();

        k.slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        k.slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        k.slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                                     juce::MathConstants<float>::pi * 2.75f, true);
        k.slider.setLookAndFeel(&knobStyle);
        if (k.detented)
            k.slider.setDoubleClickReturnValue(true, 0.0);
        // caption doubles as the readout while you are turning it
        k.slider.onDragStart = [this, i] {
            auto& k2 = knobs[i];
            k2.caption.setFont(cp80ui::condensed(kReadoutFont, true));
            k2.caption.setText(readout(k2.slider, k2.suffix), juce::dontSendNotification);
            k2.caption.setColour(juce::Label::textColourId, cp80ui::theme().arc);
        };
        k.slider.onValueChange = [this, i] {
            auto& k2 = knobs[i];
            if (k2.slider.isMouseButtonDown())
                k2.caption.setText(readout(k2.slider, k2.suffix), juce::dontSendNotification);
        };
        k.slider.onDragEnd = [this, i] {
            auto& k2 = knobs[i];
            k2.caption.setFont(cp80ui::condensed(kCaptionFont, true));
            k2.caption.setText(k2.name, juce::dontSendNotification);
            k2.caption.setColour(juce::Label::textColourId, cp80ui::theme().text.withAlpha(0.78f));
        };
        addAndMakeVisible(k.slider);

        k.caption.setText(k.name, juce::dontSendNotification);
        k.caption.setJustificationType(juce::Justification::centred);
        k.caption.setFont(cp80ui::condensed(kCaptionFont, true));
        k.caption.setColour(juce::Label::textColourId, cp80ui::theme().text.withAlpha(0.78f));
        k.caption.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(k.caption);

        attachments[i] = std::make_unique<SliderAttachment>(state, kIds[i], k.slider);
    }

    for (auto* c : { &brillianceCaption, &tremoloCaption }) {
        c->setJustificationType(juce::Justification::centred);
        c->setFont(cp80ui::condensed(kCaptionFont, true));
        c->setColour(juce::Label::textColourId, cp80ui::theme().text.withAlpha(0.78f));
        c->setInterceptsMouseClicks(false, false);
        addAndMakeVisible(*c);
    }
    brillianceCaption.setText("BRILLIANCE", juce::dontSendNotification);
    tremoloCaption.setText("TREMOLO", juce::dontSendNotification);

    // The switches drive the parameters directly: a 3-way choice and a bool do not
    // need an attachment class each, and the parameter is the single source of truth.
    brillianceParam = state.getParameter("brilliance");
    tremoloParam = state.getParameter("tremolo");
    if (brillianceParam != nullptr) {
        auto* param = brillianceParam;
        brilliance.onChange = [param](int i) {
            param->setValueNotifyingHost(float(2 - i) / 2.0f);   // HIGH at top
        };
    }
    if (tremoloParam != nullptr) {
        auto* param = tremoloParam;
        tremolo.onChange = [param](int i) { param->setValueNotifyingHost(i == 0 ? 1.0f : 0.0f); };
    }
    addAndMakeVisible(brilliance);
    addAndMakeVisible(tremolo);
    timerCallback();
    startTimerHz(20);

    plateVisible = processor.plateVisible;
    applyPlateVisibility();
}

CP80PluginEditor::~CP80PluginEditor()
{
    stopTimer();
    for (auto& k : knobs)
        k.slider.setLookAndFeel(nullptr);
}

void CP80PluginEditor::timerCallback()
{
    if (brillianceParam != nullptr)
        brilliance.setPosition(2 - int(brillianceParam->getValue() * 2.0f + 0.5f),
                               juce::dontSendNotification);
    if (tremoloParam != nullptr)
        tremolo.setPosition(tremoloParam->getValue() >= 0.5f ? 0 : 1,
                            juce::dontSendNotification);
}

void CP80PluginEditor::applyPlateVisibility()
{
    processor.plateVisible = plateVisible;
    setSize(kWidth, plateVisible ? kPlateHeight + kStripHeight : kStripHeight);
}

juce::Rectangle<int> CP80PluginEditor::arrowBounds() const
{
    return { kWidth - 40, (plateVisible ? kPlateHeight : 0) + 12, 26, 26 };
}

void CP80PluginEditor::mouseDown(const juce::MouseEvent& e)
{
    if (arrowBounds().contains(e.getPosition())) {
        plateVisible = !plateVisible;
        applyPlateVisibility();
        repaint();
    }
}

void CP80PluginEditor::mouseMove(const juce::MouseEvent& e)
{
    const bool hot = arrowBounds().contains(e.getPosition());
    if (hot != arrowHot) { arrowHot = hot; repaint(arrowBounds().expanded(4)); }
}

void CP80PluginEditor::mouseExit(const juce::MouseEvent&)
{
    if (arrowHot) { arrowHot = false; repaint(arrowBounds().expanded(4)); }
}

void CP80PluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(cp80ui::theme().page);

    if (plateVisible && plate.isValid())
        g.drawImage(plate, juce::Rectangle<int>(0, 0, kWidth, kPlateHeight).toFloat(),
                    juce::RectanglePlacement::centred);

    const auto& t = cp80ui::theme();
    const auto strip = getLocalBounds().removeFromBottom(kStripHeight);
    g.setColour(t.strip);
    g.fillRect(strip);

    // The rule separates strip from photograph.  Collapsed, there is nothing above it
    // to separate, so it goes away and the strip is just a page.
    if (plateVisible) {
        const int margin = cp80ui::kPaperFinish ? 26 : 0;
        g.setColour(t.rule);
        g.fillRect(float(strip.getX() + margin), float(strip.getY()),
                   float(strip.getWidth() - 2 * margin), t.ruleHeight);
    }

    g.setColour(t.text);
    g.setFont(cp80ui::display(26.0f));
    g.drawText("CP-80", strip.getX() + 26, strip.getY() + 12, 140, 28,
               juce::Justification::centredLeft, false);
    g.setColour(t.text.withAlpha(0.55f));
    g.setFont(cp80ui::condensed(11.0f, true));
    g.drawText("ELECTRIC GRAND", strip.getX() + 118, strip.getY() + 17, 200, 20,
               juce::Justification::centredLeft, false);

    // collapse / expand arrow
    const auto a = arrowBounds().toFloat();
    g.setColour(t.text.withAlpha(arrowHot ? 0.95f : 0.5f));
    juce::Path tri;
    const float cx = a.getCentreX(), cy = a.getCentreY(), s = 5.0f;
    if (plateVisible) { tri.startNewSubPath(cx - s, cy + s * 0.5f); tri.lineTo(cx, cy - s * 0.6f); tri.lineTo(cx + s, cy + s * 0.5f); }
    else              { tri.startNewSubPath(cx - s, cy - s * 0.5f); tri.lineTo(cx, cy + s * 0.6f); tri.lineTo(cx + s, cy - s * 0.5f); }
    g.strokePath(tri, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));
}

void CP80PluginEditor::resized()
{
    layoutStrip(getLocalBounds().removeFromBottom(kStripHeight));
}

void CP80PluginEditor::layoutStrip(juce::Rectangle<int> strip)
{
    // Hardware order: VOLUME BASS MIDDLE TREBLE | BRILLIANCE TREMOLO | DEPTH SPEED
    auto row = strip.withTrimmedTop(50).withTrimmedBottom(14).withTrimmedLeft(20).withTrimmedRight(20);
    const int cells = 8;
    const int cellW = row.getWidth() / cells;
    const int knobD = 52;

    auto placeKnob = [&](Knob& k, juce::Rectangle<int> cell) {
        k.slider.setBounds(juce::Rectangle<int>(knobD, knobD)
                               .withCentre({ cell.getCentreX(), cell.getY() + knobD / 2 }));
        k.caption.setBounds(cell.getX(), cell.getY() + knobD + 7, cell.getWidth(), 16);
    };

    for (int i = 0; i < 4; ++i)
        placeKnob(knobs[size_t(i)], row.withX(row.getX() + i * cellW).withWidth(cellW));

    auto switchCell = [&](int slot, cp80ui::SlideSwitch& sw, juce::Label& cap) {
        auto cell = row.withX(row.getX() + slot * cellW).withWidth(cellW);
        sw.setBounds(juce::Rectangle<int>(64, 46)
                         .withCentre({ cell.getCentreX() + 6, cell.getY() + knobD / 2 }));
        cap.setBounds(cell.getX(), cell.getY() + knobD + 7, cell.getWidth(), 16);
    };
    switchCell(4, brilliance, brillianceCaption);
    switchCell(5, tremolo, tremoloCaption);

    for (int i = 4; i < 6; ++i)
        placeKnob(knobs[size_t(i)], row.withX(row.getX() + (i + 2) * cellW).withWidth(cellW));
}
