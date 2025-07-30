#include "IconFactory.h"

std::unique_ptr<juce::Drawable> IconFactory::createFromSvg(const char* svgData)
{
    return juce::Drawable::createFromImageData(svgData, strlen(svgData));
}

std::unique_ptr<juce::Drawable> IconFactory::createCropIcon()
{
    // Fixed SVG with explicit white color instead of currentColor
    const char* cropSvg = R"(
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="white" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
<path d="M6.13 1L6 16a2 2 0 0 0 2 2h15"></path>
<path d="M1 6.13L16 6a2 2 0 0 1 2 2v15"></path>
</svg>)";

    auto drawable = createFromSvg(cropSvg);
    if (!drawable)
    {
        DBG("Failed to create crop icon from SVG");
    }
    return drawable;
}

std::unique_ptr<juce::Drawable> IconFactory::createCheckConnectionIcon()
{
    // Bar chart SVG with white stroke to match theme
    const char* barChartSvg = R"(
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="white" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
<line x1="12" y1="20" x2="12" y2="10"></line>
<line x1="18" y1="20" x2="18" y2="4"></line>
<line x1="6" y1="20" x2="6" y2="16"></line>
</svg>)";

    auto drawable = createFromSvg(barChartSvg);
    if (!drawable)
    {
        DBG("Failed to create check connection icon from SVG");
    }
    return drawable;
}

std::unique_ptr<juce::Drawable> IconFactory::createTrashIcon()
{
    // Trash SVG with white stroke to match theme
    const char* trashSvg = R"(
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="white" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
<polyline points="3 6 5 6 21 6"></polyline>
<path d="m19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"></path>
<line x1="10" y1="11" x2="10" y2="17"></line>
<line x1="14" y1="11" x2="14" y2="17"></line>
</svg>)";

    auto drawable = createFromSvg(trashSvg);
    if (!drawable)
    {
        DBG("Failed to create trash icon from SVG");
    }
    return drawable;
}

std::unique_ptr<juce::Drawable> IconFactory::createPlayIcon()
{
    // Play SVG with white stroke to match theme
    const char* playSvg = R"(
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="white" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
<polygon points="5 3 19 12 5 21 5 3"></polygon>
</svg>)";

    auto drawable = createFromSvg(playSvg);
    if (!drawable)
    {
        DBG("Failed to create play icon from SVG");
    }
    return drawable;
}

std::unique_ptr<juce::Drawable> IconFactory::createPauseIcon()
{
    // Pause SVG with white stroke to match theme
    const char* pauseSvg = R"(
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="white" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
<rect x="6" y="4" width="4" height="16"></rect>
<rect x="14" y="4" width="4" height="16"></rect>
</svg>)";

    auto drawable = createFromSvg(pauseSvg);
    if (!drawable)
    {
        DBG("Failed to create pause icon from SVG");
    }
    return drawable;
}

std::unique_ptr<juce::Drawable> IconFactory::createStopIcon()
{
    // Square SVG with white stroke to match theme
    const char* stopSvg = R"(
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="white" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
<rect x="3" y="3" width="18" height="18" rx="2" ry="2"></rect>
</svg>)";

    auto drawable = createFromSvg(stopSvg);
    if (!drawable)
    {
        DBG("Failed to create stop icon from SVG");
    }
    return drawable;
}

std::unique_ptr<juce::Drawable> IconFactory::createHelpIcon()
{
    // Help circle SVG with white stroke to match theme
    const char* helpSvg = R"(
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="white" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
<circle cx="12" cy="12" r="10"></circle>
<path d="M9.09 9a3 3 0 0 1 5.83 1c0 2-3 3-3 3"></path>
<line x1="12" y1="17" x2="12" y2="17.01"></line>
</svg>)";

    auto drawable = createFromSvg(helpSvg);
    if (!drawable)
    {
        DBG("failed to create help icon from svg");
    }
    return drawable;
}

std::unique_ptr<juce::Drawable> IconFactory::createDiscordIcon()
{
    // Discord SVG with white fill to match theme
    const char* discordSvg = R"(
<svg fill="white" width="24" height="24" viewBox="0 0 24 24" role="img" xmlns="http://www.w3.org/2000/svg">
<path d="M20.317 4.37a19.791 19.791 0 0 0-4.885-1.515.074.074 0 0 0-.079.037c-.21.375-.444.864-.608 1.25a18.27 18.27 0 0 0-5.487 0 12.64 12.64 0 0 0-.617-1.25.077.077 0 0 0-.079-.037A19.736 19.736 0 0 0 3.677 4.37a.07.07 0 0 0-.032.027C.533 9.046-.32 13.58.099 18.057a.082.082 0 0 0 .031.057 19.9 19.9 0 0 0 5.993 3.03.078.078 0 0 0 .084-.028 14.09 14.09 0 0 0 1.226-1.994.076.076 0 0 0-.041-.106 13.107 13.107 0 0 1-1.872-.892.077.077 0 0 1-.008-.128 10.2 10.2 0 0 0 .372-.292.074.074 0 0 1 .077-.01c3.928 1.793 8.18 1.793 12.062 0a.074.074 0 0 1 .078.01c.12.098.246.198.373.292a.077.077 0 0 1-.006.127 12.299 12.299 0 0 1-1.873.892.077.077 0 0 0-.041.107c.36.698.772 1.362 1.225 1.993a.076.076 0 0 0 .084.028 19.839 19.839 0 0 0 6.002-3.03.077.077 0 0 0 .032-.054c.5-5.177-.838-9.674-3.549-13.66a.061.061 0 0 0-.031-.03zM8.02 15.33c-1.183 0-2.157-1.085-2.157-2.419 0-1.333.956-2.419 2.157-2.419 1.21 0 2.176 1.096 2.157 2.42 0 1.333-.956 2.418-2.157 2.418zm7.975 0c-1.183 0-2.157-1.085-2.157-2.419 0-1.333.955-2.419 2.157-2.419 1.21 0 2.176 1.096 2.157 2.42 0 1.333-.946 2.418-2.157 2.418z"/>
</svg>)";

    auto drawable = createFromSvg(discordSvg);
    if (!drawable)
    {
        DBG("Failed to create Discord icon from SVG");
    }
    return drawable;
}

std::unique_ptr<juce::Drawable> IconFactory::createXIcon()
{
    // X (Twitter) SVG with white fill to match theme
    const char* xSvg = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 50 50" width="24px" height="24px">
<path fill="white" d="M 11 4 C 7.134 4 4 7.134 4 11 L 4 39 C 4 42.866 7.134 46 11 46 L 39 46 C 42.866 46 46 42.866 46 39 L 46 11 C 46 7.134 42.866 4 39 4 L 11 4 z M 13.085938 13 L 21.023438 13 L 26.660156 21.009766 L 33.5 13 L 36 13 L 27.789062 22.613281 L 37.914062 37 L 29.978516 37 L 23.4375 27.707031 L 15.5 37 L 13 37 L 22.308594 26.103516 L 13.085938 13 z M 16.914062 15 L 31.021484 35 L 34.085938 35 L 19.978516 15 L 16.914062 15 z"/>
</svg>)";

    auto drawable = createFromSvg(xSvg);
    if (!drawable)
    {
        DBG("Failed to create X icon from SVG");
    }
    return drawable;
}

juce::Image IconFactory::loadLogoImage()
{
    return juce::ImageCache::getFromMemory(BinaryData::gary4live_logo_png, BinaryData::gary4live_logo_pngSize);
}