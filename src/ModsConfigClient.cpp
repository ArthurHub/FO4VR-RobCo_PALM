#include "PCH.h"
#include "ModsConfigClient.h"
#include "PalmControls.h"
#include <ModsConfigFO4Api.h>

namespace wheel::mods_config_client {
namespace {
using ::mods_config::api::ModsConfigFO4Client;
using ::mods_config::api::Saved;
using ::mods_config::api::Value;

// The modName of PALM's config spec.
constexpr const char* kModName="RobCoPALM";

/**
 * A Save wrote PALM.toml: PALM reads it again and sets its log level; the runtime applies the controls and pointer aim
 * at its next frame.
 */
void onSaved(const Saved&) {
 const auto messages=reloadControls();
 spdlog::set_level(static_cast<spdlog::level::level_enum>(snapshotLogLevel()));
 for(const auto& message:messages)spdlog::warn("{}",message);
 spdlog::info("PALM.toml read again after a Mods Config Save");
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
  client.subscribeToSaves(&onSaved);
  if(const auto* messaging=F4SE::GetMessagingInterface())
   (void)messaging->RegisterListener(&onModsConfigMessage,::mods_config::api::MODS_CONFIG_FO4_F4SE_NAME);
  if(const int error=client.connect(kModName);error!=0)
   spdlog::warn("Mods Config FO4 VR not connected ({}): PALM's settings are edited only in PALM.toml",error);
  else spdlog::info("Connected to Mods Config FO4 VR (API {})",client.apiVersion());
 }catch(...){spdlog::error("Connecting to Mods Config FO4 VR failed");}
}
}
