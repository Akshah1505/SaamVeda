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
            centreWithSize (getWidth(), getHeight());
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
