# RobCo P.A.L.M. — Fallout 4 VR Wheel Menu

**Personal Access & Loadout Manager**

RobCo P.A.L.M. is a customisable wheel menu for Fallout 4 VR, with quick access to your equipment, consumables, hand gestures, and mod settings.

## Features

- Choose up to eight items per category and hide sections you do not use, Config included.
- Equip weapons and armor, use consumables, or bring items into your hand with ROCK. With ROCK, a weapon chosen on the wheel goes into the hand you point with.
- Hold a weapon, or a loose aid or food item, and select an empty slot on its page to add it to the wheel; it is put away. Needs ROCK for aid and food.
- Use eight hand gestures with ROCK and FRIK (off by default; switch them on on the Wheel page).
- Change PALM's settings in game on its page in Mods Config FO4 VR, which **Config** opens, and see each change on the wheel.
- Add wheel sections through supported mods.

## Requirements

Fallout 4 VR, F4SEVR, RPS UI Framework, and Mods Config FO4 VR for PALM's settings page. Without Mods Config PALM runs, and its settings are edited in `PALM.toml`.

ROCK 1.0.4 or newer adds physical item handling; hand gestures also need FRIK 0.79.1 or newer with FRIK API 2.3.

## Using PALM

**Mode 1 (default):** click and release the physical right analog stick to open the wheel, point, and click with the pointing hand's trigger or A/X to select. Click **Cancel** to close.

**Mode 2:** hold **right trigger + grab** for **0.50 seconds** to open the wheel, point, and release to select. Aim at the center/Cancel area and release to cancel.

**Mode 3 — Custom** unlocks remapping and the choice between **Release to select** and **Click to select**; its binding is retained when you switch back to a preset. In **Click to select**, release the opening buttons before selecting with the pointing hand's trigger or A/X. The wheel stays open until you click **Cancel** or enter Config. Opening bindings support press, tap, double tap, hold, long press, and release, with an optional modifier on either hand. Touch and axis inputs are excluded; thumbstick click is supported. Tap and release bindings use click-to-select.

ROCK's trigger-equip and bipod clicks, and an object placement on the same hand, take priority over opening PALM. A bound hand inside a Virtual Holsters zone or unrelated held buttons can also block opening. Release the opening buttons fully before trying again.

## Settings

**Config** on the wheel opens RobCo PALM's page in Mods Config FO4 VR; it is also in Mods Config's own list of mods. The page has:

- **Wheel:** what the ring shows, in up to ten places: the five item categories, Gestures (two places, with ROCK and FRIK), sections other mods add, and Config itself. Under it, each category that is on has a page of its items: choose up to eight from what you carry. Changes show on the wheel as you make them; **Open the wheel** sets the menu aside to see them. Each save keeps its own wheel.
- **Controls:** the input mode, Custom's binding and way of selecting, where the wheel opens (distance, height, side and size, and whether it turns to face you, shown on the wheel as you change them), and each hand's pointer aim.
- **RPS UI:** RPS UI Framework's theme, sharpness, haptic strength and grip-to-move time, which every RPS mod's panels share.
- On the front page, a **Developer** section with the log level and the Developer switch, which adds the Developer section to the wheel. Actions a mod adds only at startup, as ROCK's, appear after a restart.

The controls, wheel placement, pointer aim, log level and Developer switch apply when you save, and are kept in `Documents\My Games\Fallout4VR\Mods_Config\RobCo_PALM\PALM.toml`, where each option has its help above it; manual edits are read at startup. The RPS UI page saves to RPS UI Framework's `RPS_UI.toml`. With Config hidden from the ring, open PALM's page from Mods Config.

New configurations use `[Controls] InputMode = 1`. Presets do not use `Mode` or `OpenMenu` as their binding; those keys store Custom's settings. `OpenMenu` is a binding table in F4VR-CommonFramework's form, such as `{ hand = "right", type = "release", button = "thumbstick" }`; a one-line binding an earlier PALM wrote becomes that table at the first start. The first start after an update carries your `PALM.ini` settings into `PALM.toml` and deletes the INI; an INI from before input modes keeps its binding through the matching preset or Custom.

**RPS UI settings (legacy)**, in the front page's Help section, opens RPS UI Framework's menu: PALM's earlier Wheel items page and its Wheel settings page, which keeps only the wheel's sections, with the Spawner and the Console. Without Mods Config FO4 VR, Config opens that menu instead, and PALM's other settings are edited in `PALM.toml`. The other RPS mods' settings are on each mod's page in Mods Config FO4 VR; ROCK adds a ROCK tab to RPS UI's menu with its setup wizard. The Console's shortcut buttons are in `Documents\My Games\Fallout4VR\Mods_Config\RPS_UI_Framework\ConsoleCommands.ini`; shortcuts you wrote in PALM's folder are copied there the first time.

## Installing

Install the `.zip` asset from [GitHub Releases](https://github.com/brunocatani/RobCo_PALM/releases/latest) through your mod manager. The plugin remains `Data/F4SE/Plugins/wheelmenu.dll`, with PALM's Mods Config page in `Data/ModsConfigFO4/Config/RobCo_PALM` and its banner in `Data/Textures/RobCo_PALM`; GitHub's automatic source archives are for development.

## Adding a mod section

The public headers are in [`include/RobCoPALM/`](include/RobCoPALM/), documented in [`docs/api/Menu.md`](docs/api/Menu.md). Resolve `PALMAPI_QueryInterfaceV1` from `wheelmenu.dll` and acquire the Menu table with `palm::api::acquire`. The [`physical magazine example`](tests/ConsumerExample.cpp) compiles with the tests. A section built against PALM 1.2's `PALMMenuApi.h` is updated with [Migration](docs/api/Migration.md).

A mod registers a unique section ID, display name, icon, and selection callback, then publishes a copied snapshot of up to eight items. PALM supports up to ten registered mod sections. New mod sections start hidden until the player switches them on, on RobCo PALM's Wheel page in Mods Config FO4 VR.

Selections run on the F4SE game task thread. Hold mode closes the wheel before dispatch; press mode keeps it open. Your mod owns the action and its vanilla or ROCK integration; a magazine entry can represent your mod's physical object rather than an ammo form. Respect the owning API's thread rules when performing the action. PALM's item snapshots are cleared on a game load, and mods republish for the new session. Unregister successfully before freeing callback state; `CallbackBusy` requires a later retry.

## Local development

From this repository, run:

```powershell
cmake --preset custom-fast
cmake --build build-fast --config Release
cmake --build build-fast --config Release --target build_tests
ctest --test-dir build-fast -C Release --output-on-failure
```

The preset deploys the DLL and PDB to `F4SE/Plugins`, and `data/` (PALM's Mods Config page and banner) to the root of the mod folder set by `COPY_PLUGIN_BASE_PATH` in your `CMakeUserPresets.json`. `PALMPreview.exe` previews the wheel and PALM's pages on the desktop; `PALMCapture.exe` renders screenshots without opening a window. RPS UI Framework's `RPSUISettingsPreview.exe` previews the settings menu. Desktop previews do not validate VR input or stereo presentation.

## Source code and documentation

- [GitHub repository and source code](https://github.com/brunocatani/RobCo_PALM)
- [DevArtificial documentation](https://devartificial.pro/docs/rps-stack/palm)
- [RPS UI Framework](https://github.com/brunocatani/RPS_UI_Framework)

## Credits

- [CommonLibF4VR](https://github.com/ArthurHub/CommonLibF4VR) and its contributors.
- [F4VR-CommonFramework](https://github.com/ArthurHub/F4VR-CommonFramework) for the controller binding parser and value types.
- [Omar Cornut](https://github.com/ocornut) and the [Dear ImGui](https://github.com/ocornut/imgui) contributors.
- [Arthur](https://github.com/ArthurHub), [RollingRock](https://github.com/rollingrock), and the FRIK contributors for FRIK and its public hand-pose API.

## License

Original RobCo PALM code is licensed under GNU General Public License version 3
or later (`GPL-3.0-or-later`). See [COPYRIGHT](COPYRIGHT) and [LICENSE](LICENSE).
Third-party components retain their respective licenses; their notices are in
[THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt).

## Current ROCK integration

Optional ROCK integration negotiates feature interfaces through `ROCKAPI_QueryInterfaceV1`. PALM reads and writes no ROCK settings. The section API remains PALM-owned and independent of ROCK and RPS UI tables.
