#include "CustomComboBox.h"

CustomComboBox::CustomComboBox()
{
    applyDefaultStyling();
}

CustomComboBox::~CustomComboBox()
{
}

void CustomComboBox::setThemeColors(juce::Colour background, juce::Colour text, juce::Colour outline)
{
    setColour(juce::ComboBox::backgroundColourId, background);
    setColour(juce::ComboBox::textColourId, text);
    setColour(juce::ComboBox::outlineColourId, outline);
    setColour(juce::ComboBox::arrowColourId, text);

    repaint();
}

void CustomComboBox::applyDefaultStyling()
{
    // Apply theme colors
    setThemeColors(
        Theme::Colors::ButtonInactive,
        Theme::Colors::TextPrimary,
        Theme::Colors::TextSecondary
    );

    // Set consistent font
  //  setFont(Theme::Fonts::Body);
}

void CustomComboBox::setHierarchicalItems(const std::vector<MenuItem>& items)
{
    hierarchicalItems = items;
    useHierarchicalMenu = true;

    // Clear existing items and build flat list for display
    clear(juce::dontSendNotification);

    // Build a flat list of all selectable items for the ComboBox text display
    // We only add items that have IDs (actual selectable items, not headers/submenus)
    for (const auto& item : items)
    {
        if (item.isSectionHeader)
        {
            // Skip section headers in the flat list
            continue;
        }
        else if (item.isSubMenu && !item.subItems.empty())
        {
            // Add all sub-items from this group
            for (const auto& subItem : item.subItems)
            {
                addItem(subItem.name, subItem.itemId);
            }
        }
        else if (!item.isSubMenu)
        {
            // Regular item
            addItem(item.name, item.itemId);
        }
    }
}

void CustomComboBox::buildHierarchicalMenu(juce::PopupMenu& menu, const std::vector<MenuItem>& items)
{
    for (const auto& item : items)
    {
        if (item.isSectionHeader)
        {
            // Add section header (non-clickable)
            menu.addSectionHeader(item.name);
        }
        else if (item.isSubMenu && !item.subItems.empty())
        {
            // Create sub-menu
            juce::PopupMenu subMenu;
            for (const auto& subItem : item.subItems)
            {
                subMenu.addItem(subItem.itemId, subItem.name, true, subItem.itemId == getSelectedId());
            }
            menu.addSubMenu(item.name, subMenu);
        }
        else if (!item.isSubMenu)
        {
            // Regular selectable item
            menu.addItem(item.itemId, item.name, true, item.itemId == getSelectedId());
        }
    }
}

void CustomComboBox::showPopup()
{
    if (!useHierarchicalMenu || hierarchicalItems.empty())
    {
        // Use default ComboBox popup
        juce::ComboBox::showPopup();
        return;
    }

    // Build custom hierarchical popup menu
    juce::PopupMenu menu;
    buildHierarchicalMenu(menu, hierarchicalItems);

    // Show the menu at the correct position
    menu.showMenuAsync(juce::PopupMenu::Options()
        .withTargetComponent(this)
        .withMinimumWidth(getWidth()),
        [safeThis = juce::Component::SafePointer<CustomComboBox>(this)](int result)
        {
            if (safeThis == nullptr)
                return;

            // Always call hidePopup() to reset ComboBox internal state
            // This is crucial - without it, the ComboBox thinks popup is still showing
            safeThis->hidePopup();

            // Update selection if user picked an item
            if (result != 0)
            {
                safeThis->setSelectedId(result, juce::sendNotification);
            }
        });
}