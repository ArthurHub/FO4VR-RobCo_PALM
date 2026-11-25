#include "WheelConfig.h"
#include "DeveloperActions.h"
#include "PalmControls.h"
#include <imgui.h>
#include "tools/render/ConfigUi.h"
#include <nlohmann/json.hpp>
#include <mutex>
#include <atomic>
#include <format>

namespace wheel {
namespace {
struct State {
 std::mutex mutex;Preferences prefs;Model inventory;
 // Per category, the choices Mods Config turned off, with their ring slots, and those it turned on while every slot
 // was taken: a Discard puts each back where it was, in whatever order Mods Config gives the values.
 std::array<std::vector<Favorite>,kCategoryCount> unchosen,waiting;
 std::atomic_bool changed=false;
 std::string status="Preview — choices stay in memory";
 unsigned category{};char search[128]{};
};
State& state(){static State s;return s;}
void limitSections(Model& model) {
 unsigned used=(model.configShown?1u:0u)+static_cast<unsigned>(std::count(model.enabled.begin(),model.enabled.end(),true))+(model.gesturesEnabled?2:0);
 for(unsigned i=0;i<model.sections.count;++i)if(model.sections.sections[i].enabled && !isDeveloperSection(model.sections.sections[i].id)) {
  model.sections.sections[i].enabled=used<palm::api::menu::kMaxVisibleEntries;++used;
 }
 // Developer mode never silently hides an existing user-selected section.
 for(unsigned i=0;i<model.sections.count;++i)if(isDeveloperSection(model.sections.sections[i].id))
  model.sections.sections[i].enabled=developerActions().enabled() && used<palm::api::menu::kMaxVisibleEntries;
 normalizeWheelSelection(model);
}
bool matches(std::string_view name,std::string_view needle) {
 const auto lower=[](char c){return c>='A' && c<='Z'?static_cast<char>(c+32):c;};
 return std::search(name.begin(),name.end(),needle.begin(),needle.end(),
  [&](char a,char b){return lower(a)==lower(b);})!=name.end() || needle.empty();
}
}
Preferences snapshotWheelPreferences() {
 auto& s=state();std::scoped_lock lock(s.mutex);return s.prefs;
}
void restoreWheelPreferences(Preferences prefs) {
 auto& s=state();std::scoped_lock lock(s.mutex);
 s.prefs=std::move(prefs);s.inventory={};s.changed=false;s.category=0;s.search[0]='\0';
 for(auto& list:s.unchosen)list.clear();
 for(auto& list:s.waiting)list.clear();
 s.status="Saved with your game";
}
std::size_t publishWheelInventory(const Model& inventory){
 auto& s=state();std::scoped_lock lock(s.mutex);std::size_t removed{};
 // A weapon choice carried in the previous complete read and absent from this
 // one left the inventory (dropped, stored, sold, scrapped or modified into
 // another variant): it frees its slot. A partial read never removes a choice;
 // a load clears the previous read, so restored choices are never pruned.
 constexpr auto weapons=static_cast<unsigned>(Category::Weapons);
 const auto complete=[](const Model& model){return model.status.empty() && model.equipmentNamesAvailable && model.items[weapons].size()<kMaxItems;};
 const auto carried=[](const Model& model,const std::string& key) {
  return std::any_of(model.items[weapons].begin(),model.items[weapons].end(),[&](const Item& item){return item.key==key;});
 };
 if(complete(s.inventory) && complete(inventory)) {
  auto& favorites=s.prefs.slots[weapons];
  removed=std::erase_if(favorites,[&](const Favorite& f){return carried(s.inventory,f.key) && !carried(inventory,f.key);});
  if(removed)s.changed=true;
 }
 s.inventory=inventory;return removed;
}
bool addWheelFavorite(Category category,const Item& item,unsigned slot,const char*& message) {
 auto& s=state();std::scoped_lock lock(s.mutex);
 const auto c=static_cast<unsigned>(category);
 auto& favorites=s.prefs.slots[c];
 if(std::any_of(favorites.begin(),favorites.end(),[&](const Favorite& f){return f.key==item.key;})){message="That item is already on the wheel";return false;}
 if(!s.prefs.slotFree(c,slot)){message="That slot is already taken";return false;}
 if(!s.prefs.place(c,{item.key,item.name},slot)){message="That item cannot be added";return false;}
 s.changed=true;message="Added to the wheel";return true;
}
Model selectedWheelInventory(const Model& inventory){
 auto& s=state();std::scoped_lock lock(s.mutex);auto result=curatedInventory(inventory,s.prefs);
 result.gesturesEnabled &= gestureIntegrationAvailable();
 if(sectionRegistry().snapshot(result.sections))
  for(unsigned i=0;i<result.sections.count;++i)result.sections.sections[i].enabled=isDeveloperSection(result.sections.sections[i].id)?developerActions().enabled():s.prefs.sectionEnabled(result.sections.sections[i].id);
 limitSections(result);return result;
}
void refreshWheelSections(Model& model) {
 model.gesturesEnabled &= gestureIntegrationAvailable();
 if(model.sections.revision==sectionRegistry().revision())return;
 auto& s=state();std::unique_lock lock(s.mutex,std::try_to_lock);if(!lock.owns_lock())return;
 if(!sectionRegistry().snapshot(model.sections))return;
 for(unsigned i=0;i<model.sections.count;++i)model.sections.sections[i].enabled=isDeveloperSection(model.sections.sections[i].id)?developerActions().enabled():s.prefs.sectionEnabled(model.sections.sections[i].id);
 limitSections(model);
}
bool takeWheelConfigChange(){return state().changed.exchange(false);}
namespace {
// The item categories in the order the pages list them, by the name their client states and pages of items use.
struct PageCategory {Category category;std::string_view slug;};
constexpr std::array<PageCategory,kCategoryCount> kPageCategories{{
 {Category::Weapons,"weapons"},{Category::Armor,"armor"},{Category::Aid,"aid"},{Category::Food,"food"},{Category::Grenades,"grenades"}}};
constexpr std::string_view kSectionStatePrefix="section.",kItemStatePrefix="item.";
constexpr std::string_view kGesturesState="section.gestures",kConfigState="section.config",kModStatePrefix="section.mod.";
std::string titleCase(std::string_view upper) {
 std::string result(upper);
 for(std::size_t i=1;i<result.size();++i)if(result[i]>='A' && result[i]<='Z')result[i]=static_cast<char>(result[i]+32);
 return result;
}
const PageCategory* pageCategory(std::string_view slug) {
 for(const auto& category:kPageCategories)if(category.slug==slug)return &category;
 return nullptr;
}
nlohmann::json switcherRow(std::vector<std::string>& states,std::string_view stateName,std::string_view text,std::string_view help,bool fallback) {
 states.emplace_back(stateName);
 return nlohmann::json{{"id",stateName},{"type","switcher"},{"valueType","bool"},{"text",text},{"help",help},
  {"default",fallback},{"storage",{{"clientState",stateName}}}};
}
nlohmann::json openWheelButton() {
 return {{"type","button"},{"text","Open the wheel"},{"buttonText","Open"},{"icon","window"},
  {"help","Minimizes this menu and opens the wheel, to see your changes on it. This menu comes back when the wheel closes."},
  {"action",{{"type","CallNativeFunction"},{"function","openWheel"}}}};
}
// An item state's category and key: item.<category>.<key>.
std::optional<std::pair<Category,std::string_view>> itemState(std::string_view stateName) {
 if(!stateName.starts_with(kItemStatePrefix))return std::nullopt;
 stateName.remove_prefix(kItemStatePrefix.size());
 const auto dot=stateName.find('.');if(dot==std::string_view::npos || dot+1==stateName.size())return std::nullopt;
 const auto* category=pageCategory(stateName.substr(0,dot));if(!category)return std::nullopt;
 return std::pair{category->category,stateName.substr(dot+1)};
}
// Places a choice back at its earlier slot while it is free, else at the lowest free one.
bool placeChoice(Preferences& prefs,unsigned category,const Favorite& choice) {
 return prefs.place(category,choice,choice.slot) || prefs.select(category,choice,true);
}
}
/**
 * Mods Config's Wheel page rows: Open the wheel, then a section limited to the ring's places with the categories,
 * Gestures (two places, while ROCK and FRIK serve gestures), the registered mod sections and Config, then the shown
 * sections whose mod has not registered them, which take no place, so the player can forget them.
 */
WheelPageContent wheelSectionsContent() {
 auto& s=state();std::scoped_lock lock(s.mutex);
 WheelPageContent result;
 const auto row=[&](std::string_view stateName,std::string_view text,std::string_view help,bool fallback) {
  return switcherRow(result.states,stateName,text,help,fallback);
 };
 auto ring=nlohmann::json::array();
 for(const auto& [category,slug]:kPageCategories)
  ring.push_back(row(std::string(kSectionStatePrefix)+std::string(slug),titleCase(categoryName(category)),"Its items are on its own page under this one, shown while it is on.",true));
 if(gestureIntegrationAvailable()) {
  auto gestures=row(kGesturesState,"Gestures","The left and right hand menus of ROCK and FRIK. Takes two places.",false);
  gestures["countsAs"]=2;ring.push_back(std::move(gestures));
 }
 SectionCatalog catalog;
 const bool registered=sectionRegistry().snapshot(catalog);
 for(unsigned i=0;registered && i<catalog.count;++i) {
  const auto& section=catalog.sections[i];if(isDeveloperSection(section.id))continue;
  ring.push_back(row(std::string(kModStatePrefix)+section.id,section.name,std::format("Added by {}.",section.modName),false));
 }
 ring.push_back(row(kConfigState,"Config","Opens PALM's settings from the wheel. Without it, open them from Mods Config FO4 VR. The ring keeps it when nothing else is shown.",true));
 auto content=nlohmann::json::array();
 content.push_back(openWheelButton());
 content.push_back({{"type","text"},{"text",std::format("The ring has room for {} sections, Config with them; Cancel is in its center. Hiding a section keeps its items.",palm::api::menu::kMaxVisibleEntries)}});
 content.push_back({{"type","section"},{"text","On the ring"},{"maxOn",palm::api::menu::kMaxVisibleEntries},{"content",std::move(ring)}});
 auto waiting=nlohmann::json::array();
 for(const auto& id:s.prefs.sections) {
  if(registered && std::any_of(catalog.sections.begin(),catalog.sections.begin()+catalog.count,[&](const auto& section){return id==section.id;}))continue;
  waiting.push_back(row(std::string(kModStatePrefix)+id,id,"Its mod has not added it in this game session, so it takes no place. Turn it off to forget it.",false));
 }
 if(!waiting.empty())content.push_back({{"type","section"},{"text","Not added now"},{"content",std::move(waiting)}});
 if(developerActions().enabled())content.push_back({{"type","text"},{"text","The Developer section takes a free place on the ring when there is one."}});
 result.json=content.dump();
 return result;
}
/**
 * Whether the wheel shows the section of a Wheel page client state; none for a name that is not one.
 */
std::optional<bool> wheelSectionShown(std::string_view stateName) {
 auto& s=state();std::scoped_lock lock(s.mutex);
 if(stateName==kGesturesState)return s.prefs.gesturesEnabled;
 if(stateName==kConfigState)return s.prefs.configShown;
 if(stateName.starts_with(kModStatePrefix))return s.prefs.sectionEnabled(stateName.substr(kModStatePrefix.size()));
 if(stateName.starts_with(kSectionStatePrefix))
  if(const auto* category=pageCategory(stateName.substr(kSectionStatePrefix.size())))return s.prefs.enabled[static_cast<unsigned>(category->category)];
 return std::nullopt;
}
/**
 * Shows or hides the section of a Wheel page client state, as Mods Config previews, discards and saves it. Mods
 * Config already holds the ring's limit, and the wheel fits what does not, so a mod section is added without it.
 */
void showWheelSection(std::string_view stateName,bool shown) {
 auto& s=state();std::scoped_lock lock(s.mutex);
 bool changed=false;
 if(stateName==kGesturesState){changed=s.prefs.gesturesEnabled!=shown;s.prefs.gesturesEnabled=shown;}
 else if(stateName==kConfigState){changed=s.prefs.configShown!=shown;s.prefs.configShown=shown;}
 else if(stateName.starts_with(kModStatePrefix))changed=s.prefs.setSectionEnabled(stateName.substr(kModStatePrefix.size()),shown,0);
 else if(stateName.starts_with(kSectionStatePrefix))
  if(const auto* category=pageCategory(stateName.substr(kSectionStatePrefix.size()))) {
   auto& enabled=s.prefs.enabled[static_cast<unsigned>(category->category)];changed=enabled!=shown;enabled=shown;
  }
 if(changed)s.changed=true;
}
/**
 * The category of a page of items under the Wheel page, by the page's id (items.<category>); none for another page.
 */
std::optional<Category> wheelItemsPageCategory(std::string_view pageId) {
 constexpr std::string_view prefix="items.";
 if(!pageId.starts_with(prefix))return std::nullopt;
 const auto* category=pageCategory(pageId.substr(prefix.size()));
 return category?std::optional<Category>(category->category):std::nullopt;
}
/**
 * Mods Config's rows of a category's page of items, from the inventory PALM read last: Open the wheel, notes, then a
 * section of at most eight on with a switcher for each saved choice PALM does not see carried (it keeps its slot) and
 * for each carried item. Each is kept in the client state item.<category>.<key>. A category with nothing to choose
 * says so instead of an empty section, which Mods Config refuses.
 */
WheelPageContent wheelItemsContent(Category category) {
 auto& s=state();std::scoped_lock lock(s.mutex);
 WheelPageContent result;
 const auto c=static_cast<unsigned>(category);
 const auto page=std::find_if(kPageCategories.begin(),kPageCategories.end(),[&](const auto& entry){return entry.category==category;});
 const auto name=titleCase(categoryName(category));
 const auto stateOf=[&](std::string_view key){return std::string(kItemStatePrefix)+std::string(page->slug)+"."+std::string(key);};
 const bool equipment=isEquipment(category),namesUnavailable=equipment && !s.inventory.equipmentNamesAvailable;
 const auto& carried=s.inventory.items[c];
 auto rows=nlohmann::json::array();
 std::vector<std::string_view> listed;
 const auto add=[&](std::string_view key,std::string_view text,std::string_view help) {
  if(key.empty() || std::find(listed.begin(),listed.end(),key)!=listed.end())return;
  listed.push_back(key);rows.push_back(switcherRow(result.states,stateOf(key),text,help,false));
 };
 std::vector<Favorite> saved=s.prefs.slots[c];
 saved.insert(saved.end(),s.waiting[c].begin(),s.waiting[c].end());
 std::sort(saved.begin(),saved.end(),[](const Favorite& a,const Favorite& b){return a.slot<b.slot;});
 for(const auto& choice:saved)
  if(std::none_of(carried.begin(),carried.end(),[&](const Item& item){return item.key==choice.key;}))
   add(choice.key,choice.name,namesUnavailable?"Equipment is unavailable in this game session; the choice keeps its place.":
    "Not carried. It keeps its place on the ring until you turn it off.");
 for(const auto& item:carried) {
  if(item.name.empty())continue;
  if(equipment)add(item.key,item.equipped?std::format("{} (equipped)",item.name):item.name,item.equipped?"Equipped.":"In your inventory.");
  else add(item.key,std::format("{} ({})",item.name,item.count),std::format("Carried: {}.",item.count));
 }
 auto content=nlohmann::json::array();
 content.push_back(openWheelButton());
 content.push_back({{"type","text"},{"text",std::format("Choose up to {} items for {}. A new choice takes the lowest free place on its ring.",kSlots,name)}});
 if(namesUnavailable)content.push_back({{"type","text"},{"text","Equipment names are unavailable in this game session. Your saved choices are kept."}});
 if(!s.inventory.status.empty())content.push_back({{"type","text"},{"text",s.inventory.status}});
 if(rows.empty())content.push_back({{"type","text"},{"text",std::format("Nothing to choose: carry an item of {} first.",name)}});
 else content.push_back({{"type","section"},{"text",name},{"maxOn",kSlots},{"content",std::move(rows)}});
 result.json=content.dump();
 return result;
}
/**
 * Whether an item client state's item is on the wheel, or waits for a free slot; none for a name that is not one.
 */
std::optional<bool> wheelItemChosen(std::string_view stateName) {
 const auto parsed=itemState(stateName);if(!parsed)return std::nullopt;
 auto& s=state();std::scoped_lock lock(s.mutex);
 const auto c=static_cast<unsigned>(parsed->first);const auto key=parsed->second;
 const auto has=[&](const std::vector<Favorite>& list){return std::any_of(list.begin(),list.end(),[&](const Favorite& f){return f.key==key;});};
 return has(s.prefs.slots[c]) || has(s.waiting[c]);
}
/**
 * Puts an item client state's item on the wheel or takes it off, as Mods Config previews, discards and saves it.
 * An item taken off keeps its slot to come back to; one put on while every slot is taken waits for the next free one,
 * since a Discard can turn a choice on before it turns another off. An item PALM neither sees nor had is ignored.
 */
void chooseWheelItem(std::string_view stateName,bool chosen) {
 const auto parsed=itemState(stateName);if(!parsed)return;
 auto& s=state();std::scoped_lock lock(s.mutex);
 const auto c=static_cast<unsigned>(parsed->first);const std::string key(parsed->second);
 auto& slots=s.prefs.slots[c];auto& unchosen=s.unchosen[c];auto& waiting=s.waiting[c];
 const auto byKey=[&](const Favorite& f){return f.key==key;};
 const auto onWheel=std::find_if(slots.begin(),slots.end(),byKey);
 const auto queued=std::find_if(waiting.begin(),waiting.end(),byKey);
 if(!chosen) {
  if(queued!=waiting.end()){waiting.erase(queued);return;}
  if(onWheel==slots.end())return;
  std::erase_if(unchosen,byKey);unchosen.push_back(*onWheel);slots.erase(onWheel);
  // The slot it freed takes the first choice waiting for one.
  if(!waiting.empty() && placeChoice(s.prefs,c,waiting.front()))waiting.erase(waiting.begin());
  s.changed=true;return;
 }
 if(onWheel!=slots.end() || queued!=waiting.end())return;
 Favorite choice;
 if(const auto earlier=std::find_if(unchosen.begin(),unchosen.end(),byKey);earlier!=unchosen.end()){choice=*earlier;unchosen.erase(earlier);}
 else {
  const auto& carried=s.inventory.items[c];
  const auto item=std::find_if(carried.begin(),carried.end(),[&](const Item& i){return i.key==key;});
  if(item==carried.end())return;
  choice={key,item->name,kSlots};
 }
 if(placeChoice(s.prefs,c,choice))s.changed=true;
 else waiting.push_back(std::move(choice));
}
void drawWheelSettings() {
 auto& s=state();std::unique_lock lock(s.mutex,std::try_to_lock);if(!lock.owns_lock())return;
 using namespace devui;
 // Controls and pointer aim are on RobCo PALM's page in Mods Config, which owns PALM.toml.
 ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{26,24});
 ImGui::BeginChild("wheel-settings",{0,0},ImGuiChildFlags_AlwaysUseWindowPadding);
 SectionCatalog catalog;if(!sectionRegistry().snapshot(catalog)){ImGui::EndChild();ImGui::PopStyleVar();return;}
 const auto enabledCount=[&] {
  unsigned count=(s.prefs.configShown?1u:0u)+static_cast<unsigned>(std::count(s.prefs.enabled.begin(),s.prefs.enabled.end(),true))+(s.prefs.gesturesEnabled && gestureIntegrationAvailable()?2:0);
  for(unsigned i=0;i<catalog.count;++i)count+=(isDeveloperSection(catalog.sections[i].id)?developerActions().enabled():s.prefs.sectionEnabled(catalog.sections[i].id))?1:0;
  return count;
 };
 visual::heading("Wheel sections","Choose which sections appear. Visible sections fill the ring.");
 {visual::Font font(render::FontRole::Body,18);
  ImGui::TextColored(visual::muted(),"%u / %u slots enabled. Config and Cancel always stay available.",(std::min)(enabledCount(),palm::api::menu::kMaxVisibleEntries),palm::api::menu::kMaxVisibleEntries);
  ImGui::TextColored(visual::muted(),"Hiding a section keeps its item choices. Gestures uses two slots.");}
 if(enabledCount()>palm::api::menu::kMaxVisibleEntries)ImGui::TextWrapped("Some enabled mod sections are waiting for a free slot. Hide sections to make room.");
 ImGui::Dummy({0,16});
 const auto toggle=[&](const char* name,const char* detail,bool& enabled,unsigned cost) {
  ImGui::TableNextColumn();ImGui::PushID(name);
  ImGui::BeginDisabled(!enabled && enabledCount()+cost>palm::api::menu::kMaxVisibleEntries);
  bool changed;
  {visual::Font font(render::FontRole::Medium,24);ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{4,4});
   changed=devui::visual::checkbox(name,&enabled);ImGui::PopStyleVar();}
  ImGui::EndDisabled();
  {visual::Font font(render::FontRole::Body,18);ImGui::TextColored(visual::muted(),"%s",detail);}
  ImGui::Dummy({0,14});ImGui::PopID();return changed;
 };
 if(ImGui::BeginTable("wheel-section-toggles",2,ImGuiTableFlags_SizingStretchSame)) {
  for(const auto category:{Category::Weapons,Category::Armor,Category::Aid,Category::Food,Category::Grenades}) {
   const auto c=static_cast<unsigned>(category);
   if(toggle(categoryName(category),"Inventory items",s.prefs.enabled[c],1))s.changed=true;
  }
  if(gestureIntegrationAvailable() && toggle("Gestures","Left and right hand menus",s.prefs.gesturesEnabled,2))s.changed=true;
  ImGui::EndTable();
 }
 {
  visual::caption("MOD SECTIONS");ImGui::Dummy({0,12});
  if(catalog.count==0 && s.prefs.sections.empty()) {
   visual::Font font(render::FontRole::Body,20);ImGui::TextColored(visual::muted(),"No mod sections registered.");
  }else if(ImGui::BeginTable("mod-section-toggles",2,ImGuiTableFlags_SizingStretchSame)) {
   for(unsigned i=0;i<catalog.count;++i) {
    const auto& section=catalog.sections[i];bool enabled=s.prefs.sectionEnabled(section.id);
    if(isDeveloperSection(section.id)){ImGui::TableNextColumn();ImGui::TextUnformatted("Developer — controlled by PALM.toml");continue;}
    ImGui::PushID(section.id);
    if(toggle(section.name,section.modName,enabled,1) && s.prefs.setSectionEnabled(section.id,enabled,enabledCount()))s.changed=true;
    ImGui::PopID();
   }
   // Preserve unavailable sections without charging them against the visible ring.
   for(std::size_t i=0;i<s.prefs.sections.size();) {
    const auto& id=s.prefs.sections[i];
    const bool present=std::any_of(catalog.sections.begin(),catalog.sections.begin()+catalog.count,[&](const auto& value){return id==value.id;});
    if(present){++i;continue;}
    bool enabled=true;
    if(toggle(id.c_str(),"Mod not currently registered",enabled,1)) {
     s.prefs.sections.erase(s.prefs.sections.begin()+i);s.changed=true;
    }else ++i;
   }
   ImGui::EndTable();
  }
 }
 ImGui::EndChild();ImGui::PopStyleVar();
}
void drawWheelConfig() {
 auto& s=state();std::unique_lock lock(s.mutex,std::try_to_lock);if(!lock.owns_lock())return;
 using namespace devui;
 const float rail=visual::railWidth(ImGui::GetContentRegionAvail().x);
 ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{20,24});
 ImGui::BeginChild("wheel-categories",{rail,0},ImGuiChildFlags_AlwaysUseWindowPadding);
 visual::caption("QUICK ACCESS");ImGui::Dummy({0,12});
 for(unsigned c=0;c<kCategoryCount;++c) {
  ImGui::PushID(static_cast<int>(c));
  if(visual::navigation(categoryName(static_cast<Category>(c)),s.category==c,s.prefs.enabled[c]?nullptr:"OFF"))s.category=c;
  ImGui::PopID();
 }
 ImGui::EndChild();ImGui::SameLine(0,0);
 ImGui::PopStyleVar();
 ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{26,24});
 ImGui::BeginChild("wheel-category-content",{0,0},ImGuiChildFlags_AlwaysUseWindowPadding);
 visual::heading(categoryName(static_cast<Category>(s.category)), "Choose up to eight items for this category.");
 const bool namesUnavailable=isEquipment(static_cast<Category>(s.category)) && !s.inventory.equipmentNamesAvailable;
 if(namesUnavailable){visual::Font font(render::FontRole::Body,18);ImGui::TextWrapped("Equipment names are unavailable this session. Your saved choices are kept; other PALM features remain available.");ImGui::Dummy({0,8});}
 if(!s.prefs.enabled[s.category]){visual::Font font(render::FontRole::Body,18);ImGui::TextColored(visual::muted(),"Hidden from the wheel. Enable it in PALM settings.");ImGui::Dummy({0,8});}
 auto& favorites=s.prefs.slots[s.category];const auto& catalog=s.inventory.items[s.category];
 const float searchWidth=(std::max)(220.0f,ImGui::GetContentRegionAvail().x-280);
 ImGui::SetNextItemWidth(searchWidth);
 ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{16,12});
 ImGui::InputTextWithHint("##item-search","Search inventory...",s.search,sizeof(s.search));
 ImGui::PopStyleVar();ImGui::SameLine(0,24);
 ImGui::BeginGroup();
 ImGui::Text("%zu / 8 selected",favorites.size());
 { visual::Font font(render::FontRole::Body,14);ImGui::TextColored(visual::muted(),"%s",s.status.c_str()); }
 ImGui::EndGroup();ImGui::Dummy({0,16});
 constexpr auto flags=ImGuiTableFlags_ScrollY|ImGuiTableFlags_BordersOuter|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_RowBg|ImGuiTableFlags_SizingStretchProp;
 if(ImGui::BeginTable("wheel-item-selection",2,flags,{0,0})) {
  ImGui::TableSetupColumn("ITEM",ImGuiTableColumnFlags_WidthStretch);
  const bool equipment=isEquipment(static_cast<Category>(s.category));
  const char* statusColumn=equipment?"EQUIPMENT":"CARRIED";
  ImGui::TableSetupColumn(statusColumn,ImGuiTableColumnFlags_WidthFixed,160);
  visual::tableHeader("ITEM",statusColumn);
  const auto row=[&](std::string_view key,std::string_view name,unsigned carried,bool available,bool selected,bool equipped=false) {
   ImGui::PushID(key.data(),key.data()+key.size());
   ImGui::TableNextRow(0,44);ImGui::TableSetColumnIndex(0);
   if(selected)ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,visual::color(visual::accent(0.13f)));
   ImGui::BeginDisabled(!selected && favorites.size()>=kSlots);
   ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{3,3});
   const bool changed=devui::visual::checkbox("##selected",&selected);
   ImGui::PopStyleVar();ImGui::SameLine(0,18);
   ImGui::TextUnformatted(name.data(),name.data()+name.size());ImGui::EndDisabled();
   ImGui::TableSetColumnIndex(1);
   if(available && equipment){visual::Font font(render::FontRole::Medium,20);ImGui::TextColored(equipped?visual::accent():visual::muted(),"%s",equipped?"Equipped":"In inventory");}
   else if(available)ImGui::TextColored(visual::muted(),"%u",carried);
   else {visual::Font font(render::FontRole::Body,16);ImGui::TextDisabled("%s",namesUnavailable?"Unavailable":"Not carried");}
   if(changed && s.prefs.select(s.category,{std::string(key),std::string(name)},selected))s.changed=true;
   ImGui::PopID();return changed;
  };
  // Unavailable favorites remain removable and retain their slot identity.
  for(std::size_t i=0;i<favorites.size();) {
   const auto& f=favorites[i];
   const bool carried=std::any_of(catalog.begin(),catalog.end(),[&](const Item& item){return item.key==f.key;});
   if(carried || !matches(f.name,s.search)){++i;continue;}
   if(!row(f.key,f.name,0,false,true))++i;
  }
  std::array<std::size_t,kMaxItems> filtered{};int count=0;
  for(std::size_t i=0;i<catalog.size() && count<static_cast<int>(filtered.size());++i)
   if(!catalog[i].key.empty() && matches(catalog[i].name,s.search))filtered[count++]=i;
  ImGuiListClipper clipper;clipper.Begin(count);
  while(clipper.Step())for(int n=clipper.DisplayStart;n<clipper.DisplayEnd;++n) {
   const auto& item=catalog[filtered[n]];
   const bool selected=std::any_of(favorites.begin(),favorites.end(),[&](const Favorite& f){return f.key==item.key;});
   row(item.key,item.name,item.count,true,selected,item.equipped);
  }
  if(!count && favorites.empty()){ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::TextDisabled("No matching inventory items.");}
  ImGui::EndTable();
 }
 ImGui::EndChild();ImGui::PopStyleVar();
}
}
