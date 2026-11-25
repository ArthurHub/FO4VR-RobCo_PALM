#include "PalmControls.h"
#include <Windows.h>
#include <atomic>
#include <mutex>
#include <format>
#include <cwchar>
#include <algorithm>
#include <charconv>
#include <span>
#include <vector>

namespace wheel {
namespace {
namespace settings=RPS::Settings;
// controls retains the custom binding while a fixed input mode is selected.
// store is PALM.toml's store; every use holds mutex.
struct State {std::mutex mutex;Controls controls;InputMode inputMode{InputMode::StickClick};PointerAim aim;std::unique_ptr<settings::Store> store;std::atomic_bool aimChanged{},gestures{true};};
State& state(){static State value;return value;}
constexpr std::array pitchKeys{"LeftPitchDegrees","RightPitchDegrees"};
constexpr std::array yawKeys{"LeftYawDegrees","RightYawDegrees"};
constexpr std::string_view kDefaultMode="press";
// PALM.ini's sOpenMenu, which only OpenMenu's conversion reads, and the default PALM.ini wrote for it.
constexpr std::string_view kLegacyOpenMenuKey="LegacyOpenMenu";
constexpr std::string_view kLegacyOpenMenuDefault="right release thumbstick suppress";
bool readAngle(const settings::Store& store,const char* key,float& value) {
 const auto& text=store.find("Pointer",key)->value;float parsed{};
 const auto result=std::from_chars(text.data(),text.data()+text.size(),parsed);
 if(result.ec!=std::errc{} || result.ptr!=text.data()+text.size() || !std::isfinite(parsed) || std::fabs(parsed)>90)return false;
 value=parsed;return true;
}
std::string angleText(float value){return settings::normalizeValue(settings::ValueType::Float,std::to_string(value)).value_or("0.0");}
std::vector<settings::Definition> palmDefinitions() {
 const auto player=settings::Group::Player;const PointerAim aim;
 std::vector<settings::Definition> result{
  {"Controls","InputMode","01. Controls","How PALM opens: 1 = click the right thumbstick (or ROCK's Palm binding when ROCK's setup wizard moves it), 2 = hold grip and trigger, 3 = Custom (Mode and OpenMenu).\n"
   "Change it in game on RobCo PALM's Controls page in Mods Config FO4 VR.",player,settings::ValueType::Integer,"1"},
  {"Controls","Mode","01. Controls","Custom only: press (click to select) or hold (release to select).",player,settings::ValueType::String,std::string(kDefaultMode)},
  {"Controls","OpenMenu","01. Controls","Custom only: the button that opens PALM, as a table of hand, type and button, for example\n"
   "{ hand = \"right\", type = \"release\", button = \"thumbstick\" }, with an optional duration and modifier = { button = \"grip\" }.\n"
   "Set it in game on RobCo PALM's Controls page in Mods Config FO4 VR.",player,settings::ValueType::Table,bindingText(Controls{})},
 };
 for(unsigned hand=0;hand<2;++hand) {
  const auto side=hand?"right":"left";
  result.push_back({"Pointer",pitchKeys[hand],"02. Pointer Aim",std::format("Tilts the {} hand's pointer up or down, in degrees. Range: -90 to 90.",side),player,settings::ValueType::Float,angleText(aim.pitch[hand])});
  result.push_back({"Pointer",yawKeys[hand],"02. Pointer Aim",std::format("Turns the {} hand's pointer left or right, in degrees. Range: -90 to 90.",side),player,settings::ValueType::Float,angleText(aim.yaw[hand])});
 }
 result.push_back({"Developer","Enabled","03. Developer","Adds the Developer section to the wheel, which lists the actions mods add to it.\n"
  "Applies when Mods Config saves; a mod that adds its actions only at startup, as ROCK does, shows them after a restart.",player,settings::ValueType::Boolean,"false"});
 result.push_back({"Logging","LogLevel","04. Logging","How much PALM writes to RobCoPALM.log: 2 = Info, 3 = Warnings, 4 = Errors, 6 = Off.\n"
  "Numbered as in ROCK and FRIK; 0 = Trace and 1 = Debug write the same as Info. Applies when Mods Config saves.",player,settings::ValueType::Integer,"2"});
 return result;
}
// The keys of PALM.ini, with the defaults it was written with.
std::span<const settings::LegacyKey> palmLegacyKeys() {
 static const auto table=[] {
  // texts owns every string the keys view; reserved so it never reallocates.
  struct Table{std::vector<std::string> texts;std::vector<settings::LegacyKey> keys;} result;
  const auto definitions=palmDefinitions();result.texts.reserve(definitions.size());
  const auto row=[&](std::string_view oldKey,std::string_view section,std::string_view key) {
   const auto found=std::find_if(definitions.begin(),definitions.end(),[&](const auto& d){return d.table==section && d.key==key;});
   settings::LegacyKey legacy{};legacy.oldKey=oldKey;legacy.table=section;legacy.key=key;legacy.oldSection=section;
   legacy.oldDefault=result.texts.emplace_back(found->defaultValue);
   result.keys.push_back(legacy);
  };
  row("iInputMode","Controls","InputMode");row("sMode","Controls","Mode");
  row("fLeftPitchDegrees","Pointer","LeftPitchDegrees");row("fRightPitchDegrees","Pointer","RightPitchDegrees");
  row("fLeftYawDegrees","Pointer","LeftYawDegrees");row("fRightYawDegrees","Pointer","RightYawDegrees");
  row("bEnabled","Developer","Enabled");
  settings::LegacyKey openMenu{};openMenu.oldKey="sOpenMenu";openMenu.table="Controls";openMenu.key=kLegacyOpenMenuKey;
  openMenu.kind=settings::LegacyKind::ConversionInput;openMenu.oldDefault=kLegacyOpenMenuDefault;openMenu.oldSection="Controls";
  result.keys.push_back(openMenu);
  return result;
 }();
 return table.keys;
}
/**
 * OpenMenu's table from the one-line binding PALM wrote before it kept a table: from PALM.toml's old string, or from
 * PALM.ini's sOpenMenu. Nothing once the file holds the table. A line the parser cannot read becomes the default.
 */
std::optional<std::string> convertOpenMenu(const settings::ConversionSource& source) {
 auto line=source.value("Controls","OpenMenu",settings::ValueType::String);
 if(!line)line=source.value("Controls",kLegacyOpenMenuKey,settings::ValueType::String);
 if(!line)return std::nullopt;
 Controls controls;
 if(const auto parsed=f4cf::vrcf::parseInputBinding(*line))controls.binding=*parsed;
 return bindingText(controls);
}
constexpr std::array kConversions{settings::ValueConversion{"Controls","OpenMenu",&convertOpenMenu}};
// An INI without iInputMode kept its binding: the matching preset, else Custom.
InputMode inferredMode(const Controls& controls) {
 return controls==Controls{}?InputMode::StickClick:controls==controlsForMode(InputMode::GripTriggerHold)?InputMode::GripTriggerHold:InputMode::Custom;
}
constexpr std::array activationTypes{f4cf::vrcf::ActivationType::Press,f4cf::vrcf::ActivationType::Tap,f4cf::vrcf::ActivationType::DoublePress,f4cf::vrcf::ActivationType::HoldDown,f4cf::vrcf::ActivationType::LongPress,f4cf::vrcf::ActivationType::Release};
constexpr std::array buttons{vr::k_EButton_SteamVR_Trigger,vr::k_EButton_Grip,vr::k_EButton_A,vr::k_EButton_ApplicationMenu,vr::k_EButton_SteamVR_Touchpad};
bool supported(vr::EVRButtonId button){return std::find(buttons.begin(),buttons.end(),button)!=buttons.end();}
/**
 * The binding of an inline table's TOML text, read as CommonFramework reads its config, or why it is none.
 */
std::expected<f4cf::vrcf::InputBinding,std::string> parseBindingTable(std::string_view text) {
 try {
  const auto document=toml::parse(std::format("binding = {}",text));
  if(const auto* node=document.get("binding");node && document.size()==1)return f4cf::vrcf::parseInputBindingTable(*node);
 }catch(const toml::parse_error&){}
 return std::unexpected(std::string(R"(OpenMenu is not a table, as { hand = "right", type = "release", button = "thumbstick" })"));
}
}
/**
 * The binding as OpenMenu's table, as CommonFramework writes it. PALM always suppresses its binding, so the table
 * leaves suppress out.
 */
std::string bindingText(const Controls& controls) {
 auto binding=controls.binding;binding.suppress=false;
 return f4cf::vrcf::inputBindingTomlText(binding);
}
bool parseControls(std::string_view mode,std::string_view text,Controls& out,std::string& error) {
 using namespace f4cf::vrcf;
 if(mode!="hold" && mode!="press"){error="Mode must be hold or press";return false;}
 const auto parsed=parseBindingTable(text);
 if(!parsed){error=parsed.error();return false;}
 if(!parsed->isEnabled() || !supported(parsed->button) || (parsed->modifier && !supported(parsed->modifier->button))){error="Choose a button, optionally with one modifier";return false;}
 if(parsed->hand==Hand::Any || (parsed->modifier && parsed->modifier->hand==Hand::Any)){error="Choose one hand: primary, offhand, right or left";return false;}
 if(std::find(activationTypes.begin(),activationTypes.end(),parsed->type)==activationTypes.end()) {
  error="Choose press, tap, double tap, hold, long press, or release. Touch and axes are not supported.";return false;
 }
 if(!std::isfinite(parsed->duration) || parsed->duration<0 || parsed->duration>5){error="Timing must be between 0 and 5 seconds";return false;}
 if(parsed->modifier && parsed->modifier->button==parsed->button && parsed->modifier->hand.value_or(parsed->hand)==parsed->hand){error="Choose two different buttons for a chord";return false;}
 out.mode=mode=="hold" && supportsReleaseSelection(parsed->type)?OpenMode::Hold:OpenMode::Press;out.binding=*parsed;
 out.binding.suppress=true;
 if(parsed->type==ActivationType::Press || parsed->type==ActivationType::Tap)out.binding.duration=0;
 error.clear();return true;
}
Controls snapshotControls(InputMode* inputMode){auto& s=state();std::scoped_lock lock(s.mutex);if(inputMode)*inputMode=s.inputMode;return controlsForMode(s.inputMode,s.controls);}
InputMode snapshotInputMode(){auto& s=state();std::scoped_lock lock(s.mutex);return s.inputMode;}
std::unique_ptr<settings::Store> openPalmSettings(const std::filesystem::path& path) {
 if(path.empty())return {};
 const auto directory=path.parent_path(),ini=directory/"PALM.ini";
 std::filesystem::create_directories(directory);
 // An INI written before input modes existed kept its binding through the
 // matching preset or Custom; the migration keeps that choice.
 bool inferMode=false;
 if(!std::filesystem::exists(path) && std::filesystem::exists(ini)) {
  std::array<wchar_t,16> mode{};
  GetPrivateProfileStringW(L"Controls",L"iInputMode",L"",mode.data(),static_cast<DWORD>(mode.size()),ini.c_str());
  inferMode=mode[0]==L'\0';
 }
 auto store=std::make_unique<settings::Store>(settings::Files{directory,"RobCo PALM",path.filename().string(),{},
   "## PALM.toml - RobCo PALM controls and pointer aim\n"
   "## Change these in game on RobCo PALM's page in Mods Config FO4 VR, or edit this file while the game is closed.\n"
   "## Each option has its help above it. When PALM starts it adds new options with their defaults and rewrites its\n"
   "## help lines (##). Your own notes are kept when they start with a single # on the lines above an option.\n",{}},
  palmDefinitions(),settings::LegacyIni{{ini},palmLegacyKeys()},std::span<const settings::RenamedKey>{},std::span<const settings::ValueConversion>(kConversions));
 store->load(true);
 if(inferMode) {
  Controls controls;std::string error;
  if(parseControls(store->find("Controls","Mode")->value,store->find("Controls","OpenMenu")->value,controls,error))
   if(const auto mode=inferredMode(controls);mode!=InputMode::StickClick &&
    store->setValue(settings::Group::Player,"Controls","InputMode",std::to_string(static_cast<int>(mode))))
    store->load(false); // A write changes the file; reading it shows the new value.
 }
 return store;
}
namespace {
/**
 * Applies the values the store read: the controls, the input mode and the pointer aim, and returns the problems of
 * those that do not read, each keeping the value PALM has. The runtime takes the aim at its next frame.
 */
std::vector<std::string> readStoreValues(State& s) {
 const auto& store=*s.store;
 std::vector<std::string> problems;
 Controls value;std::string error;
 if(parseControls(store.find("Controls","Mode")->value,store.find("Controls","OpenMenu")->value,value,error))s.controls=value;
 else problems.push_back("PALM.toml [Controls] OpenMenu: "+error+"; the binding PALM has is kept");
 // Presets are authoritative; Mode and OpenMenu store Custom's binding.
 const auto& selected=store.find("Controls","InputMode")->value;
 if(selected=="1")s.inputMode=InputMode::StickClick;
 else if(selected=="2")s.inputMode=InputMode::GripTriggerHold;
 else if(selected=="3")s.inputMode=InputMode::Custom;
 else problems.push_back("PALM.toml [Controls] InputMode is not 1, 2 or 3; mode 1 is used");
 bool valid=true;
 for(unsigned hand=0;hand<2;++hand) {
  valid=readAngle(store,pitchKeys[hand],s.aim.pitch[hand]) && valid;
  valid=readAngle(store,yawKeys[hand],s.aim.yaw[hand]) && valid;
 }
 if(!valid)problems.push_back("PALM.toml [Pointer]: an angle that is not a number from -90 to 90 keeps its default");
 s.aimChanged=true;
 return problems;
}
}
/**
 * Opens PALM.toml at path and applies its controls and pointer aim, returning the problems of the values that do not
 * read. An empty path keeps the defaults in memory only (the desktop preview).
 */
std::vector<std::string> initializeControls(const std::filesystem::path& path) {
 auto& s=state();std::scoped_lock lock(s.mutex);s.store.reset();s.controls={};s.inputMode=InputMode::StickClick;s.aim={};s.aimChanged=true;
 if(path.empty())return {};
 try{s.store=openPalmSettings(path);}
 catch(const std::exception& error){return {std::format("Could not open {}: {}",path.filename().string(),error.what())};}
 return readStoreValues(s);
}
/**
 * Reads PALM.toml again after another editor saved it, as Mods Config FO4 VR does, and returns the problems the read
 * found, each naming its line. Nothing happens before initializeControls opened the file.
 */
std::vector<std::string> reloadControls() {
 auto& s=state();std::scoped_lock lock(s.mutex);
 if(!s.store)return {};
 s.store->load(false);
 auto problems=s.store->messages();
 for(auto& problem:readStoreValues(s))problems.push_back(std::move(problem));
 return problems;
}
/**
 * [Logging] LogLevel as spdlog's level number: 0 = trace to 6 = off, as ROCK and FRIK number it. A value outside that
 * range, or one that does not read, is 2 (info).
 */
int logLevel(const settings::Store& store) {
 const auto& text=store.find("Logging","LogLevel")->value;int level{};
 const auto result=std::from_chars(text.data(),text.data()+text.size(),level);
 return result.ec==std::errc{} && result.ptr==text.data()+text.size() && level>=0 && level<=6?level:2;
}
/**
 * The log level PALM.toml held at its last read; 2 (info) before initializeControls opened it.
 */
int snapshotLogLevel(){auto& s=state();std::scoped_lock lock(s.mutex);return s.store?logLevel(*s.store):2;}
/**
 * [Developer] Enabled as PALM.toml held it at its last read; off before initializeControls opened it.
 */
bool snapshotDeveloperEnabled(){auto& s=state();std::scoped_lock lock(s.mutex);return s.store && s.store->find("Developer","Enabled")->value=="true";}
PointerAim snapshotPointerAim(){auto& s=state();std::scoped_lock lock(s.mutex);return s.aim;}
bool takePointerAimChange(){return state().aimChanged.exchange(false);}
void setGestureIntegrationAvailable(bool available){state().gestures=available;}
bool gestureIntegrationAvailable(){return state().gestures.load();}
}
