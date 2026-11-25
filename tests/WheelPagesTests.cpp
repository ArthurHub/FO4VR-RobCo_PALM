#include "WheelConfig.h"
#include "PalmControls.h"
#include "SectionRegistry.h"
#include <nlohmann/json.hpp>
#include <cstring>
#include <iostream>
#include <stdexcept>

// Mods Config's Wheel page and its pages of items: their rows, and the client states that keep them.
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

 // Items: an Aid page with a carried choice, a choice no longer carried and carried items to choose.
 constexpr auto aid=static_cast<unsigned>(Category::Aid);
 Preferences items;
 check(items.place(aid,{"a|1","Stimpak"},2) && items.place(aid,{"a|9","Old Aid"},5),"the fixture choices did not place");
 restoreWheelPreferences(items);
 Model inventory;
 for(unsigned i=1;i<=8;++i)inventory.items[aid].push_back({i,i==1?"Stimpak":"Aid "+std::to_string(i),i,false,"a|"+std::to_string(i)});
 (void)publishWheelInventory(inventory);
 check(wheelItemsPageCategory("items.aid")==Category::Aid && !wheelItemsPageCategory("wheel"),"a Wheel items page id does not name its category");
 const auto itemsPage=wheelItemsContent(Category::Aid);
 const auto aidRows=nlohmann::json::parse(itemsPage.json);
 const auto& aidSection=aidRows.back();
 check(aidRows[0]["action"]["function"]=="openWheel" && aidSection["maxOn"]==kSlots,"the Aid page is not Open the wheel and a section of at most eight");
 check(aidSection["content"].size()==9 && itemsPage.states.size()==9,"the Aid page does not list the choice not carried and the eight carried items");
 check(aidSection["content"][0]["id"]=="item.aid.a|9" && aidSection["content"][0]["text"]=="Old Aid","the choice not carried does not come first");
 const auto* stimpak=findRow(aidSection["content"],"item.aid.a|1");
 check(stimpak && (*stimpak)["text"]=="Stimpak (1)" && (*stimpak)["default"]==false,"a carried item does not show its count");
 check(wheelItemChosen("item.aid.a|1")==true && wheelItemChosen("item.aid.a|2")==false && !wheelItemChosen("item.nothing.a|1"),"an item state does not read the wheel");
 const auto slotOf=[&](std::string_view key) {
  const auto prefs=snapshotWheelPreferences();const auto& list=prefs.slots[aid];
  const auto found=std::find_if(list.begin(),list.end(),[&](const Favorite& f){return f.key==key;});
  return found==list.end()?kSlots:found->slot;
 };
 chooseWheelItem("item.aid.a|1",false);
 check(wheelItemChosen("item.aid.a|1")==false,"turning an item off left it on the wheel");
 chooseWheelItem("item.aid.a|2",true);
 check(slotOf("a|2")==0,"a new choice did not take the lowest free slot");
 chooseWheelItem("item.aid.a|1",true);
 check(slotOf("a|1")==2,"an item turned off and on again lost its slot");
 for(unsigned i=3;i<=7;++i)chooseWheelItem("item.aid.a|"+std::to_string(i),true);
 check(snapshotWheelPreferences().slots[aid].size()==kSlots,"the Aid ring did not fill");
 // A Discard can turn a choice on before it turns another off: the choice waits for that slot.
 chooseWheelItem("item.aid.a|8",true);
 check(wheelItemChosen("item.aid.a|8")==true && slotOf("a|8")==kSlots,"a choice made while the ring is full did not wait");
 chooseWheelItem("item.aid.a|9",false);
 check(slotOf("a|8")==5 && !wheelItemChosen("item.aid.a|9").value(),"the waiting choice did not take the freed slot");
 std::cout<<"Wheel page and pages of items rows and states passed\n";
 return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
