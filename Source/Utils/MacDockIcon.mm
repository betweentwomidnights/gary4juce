#include "MacDockIcon.h"
#include <JuceHeader.h>

#if JUCE_MAC
 #import <AppKit/AppKit.h>
#endif

void applyStandaloneDockIconIfAvailable()
{
#if JUCE_MAC
    static bool applied = false;
    if (applied || !juce::JUCEApplicationBase::isStandaloneApp())
        return;

    if (BinaryData::icon_icns == nullptr || BinaryData::icon_icnsSize <= 0)
        return;

    @autoreleasepool
    {
        NSData* iconData = [NSData dataWithBytes: BinaryData::icon_icns
                                          length: static_cast<NSUInteger>(BinaryData::icon_icnsSize)];
        if (iconData == nil || [iconData length] == 0)
            return;

        if (NSImage* dockIcon = [[NSImage alloc] initWithData: iconData])
        {
            [NSApp setApplicationIconImage: dockIcon];
#if ! __has_feature(objc_arc)
            [dockIcon release];
#endif
            applied = true;
        }
    }
#endif
}
