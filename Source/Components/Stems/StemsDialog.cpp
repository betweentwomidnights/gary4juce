// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

#include "StemsDialog.h"
#include "StemsSettings.h"
#include "../../Utils/IconFactory.h"
#include "../../Utils/Theme.h"

namespace
{
    constexpr int kMargin = 16;
    constexpr int kTopBarHeight = 34;
    constexpr int kCellHeight = 104;
    constexpr int kCellGap = 10;
    constexpr int kNameHeight = 24;
    constexpr int kFooterHeight = 46;
    constexpr int kDialogWidth = 720;

    bool loadAudio(const juce::File& file, juce::AudioBuffer<float>& audio, double& sampleRate)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        if (reader == nullptr || reader->lengthInSamples <= 0)
            return false;
        audio.setSize((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read(&audio, 0, audio.getNumSamples(), 0, true, true);
        sampleRate = reader->sampleRate;
        return true;
    }
}

// --- peaks ---------------------------------------------------------------------------------------

std::vector<std::pair<float, float>> StemsDialog::computePeaks(const juce::AudioBuffer<float>& audio, int width)
{
    std::vector<std::pair<float, float>> peaks;
    const int numSamples = audio.getNumSamples();
    const int channels = audio.getNumChannels();
    if (width <= 0 || numSamples <= 0 || channels <= 0)
        return peaks;

    peaks.resize((size_t) width);
    const int samplesPerPixel = juce::jmax(1, numSamples / width);
    for (int x = 0; x < width; ++x)
    {
        const int start = x * samplesPerPixel;
        const int end = juce::jmin(start + samplesPerPixel, numSamples);
        float lo = 0.0f, hi = 0.0f;
        for (int i = start; i < end; ++i)
        {
            float value = 0.0f;
            for (int c = 0; c < channels; ++c)
                value += audio.getSample(c, i);
            value /= (float) channels;
            lo = juce::jmin(lo, value);
            hi = juce::jmax(hi, value);
        }
        peaks[(size_t) x] = { lo, hi };
    }
    return peaks;
}

void StemsDialog::drawPeaks(juce::Graphics& g, juce::Rectangle<int> area,
                            const std::vector<std::pair<float, float>>& peaks, float opacity)
{
    const int waveHeight = area.getHeight() - 2;
    const int centreY = area.getCentreY();
    g.setColour(juce::Colours::red.withAlpha(opacity));
    for (size_t x = 0; x < peaks.size(); ++x)
    {
        const int minY = juce::jlimit(area.getY(), area.getBottom(), centreY - (int) (peaks[x].first * waveHeight * 0.4f));
        const int maxY = juce::jlimit(area.getY(), area.getBottom(), centreY - (int) (peaks[x].second * waveHeight * 0.4f));
        const int drawX = area.getX() + 1 + (int) x;
        if (maxY != minY)
            g.drawVerticalLine(drawX, (float) maxY, (float) minY);
        else
            g.fillRect(drawX, centreY - 1, 1, 2);
    }
}

// --- one stem ------------------------------------------------------------------------------------

class StemsDialog::StemCell : public juce::Component,
                              public juce::SettableTooltipClient
{
public:
    StemCell(StemsDialog& dialog, const juce::String& stemName, const juce::File& stemFile)
        : owner(dialog), name(stemName), file(stemFile)
    {
        hasAudio = file.existsAsFile() && loadAudio(file, audio, sampleRate);

        playButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
        playButton.setIcon(IconFactory::createPlayIcon());
        playButton.setTooltip("play " + name);
        playButton.onClick = [this] { owner.host.togglePlayback(file); owner.updateControls(); };
        addChildComponent(playButton);

        stopButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
        stopButton.setIcon(IconFactory::createStopIcon());
        stopButton.setTooltip("stop");
        stopButton.onClick = [this] { owner.host.stopPlayback(file); repaint(); owner.updateControls(); };
        addChildComponent(stopButton);

        playButton.setVisible(hasAudio);
        stopButton.setVisible(hasAudio);
        setTooltip(hasAudio ? "click to seek, drag into your daw to keep " + name : juce::String());
    }

    bool isActive() const { return hasAudio && owner.host.activeStem() == file; }

    void update()
    {
        const bool playing = isActive() && owner.host.isPlaying();
        if (playing != showingPause)
        {
            showingPause = playing;
            playButton.setIcon(playing ? IconFactory::createPauseIcon() : IconFactory::createPlayIcon());
            repaint();   // the cursor dims or brightens with it
        }
        if (playing || owner.separating)
            repaint();
    }

    void resized() override
    {
        auto top = getLocalBounds().removeFromTop(kNameHeight);
        stopButton.setBounds(top.removeFromRight(36).reduced(2, 1));
        top.removeFromRight(4);
        playButton.setBounds(top.removeFromRight(36).reduced(2, 1));
        waveArea = getLocalBounds().withTrimmedTop(kNameHeight + 4);
        peaks = hasAudio ? computePeaks(audio, waveArea.getWidth() - 2) : decltype(peaks)();
    }

    void paint(juce::Graphics& g) override
    {
        g.setFont(Theme::Fonts::Header);
        g.setColour(hasAudio ? Theme::Colors::TextPrimary : Theme::Colors::TextSecondary);
        g.drawText(name, getLocalBounds().removeFromTop(kNameHeight), juce::Justification::centredLeft);

        g.setColour(juce::Colours::black);
        g.fillRect(waveArea);
        g.setColour(juce::Colour(0x40, 0x40, 0x40));
        g.drawRect(waveArea, 1);

        if (hasAudio)
        {
            drawPeaks(g, waveArea, peaks, 1.0f);
            if (dragging)
            {
                g.setColour(juce::Colours::white.withAlpha(0.15f));
                g.fillRect(waveArea.reduced(1));
            }
            drawCursor(g);
            return;
        }

        // Not separated yet: empty, with a hint, as the recording buffer is before it has audio.
        if (!owner.separating)
        {
            g.setFont(juce::FontOptions(14.0f));
            g.setColour(juce::Colours::darkgrey);
            g.drawText("choose a model and press separate", waveArea, juce::Justification::centred);
            return;
        }

        const int width = juce::roundToInt((waveArea.getWidth() - 2) * juce::jlimit(0.0, 1.0, owner.shownProgress));
        if (width > 0)
        {
            g.setColour(juce::Colours::red.withAlpha(0.4f));
            g.fillRect(waveArea.getX() + 1, waveArea.getY() + 1, width, waveArea.getHeight() - 2);
            g.setColour(juce::Colours::white.withAlpha(0.8f));
            g.drawVerticalLine(waveArea.getX() + 1 + width, (float) waveArea.getY(), (float) waveArea.getBottom());
        }
        g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        g.setColour(juce::Colours::white);
        g.drawText(owner.loadingModel ? juce::String("loading model")
                                      : "separating: " + juce::String(juce::roundToInt(owner.shownProgress * 100.0)) + "%",
                   waveArea, juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        dragStarted = false;
        downPosition = e.getPosition();
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!hasAudio || dragStarted || downPosition.getDistanceFrom(e.getPosition()) <= 10)
            return;
        dragStarted = true;
        dragging = true;
        repaint();
        owner.host.drag(file, name);
        dragging = false;
        repaint();
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (dragStarted || !hasAudio || !waveArea.contains(e.getPosition()) || duration() <= 0.0)
            return;
        const double fraction = juce::jlimit(0.0, 1.0, (double) (e.x - waveArea.getX() - 1) / juce::jmax(1, waveArea.getWidth() - 2));
        owner.host.seek(file, fraction * duration());
        owner.updateControls();
    }

private:
    double duration() const { return sampleRate > 0.0 ? audio.getNumSamples() / sampleRate : 0.0; }

    // Every stem keeps its own place: bright while it plays, dimmed where it was paused or seeked.
    void drawCursor(juce::Graphics& g)
    {
        if (duration() <= 0.0)
            return;
        const bool playing = isActive() && owner.host.isPlaying();
        const double position = owner.host.position(file);
        if (position <= 0.0 && !playing)
            return;
        const int x = waveArea.getX() + 1
            + (int) (juce::jlimit(0.0, 1.0, position / duration()) * (waveArea.getWidth() - 2));
        g.setColour(playing ? juce::Colours::white.withAlpha(0.9f) : juce::Colours::white.withAlpha(0.5f));
        g.drawVerticalLine(x, (float) waveArea.getY() + 1.0f, (float) waveArea.getBottom() - 1.0f);
    }

    StemsDialog& owner;
    juce::String name;
    juce::File file;
    juce::AudioBuffer<float> audio;
    double sampleRate = 0.0;
    bool hasAudio = false;
    std::vector<std::pair<float, float>> peaks;
    juce::Rectangle<int> waveArea;
    CustomButton playButton, stopButton;
    bool showingPause = false;
    juce::Point<int> downPosition;
    bool dragStarted = false, dragging = false;
};

// --- dialog --------------------------------------------------------------------------------------

StemsDialog::StemsDialog(std::shared_ptr<stems::StemsService> s, juce::PropertiesFile& prefs, Host h,
                         const std::function<bool(const juce::File&)>& writeSource, const juce::String& sourceName)
    : service(std::move(s)), preferences(prefs), host(std::move(h))
{
    // The stems belong to the audio as it is now, not to whatever replaces it later.
    session = service->createSessionDirectory();
    source = session.getChildFile("source.wav");
    sourceReady = writeSource(source) && source.getSize() > 0;
    if (!sourceReady)
        status.setText("could not read the " + sourceName, juce::dontSendNotification);

    title.setText(sourceName + " stems", juce::dontSendNotification);
    title.setFont(Theme::Fonts::HeaderLarge);
    title.setColour(juce::Label::textColourId, Theme::Colors::TextPrimary);
    addAndMakeVisible(title);

    const auto preferred = preferences.getValue(StemsSettings::kModelPreference, stems::defaultModelId());
    for (const auto& model : stems::modelCatalog())
    {
        if (!service->isModelInstalled(model.id))
            continue;
        modelIds.add(model.id);
        modelBox.addItem(model.id + "  (" + model.stemsSummary + ")", modelIds.size());
    }
    const int preferredIndex = modelIds.indexOf(preferred);
    modelBox.setSelectedId(preferredIndex >= 0 ? preferredIndex + 1 : 1, juce::dontSendNotification);
    modelBox.onChange = [this]
    {
        preferences.setValue(StemsSettings::kModelPreference, currentModelId());
        preferences.saveIfNeeded();
        status.setText({}, juce::dontSendNotification);
        rebuildCells();
    };
    addAndMakeVisible(modelBox);

    separateButton.setButtonStyle(CustomButton::ButtonStyle::Gary);
    separateButton.onClick = [this]
    {
        if (separating)
            service->cancelJob();
        else
            separate();
    };
    addAndMakeVisible(separateButton);

    status.setFont(Theme::Fonts::Body);
    status.setColour(juce::Label::textColourId, Theme::Colors::PrimaryRed);
    addAndMakeVisible(status);

    hint.setText("drag the stems you want into your daw. the rest are deleted when this closes.",
                 juce::dontSendNotification);
    hint.setFont(Theme::Fonts::Small);
    hint.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    addAndMakeVisible(hint);

    rebuildCells();
    updateControls();
    startTimerHz(30);
}

StemsDialog::~StemsDialog()
{
    stopTimer();
    if (separating)
        service->cancelJob();
    // Nothing may still be reading a stem when its folder goes.
    host.releasePlayback();
    cells.clear();
    service->deleteSessionDirectory(session);
}

juce::String StemsDialog::currentModelId() const
{
    const int index = modelBox.getSelectedId() - 1;
    return juce::isPositiveAndBelow(index, modelIds.size()) ? modelIds[index] : juce::String();
}

void StemsDialog::rebuildCells()
{
    cells.clear();
    const auto model = currentModelId();
    const auto found = separations.find(model);
    const bool separated = found != separations.end() && !(separating && separatingModel == model);
    const auto names = separated ? found->second.names : stems::stemNamesFor(model);
    for (int i = 0; i < names.size(); ++i)
    {
        const juce::File file = separated ? found->second.files[i] : juce::File();
        cells.push_back(std::make_unique<StemCell>(*this, names[i], file));
        addAndMakeVisible(*cells.back());
    }
    layoutWindow();
}

void StemsDialog::layoutWindow()
{
    // One column up to four stems; six (htdemucs_6s) as two columns of three.
    const int columns = cells.size() > 4 ? 2 : 1;
    const int rows = juce::jmax(1, ((int) cells.size() + columns - 1) / columns);
    const int height = kMargin + 30 + 8 + kTopBarHeight + 12 + rows * kCellHeight + (rows - 1) * kCellGap
                     + 8 + kFooterHeight + kMargin;
    const int width = columns == 2 ? kDialogWidth + 260 : kDialogWidth;
    if (getWidth() != width || getHeight() != height)
        setSize(width, height);   // a DialogWindow follows its content's size
    else
        resized();
}

void StemsDialog::separate()
{
    const auto model = currentModelId();
    if (model.isEmpty())
        return;
    if (host.isGenerating())
    {
        status.setText("wait for the current generation to finish", juce::dontSendNotification);
        return;
    }
    // A stem of this model may be playing, and its file is about to be rewritten.
    host.releasePlayback();
    const bool useGpu = preferences.getBoolValue(StemsSettings::kUseGpuPreference, true);
    if (!service->startSeparation(model, useGpu, source, session))
    {
        status.setText("the stem separator is busy with a download or test in settings",
                       juce::dontSendNotification);
        return;
    }
    separations.erase(model);
    separating = true;
    separatingModel = model;
    targetProgress = shownProgress = 0.0;
    loadingModel = true;
    status.setText({}, juce::dontSendNotification);
    rebuildCells();
    updateControls();
}

void StemsDialog::updateControls()
{
    separateButton.setButtonText(separating ? "cancel" : "separate");
    separateButton.setButtonStyle(separating ? CustomButton::ButtonStyle::Standard : CustomButton::ButtonStyle::Gary);
    separateButton.setEnabled(separating || (!host.isGenerating() && currentModelId().isNotEmpty()
                                             && sourceReady));
    modelBox.setEnabled(!separating);
}

void StemsDialog::timerCallback()
{
    if (separating)
    {
        const auto job = service->getJob();
        if (job.kind == stems::StemsService::JobKind::Separate && job.running)
        {
            loadingModel = job.progress < 0.0;
            targetProgress = juce::jmax(0.0, job.progress);
            // stems.cpp reports once per ~7.8 s segment, so ease toward each report and creep a
            // little past it, as the output waveform does between server updates.
            shownProgress += (targetProgress - shownProgress) * 0.2;
            if (!loadingModel && shownProgress < juce::jmin(targetProgress + 0.12, 0.97))
                shownProgress += 0.002;
        }
        else
        {
            separating = false;
            if (job.succeeded && job.kind == stems::StemsService::JobKind::Separate)
                separations[separatingModel] = { job.stemNames, job.stemFiles };
            else
                status.setText(job.error == "cancelled" ? juce::String("cancelled") : job.error,
                               juce::dontSendNotification);
            rebuildCells();
            updateControls();
        }
    }

    for (auto& cell : cells)
        cell->update();

    // A generation can start or end while this is open only before it took focus; keep the button honest.
    const bool canSeparate = separating || (!host.isGenerating() && currentModelId().isNotEmpty()
                                            && sourceReady);
    if (separateButton.isEnabled() != canSeparate)
        updateControls();
}

void StemsDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e1e));
}

void StemsDialog::resized()
{
    auto area = getLocalBounds().reduced(kMargin);
    title.setBounds(area.removeFromTop(30));
    area.removeFromTop(8);

    auto bar = area.removeFromTop(kTopBarHeight);
    separateButton.setBounds(bar.removeFromRight(120));
    bar.removeFromRight(10);
    modelBox.setBounds(bar);
    area.removeFromTop(12);

    auto footer = area.removeFromBottom(kFooterHeight);
    status.setBounds(footer.removeFromTop(22));
    hint.setBounds(footer);
    area.removeFromBottom(8);

    const int columns = cells.size() > 4 ? 2 : 1;
    const int rows = juce::jmax(1, ((int) cells.size() + columns - 1) / columns);
    const int cellWidth = (area.getWidth() - (columns - 1) * kCellGap) / columns;
    for (size_t i = 0; i < cells.size(); ++i)
    {
        const int column = (int) i / rows;   // fill down the first column, then the second
        const int row = (int) i % rows;
        cells[i]->setBounds(area.getX() + column * (cellWidth + kCellGap),
                            area.getY() + row * (kCellHeight + kCellGap),
                            cellWidth, kCellHeight);
    }
}
