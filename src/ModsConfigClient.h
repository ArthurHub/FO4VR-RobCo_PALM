#pragma once

// PALM's client of Mods Config FO4 VR's native API, under "RobCoPALM", the modName of
// PALM's config spec (data/ModsConfigFO4/Config/RobCo_PALM/spec.json). The page edits
// PALM.toml itself; after each Save the client reads PALM.toml again, so the controls and
// pointer aim follow without a restart. It also gives the page its version line.
// Without Mods Config nothing is called, and PALM.toml is edited by hand.
// Threads: Mods Config calls on the game's main thread.

namespace wheel::mods_config_client {
void connect() noexcept;
}
