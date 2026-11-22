#include "MacDockIcon.h"
#include <JuceHeader.h>

#if JUCE_MAC
 #import <AppKit/AppKit.h>
#endif

// The standalone's Dock icon. The Xcode exporter puts the icon in the bundle's
// Resources, so it is read from there rather than embedded in BinaryData, which
// would also carry it into the Windows and Linux binaries.
void applyStandaloneDockIconIfAvailable()
{
#if JUCE_MAC
    static bool applied = false;
    if (applied || !juce::JUCEApplicationBase::isStandaloneApp())
        return;

    @autoreleasepool
    {
        NSBundle* bundle = [NSBundle mainBundle];
        NSImage* dockIcon = [bundle imageForResource: @"icon"];
        if (dockIcon == nil)
            dockIcon = [bundle imageForResource: @"Icon"];
        if (dockIcon == nil)
            return;

        [NSApp setApplicationIconImage: dockIcon];
        applied = true;
    }
#endif
}
