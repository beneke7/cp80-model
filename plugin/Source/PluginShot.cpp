// Offscreen editor snapshot, so the UI can be reviewed without launching a host.
#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

static void shoot(CP80PluginEditor& editor, const juce::File& out)
{
    const auto image = editor.createComponentSnapshot(editor.getLocalBounds(), false, 2.0f);
    out.deleteFile();
    juce::FileOutputStream stream(out);
    juce::PNGImageFormat png;
    png.writeImageToStream(image, stream);
    juce::Logger::outputDebugString(out.getFullPathName() + "  " +
                                    juce::String(image.getWidth()) + "x" +
                                    juce::String(image.getHeight()));
}

int main(int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI gui;
    const juce::File dir = juce::File::getCurrentWorkingDirectory()
                               .getChildFile(argc > 1 ? argv[1] : "out");
    dir.createDirectory();

    CP80PluginProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    auto* base = processor.createEditor();
    auto* editor = dynamic_cast<CP80PluginEditor*>(base);
    if (editor == nullptr) { delete base; return 1; }

    shoot(*editor, dir.getChildFile("ui-expanded.png"));
    editor->mouseDown(juce::MouseEvent(
        juce::Desktop::getInstance().getMainMouseSource(),
        { float(editor->getWidth() - 27), float(570 + 25) },
        juce::ModifierKeys::leftButtonModifier, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        editor, editor, juce::Time::getCurrentTime(), {}, juce::Time::getCurrentTime(), 1, false));
    shoot(*editor, dir.getChildFile("ui-collapsed.png"));

    delete base;
    return 0;
}
