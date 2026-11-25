#pragma once

// PALM's client of Mods Config FO4 VR's native API, under "RobCoPALM", the modName of
// PALM's config spec (data/ModsConfigFO4/Config/RobCo_PALM/spec.json). The page edits
// PALM.toml itself; after each Save the client reads PALM.toml again, so the controls and
// pointer aim, the log level and the Developer section follow without a restart. It also gives
// the page its version line and its Open the wheel and Open settings buttons, which minimize
// the menu while the wheel or RPS UI's settings menu is open. Without Mods Config nothing is called, and PALM.toml is edited by hand.
// Threads: Mods Config calls on the game's main thread, and restoreMenu is called there too.

namespace wheel::mods_config_client {
void connect() noexcept;
void restoreMenu() noexcept;
}
