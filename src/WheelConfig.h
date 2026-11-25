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
// A dynamic page of PALM's config spec in Mods Config FO4 VR: the JSON text of its content array, and the client
// states its switchers are kept in. The Wheel page (page wheel) names them section.<category>, section.gestures,
// section.config and section.mod.<id>; its sub-pages of items (pages items.<category>) item.<category>.<key>.
struct WheelPageContent {std::string json;std::vector<std::string> states;};
WheelPageContent wheelSectionsContent();
std::optional<bool> wheelSectionShown(std::string_view stateName);
void showWheelSection(std::string_view stateName,bool shown);
std::optional<Category> wheelItemsPageCategory(std::string_view pageId);
WheelPageContent wheelItemsContent(Category category);
std::optional<bool> wheelItemChosen(std::string_view stateName);
void chooseWheelItem(std::string_view stateName,bool chosen);
void refreshWheelSections(Model& model);
void drawWheelConfig();
void drawWheelSettings();
}
