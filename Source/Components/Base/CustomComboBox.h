#pragma once
#include <JuceHeader.h>
#include "../../Utils/Theme.h"

class CustomComboBox : public juce::ComboBox
{
public:
    CustomComboBox();
    ~CustomComboBox() override;

    // Easy theming interface
    void setThemeColors(juce::Colour background, juce::Colour text, juce::Colour outline);

    // Hierarchical menu structure for sub-menus
    struct MenuItem {
        juce::String name;
        int itemId;
        bool isSectionHeader;
        bool isSubMenu;
        std::vector<MenuItem> subItems;
    };

    // Set hierarchical menu items (for models with groups/sub-menus)
    void setHierarchicalItems(const std::vector<MenuItem>& items);

    // Check if using hierarchical mode
    bool isHierarchicalMode() const { return useHierarchicalMenu; }

protected:
    // Override to show custom hierarchical popup
    void showPopup() override;

private:
    void applyDefaultStyling();
    void buildHierarchicalMenu(juce::PopupMenu& menu, const std::vector<MenuItem>& items);

    bool useHierarchicalMenu = false;
    std::vector<MenuItem> hierarchicalItems;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomComboBox)
};