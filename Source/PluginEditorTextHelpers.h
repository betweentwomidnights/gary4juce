#pragma once

#include <JuceHeader.h>

namespace plugin_editor_detail
{
    inline juce::String stripAnsiAndControlChars(const juce::String& text)
    {
        juce::String sanitized;
        for (int i = 0; i < text.length(); ++i)
        {
            const auto ch = text[i];

            if (ch == 0x1b && (i + 1) < text.length() && text[i + 1] == '[')
            {
                i += 2;
                while (i < text.length() && ((text[i] >= '0' && text[i] <= '9') || text[i] == ';'))
                    ++i;
                continue;
            }

            if (ch < 32)
                continue;

            sanitized += ch;
        }

        return sanitized.trim();
    }
}
