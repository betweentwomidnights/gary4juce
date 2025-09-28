#pragma once

#include <JuceHeader.h>

#include "../Base/CustomButton.h"
#include "../Base/CustomSlider.h"
#include "../Base/CustomTextEditor.h"

#include <functional>
#include <vector>

class DariusUI : public juce::Component
{
public:
    enum class SubTab
    {
        Backend = 0,
        Model,
        Generation
    };

    DariusUI();

    // juce::Component
    void paint(juce::Graphics&) override;
    void resized() override;

    // View setters
    void setBackendUrl(const juce::String& url);
    void setConnectionStatusText(const juce::String& text);
    void setConnectionStatus(const juce::String& text, juce::Colour colour);
    void setConnected(bool connected);
    void setUsingBaseModel(bool useBaseModel);
    void setFinetuneRepo(const juce::String& repoText);
    void setCheckpointSteps(const juce::Array<int>& steps);
    void setSelectedCheckpointStep(const juce::String& stepText);
    void setIsFetchingCheckpoints(bool isFetching);
    void requestOpenCheckpointMenuAfterFetch();
    void setApplyInProgress(bool isApplying);
    void setWarmInProgress(bool isWarming);
    void setHealthCheckInProgress(bool inProgress);
    void startWarmDots();
    void stopWarmDots();
    void setModelStatus(const juce::String& size,
                        const juce::String& repo,
                        const juce::String& step,
                        bool loaded,
                        bool warm);
    void setBpm(double bpm);
    void setSavedRecordingAvailable(bool available);
    void setOutputAudioAvailable(bool available);
    void setAudioSourceRecording(bool useRecording);
    void setGenerating(bool generating);
    void setCurrentSubTab(SubTab tab);
    void setSteeringAssets(bool meanAvailable,
                           int centroidCount,
                           const std::vector<double>& centroidWeights);
    void setSteeringWeights(const std::vector<double>& centroidWeights);
    void setMeanValue(double mean);
    void setSteeringOpen(bool open);
    void setAdvancedOpen(bool open);

    // View getters
    juce::String getBackendUrl() const;
    bool getUsingBaseModel() const;
    juce::String getFinetuneRepo() const;
    juce::String getSelectedCheckpointStep() const;
    juce::String getStylesCSV() const;
    juce::String getStyleWeightsCSV() const;
    double getLoopInfluence() const;
    double getTemperature() const;
    int getTopK() const;
    double getGuidance() const;
    int getBars() const;
    double getBpm() const;
    bool getAudioSourceUsesRecording() const;
    double getMean() const;
    const std::vector<double>& getCentroidWeights() const;

    // Behaviour helpers
    void refreshModelControls();
    void refreshCheckpointButton();
    void ensureStylesRowCount(int rows);
    void openCheckpointMenu();

    // Callbacks wired by PluginEditor
    std::function<void(const juce::String&)> onUrlChanged;
    std::function<void()> onHealthCheckRequested;
    std::function<void()> onRefreshConfigRequested;
    std::function<void()> onFetchCheckpointsRequested;
    std::function<void()> onOpenCheckpointMenuRequested;
    std::function<void(const juce::String&)> onFinetuneRepoChanged;
    std::function<void(bool)> onUseBaseModelToggled;
    std::function<void()> onApplyWarmRequested;
    std::function<void()> onGenerateRequested;
    std::function<void(bool)> onAudioSourceChanged;
    std::function<void(const juce::String&)> onCheckpointSelected;

private:
    void addGenStyleRowInternal(const juce::String& text, double weight);
    void rebuildGenStylesUI();
    void layoutGenStylesUI(juce::Rectangle<int>& area);
    void layoutGenAdvancedUI(juce::Rectangle<int>& area);
    void layoutGenSteeringUI(juce::Rectangle<int>& area);
    void updateGenLoopLabel();
    void updateGenAdvancedToggleText();
    void updateGenBarsButtons();
    void updateGenSourceButtons();
    void updateGenSourceEnabled();
    void updateSubTabStates();
    void updateGenSteeringToggleText();
    void rebuildGenCentroidRows();

    void handleCheckpointButtonClicked();
    void handleAddStyleRow();
    void handleRemoveStyleRow(int index);

    struct GenStyleRow
    {
        std::unique_ptr<juce::TextEditor> text;
        std::unique_ptr<juce::Slider> weight;
        std::unique_ptr<CustomButton> remove;
    };

    // Backend
    juce::Label dariusLabel;
    juce::TextEditor dariusUrlEditor;
    juce::Label dariusUrlLabel;
    CustomButton dariusHealthCheckButton;
    juce::Label dariusStatusLabel;

    // Subtabs
    SubTab currentSubTab = SubTab::Backend;
    CustomButton dariusBackendTabButton;
    CustomButton dariusModelTabButton;
    CustomButton dariusGenerationTabButton;

    // Model subtab
    std::unique_ptr<juce::Viewport> dariusModelViewport;
    std::unique_ptr<juce::Component> dariusModelContent;
    juce::Label dariusModelHeaderLabel;
    juce::Label dariusModelGuardLabel;
    CustomButton dariusRefreshConfigButton;
    juce::ToggleButton dariusUseBaseModelToggle;
    juce::Label dariusRepoFieldLabel;
    juce::TextEditor dariusRepoField;
    CustomButton dariusCheckpointButton;
    CustomButton dariusApplyWarmButton;
    juce::Label dariusWarmStatusLabel;
    juce::Label dariusActiveSizeLabel;
    juce::Label dariusRepoLabel;
    juce::Label dariusStepLabel;
    juce::Label dariusLoadedLabel;
    juce::Label dariusWarmupLabel;

    // Generation subtab
    std::unique_ptr<juce::Viewport> dariusGenerationViewport;
    std::unique_ptr<juce::Component> dariusGenerationContent;

    juce::Label genStylesHeaderLabel;
    CustomButton genAddStyleButton;
    std::vector<GenStyleRow> genStyleRows;
    int genStylesMax = 4;

    juce::Label genLoopLabel;
    juce::Slider genLoopSlider;
    double genLoopInfluence = 0.5;

    CustomButton genAdvancedToggle;
    bool genAdvancedOpen = false;
    juce::Label genTempLabel;
    juce::Slider genTempSlider;
    double genTemperature = 1.2;
    juce::Label genTopKLabel;
    juce::Slider genTopKSlider;
    int genTopK = 40;
    juce::Label genGuidanceLabel;
    juce::Slider genGuidanceSlider;
    double genGuidance = 5.0;

    juce::Label genBarsLabel;
    CustomButton genBars4Button;
    CustomButton genBars8Button;
    CustomButton genBars16Button;
    int genBars = 4;

    juce::Label genBpmLabel;
    juce::Label genBpmValueLabel;
    double genBPM = 120.0;

    juce::Label genSourceLabel;
    CustomButton genRecordingButton;
    CustomButton genOutputButton;
    juce::Label genSourceGuardLabel;
    enum class GenAudioSource { Recording, Output };
    GenAudioSource genAudioSource = GenAudioSource::Output;

    CustomButton genGenerateButton;
    bool genIsGenerating = false;

    CustomButton genSteeringToggle;
    bool genSteeringOpen = false;
    juce::Label genMeanLabel;
    juce::Slider genMeanSlider;
    double genMean = 1.0;
    juce::Label genCentroidsHeaderLabel;
    juce::OwnedArray<juce::Label> genCentroidLabels;
    juce::OwnedArray<juce::Slider> genCentroidSliders;
    std::vector<double> genCentroidWeights;
    static constexpr int kMaxCentroidsUI = 5;

    // State used by UI updates
    bool connected = false;
    bool useBaseModel = true;
    bool isFetchingCheckpoints = false;
    bool openMenuAfterFetch = false;
    juce::Array<int> checkpointSteps;
    juce::String selectedCheckpointStep { "latest" };
    bool isApplying = false;
    bool isWarming = false;
    bool savedRecordingAvailable = false;
    bool healthCheckInProgress = false;
    bool outputAudioAvailable = false;
    bool steeringMeanAvailable = false;
    int steeringCentroidCount = 0;
    bool warmDotsTicking = false;
    int warmDots = 0;
    juce::String backendUrl;
    juce::String connectionStatusText { "not checked" };
    juce::String finetuneRepoText;
};
