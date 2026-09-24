# Half-Life: Gladder — v1 Master Plan & GitHub Issues Roadmap

Based on the requirements in [`SPEC.md`](file:///e:/Dev/urgorri/halflife-refactored-gladder/SPEC.md), the Zero Core Pollution guidelines in [`AGENTS.md`](file:///e:/Dev/urgorri/halflife-refactored-gladder/AGENTS.md), and the integration findings in [`reports/upstream_integration_feasibility_report.md`](file:///e:/Dev/urgorri/halflife-refactored-gladder/reports/upstream_integration_feasibility_report.md), the complete development plan for **Half-Life: Gladder v1** has been decomposed and published directly to GitHub Issues in [`urgorri/halflife-refactored-gladder`](https://github.com/urgorri/halflife-refactored-gladder).

---

## 1. Architectural Strategy & Guarantees

* **Zero Core Pollution**: All implementation code lives exclusively under `dlls/gladder/` and `cl_dll/gladder/`. No files under `dlls/core/`, `dlls/ai/`, `cl_dll/core/`, or `pm_shared/` are modified, guaranteeing seamless synchronization with `upstream/master`.
* **Subsystem Registries**: Uses upstream's decoupled extension points (`REGISTER_GAMERULES`, `REGISTER_HUD_ELEMENT`, `REGISTER_USER_MSG`, `REGISTER_WEAPON_EX`, `ClientWeaponManager`, and `EntityVisualRegistry`).
* **Multi-Build Synchronization**: Every file addition maintains alignment across CMake (`CMakeLists.txt`), Visual Studio 2019 (`projects/vs2019/*.vcxproj`), and Linux (`linux/Makefile.*`).

---

## 2. Issues Breakdown by Implementation Phase

### Phase 1: Foundation, Build Systems & Lifecycle Core
| Issue | Title | Subsystems | Labels |
| :--- | :--- | :--- | :--- |
| [**#2**](https://github.com/urgorri/halflife-refactored-gladder/issues/2) | `feat(build): downstream scaffold and build system synchronization` | Build | `subsystem:build`, `phase:1-foundation` |
| [**#3**](https://github.com/urgorri/halflife-refactored-gladder/issues/3) | `feat(gamemode): implement CGladderRules lifecycle, loadout & save/load prevention` | Gamemode | `subsystem:gamemode`, `phase:1-foundation` |
| [**#4**](https://github.com/urgorri/halflife-refactored-gladder/issues/4) | `feat(entities): custom triggers, area volumes, Point A/B lifecycle & relays` | Entities | `subsystem:entities`, `phase:1-foundation` |
| [**#5**](https://github.com/urgorri/halflife-refactored-gladder/issues/5) | `feat(gamemode): wave lifecycle state machine, session timer & pacing telemetry` | Gamemode | `subsystem:gamemode`, `phase:1-foundation` |

### Phase 2: Procedural Spatial Indexing & Dynamic Spawner
| Issue | Title | Subsystems | Labels |
| :--- | :--- | :--- | :--- |
| [**#6**](https://github.com/urgorri/halflife-refactored-gladder/issues/6) | `feat(spawner): 32-unit spatial grid indexer, raycasting & binary cache (.grid)` | Spawner | `subsystem:spawner`, `phase:2-procedural` |
| [**#7**](https://github.com/urgorri/halflife-refactored-gladder/issues/7) | `feat(spawner): dynamic weighted tier distribution & edict garbage collection` | Spawner | `subsystem:spawner`, `phase:2-procedural` |

### Phase 3: Arcade Gameplay, Combos & Modifiers
| Issue | Title | Subsystems | Labels |
| :--- | :--- | :--- | :--- |
| [**#8**](https://github.com/urgorri/halflife-refactored-gladder/issues/8) | `feat(combat): kill streak combo tracking, decaying multiplier & species kill log` | Combat | `subsystem:combat`, `phase:3-gameplay` |
| [**#9**](https://github.com/urgorri/halflife-refactored-gladder/issues/9) | `feat(combat): composite score calculation & arcade performance grading (S-F)` | Combat | `subsystem:combat`, `phase:3-gameplay` |
| [**#10**](https://github.com/urgorri/halflife-refactored-gladder/issues/10) | `feat(gameplay): skill progression, elite champions, mutators & diminishing chargers` | Combat | `subsystem:combat`, `phase:3-gameplay` |

### Phase 4: Items, Visuals & Extended Arsenal
| Issue | Title | Subsystems | Labels |
| :--- | :--- | :--- | :--- |
| [**#11**](https://github.com/urgorri/halflife-refactored-gladder/issues/11) | `feat(entities): lambda collectible item (item_gladder_lambda) & arcade audio cues` | Entities, Audio | `subsystem:entities`, `subsystem:visuals-audio`, `phase:4-items-weapons` |
| [**#12**](https://github.com/urgorri/halflife-refactored-gladder/issues/12) | `feat(visuals): arcade floating/bobbing pickups, glows & dynamic lights` | Visuals | `subsystem:visuals-audio`, `phase:4-items-weapons` |
| [**#13**](https://github.com/urgorri/halflife-refactored-gladder/issues/13) | `feat(weapons): opposing force extended arsenal & client prediction` | Weapons | `subsystem:weapons`, `phase:4-items-weapons` |

### Phase 5: Client HUD, Telemetry & Persistence
| Issue | Title | Subsystems | Labels |
| :--- | :--- | :--- | :--- |
| [**#14**](https://github.com/urgorri/halflife-refactored-gladder/issues/14) | `feat(hud): custom network user messages (wave, stats, combo, summary)` | HUD | `subsystem:hud`, `phase:5-client-hud` |
| [**#15**](https://github.com/urgorri/halflife-refactored-gladder/issues/15) | `feat(hud): real-time gauntlet HUD overlay & animated combo meter` | HUD | `subsystem:hud`, `phase:5-client-hud` |
| [**#16**](https://github.com/urgorri/halflife-refactored-gladder/issues/16) | `feat(hud): post-match summary screen, camera backdrop & grade presentation` | HUD | `subsystem:hud`, `phase:5-client-hud` |
| [**#17**](https://github.com/urgorri/halflife-refactored-gladder/issues/17) | `feat(storage): persistent local map leaderboards (gladder/scores/*.dat)` | Storage | `subsystem:storage`, `phase:5-client-hud` |
