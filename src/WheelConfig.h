#pragma once
#include "WheelPreferences.h"
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace wheel {
// Serialization snapshots UI edits under the same mutex. Restoring a game
// clears its predecessor's inventory and pending refresh, including no-data saves.
Preferences snapshotWheelPreferences();
void restoreWheelPreferences(Preferences prefs);
// Returns the weapon choices removed because they left the inventory.
std::size_t publishWheelInventory(const Model& inventory);
// Adds a carried item at one free ring slot of the category; message is the status text.
bool addWheelFavorite(Category category,const Item& item,unsigned slot,const char*& message);
Model selectedWheelInventory(const Model& inventory);
bool takeWheelConfigChange();
// Mods Config FO4 VR's Wheel sections page (page wheelSections of PALM's config spec): the JSON text of its content
// array, and the client states its switchers are kept in: section.<category>, section.gestures and section.mod.<id>.
struct WheelSectionsContent {std::string json;std::vector<std::string> states;};
WheelSectionsContent wheelSectionsContent();
std::optional<bool> wheelSectionShown(std::string_view stateName);
void showWheelSection(std::string_view stateName,bool shown);
void refreshWheelSections(Model& model);
void drawWheelConfig();
void drawWheelSettings();
}
