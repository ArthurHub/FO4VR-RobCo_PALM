#include "WheelConfig.h"
#include "PalmControls.h"
#include "SectionRegistry.h"
#include <nlohmann/json.hpp>
#include <cstring>
#include <iostream>
#include <stdexcept>

// Mods Config's Wheel sections page: its rows, and the client states that keep them.
namespace {
void check(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
const nlohmann::json* findRow(const nlohmann::json& rows,std::string_view id) {
 for(const auto& row:rows)if(row.value("id","")==id)return &row;
 return nullptr;
}
}

int main(){try {
 using namespace wheel;
 palm::api::menu::Section section;std::strcpy(section.id,"fixture.page");
 std::strcpy(section.name,"Magazines");std::strcpy(section.modName,"Fixture \"Mod\"");
 section.onSelect=[](std::uint32_t,std::uint64_t,void*) noexcept {};
 palm::api::menu::SectionHandle handle{};check(sectionRegistry().registerSection(&section,&handle)==palm::api::Result::Ok,"the fixture section did not register");
 Preferences prefs;prefs.sections={"fixture.gone"};restoreWheelPreferences(prefs);
 setGestureIntegrationAvailable(true);

 auto page=wheelSectionsContent();
 const auto content=nlohmann::json::parse(page.json);
 check(content.is_array() && content.size()==4,"the page is not Open the wheel, a text, the ring and the sections not added now");
 check(content[0]["type"]=="button" && content[0]["action"]["function"]=="openWheel","the page does not start with Open the wheel");
 const auto& ring=content[2];
 check(ring["type"]=="section" && ring["maxOn"]==palm::api::menu::kMaxVisibleEntries,"the ring is not limited to its places");
 check(ring["content"].size()==8,"the ring does not list five categories, Gestures, the mod section and Config");
 const auto* config=findRow(ring["content"],"section.config");
 check(config && (*config)["default"]==true,"Config is not a switcher, shown by default");
 const auto* weapons=findRow(ring["content"],"section.weapons");
 check(weapons && (*weapons)["default"]==true && (*weapons)["storage"]["clientState"]=="section.weapons","a category row is not a switcher kept in its state");
 const auto* gestures=findRow(ring["content"],"section.gestures");
 check(gestures && (*gestures)["countsAs"]==2,"Gestures does not take two places");
 const auto* mod=findRow(ring["content"],"section.mod.fixture.page");
 check(mod && (*mod)["text"]=="Magazines" && (*mod)["help"]=="Added by Fixture \"Mod\".","a mod section row lost its names");
 check(findRow(content[3]["content"],"section.mod.fixture.gone")!=nullptr,"a section its mod has not added is not listed apart");
 check(page.states.size()==9,"a row has no client state");

 check(wheelSectionShown("section.weapons")==true && wheelSectionShown("section.mod.fixture.gone")==true,"a shown section does not read shown");
 check(!wheelSectionShown("section.unknown"),"an unknown state has a value");
 (void)takeWheelConfigChange();
 showWheelSection("section.weapons",false);
 check(wheelSectionShown("section.weapons")==false && takeWheelConfigChange(),"hiding a category did not reach the wheel");
 showWheelSection("section.weapons",false);
 check(!takeWheelConfigChange(),"a value the wheel has changed it again");
 showWheelSection("section.mod.fixture.page",true);showWheelSection("section.gestures",true);
 const auto shown=snapshotWheelPreferences();
 check(shown.sectionEnabled("fixture.page") && shown.gesturesEnabled,"showing a mod section or Gestures did not reach the wheel");
 showWheelSection("section.config",false);
 check(wheelSectionShown("section.config")==false && !snapshotWheelPreferences().configShown,"hiding Config did not reach the wheel");
 showWheelSection("section.mod.fixture.gone",false);
 check(!snapshotWheelPreferences().sectionEnabled("fixture.gone"),"turning off a section not added now did not forget it");

 setGestureIntegrationAvailable(false);
 const auto withoutGestures=nlohmann::json::parse(wheelSectionsContent().json);
 check(!findRow(withoutGestures[2]["content"],"section.gestures") && withoutGestures.size()==3,"Gestures is listed without ROCK and FRIK");
 std::cout<<"Wheel sections page rows and states passed\n";
 return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
