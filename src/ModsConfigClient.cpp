#include "PCH.h"
#include "ModsConfigClient.h"
#include "DeveloperActions.h"
#include "PalmControls.h"
#include "Runtime.h"
#include "WheelConfig.h"
#include "Inventory.h"
#include <ModsConfigFO4Api.h>

namespace wheel::mods_config_client {
namespace {
using ::mods_config::api::FunctionResult;
using ::mods_config::api::ModsConfigFO4Client;
using ::mods_config::api::Params;
using ::mods_config::api::Saved;
using ::mods_config::api::Value;

// The modName of PALM's config spec.
constexpr const char* kModName="RobCoPALM";

/**
 * A Save wrote PALM.toml: PALM reads it again, sets its log level and turns its Developer section on or off; the
 * runtime applies the controls and pointer aim at its next frame.
 */
void onSaved(const Saved&) {
 const auto messages=reloadControls();
 spdlog::set_level(static_cast<spdlog::level::level_enum>(snapshotLogLevel()));
 for(const auto& message:messages)spdlog::warn("{}",message);
 if(const bool developer=snapshotDeveloperEnabled();developer!=developerActions().enabled()) {
  if(!developerActions().initialize(developer))spdlog::error("PALM Developer section unavailable; existing mod sections preserved");
  else spdlog::info("PALM Developer wheel {} after a Mods Config Save",developer?"enabled":"disabled");
 }
 spdlog::info("PALM.toml read again after a Mods Config Save");
}

/**
 * The front page's Open the wheel: minimizes the menu, which gives RPS UI the controllers back, and asks the runtime
 * to open the wheel; the runtime shows the menu again once the wheel closes.
 */
FunctionResult openWheel(const Params&) {
 if(!requestWheelOpen())return {false,"The wheel opens only in a loaded game."};
 ModsConfigFO4Client::get().minimizeMenu();
 return {true,{}};
}

/**
 * The front page's RPS UI settings (legacy): minimizes the menu and asks the runtime to open RPS UI's settings menu in
 * front of the headset, with PALM's Wheel items and Wheel settings, once RPS UI has input back; the runtime shows the
 * menu again once that menu closes, or at once when it does not open.
 */
FunctionResult openSettings(const Params&) {
 if(!requestSettingsMenuOpen())return {false,"PALM's pages open only in a loaded game."};
 ModsConfigFO4Client::get().minimizeMenu();
 return {true,{}};
}

// The Wheel page's sub-pages of items, one per category, in the config spec's order.
constexpr std::array<const char*,5> kItemsPages{"items.weapons","items.armor","items.aid","items.food","items.grenades"};
constexpr const char* kLoadGameFirst=R"([{"type":"text","text":"Load a game first: each save keeps its own wheel."}])";

/**
 * Gives a dynamic page's rows, adding the client state of each switcher: its getter reads the wheel's preferences and
 * its setter applies the value, for each live preview, Discard and Save. A state added before keeps its first.
 */
std::string pageRows(WheelPageContent content,std::optional<bool>(*read)(std::string_view),void(*write)(std::string_view,bool)) {
 auto& client=ModsConfigFO4Client::get();
 for(const auto& name:content.states)
  client.addState(name,[name,read]{const auto value=read(name);return value?std::optional<Value>(*value):std::nullopt;},
   [name,write](const Value& value){if(const auto* on=std::get_if<bool>(&value))write(name,*on);});
 return std::move(content.json);
}

/**
 * The Wheel page's rows, what the ring shows. The co-save keeps them, so outside a loaded game the page only says to load one.
 */
std::optional<std::string> wheelSectionsPage() {
 if(!gameSessionReady())return std::string(kLoadGameFirst);
 return pageRows(wheelSectionsContent(),&wheelSectionShown,&showWheelSection);
}

/**
 * A category's page of items under the Wheel page, from the inventory read as the page is shown: Mods Config asks on the game's main thread,
 * where the inventory is read. Outside a loaded game the page only says to load one.
 */
std::optional<std::string> wheelItemsPage(Category category) {
 if(!gameSessionReady())return std::string(kLoadGameFirst);
 try{(void)readInventory();}
 catch(...){spdlog::error("PALM could not read the inventory for Mods Config's pages of wheel items");}
 return pageRows(wheelItemsContent(category),&wheelItemChosen,&chooseWheelItem);
}

/**
 * Mods Config's F4SE message after a Save, sent to the plugins of the spec's f4sePluginRequirements. PALM learns of
 * Saves through onSaved, so it ignores the message; without a listener F4SE logs a failed dispatch.
 */
void onModsConfigMessage(F4SE::MessagingInterface::Message*) {}
}

/**
 * Connects PALM's client, from F4SE's kPostLoad on. Mods Config FO4 VR is a requirement of PALM's settings page:
 * without it PALM runs, and its settings are edited only in PALM.toml.
 */
void connect() noexcept {
 try {
  auto& client=ModsConfigFO4Client::get();
  client.addState("modVersion",[]{return std::optional<Value>(std::string(PALM_VERSION));});
  client.addFunction("openWheel",&openWheel);
  client.addFunction("openSettings",&openSettings);
  client.addPageContent("wheel",&wheelSectionsPage);
  for(const char* page:kItemsPages)
   if(const auto category=wheelItemsPageCategory(page))client.addPageContent(page,[category=*category]{return wheelItemsPage(category);});
  client.subscribeToSaves(&onSaved);
  if(const auto* messaging=F4SE::GetMessagingInterface())
   (void)messaging->RegisterListener(&onModsConfigMessage,::mods_config::api::MODS_CONFIG_FO4_F4SE_NAME);
  if(const int error=client.connect(kModName);error!=0)
   spdlog::warn("Mods Config FO4 VR not connected ({}): PALM's settings are edited only in PALM.toml",error);
  else spdlog::info("Connected to Mods Config FO4 VR (API {})",client.apiVersion());
 }catch(...){spdlog::error("Connecting to Mods Config FO4 VR failed");}
}

/**
 * Shows Mods Config's menu again after the wheel its button opened has closed. Nothing while the menu is not
 * minimized, so a menu the player showed or closed meanwhile is left as it is.
 */
void restoreMenu() noexcept {
 try{ModsConfigFO4Client::get().restoreMenu();}
 catch(...){spdlog::error("Showing Mods Config FO4 VR's menu again failed");}
}

/**
 * Opens Mods Config's menu at RobCo PALM's front page, as the wheel's Config does; a minimized menu is shown. False
 * while PALM's client is not connected.
 */
bool openMenu() noexcept {
 try{return ModsConfigFO4Client::get().openMenu();}
 catch(...){spdlog::error("Opening Mods Config FO4 VR's menu failed");return false;}
}
}
