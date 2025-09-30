#pragma once

#include <JuceHeader.h>

#include "../Base/CustomButton.h"
#include "../Base/CustomSlider.h"
#include "../Base/CustomTextEditor.h"

#include <functional>

class JerryUI : public juce::Component
{
public:
    JerryUI();

    void paint(juce::Graphics&) override;
    void resized() override;

    void setVisibleForTab(bool visible);

    void setBpm(int bpm);
    void setPromptText(const juce::String& text);
    void setCfg(float value);
    void setSteps(int value);
    void setSmartLoop(bool enabled);
    void setLoopType(int index);
    void setButtonsEnabled(bool canGenerate, bool canSmartLoop, bool isGenerating);
    void setGenerateButtonText(const juce::String& text);

    juce::String getPromptText() const;
    float getCfg() const;
    int getSteps() const;
    bool getSmartLoop() const;
    int getLoopType() const;

    juce::Rectangle<int> getTitleBounds() const;

    std::function<void(const juce::String&)> onPromptChanged;
    std::function<void(float)> onCfgChanged;
    std::function<void(int)> onStepsChanged;
    std::function<void(bool)> onSmartLoopToggled;
    std::function<void(int)> onLoopTypeChanged;
    std::function<void()> onGenerate;

private:
    void refreshLoopTypeVisibility();
    void updateLoopTypeStyles();
    void updateSmartLoopStyle();
    void applyEnablement(bool canGenerate, bool canSmartLoop, bool isGenerating);

    juce::Label jerryLabel;
    juce::Label jerryPromptLabel;
    CustomTextEditor jerryPromptEditor;
    CustomSlider jerryCfgSlider;
    juce::Label jerryCfgLabel;
    CustomSlider jerryStepsSlider;
    juce::Label jerryStepsLabel;
    juce::Label jerryBpmLabel;
    CustomButton generateWithJerryButton;
    CustomButton generateAsLoopButton;
    CustomButton loopTypeAutoButton;
    CustomButton loopTypeDrumsButton;
    CustomButton loopTypeInstrumentsButton;

    juce::String promptText;
    float cfg { 1.0f };
    int steps { 8 };
    bool smartLoop { false };
    int loopTypeIndex { 0 };
    int bpmValue { 120 };

    bool lastCanGenerate { false };
    bool lastCanSmartLoop { false };
    bool lastIsGenerating { false };

    juce::Rectangle<int> titleBounds;
};
