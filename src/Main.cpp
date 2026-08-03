#include <juce_gui_extra/juce_gui_extra.h>

#include "ui/MainComponent.h"

namespace saamveda
{

class SaamVedaApplication : public juce::JUCEApplication
{
public:
    SaamVedaApplication() = default;

    const juce::String getApplicationName() override    { return "SaamVeda Studio"; }
    const juce::String getApplicationVersion() override  { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override           { return true; }

    void initialise (const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : DocumentWindow (name,
                              juce::Desktop::getInstance().getDefaultLookAndFeel()
                                  .findColour (juce::ResizableWindow::backgroundColourId),
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new ui::MainComponent(), true);
            setResizable (true, false);
            setResizeLimits (760, 520, 6000, 4000);

            // The content's preferred size is in logical units; on a scaled
            // display it can exceed the screen. Clamp to what actually fits so
            // the transport row is never off the bottom or right edge.
            auto available = juce::Desktop::getInstance().getDisplays()
                                 .getPrimaryDisplay()->userArea;
            centreWithSize (juce::jmin (getWidth(), available.getWidth() - 40),
                            juce::jmin (getHeight(), available.getHeight() - 40));
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

    std::unique_ptr<MainWindow> mainWindow;
};

} // namespace saamveda

START_JUCE_APPLICATION (saamveda::SaamVedaApplication)
