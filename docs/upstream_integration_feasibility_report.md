# Upstream Integration & Feasibility Audit Report: Half-Life Gladder

**Auditor:** Richard the Super Integration Checker 3000  
**Target Mod:** Half-Life: Gladder ([`SPEC.md`](../SPEC.md))  
**Repository:** `urgorri/halflife-refactored-gladder`  
**Upstream Repository:** `urgorri/halflife-refactored`  
**Compliance Standard:** Zero Core Pollution Policy ([`AGENTS.md`](../AGENTS.md))  
**Date:** September 2026  

---

## 1. Executive Verdict

### **VERDICT: GREEN (Frictionless Downstream Integration Path)**

The upstream refactoring codebase (`urgorri/halflife-refactored`) has already implemented an exceptionally mature, decoupled extension infrastructure. All gameplay subsystems, procedural mechanisms, telemetry pipelines, visual effects, and arcade features specified in [`SPEC.md`](../SPEC.md) can be implemented **purely as downstream modules within `dlls/gladder/` and `cl_dll/gladder/`**.

**Zero core engine files (`dlls/core/`, `dlls/ai/`, `cl_dll/core/`, `pm_shared/`) need to be modified downstream.**

Two minor optional upstream enhancements have been identified (damage observation hooks in `CGameRules`), but downstream implementation can proceed immediately using existing baseline mechanisms without any blockers.

---

## 2. Comprehensive Feature Compatibility Matrix

| SPEC.md Feature / Subsystem | Engine / SDK Touch Point | Available Upstream Hook / Mechanism | Friction Level | Downstream Isolation Strategy |
| :--- | :--- | :--- | :---: | :--- |
| **Custom Game Mode Registration** | Gamemode factory & instantiation | `REGISTER_GAMERULES`, `GameRulesFactory` | **Zero** | Subclass `CGameRules` as `CGladderRules` in `dlls/gladder/gladder_rules.cpp`. |
| **Safe Staging (Point A) Initial Loadout** | Player spawn lifecycle | `CGameRules::PlayerSpawn(CBasePlayer*)` | **Zero** | `pPlayer->GiveNamedItem("item_suit")` & `weapon_crowbar` on initial match spawn. |
| **On-Demand Wave Activation** | In-map boundary triggers | `LINK_ENTITY_TO_CLASS(trigger_gladder_start, ...)` | **Zero** | Custom trigger entity invokes `CGladderRules::StartWave()`. |
| **Wave Traversal & Point B Finish** | In-map extraction trigger | `LINK_ENTITY_TO_CLASS(trigger_gladder_finish, ...)` | **Zero** | Custom trigger invokes `CGladderRules::CompleteWave()`. |
| **Player Teleport & State Persistence** | Wave reset lifecycle | Direct `pev->origin`/`pev->angles` update | **Zero** | Teleport player to Point A without stripping weapons/ammo/health. |
| **Save/Load Elimination** | Engine save/load routines | `CGameRules::FAllowAutoSave()`, `FPlayerCanRespawn()` | **Zero** | Disable autosave; override `PlayerRespawn` and `FPlayerCanRespawn` to block `reload`. |
| **Wave Garbage Collection (Edict Budget)** | Entity lifecycle & cleanup | `gpGlobals->maxEntities`, `UTIL_Remove` | **Zero** | Iterate active edicts at wave end; purge monsters, dropped items, gibs, decals. |
| **Spatial Grid Indexer (`maps/grid/`)** | Geometric raycasting & caching | `UTIL_TraceLine`, `UTIL_TraceHull`, `fopen` | **Zero** | Scan area volumes sampled at 32 units, verify hulls, cache binary `.grid` files. |
| **Area Bounding Volumes** | Mapper boundary markers | `LINK_ENTITY_TO_CLASS(trigger_gladder_area, ...)` | **Zero** | Custom volume brush/point entity registering bounds with grid indexer. |
| **Dynamic Spawner & Tier Distribution** | Procedural entity creation | `CBaseEntity::Create`, `CREATE_NAMED_ENTITY` | **Zero** | Weighted random distribution selecting valid grid cells and spawning monsters/items. |
| **Elite Champion Monsters** | Monster visual & stat tier | `pev->renderfx = kRenderFxGlowShell`, `pev->health` | **Zero** | Procedurally tag monster on spawn with red glow shell, increased HP, and aggression. |
| **Engine Skill Tier Scaling** | Dynamic difficulty milestones | `SkillManager::SetSkillLevel`, `RefreshSkillData` | **Zero** | Switch engine skill (`SKILL_EASY` $\rightarrow$ `SKILL_MEDIUM` $\rightarrow$ `SKILL_HARD`) at wave milestones. |
| **Diminishing Wall Chargers** | `func_healthcharger`, `func_recharge` | `CGameRules::FlHealthChargerCapacity()`, `CBaseWallCharger::Recharge()` | **Zero** | Override capacity in `CGladderRules`; call `pCharger->Recharge()` on all chargers at wave start. |
| **Special Wave Mutators** | Lightstyles, gravity, swarm | `LIGHT_STYLE`, `CVAR_SET_FLOAT("sv_gravity", ...)` | **Zero** | Apply mutator state in `StartWave()` and restore defaults on wave completion. |
| **Arcade Kill Streak & Combo Multiplier** | Monster kill events | `CGameRules::MonsterKilled(pVictim, pKiller, pInflictor)` | **Zero** | Inspect `pVictim->m_LastHitGroup` (headshots), track streak decay timer in `dlls/gladder/`. |
| **Lambda Collectible Item** | Unique per-wave item | `ItemRegistry::Register`, `LINK_ENTITY_TO_CLASS` | **Zero** | Custom `item_gladder_lambda` item; floating model; bonus score; acoustic cue. |
| **Arcade Floating/Bobbing Pickups** | Client-side pickup rendering | `EntityVisualRegistry::RegisterModifier` | **Zero** | Hook `HUD_AddEntity` in `cl_dll/gladder/` to apply rotation, sine bobbing, glow, dlights. |
| **Opposing Force Extended Arsenal** | Weapon prediction & registration | `REGISTER_WEAPON_EX`, `ClientWeaponManager` | **Zero** | Implement weapons in `dlls/gladder/weapons/` and register client prediction cleanly. |
| **Custom Network User Messages** | Client/Server telemetry | `REGISTER_USER_MSG`, `gEngfuncs.pfnHookUserMsg` | **Zero** | Define `GladderWave`, `GladderStats`, `GladderCombo`, `GladderSummary`. |
| **Real-Time HUD Overlays** | HUD rendering pipeline | `REGISTER_HUD_ELEMENT`, `CHudBase` | **Zero** | Subclass `CHudBase` for combo meter, stopwatch, and wave stats; auto-attached by `CHud`. |
| **End-of-Run Summary Screen** | Post-match GUI & camera | `CHudBase::Draw`, `SET_VIEW`, `trigger_camera` | **Zero** | Switch view to camera entity, calculate composite grade ($S/A/B/C/D/F$), draw scoreboard. |
| **Local Map Leaderboards** | Persistent High Scores | File I/O (`gladder/scores/<mapname>.dat`) | **Zero** | Read/write Top 10 records per map on match conclusion. |

---

## 3. Upstream Extension Point Audit: Technical Verification

### 3.1 Gamemode Lifecycle & Virtual Callbacks (`CGameRules`)
* **Registration:** `GameRulesFactory::Register` via `REGISTER_GAMERULES("gladder", CreateGladderRules, ConditionGladder, 100)` allows `CGladderRules` to take priority over vanilla singleplayer when the Gladder gamemode is active.
* **Monster Kill Hook:** `CBaseMonster::Killed` explicitly calls `g_pGameRules->MonsterKilled( this, pevAttacker, g_pevLastInflictor );` ([`dlls/core/combat.cpp#L246`](file:///e:/Dev/urgorri/halflife-refactored-gladder/dlls/core/combat.cpp#L246)).
* **Headshot Detection:** `pVictim->m_LastHitGroup` accurately reflects `HITGROUP_HEAD` when `MonsterKilled` is invoked ([`dlls/ai/basemonster.h#L57`](file:///e:/Dev/urgorri/halflife-refactored-gladder/dlls/ai/basemonster.h#L57)).
* **Server Tick:** `g_pGameRules->Think()` executes on every server frame from `StartFrame()` ([`dlls/core/client.cpp#L410`](file:///e:/Dev/urgorri/halflife-refactored-gladder/dlls/core/client.cpp#L410)), enabling the wave stopwatch and combo decay timers to run with microsecond precision.
* **Player Tick:** `g_pGameRules->PlayerThink( this )` executes during `CBasePlayer::PreThink()` ([`dlls/core/player_physics.cpp#L264`](file:///e:/Dev/urgorri/halflife-refactored-gladder/dlls/core/player_physics.cpp#L264)).

### 3.2 Wall Chargers & Dynamic Capacity Degradation
* **Capacity Overrides:** `CWallHealth::GetCapacity()` and `CWallRecharge::GetCapacity()` directly call `g_pGameRules->FlHealthChargerCapacity()` and `g_pGameRules->FlHEVChargerCapacity()` ([`dlls/systems/chargers.h#L54-L73`](file:///e:/Dev/urgorri/halflife-refactored-gladder/dlls/systems/chargers.h#L54-L73)).
* **Re-energization:** `CBaseWallCharger::Recharge()` is an exported public method ([`dlls/systems/chargers.h#L26`](file:///e:/Dev/urgorri/halflife-refactored-gladder/dlls/systems/chargers.h#L26)), allowing `CGladderRules` to recharge all map chargers to the diminished wave capacity at the start of every wave.

### 3.3 Dynamic Difficulty Scaling (`SkillManager`)
* Upstream provides runtime difficulty queries and dynamic skill level selection via `SkillManager::SetSkillLevel( iLevel )` and `SkillManager::InvalidateCache()` ([`dlls/core/skill_manager.h#L59-L63`](file:///e:/Dev/urgorri/halflife-refactored-gladder/dlls/core/skill_manager.h#L59-L63)).
* When `CGladderRules` promotes waves from Easy $\rightarrow$ Medium $\rightarrow$ Hard, it simply invokes `SkillManager::SetSkillLevel` followed by `RefreshSkillData()`, instantly updating all engine monster stats without requiring level restarts.

### 3.4 Client-Side HUD Pipeline (`HudRegistry` & `CHudBase`)
* Any HUD element subclassing `CHudBase` and declared with `REGISTER_HUD_ELEMENT(s_MyHudElem, iDrawOrder)` is dynamically attached to `CHud` during `CHud::Init()` ([`cl_dll/hud/hud.cpp#L356`](file:///e:/Dev/urgorri/halflife-refactored-gladder/cl_dll/hud/hud.cpp#L356)), vid-inited via `HudRegistry::VidInitAll()`, ticked in `CHud::Think()`, and drawn in `CHud::Redraw()`.
* Downstream HUD elements never need to touch `hud.cpp` or `hud_redraw.cpp`.

### 3.5 Client-Side Visual Modifiers (`EntityVisualRegistry`)
* `cl_dll/entities/entity.cpp` invokes `EntityVisualRegistry::ApplyModifiers( type, ent, modelname )` inside `HUD_AddEntity` ([`cl_dll/entities/entity.cpp#L36-L39`](file:///e:/Dev/urgorri/halflife-refactored-gladder/cl_dll/entities/entity.cpp#L36-L39)).
* Downstream can register `REGISTER_ENTITY_VISUAL_MODIFIER(GladderPickupVisuals)` to apply 90°/sec yaw rotation, sine levitation, `kRenderFxGlowShell` color shells, and client-side dynamic lights (`CL_AllocDlight`) with zero server network overhead.

### 3.6 User Message System (`UserMessageRegistry`)
* Server network messages declared with `REGISTER_USER_MSG(GladderWave, -1, &gmsgGladderWave)` in `dlls/gladder/` are automatically linked during `ClientDLL_Init` ([`dlls/core/client_usermsg.cpp#L108`](file:///e:/Dev/urgorri/halflife-refactored-gladder/dlls/core/client_usermsg.cpp#L108)).
* Client hooks in `cl_dll/gladder/` register corresponding handlers via `gEngfuncs.pfnHookUserMsg("GladderWave", MsgFunc_GladderWave)` inside `CHudBase::Init()`.

### 3.7 Custom Items & Opposing Force Weapons
* **Item Tracking:** `ItemRegistry` provides built-in auxiliary player inventory storage (`ItemRegistry::AddPlayerCustomItem`, `GetPlayerCustomItemCount`) ([`dlls/items/item_registry.h#L38-L45`](file:///e:/Dev/urgorri/halflife-refactored-gladder/dlls/items/item_registry.h#L38-L45)).
* **Weapons:** `WeaponRegistry` and `ClientWeaponManager` provide factory-based registration (`REGISTER_WEAPON_EX` and `REGISTER_CLIENT_WEAPON`) supporting up to 64 client prediction entities without touching core weapon files ([`cl_dll/hl/client_weapon_manager.h#L61-L81`](file:///e:/Dev/urgorri/halflife-refactored-gladder/cl_dll/hl/client_weapon_manager.h#L61-L81)).

---

## 4. Upstream Refactoring Recommendations (Optional / Ergonomic)

While there are **no blockers** preventing immediate downstream development, the following 2 upstream refactoring requests are proposed for future inclusion in `urgorri/halflife-refactored` to provide maximum architectural purity:

### Recommendation 1: Agnostic Player Damage Hook in `CGameRules`
* **Current Situation:** `CGameRules::FPlayerCanTakeDamage(CBasePlayer*, CBaseEntity*)` determines if damage is permitted, but does not receive the damage amount (`flDamage`) or damage type (`bitsDamageType`). To track damage for performance grading ($S/A/B/C/D/F$), downstream monitors player health/armor deltas in `PlayerThink()`.
* **Proposed Upstream Addition:**
  Add a non-pure virtual callback to `CGameRules`:
  ```cpp
  // dlls/gameplay/gamerules.h
  virtual void PlayerDamaged( CBasePlayer *pPlayer, CBaseEntity *pAttacker, float flDamage, int bitsDamageType ) {}
  ```
  Call inside `CBasePlayer::TakeDamage` ([`dlls/core/player_combat.cpp#L263`](file:///e:/Dev/urgorri/halflife-refactored-gladder/dlls/core/player_combat.cpp#L263)):
  ```cpp
  if ( g_pGameRules )
  {
      g_pGameRules->PlayerDamaged( this, pAttacker, flDamage, bitsDamageType );
  }
  ```
* **Behavioral Equivalence:** 100% backward-compatible. Vanilla default implementation is an empty inline body `{}`.

### Recommendation 2: Agnostic Monster Damage Modification Hook
* **Current Situation:** Monster deaths are hooked via `MonsterKilled()`. Per-hit damage modifications (such as mutators or vampire mechanics) can be applied in `MonsterKilled()` or weapon code.
* **Proposed Upstream Addition:**
  Add a damage filter virtual method to `CGameRules`:
  ```cpp
  // dlls/gameplay/gamerules.h
  virtual float FlMonsterDamage( CBaseMonster *pMonster, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
  {
      return flDamage;
  }
  ```
  Call inside `CBaseMonster::TakeDamage` ([`dlls/core/combat_damage.cpp#L77`](file:///e:/Dev/urgorri/halflife-refactored-gladder/dlls/core/combat_damage.cpp#L77)):
  ```cpp
  if ( g_pGameRules )
  {
      flTake = g_pGameRules->FlMonsterDamage( this, pevInflictor, pevAttacker, flDamage, bitsDamageType );
  }
  ```
* **Behavioral Equivalence:** 100% backward-compatible. Default returns unmodified `flDamage`.

---

## 5. Downstream Implementation Blueprint

To strictly maintain the Zero Core Pollution Policy, all Gladder mod files should be organized according to the following folder and component architecture:

```
halflife-refactored-gladder/
├── dlls/
│   └── gladder/
│       ├── gladder_rules.h / .cpp           # CGladderRules (CGameRules subclass & lifecycle)
│       ├── gladder_wave_manager.h / .cpp    # Wave state machine, stopwatch & timers
│       ├── gladder_grid_indexer.h / .cpp    # 32-unit raycasting indexer & maps/grid/*.grid cache
│       ├── gladder_spawner.h / .cpp         # Procedural weighted spawner & edict budget manager
│       ├── gladder_combo_tracker.h / .cpp   # Kill streak multiplier & combo decay logic
│       ├── gladder_scoring.h / .cpp         # Composite score calculator & arcade grade ranker
│       ├── gladder_entities.h / .cpp        # trigger_gladder_start, finish, area, relay
│       ├── gladder_items.h / .cpp           # item_gladder_lambda collectible entity
│       ├── gladder_usermsg.h / .cpp         # Network message descriptors & registration
│       ├── gladder_leaderboard.h / .cpp     # Local High Score storage (gladder/scores/*.dat)
│       └── weapons/                         # Opposing Force weapon classes
│           ├── weapon_pipewrench.cpp
│           ├── weapon_knife.cpp
│           ├── weapon_eagle.cpp
│           ├── weapon_sniper.cpp
│           ├── weapon_m249.cpp
│           ├── weapon_sporelauncher.cpp
│           ├── weapon_shockrifle.cpp
│           ├── weapon_displacer.cpp
│           └── weapon_grapple.cpp
│
├── cl_dll/
│   └── gladder/
│       ├── hud_gladder_overlay.h / .cpp     # Persistent gauntlet HUD (wave, timer, frags, collectibles)
│       ├── hud_gladder_combo.h / .cpp       # Active kill streak & combo decay gauge
│       ├── hud_gladder_summary.h / .cpp     # End-of-run scoreboard & performance rank grade screen
│       ├── gladder_visuals.h / .cpp         # Floating/bobbing item visuals & dynamic light modifiers
│       └── gladder_cl_weapons.cpp           # Client-side weapon prediction registrations
│
└── reports/
    └── upstream_integration_feasibility_report.md
```

### Build Systems Synchronization Checklist
When adding these files:
1. **CMake (`CMakeLists.txt`):** Automatically collects `dlls/**/*.cpp` and `cl_dll/**/*.cpp` via dynamic `file(GLOB_RECURSE)`.
2. **Visual Studio 2019 (`projects/vs2019/`):** Add files to `hldll.vcxproj`, `hl_cdll.vcxproj`, and update their `.filters`.
3. **Linux Makefiles (`linux/`):** Add files to `Makefile.hldll` and `Makefile.hl_cdll`.
4. **Symbol Verification:** Run `python tests/verify_symbols.py` to ensure export tables remain clean.

---

## 6. Conclusion

The upstream codebase `urgorri/halflife-refactored` provides 100% of the required decoupled hooks, registries, and virtual overrides needed to build **Half-Life: Gladder**. Developers can proceed immediately with full implementation in `dlls/gladder/` and `cl_dll/gladder/` with complete confidence in upstream synchronization and zero core pollution.
