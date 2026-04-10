#pragma once

#include <JuceHeader.h>

namespace plugin_editor_detail
{
    inline int loopTypeStringToIndex(const juce::String& type)
    {
        if (type.equalsIgnoreCase("drums"))
            return 1;
        if (type.equalsIgnoreCase("instruments"))
            return 2;
        return 0;
    }

    inline juce::String loopTypeIndexToString(int index)
    {
        switch (index)
        {
        case 1: return "drums";
        case 2: return "instruments";
        default: return "auto";
        }
    }
}
