# Half-Life: Gladder - Non-Technical Game Specification

## 1. Overview & Vision

**Half-Life: Gladder** is an arcade-style, wave-based gauntlet runner mod for the Half-Life (GoldSrc) engine. The mod delivers high-replayability, procedural arcade action while preserving the classic atmospheric look, feel, and universe of the original game.

Instead of progressing linearly through a series of maps, players run through a single map repeatedly across sequential "waves" or "laps". The environment remains constant, but each wave dramatically transforms the gameplay by procedurally populating the map with new configurations of enemies, hazards, weapons, and pickups with escalating difficulty.

---

## 2. Core Gameplay Loop

1. **Spawn at Point A (Safe Staging & Initial Loadout)**: The player begins any game session already equipped with the **HEV Suit (`item_suit`)** and the **Crowbar (`weapon_crowbar`)**, eliminating the need for map-placed suit entities. Point A is an author-guaranteed safe staging area with no monster spawn points.
2. **On-Demand Wave Activation**: The wave does not force immediate combat upon arrival. The player triggers wave activation on demand (e.g., crossing a start boundary or cycling airlock doors), initiating procedural spawns and starting the active wave stopwatch.
3. **Traversal & Combat**: The player navigates through the map toward Point B while battling procedurally spawned enemies and scavenging randomized supplies.
4. **Reaching Point B (Finish)**: Entering the designated extraction / completion zone (Point B) immediately completes the current wave.
5. **Screen Fade & Teleportation**: The moment Point B is touched, the screen immediately turns solid black (instant 100% black fade). The viewport remains completely black during the exact instant the player is teleported back to Point A, followed by a smooth progressive screen fade from black back to 0% transparency to reveal the reset staging area.
6. **Wave Increment & Respawn**: The wave counter increases (Wave 1 → Wave 2 → ...), a comprehensive garbage collection clears leftover monsters and entities, and a fresh wave of threats and supplies is generated across the map.
7. **Player State Persistence**: The player preserves all collected weapons, remaining ammunition, and current health/armor across waves, making continuous resource preservation critical.
8. **Persistence**: The loop repeats continuously without map reloads or level transitions until match completion.

---

## 3. Wave Progression & Difficulty Scaling

* **Dynamic Spawning & Weighted Tier Distribution**:
  * Every wave procedurally generates monsters, weapons, and pickups across indexed grid points.
  * **Non-Exclusive Tier Spawning**: Higher waves do not eliminate basic enemies. Low-tier threats (such as Headcrabs or Zombies) can appear from Wave 1 to the final wave.
  * **Dynamic Probability Weights**:
    * In early waves, probability distribution is heavily skewed toward basic, low-tier threats.
    * As waves advance, the probability weights smoothly shift, granting progressively higher odds for advanced permitted tiers (e.g., Alien Grunts, Human Assassins, HECU Soldiers) to spawn, while basic monsters still remain in the pool as secondary/ambient fodder.
    * Spawning is strictly constrained by each map's whitelist/blacklist (e.g., if a map permits only specific enemy types, the probability curve operates exclusively within that allowed subset).
* **Progressive Engine Skill Tiers (`skill 1` → `skill 2` → `skill 3`)**:
  * Rather than introducing arbitrary stat multipliers, the mod utilizes the native, carefully balanced GoldSrc skill definitions (`skill 1` = Easy, `skill 2` = Medium, `skill 3` = Hard).
  * The active engine `skill` level advances **gradually and automatically** based on wave milestones:
    * **Early Waves**: Start at `skill 1`, allowing the player to establish their initial inventory and route.
    * **Mid-Game Milestone**: After a set number of waves, the engine promotes to `skill 2`, activating medium monster health pools, higher damage outputs, and sharper combat reactions.
    * **Late-Game Milestone**: Advances to `skill 3` (Hard), bringing maximum monster toughness, lethal damage, and aggressive AI behavior.
  * In addition to engine skill progression, procedural spawn density and Elite Champion spawn rates continue scaling with wave count.
* **Time-Driven Match Objective & Session Expiration Lifecycle**:
  * The core goal of each run is **time-based**: players must survive, navigate the gauntlet, and clear as many waves as possible within a configured session duration / time limit (e.g., 10, 15, or 20 minutes) or survival clock.
  * Wave completions are continuously counted and logged as the primary scoring metric, but the match concludes when the time limit expires (or upon player death).
  * **Session Time Expiration Flow**:
    * When the session time limit expires (`m_flSessionEndTime` reached, remaining time = 0), the match transitions definitively into `MATCH OVER`.
    * Rather than becoming inactive or leaving the mod in an unhandled limbo state, session expiration must execute the full, seamless match termination lifecycle (identical to player death, but with survival outcome):
      * **Player Freezing & Combat Halting**: Player movement, jumping, and weapon attacks are immediately frozen (`FL_FROZEN`), halting combat while preserving the current scene.
      * **Audio Silencing & Mutator Termination**: Looping weapon sound channels (`CHAN_WEAPON`) and mutator alert sirens are silenced immediately.
      * **Match End Acoustic Sting**: Plays `GLADDER_CUE_MATCH_END`.
      * **Camera Backdrop Transition**: The player's viewport switches immediately to the end-game backdrop cameras (`gladder_camera`), starting with the third-person camera.
      * **Score Calculation & Persistence**: Final composite score, wave milestones, and grade are computed (`STATUS: SURVIVED - TIME EXPIRED`) and saved to local map leaderboards (`gladder/scores/<mapname>.dat`).
      * **Summary Screen Modal**: Activates the undismissable end-of-run summary screen on the client HUD.
* **Arcade Scoring & Combo Multiplier**:
  * **End-of-Match Score Calculation**: To preserve clean screen visibility and immersion, the cumulative score is **calculated and revealed exclusively at the end of the match** on the summary screen.
  * **In-Game Combo Meter**: During active combat, rapid consecutive kills within a decaying window build a streak multiplier (`x2`, `x3`, `x4`, etc.). The game logs every combo event, streak count, and the peak maximum combo attained for the final score calculation.
  * **Direct & Indirect Player Kill Attribution**:
    * Combat kills, frags, and combo progression MUST be strictly credited only when caused directly or indirectly by the player against living, hostile combatants.
    * **Direct Attribution**: Eliminations caused by player weapon discharges, bullets, projectiles, and placed explosives (satchels, tripmines, grenades).
    * **Indirect Attribution**: Eliminations caused by environmental detonations instigated by the player (e.g. shooting or breaking an explosive crate or barrel, `func_breakable`). The fatal explosion inherits player ownership so that surrounding enemy casualties count toward player frags and combo chaining.
    * **Non-Player & Non-Combatant Exclusion**:
      * Any monster death resulting from environmental hazards (slime, pits, fall damage, trigger_hurt) or AI monster infighting / accidental friendly fire without player instigation is strictly excluded.
      * **Dead Monster Corpse Props Exclusion**: Striking, shooting, or exploding pre-placed dead monster props (such as `monster_barney_dead`, `monster_scientist_dead`, `monster_hgrunt_dead`, `monster_hevsuit_dead`, or any entity with `pev->deadflag != DEAD_NO` at the time of damage) must NEVER increment `FRAGS`, advance or refresh combo meters, or register in itemized species kill logs. These props serve purely as ambient environmental decor.
      * **Projectile & Bioweapon Sub-Entity Exclusion**: Biological weapon projectiles and sub-entities that inherit from `CBaseMonster` (most notably Alien Grunt hornets `hornet` / `CHornet`, or player bioweapons) that can be shot or destroyed in flight must NEVER be credited as monster frags, combos, or species eliminations.
      * Excluded entities do not increment `FRAGS`, do not advance or refresh the combo meter, and do not contribute to final combat scores or telemetry.
* **Elite Monster Variants (Champions)**:
  * Enemies have a procedurally rolled chance to spawn as **Elite Champions**, with the spawn probability increasing as wave numbers climb.
  * **Visual Identification**: Rendered using GoldSrc's engine glow shell (`kRenderFxGlowShell` with red RGB color `RGB(255, 32, 32)`). The shell aura thickness (`renderamt`) must be kept compact and contouring closely to the monster's body (matching the subtle outline thickness used on weapon pickups, e.g. `renderamt = 16`), preventing oversized, blinding, or detached shell visual artifacts while preserving clear champion identification.
  * **Attributes**: Enhanced durability, heightened aggression, and elevated point payouts upon defeat.
* **Dynamic Supply Balance, Arsenal Categorization & Ammunition Scarcity**:
  * **Categorical Separation of Weapons (Fixed vs. Consumable Ordnance)**:
    * **Persistent Fixed Weapons**: Primary firearms and permanent combat tools (`weapon_glock`, `weapon_shotgun`, `weapon_mp5`, `weapon_357`, `weapon_crossbow`, `weapon_rpg`, `weapon_gauss`, `weapon_egon`, `weapon_knife`, `weapon_pipewrench`, `weapon_eagle`, `weapon_sniper`, `weapon_m249`). These are durable assets retained across all wave laps.
    * **Consumable / Stackable Ordnance Weapons**: Deployable and throwable ordnance (`weapon_handgrenade`, `weapon_tripmine`, `weapon_satchel`, `weapon_snark`). These items are consumed upon use and function as tactical expendables.
    * Consumable ordnance operates in a distinct procedural category separate from persistent fixed weapons, with its own dedicated spawn weights, tier distribution, and independent wave probability curves.
  * **Loadout-Aware Fixed Weapon Spawning**:
    * Procedural spawning of **Persistent Fixed Weapons** must dynamically inspect the player's active inventory (`HasNamedPlayerItem`).
    * **Unacquired Spawning Only**: Only fixed weapons that the player does NOT currently possess in their loadout are permitted to spawn. This eliminates redundant fixed weapon drops, keeps arenas uncluttered, and reserves spawner capacity exclusively for weapons the player has yet to acquire.
    * If the player already owns all permitted fixed weapons for the active map, fixed weapon spawn opportunities automatically fall back to consumable ordnance or ammunition drops.
  * Procedural pickup generation must strictly prioritize consumable survival resources over weapons:
    * **Ammunition**: Must represent the majority of procedural supply spawns, ensuring players are rewarded for exploration and forced to manage reserves across escalating combat encounters.
    * **Health & Armor**: Balanced secondary consumable drops to recover from attrition.
    * **Consumable Ordnance**: Secondary tactical drops distributed according to ordnance tier weights.
    * **Weapons**: Assigned substantially lower procedural probability weights compared to ammunition, serving as rare, high-impact tactical upgrades rather than common drops.
    * **Decoupled Ammunition & Explosives Carrying Capacity**: All ammunition types and consumable ordnance carry caps are decoupled from classic low limits and extended towards infinity (§7.14). If the spawner generates an overabundance of a specific ammunition type (such as crossbow bolts or 7.62 rounds), the player is never prevented from collecting them, creating large reserves that strategically encourage and compel the player to rely on that weapon.
  * **End-to-End Supply Weight Audit & Ammo Coverage Guarantee**:
    * All procedural supply weights (ammunition per type, health, armor, consumable ordnance, fixed weapons, and their per-wave probability curves) must be reviewed as a whole and kept in a single, documented, data-driven weight table rather than scattered magic numbers.
    * **Category Allocations & Calibrated Weights**:
      * **Ammunition**: 59.5% target allocation (guaranteeing ammo constitutes the strict majority of drops across all waves, SPEC > 55%).
      * **Health & Armor**: 22.0% target allocation (balanced attrition recovery, SPEC ~20-25%).
      * **Consumable Ordnance**: 11.5% target allocation (tactical expendables).
      * **Fixed Weapons**: 7.0% target allocation (rare high-impact upgrades, keeping total weapons < 20%, strictly unowned, falling back to ordnance/ammo when exhausted).
    * **Ammunition ↔ Weapon Mapping & Coverage Baseline**:
      | Ammunition Entity | Ammo Pool Type | Usable by Weapons | Base Rel. Weight |
      |---|---|---|---|
      | `ammo_9mmclip` | 9mm | `weapon_glock`, `weapon_mp5` | 1.0 |
      | `ammo_9mmAR` | 9mm | `weapon_glock`, `weapon_mp5` | 1.0 |
      | `ammo_buckshot` | buckshot | `weapon_shotgun` | 1.0 |
      | `ammo_357` | 357 | `weapon_357`, `weapon_eagle` | 0.8 |
      | `ammo_ARgrenades` | ARgrenades | `weapon_mp5` | 0.6 |
      | `ammo_crossbow` | bolts | `weapon_crossbow` | 0.6 |
      | `ammo_gaussclip` | uranium | `weapon_gauss`, `weapon_egon` | 0.5 (shared) |
      | `ammo_egonclip` | uranium | `weapon_gauss`, `weapon_egon` | 0.5 (shared) |
      | `ammo_762` | 762 | `weapon_sniper` | 0.6 |
      | `ammo_556` | 556 | `weapon_m249` | 0.7 |
      | `ammo_rpgclip` | rockets | `weapon_rpg` | 0.5 |
    * **Loadout Multipliers & Scarcity Boosts**:
      * **Owned Weapon Multiplier**: 1.0x base weight for ammunition usable by player's active inventory.
      * **Unowned Weapon Down-weighting**: 0.1x multiplier for ammunition whose weapons are unacquired, preventing clutter without hard starvation.
      * **Scarcity Multiplier**: 3.0x boost applied when ammo reserve drops below 25% of maximum capacity.
      * **Pity Protection Timer**: 3.0x boost applied when an owned weapon's ammo has not spawned for 3 consecutive waves.
    * **Verification**: A deterministic unit test must simulate the spawner over many seeded 10-wave runs and assert that each in-scope pickup class appears with at least a minimum target frequency (e.g. the probability of a given ammo type appearing at least once in 10 waves is >= 90% when the player owns a compatible weapon), and that the overall category ratios (ammo majority, weapons rare) from this section still hold.
    * **Monster Weapon Dropping Prohibition**: Monsters (specifically HECU Grunts or Barney) are strictly prohibited from dropping weapons or ammunition upon death (`DropItem` calls suppressed). All weapons, ammunition, and items present in the map must originate exclusively from the procedural spawner, ensuring complete deterministic authority over supply distribution and pacing.
* **Special Waves & Random Mutators**:
  * Every few waves (at configurable intervals, e.g. every 5th wave), the mod rolls a random gameplay mutator:
    * **Blackout**: Level lights are extinguished into pitch darkness; navigation relies heavily on the HEV flashlight.
    * **Low Gravity**: Xen-like reduced gravity physics applied to both the player and physical debris.
    * **Swarm**: Standard enemy distribution is replaced by a massive horde composed exclusively of a single randomly selected monster species (e.g., all Houndeyes, all Headcrabs, all Bullsquids, or all Alien Grunts).
      * **Horde AI & Gib Stability**: In dense single-species swarms (such as Bullsquid hordes), monsters responding to combat stimuli, biological gibs, and meat scents (`bits_SOUND_MEAT`) must maintain robust schedule evaluation. Sound and scent listening routines must avoid shared-pointer list corruptions or NULL-pointer dereferences when dozens of monsters detect scents or gibs simultaneously.
    * **Mutator Siren Silencing on Wave Completion**: The ominous alert siren played when a special mutator starts (such as Blackout) must be explicitly stopped upon wave completion (`trigger_gladder_end`) or when mutators are reverted, preventing the audio loop from persisting indefinitely during subsequent waves.
* **Modernized Monster Turning Speed (`yawspeed`)**:
  * Original Half-Life monsters feature sluggish turning rates (`m_flYawSpeed`), resulting in slow rotational reaction times that feel dated and allow players to easily bypass or stand behind monsters without being tracked.
  * In this mod, the default `yawspeed` across all monster species is increased globally to deliver snappy, modern arcade responsiveness.
  * Monsters turn and re-orient toward flanking or sprinting players significantly faster, eliminating exploitable blind spots and maintaining high-intensity combat pressure during fast runs.
* **Arcade Monster Combat Reactivity & Damage Interruption During Movement**:
  * **Elimination of Path-Marching Vulnerability**: In standard GoldSrc AI, monsters traversing movement routes, path corners, or pursuit goals (e.g. `SCHED_CHASE_ENEMY`, `slGruntEstablishLineOfFire`, `slSquidChaseEnemy`) omit damage conditions (`bits_COND_LIGHT_DAMAGE`, `bits_COND_HEAVY_DAMAGE`) from their schedule interrupt masks. Consequently, if a monster is attacked, whacked with the crowbar, or shot while walking between waypoints from Point A to Point B, it ignores the player's strikes, suppression, or flinches, continuing its march blindly until arriving at its destination before finally reacting. This creates a severe combat exploit where enemies are left completely defenseless during movement.
  * **Immediate Schedule Damage Interruption**: In Half-Life: Gladder, monster routines are modernized to maintain aggressive combat challenge and eliminate vulnerability states. Any combat damage sustained during movement or route following immediately interrupts the active movement schedule via gamemode schedule interrupt augmentation (`FlMonsterScheduleInterruptMask` in `CGladderRules`).
  * **Immediate Reorientation & Retaliation**: Upon receiving damage while moving, monsters immediately break their transit, play appropriate pain flinches or species-specific defensive evasions (such as Bullsquid hurt-hop turnarounds), snap their yaw orientation toward the attacker's last known position (`m_vecEnemyLKP`), re-acquire the player as their active enemy, and transition directly into offensive retaliation instead of finishing their march.
* **Diminishing Wall Charger Capacity & Active State Glow**:
  * Wall-mounted medical stations (`func_healthcharger`) and HEV suit rechargers (`func_recharge`) are manually placed in map architecture.
  * At the start of each wave, the code re-energizes these stations, but their total restorative capacity ("juice") degrades incrementally per wave, creating heightened tension around health conservation in later waves.
  * **Arcade Visual Glow & Dynamic Light**: While a station retains restorative capacity ("juice", `frame == 0`), it emits the mod's signature arcade visual glow (`kRenderFxGlowShell`) and casts a localized dynamic point light. Because chargers are solid world brush entities, the glow shell must be rendered directly over and superimposed across the brush surfaces and graphics (rather than occluded or rendered behind them), enveloping the unit in a vibrant colored aura:
    * `func_healthcharger`: Vibrant Green glow (`RGB(32, 255, 32)`) and localized green dynamic light.
    * `func_recharge`: Golden-Amber / Orange glow (`RGB(255, 140, 20)`) and localized amber dynamic light.
  * **Depletion Feedback**: The instant the station's capacity is exhausted by player use (`frame == 1`), the glow shell and dynamic light immediately extinguish, providing clear visual feedback that the station is depleted until the next wave reset.
  * **Healing & Charging Sound Termination at Maximum Capacity**: When a player interacts with a wall charger (`func_healthcharger` or `func_recharge`), continuous charging must terminate immediately once the player reaches full health or armor capacity (100). If the player continues holding `+USE` while fully saturated, the charger stops the continuous looping charging sound, shuts off, and emits the denial acoustic cue (`GetDenySound()`), preventing the historic GoldSrc bug where chargers loop their charging sound indefinitely while remaining juice is present.



---

## 4. Map Spatial Indexing & Grid Generation

To spawn entities intelligently without hand-placing thousands of spawn markers, the mod indexes map geometry into a reusable spatial cache file.

### 4.1 Indexing Workflow & Storage (`maps/grid/`)
Mirroring the native GoldSrc convention for node navigation graphs (`maps/graphs/<mapname>.nod`), the spatial index files are stored in a dedicated subfolder within `maps/`:
* **Directory & File Convention**: Stored as `maps/grid/<mapname>.grid` (inside the mod's game directory).
* **Execution & Caching Flow**:
  * **Cache Check on Map Load**: Whenever a map is loaded, the engine immediately checks whether `maps/grid/<mapname>.grid` exists.
  * **Cached Fast Path**: If the file exists, the indexing process is completely skipped and the pre-computed grid data is loaded directly from disk into memory, ensuring zero startup delay.
  * **Generation on First Load**: If the file does not exist, the mod automatically executes the spatial scan/raycasting algorithm once upon map load, generates the 32-unit surface grid, and writes the cache file to `maps/grid/<mapname>.grid` (creating the `maps/grid/` directory if needed) for all future sessions.

### 4.2 Area Bounding Volumes
* Level designers define navigable gameplay zones using custom volume entities specifying 3D bounds:
  * Minimum coordinates `(x_min, y_min, z_min)`.
  * Maximum coordinates `(x_max, y_max, z_max)`.
  * Unique IDs and descriptive names (e.g., `courtyard_lower`, `ventilation_shaft`, `warehouse_floor`).

### 4.3 32-Unit Spatial Grid & Surface Raycasting
* Within each defined bounding area, the indexing system generates a regular grid sampled every 32 engine units along the horizontal plane (X, Y).
* At each grid coordinate, a downward vertical raycast traces from the ceiling or upper boundary of the defined area to detect the highest solid supporting surface:
  * **Dynamic Surface / Floor Detection**: The raycast identifies the actual walkable surface at that coordinate—whether it is the primary level floor, a raised platform, stairs, or the top of an obstacle such as a crate or container.
  * **Surface Top Placement**: By recording the exact impact height (Z), any entity spawned at that grid coordinate is placed resting cleanly on top of the detected surface (e.g., directly on top of the crate) rather than inside the obstacle or below it.
  * **Clearance Verification & Structural Obstacle Avoidance**:
    * An upward check or clearance hull trace verifies that the space between the detected surface and any overhead ceiling/obstruction provides adequate vertical clearance for entities.
    * **Static Architecture & Solid Brush Entity Collision Check**:
      * To prevent monsters or items from spawning embedded inside structural elements or stuck against solid surfaces, both spatial grid indexing and runtime procedural spawning must perform collision clearance sweeps not only against world brush geometry (`contents == CONTENTS_SOLID`), but comprehensively against all solid brush entities (`SOLID_BSP` and `SOLID_BBOX`).
      * **Covered Brush Entities**:
        * `func_wall` and `func_wall_toggle`: Structural columns, decorative walls, barriers, and dividers.
        * `func_monsterclip`: Invisible clip brushes specifically placed to prevent monster movement.
        * `func_door`, `func_door_rotating`, `momentary_door`: Dynamic or static door assemblies.
        * `func_plat`, `func_platrot`, `func_train`, `func_tracktrain`: Moving platforms and machinery.
        * `func_conveyor`, `func_vehicle`, `func_friction`: Conveyor belts and interactive vehicles.
        * `func_breakable`, `gladder_breakable`: Destructible world props.
      * **Clearance Margins**: Both spatial indexing and runtime spawn clearance sweeps (`UTIL_TraceHull` and lateral margin checks) must verify that candidate spawn positions have full lateral and vertical clearance against these solid brush entities. Cells overlapping or immediately flush against solid brush entities without standard monster hull clearance are strictly excluded to avoid monsters spawning stuck inside walls or triggering pathfinding navigation errors.
* Cells identified with valid supporting surfaces, zero architectural collision overlap, and adequate vertical clearance are recorded into the grid database, ready to receive randomized entity spawns during waves.

### 4.4 Special Entity Spawning Behaviors
To accommodate distinct enemy mechanics and level geometry, specific monster types follow modified spawning rules when selected from the procedural spawn pool:
* **Barnacle Ceiling Attachment (`monster_barnacle`)**:
  * Barnacles feature a unique ceiling-bound spawning mechanism. When selected to spawn at a designated grid coordinate, the spawner executes an upward vertical trace/raycast from the grid point to locate the exact ceiling geometry directly above it (or uses cached ceiling heights stored in the spatial grid indexer).
  * The barnacle entity is attached flush to the detected ceiling surface rather than placed on the floor, preserving authentic barnacle placement and ambush behavior across procedural waves.
* **Flying Monsters Variable Altitude Spawning**:
  * Flying enemies (such as `monster_flyer`, `monster_alien_controller`, or other airborne species) do not spawn grounded on floor surfaces.
  * When spawned at a grid coordinate, their vertical position (Z axis) is randomized dynamically along a variable altitude range between the floor elevation and the ceiling clearance height of that grid cell, creating diverse vertical engagement angles and unpredictable combat encounters in every wave.
* **Floor-Grounded Monsters Drop-In Elevation (+8 Units) & Pre-Spawn Walkability Validation**:
  * In GoldSrc, spawning monsters with their bounding box bottom flush against floor geometry often causes `stuck in wall/floor` collision failures on their initial physics tick due to micro-uneven brush seams, floor bevels, or ramp slopes.
  * To ensure clean collision initialization, ground-based monsters spawn with a vertical clearance offset of approximately +8 units above the detected surface. Native engine step physics and gravity (`MOVETYPE_STEP`) automatically settle the monster cleanly onto the ground on its first physics frame, preventing false stuck-in-world detections.
  * **Pre-Spawn Clearance & Stuck Prevention Guarantee**:
    * Before final entity creation, the spawner MUST comprehensively validate that the candidate location will not result in a stuck monster (`Monster stuck in wall--level design error`).
    * In addition to tracing the full bounding hull (`UTIL_TraceHull`) at spawn elevation and verifying lateral margins against solid brush entities, the spawner verifies unobstructed hull volume across all cardinal directions. If an instantiated monster fails initial movement check or drop-to-floor validation, the spawner aborts or relocates the spawn to another grid cell rather than leaving a stuck monster in the world with console errors.

---

## 5. Map-Specific Configuration & Filtering Rules

Each map supports its own configuration profile to dictate thematic and gameplay constraints:
* **Monster Blacklists & Whitelists**: Level designers can prohibit specific monster types that do not fit the map's geometry or theme (e.g., disallowing Headcrabs in areas without floor traversal, or restricting Gargantuas to large outdoor spaces).
* **Arsenal Restrictions**: Maps can tailor which weapons or ammunition types are eligible to spawn.
* **Pacing & Spawn Density Modifiers**: Maps can define unique spawn density ceilings and wave progression curves based on physical level size.

---

## 6. Level Design Integration & Custom Triggers

The mod provides mapper-friendly entities to simplify setting up any custom or existing map:
* **Wave Start Staging Zone (Point A)**: Defines the player's initial spawn point and return destination. Level designers configure Point A as a secure staging area free of monster spawn grids, with manual initiation mechanisms (e.g., airlock doors, start line triggers) that fire wave commencement on demand.
  * **Start Trigger Visual Presentation (`trigger_gladder_start`)**: The start boundary trigger features the mod's signature arcade bounding glow shell (`kRenderFxGlowShell`) rendered over its brush bounds via the client Triangle API in vibrant Red (`RGB(255, 32, 32)`). The underlying trigger brush faces and tool textures (such as `aaatrigger`) must NEVER be visible under any circumstance (brush faces remain completely invisible / `EF_NODRAW` or zero-alpha transparent), rendering exclusively the outer glowing wire/shell aura in 3D world space to provide an unmistakable, pristine visual demarcation of the wave start line without showing editor trigger textures.
* **Wave Completion Trigger (Point B)**: Brush or point trigger placed at the end of the run that detects player arrival, triggers wave completion feedback, immediately snaps the screen to solid black (instant 100% black fade), teleports the player back to Point A while completely obscured, and fades the screen progressively back to 0% transparency to reveal the staging area.
  * **Finish Trigger Visual Presentation (`trigger_gladder_finish`)**: Like the start trigger, the completion trigger volume renders the identical arcade bounding glow shell via Triangle API in vibrant Red (`RGB(255, 32, 32)`). The underlying trigger brush geometry and `aaatrigger` texture must remain completely invisible and suppressed, ensuring that only the red glowing boundary shell outlines the extraction / finish threshold across corridors and doorways without displaying trigger brush texture surfaces.
* **Wave State Relays**: Input/output events that fire on wave start, wave victory, and game over, enabling mappers to trigger map-specific environmental events (doors opening, lights flickering, sirens, hazards).
* **End-Game Backdrop Cameras (`gladder_camera`)**: Support for placing multiple `gladder_camera` point entities (subclass of `trigger_camera`) across the map without requiring a specific `targetname`. During the post-game summary screen, the mod automatically cycles through backdrop cameras. The sequence ALWAYS begins with the dynamic third-person camera overlooking the player first. If map-placed `gladder_camera` entities exist in the level, the view then smoothly transitions sequentially through each of them in order, splitting view time equally among them, before looping back to the third-person camera and repeating the sequence indefinitely.
* **Area Definition Volumes**: Bounding box entities with designer parameters for indexing scope and zoning.
* **Wall-Mounted Stations**: Mappers place standard `func_healthcharger` and `func_recharge` brush entities across the map or at staging areas; the mod engine automatically manages their recharge cycle and wave-based diminishing capacity.

---

## 7. Extended Arsenal & Visual Arcade Presentation

### 7.1 Extended Weapon Roster
* **Authentic Half-Life Universe**: Retains original GoldSrc art assets, audio, HUD styling, and movement mechanics.
* **Expanded Weapon Selection**: Integrates weapons from official expansions (such as *Half-Life: Opposing Force*), including:
  * Pipe Wrench and Combat Knife.
  * Desert Eagle (.357 caliber sidearm with laser sight).
  * M40A1 Sniper Rifle.
  * M249 SAW (Squad Automatic Weapon).
  * Spore Launcher.
  * Shock Rifle.
  * Displacer Cannon.
  * Barnacle Grapple (where map geometry allows).

### 7.2 Arcade Pickup Presentation (Quake 3 Style Floating, Bobbing & Colored Glow)
To reinforce the fast-paced arcade feel, dropped and procedurally spawned items abandon static floor placement in favor of high-energy arena shooter visuals (reminiscent of *Quake III Arena*):
* **Floating & Bobbing Motion**: Pickups (weapons, ammunition crates, health kits, and HEV batteries) levitate slightly above the ground/supporting surface, gently bobbing up and down while continuously rotating on their vertical axis.
* **Dynamic Colored Point Lights**: Each item projects a localized colored dynamic light onto nearby surfaces, enhancing visibility in dark corridors.
* **Color-Coded Render Glows**: Items feature a radiant colored aura/glow (`kRenderFxGlowShell` or engine render effects) categorized by pickup type:
  * **Health & Medical**: Vibrant Green glow and lighting (`RGB(32, 255, 32)`).
  * **Armor & HEV Batteries**: Cyan / Bright Blue glow and lighting (`RGB(32, 192, 255)`).
  * **Ammunition**: Amber / Golden-Orange glow and lighting (`RGB(255, 180, 20)`).
  * **Fixed Weapons**: Arcade Magenta / Violet glow and lighting (`RGB(220, 40, 220)`).
  * **Throwable & Consumable Ordnance**: Radiant Blue glow and dynamic lighting (`RGB(32, 128, 255)`), distinctly separated from red to eliminate visual ambiguity with Elite Champion monsters and start/finish boundary shells.
* **Exclusion of Active Combat Ordnance & Thrown Projectiles**:
  * The arcade visual modifier (levitation, sine bobbing, yaw rotation, glow shell, and dynamic point light) applies strictly to idle, uncollected pickup items sitting in the map awaiting collection.
  * Active combat projectiles, deployed explosives, and thrown ordnance—specifically hand grenades (`models/w_grenade.mdl` / `monster_handgrenade`), active satchel charges (`models/w_satchel.mdl` / `monster_satchel`), released live snarks (`models/w_squeak.mdl` / `monster_snark`), and planted tripmines (`monster_tripmine`)—must **never** receive the pickup visual modifier. Because they are being actively utilized in combat rather than acting as collectible items, they must retain their normal physics, flight trajectory, and original visual presentation without floating, bobbing, rotating, or glowing like pickups.

### 7.3 Iconic Lambda Collectible Item
* **Unique Per-Wave Spawn**: In each wave, exactly **one single collectible item** spawns procedurally at a randomly selected valid grid location in the map.
* **Custom Model & Presentation**: Features a custom 3D model of the classic Half-Life Lambda (`λ`) insignia. It follows the same arcade visual language: floating suspended in mid-air, rotating on its vertical axis, gently bobbing up and down, and emitting a vibrant golden-orange glow and light.
* **Score & Stat Progression**:
  * Collecting the Lambda grants a substantial score point bonus.
  * Increments the player's total match collectible counter.
* **Acoustic Feedback**: Immediately triggers a dedicated, distinct pickup sound cue upon collection.
* **Center-Screen Pickup Notification**: Upon collection, a brief text message (e.g. `LAMBDA COLLECTED`, optionally including the score bonus and updated collectible count) appears in the center of the screen and fades out after roughly 1.5-2 seconds. It is delivered through a dedicated user message and rendered by a client HUD element, must not overlap or block the wave start/complete overlay (§12), and must not stack indefinitely if multiple collectibles are picked up in quick succession (a new message replaces the current one).
* **Wave Lifecycle**: If missed or left behind when Point B is reached, the collectible is wiped during wave garbage collection, and a single new Lambda collectible spawns at a newly randomized grid coordinate for the next wave.

### 7.4 Universal Weapon Reticle & Crosshair Delivery
* In standard Half-Life, melee weapons (`weapon_crowbar`, `weapon_pipewrench`, `weapon_knife`) and throwable ordnance (`weapon_handgrenade`, `weapon_satchel`, `weapon_snark`) lack an on-screen crosshair, leaving the player with an empty reticle.
* In Half-Life: Gladder, **all weapons must have a visible crosshair** on the client HUD.
* The default fallback crosshair for weapons that traditionally lack a reticle (melee weapons, grenades, satchels, snarks) must use the standard centered crosshair used by the 9mm handgun / Glock (`crosshair.spr`), rather than an empty/null crosshair. This guarantees consistent, immediate visual targeting alignment during high-speed gauntlet runs.

### 7.5 Auto-Switch on Weapon Pickup Prevention (`gladder_autoswitch_on_pickup`)
* In vanilla Half-Life, picking up a newly acquired weapon automatically forces the player to holster their current weapon and immediately deploy the new one. In high-speed gauntlet runner combat, this behavior is severely disruptive when sprinting and firing through dense hostile encounters.
* In Half-Life: Gladder, **automatic weapon switching upon pickup is disabled by default** whenever the player already has an active weapon drawn.
* The mechanic is governed by a console variable (`gladder_autoswitch_on_pickup`, default `0`):
  * `gladder_autoswitch_on_pickup 0` (Default): Running over a new weapon silently adds it to the player's inventory/ammo pool without interrupting combat or switching the active weapon. (If the player has no weapon drawn, e.g. empty hands, it automatically equips).
  * `gladder_autoswitch_on_pickup 1`: Restores classic Half-Life autoswitch behavior on pickup.

### 7.6 Melee Combat Cadence Normalization & Corpse Impact Decals
* **Elimination of Rapid Corpse Hit Exploit**: In vanilla Half-Life, striking dead monster corpses or player bodies with melee weapons (`weapon_crowbar`, `weapon_knife`, `weapon_pipewrench`) bypasses the attack cooldown timer due to a premature return in the strike routine, enabling players to swing at supersonic frame rates and instantly shred corpses.
* **Consistent 0.25s Cadence**: In Half-Life: Gladder, melee weapon strikes against dead entities maintain the identical standard attack delay (0.25 seconds) as living targets, preventing audio spam and unintended exploit speeds.
* **Corpse Blood Decals**: Impact smack routines are properly scheduled so that melee strikes against flesh corpses consistently spawn blood impact decals (`DecalGunshot` with `BULLET_PLAYER_CROWBAR`).

### 7.7 Looping Weapon Audio Termination on Player Death
* In GoldSrc, weapons with continuous looping sound channels (most notably the Gauss gun `weapon_gauss` charging loop `weapons/electro4.wav` or Egon beam loops) fail to terminate their sound channel when the player dies, causing the audio loop to play indefinitely in 3D space at the corpse position.
* In Half-Life: Gladder, player death immediately silences all looping weapon channels on `CHAN_WEAPON`, guaranteeing a clean acoustic transition to third-person death cameras and the post-match summary screen.

### 7.8 Shotgun Interrupted Reload & Pump State Synchronization
* In vanilla Half-Life, interrupting the multi-shell reload cycle (`weapon_shotgun`) by firing or quick-switching to another weapon leaves residual pump timers (`m_flPumpTime`) active, causing delayed pump sound triggers and animation desyncs between client prediction and the server.
* In Half-Life: Gladder, interrupting a shotgun reload immediately and cleanly resets the reload state machine (`m_fInSpecialReload = 0`) and clears pending pump timers (`m_flPumpTime = 0`), guaranteeing seamless weapon swaps and instant responsiveness without phantom reload sounds.

### 7.9 Submerged Weapon Firing Debounce Normalization (MP5 Grenades)
* In standard GoldSrc, attempting to fire the MP5 grenade launcher (`weapon_mp5` secondary attack) underwater incorrectly updates the primary attack timer instead of secondary attack, using a hardcoded static float (`0.15`) rather than a relative timebase offset (`UTIL_WeaponTimeBase() + 0.15`). Holding down secondary fire while submerged causes the empty sound cue to spam uncontrollably on every frame.
* In Half-Life: Gladder, secondary attack attempts underwater correctly debounce both attack channels (`m_flNextSecondaryAttack = m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.15`), preventing audio click storms and predicting properly on the client.

### 7.10 Guaranteed Weapon Pickup HUD Sprite Delivery
* In classic Half-Life GoldSrc, acquiring a new weapon flashes the corresponding weapon icon/sprite on the bottom-right corner of the HUD (positioned immediately above the active ammunition display) via the `WeapPickup` user message history slot (`HISTSLOT_WEAP`).
* However, historical GoldSrc weapon classes omitted `WeapPickup` dispatches from the base weapon class (`CBasePlayerWeapon::AddToPlayer`) and various standard weapons (such as `weapon_glock`, `weapon_crowbar`, and deployable/throwable ordnance), causing the HUD pickup notification to appear intermittently or fail entirely for newly acquired weapons and modular arsenal items.
* In Half-Life: Gladder, **all weapons**—including standard firearms, melee weapons, consumable ordnance (`weapon_handgrenade`, `weapon_tripmine`, `weapon_satchel`, `weapon_snark`), and extended arsenal weapons (`weapon_knife`, `weapon_pipewrench`, `weapon_eagle`, `weapon_sniper`, `weapon_m249`)—must reliably dispatch `WeapPickup` upon acquisition, guaranteeing instant visual HUD confirmation above the ammunition display whenever any weapon is collected.

### 7.11 Pipe Wrench Charged Attack Lethal Resolution & Gib Stability
* The Opposing Force Pipe Wrench (`weapon_pipewrench`) features a heavy charged secondary attack (`BigSwing`), dealing up to 85+ damage and capable of instantly gibbing medium-tier monsters (such as Bullsquids or Zombies).
* Striking and fatally gibbing entities with the charged attack must safely validate target entity pointers both before and after applying multi-damage (`ApplyMultiDamage`), preventing stale entity dereferences when the victim is gibbed into fragments or removed from memory.
* Additionally, client prediction for the heavy swing must remain fully aligned with the server state machine, ensuring hit animations and impact acoustic cues predict cleanly without desynchronization.

### 7.12 Developer Cheat: `impulse 101` Full Loadout
* With `sv_cheats 1`, `impulse 101` must grant the **complete available equipment set**, not only the vanilla Half-Life arsenal.
* **Vanilla set (unchanged)**: HEV suit, battery, crowbar, 9mm handgun, shotgun, MP5 and M203 grenades, hand grenades, tripmines, .357, crossbow, Egon, Gauss, RPG, satchel charges, snarks, hivehand, plus their ammunition.
* **Gladder extended set (additive)**: every extended weapon implemented in the mod (`weapon_pipewrench`, `weapon_knife`, `weapon_eagle`, `weapon_sniper`, `weapon_m249`, and any future Opposing Force weapon such as the Spore Launcher, Shock Rifle or Displacer Cannon once implemented), plus the matching ammunition (`ammo_762`, `ammo_556`, and any other ammo entity those weapons use).
* **Maintainability**: new weapons and ammo must be added to the cheat by registering them with the cheat loadout extension point, not by editing the vanilla list. Forgetting a new weapon must be caught by a unit test that compares the registered weapon roster (§3, §7.1) with the `impulse 101` grant list.
* The cheat must respect the existing `MAX_WEAPONS` limit (weapon IDs up to 31) and must not alter vanilla behavior in non-Gladder game rules.

### 7.13 Extended Arsenal HUD Slot Alignment & Sprite Script Integrity
* **Conflict-Free HUD Slot & Position Layout**:
  * In the GoldSrc HUD weapon selection grid (`rgSlots[slot][pos]`), each `(iSlot, iPosition)` coordinate pair within a weapon bucket must be strictly unique.
  * Colliding slot and position coordinates cause weapons to overwrite each other in memory during inventory synchronization (`CHudAmmo::Think`), rendering shadowed weapons completely invisible and unselectable in the HUD.
  * **Standard & Extended Layout (Keys 1-5 / Slots 0-4)**:
    * **Slot 0 (Melee / Key 1)**: Crowbar (pos 0), Pipe Wrench (pos 1), Combat Knife (pos 2).
    * **Slot 1 (Sidearms / Key 2)**: Glock 9mm (pos 0), .357 Magnum (pos 1), Desert Eagle (pos 2).
    * **Slot 2 (Rifles & Heavy Ballistics / Key 3)**: MP5 (pos 0), Shotgun (pos 1), Crossbow (pos 2), M249 SAW (pos 3), M40A1 Sniper Rifle (pos 4).
    * **Slot 3 (Energy & Heavy Ordnance / Key 4)**: RPG (pos 0), Gauss Gun (pos 1), Egon Gun (pos 2), Hornetgun (pos 3). *(Note: M249 must not occupy Slot 3 pos 2 to prevent collision with Egon).*
    * **Slot 4 (Consumable Ordnance / Key 5)**: Hand Grenades (pos 0), Satchel Charges (pos 1), Tripmines (pos 2), Snarks (pos 3).
* **Sprite Script Completeness & Classname Aliasing**:
  * Every weapon registered in the mod must have a matching HUD sprite descriptor script in `sprites/` corresponding to its entity classname (`sprites/<classname>.txt`).
  * For the sniper rifle, both `weapon_sniper` and `weapon_sniperrifle` entity names and descriptor scripts (`sprites/weapon_sniper.txt` and `sprites/weapon_sniperrifle.txt`) must be provided and synchronized, containing complete definitions for active icons (`weapon`), inactive icons (`weapon_s`), ammunition icons (`ammo`), reticles (`crosshair`), and zoom overlays (`zoom`).
  * Missing or mismatched sprite scripts prevent client sprite allocation, which causes uninitialized crosshair handles, missing ammunition counters, and broken rendering (stretched or white previous textures).

### 7.14 Extended Ammunition & Explosives Capacity (Decoupled Limits Tending to Infinity)
* **Elimination of Static Carry & Byte Clamping Limits**:
  * Classic Half-Life enforces low static carry caps (`_9MM_MAX_CARRY = 250`, `BOLT_MAX_CARRY = 50`, `ROCKET_MAX_CARRY = 5`, `HANDGRENADE_MAX_CARRY = 10`, etc.) and hard-clamps ammunition network synchronization messages (`AmmoX`, `AmmoPickup`) to a single unsigned byte (0–254).
  * In Half-Life: Gladder, ammunition and explosive ordnance carry limits are decoupled and extended to high capacity limits (tending to infinity, e.g. thousands or practically uncapped for gauntlet gameplay).
* **Gameplay Incentive & Tactical Pacing**:
  * In procedural gauntlet runs, random supply rolls can yield disproportionately high amounts of specific ammunition (such as Crossbow bolts, 7.62 rounds, or explosive grenades).
  * Rather than denying pickups when arbitrary low caps are reached, the player is permitted to continuously vacuum and hoard that ammunition type. Accumulating huge reserves naturally encourages and strategically forces the player to switch to and expend that weapon to survive subsequent high-density waves.
* **Networking & Protocol Architecture (Upstream Dependency)**:
  * Ammunition networking (`AmmoX`, `AmmoPickup`, and `WeaponList`) must support 16-bit counts (`uint16` / `short`) to synchronize counts well above 254 without byte truncation, modulo-256 wrapping, or debug assertions (tracked upstream in `urgorri/halflife-refactored#201`).
  * Game rules override capacity queries dynamically via virtual hooks without modifying core vanilla weapon implementations.

---

## 8. Arcade Audio Cues & Soundscapes

Distinct audio cues provide immediate acoustic feedback for critical match state transitions (specific sound asset files to be designated during implementation):
* **Wave Start Cue**: A punchy, energizing audio sting triggered when the wave commences from Point A.
* **Wave Completion Cue**: A rewarding, triumphant sound effect played the instant Point B is touched and the wave loop finishes.
* **Special Wave / Mutator Alert Cue**: An ominous, siren-like warning sound indicating that a special wave modifier (Blackout, Low Gravity, Swarm) is active. The cue is explicitly terminated and silenced upon wave completion at Point B, preventing continuous looping into subsequent waves.
* **Collectible Pickup Cue**: A bright, distinctive chime played when collecting the wave's hidden Lambda insignia item.
* **Match End Cue**: A dramatic, definitive sound effect played upon run termination (clock expiration or player death) before transitioning to the summary screen.

---

## 9. Real-Time HUD & Player Statistics Tracking

The mod continuously aggregates gameplay statistics across waves and displays them permanently on the player's heads-up display (HUD).

### 9.1 Persistent HUD Elements
A dedicated on-screen text overlay is rendered at all times (matching classic Half-Life HUD green/amber typography and aesthetic):
* **Match Clock / Time Remaining**: Displays the active session timer (either a countdown against the time limit, e.g., `TIME LEFT: 05:24`, or total elapsed run time).
* **Active Kill Streak Multiplier**: Live indicator showing the active combo multiplier (`COMBO: x3`) while chaining rapid monster kills. *(Note: Cumulative score points are calculated and revealed exclusively at match conclusion).*
* **Current Wave Counter**: Displays the active wave number (e.g., `WAVE: 14`).
* **Collectibles Counter**: Displays the total count of Lambda items collected during the match (`COLLECTIBLES: 08`).
* **Current Wave Timer**: Stopwatch tracking elapsed time spent in the active wave (`WAVE TIME: 00:38`).
* **Total Frags / Kills**: Real-time counter of total living hostile monsters eliminated directly or indirectly by the player during the match (`FRAGS: 187`). Excludes environmental deaths, monster infighting, pre-placed dead corpse props (`monster_*_dead`), and hostile projectile entities (`hornet`).
* **Average Wave Duration**: Dynamically recalculated average completion time across all finished waves (`AVG WAVE: 00:46`).
* **Weapon Pickup Notification**: Temporary weapon icon sprite flashed in the bottom-right HUD quadrant (positioned immediately above the primary/secondary ammunition display) upon acquiring any new weapon or ordnance item, confirming immediate loadout delivery.

### 9.2 End-of-Run Summary, Score Calculation & Grading
Upon match conclusion—whether through **time limit expiration** or **player death**—the mod overrides the standard engine reload/menu routine and immediately activates the summary screen over the static camera backdrop:

* **Undismissable Post-Match Summary Screen**:
  * Upon match conclusion (whether via time expiration or player death), the match has definitively ended. The summary screen acts as a persistent end-of-run modal display and **cannot be dismissed** (i.e. `[USE / E]` or click dismiss inputs are disabled and removed).
  * Players can freely toggle between the statistical summary view and the local leaderboard/high-scores view using `[TAB]`.
  * The interface remains on screen until the player exits or restarts the level through standard engine commands (`restart`, `disconnect`, `map`). *(Note: During active gameplay, holding `[TAB]` allows peeking at the leaderboard and releasing `[TAB]` dismisses it).*
* **Multi-Camera Backdrop Rotation (`gladder_camera`)**:
  * The player's viewport switches immediately to end-game backdrop cameras. The rotation sequence ALWAYS begins with the dynamic third-person view overlooking the player. It then sequentially and equitably cycles through all `gladder_camera` entities placed in the map, holding on each camera for an equal duration to showcase multiple dynamic vantage points of the arena, before looping back to the third-person view and repeating cyclically.
  * The living environment remains visible and active in the background while the statistical results, animated score tally, and final rank grade are rendered clearly overlaid on screen.
* **Darkened Viewport Overlay**:
  * While rendering the post-match summary screen or in-game leaderboard/scoreboard, the underlying 3D viewport is dimmed / darkened with a translucent dark overlay (rather than lightened/additive tinting), giving the distinct aesthetic feel of a focused modal HUD overlay while keeping the living 3D world visible behind.
* **Match World Freezing & Model Pose Preservation (Frozen in Time Effect)**:
  * Upon match conclusion (whether through session time limit expiration or player death), the game world freezes all active combat elements, monsters, projectiles, and entities in place to convey a cinematic "time stopped" effect.
  * **Timing**: To allow dynamic events (such as the player's death animation, fatal physics recoil, or final projectile impacts) to resolve naturally, the world freeze must wait approximately 1.0 second before locking the scene.
  * **Pose Integrity Preservation**:
    * The freeze must NEVER reset or change the 3D model poses of the player or monsters back to a default idle stance (`ACT_IDLE`).
    * If the player died, the player model must settle into and remain in their authentic death pose (`PLAYER_DIE`), locking the exact animation sequence and frame as settled on the floor.
    * Active monsters, in-flight projectiles, and props must freeze preserving their exact current animation sequence, frame, and physics velocities at the moment of freeze, perfectly preserving the scene as it stood.
* **Match Outcome Status**:
  * Displays run conclusion state (e.g., `STATUS: SURVIVED - TIME EXPIRED` vs. `STATUS: KIA - FALLEN IN COMBAT`).
  * **Death Is Not a Failure**: Dying ends the run, but the player keeps all earned stats, frags, collectibles, and points. The full score is calculated and recorded in local leaderboards regardless of how the match ended.
* **Granular Score Calculation Breakdown**:
  The total score is computed and tallied on screen using all accumulated match data:
  * **Base Combat Points**: Sum of all eliminated monsters multiplied by their species tier value.
  * **Combo Streak Breakdown**: Detailed count of combo multipliers achieved during the run (e.g., `x2 Combos: 15`, `x3 Combos: 8`, `x4+ Combos: 4`), plus a dedicated bonus for the **Maximum Combo Streak** attained.
  * **Wave Completion Milestone Bonus**: Payout scaled by the total number of wave laps completed.
  * **Lambda Collectibles Bonus**: High-value score payout awarded for each Lambda insignia retrieved.
  * **Final Composite Score**: Displayed prominently at the top of the summary.
* **Overall Time & Lap Pacing**:
  * Total match time played.
  * Fastest single wave lap time vs. slowest wave lap time.
  * Average lap duration across the entire run.
* **Granular Per-Enemy-Type Kill Log**:
  * An itemized kill log breaking down exact frags for each distinct enemy species encountered (e.g., `Headcrabs: 42`, `Zombies: 28`, `Houndeyes: 16`, `Bullsquids: 9`, `Vortigaunts: 14`, `Alien Grunts: 6`, `HECU Grunts: 19`, etc.).
* **Arcade Performance Rating (S / A / B / C / D / F)**:
  * An arcade grade calculated via a composite performance formula evaluating:
    * Total waves cleared within the time limit (primary weight).
    * Final composite score and combat diversity.
    * Peak combo streaks and combo frequency.
    * Collectibles gathered (rewarding thorough exploration under time pressure).
    * Average wave completion speed (pace / aggressiveness).
    * Survival efficiency (penalties for excessive damage taken or deaths).
  * Grade scale:
    * **S Rank**: Exceptional mastery—maximum wave clearance speed, high frag count, near-flawless route execution.
    * **A Rank**: Excellent performance—high wave count and strong combat throughput.
    * **B Rank**: Solid run—consistent pacing with moderate wave completion.
    * **C Rank**: Average performance—slower wave pacing or cautious play.
    * **D / F Rank**: Subpar performance—early death, low wave count, or failing to maintain gauntlet momentum.

---

## 10. Game Mode Scope, High Scores & Arcade Integrity

### 10.1 Single-Player Focus
* **Dedicated Single-Player Gameplay**: The gauntlet rules, telemetry, and camera transitions are designed strictly for single-player play.
* **Architecture Preservation**: Core multiplayer networking infrastructure (client-side prediction, weapon dispatch, shared protocols) is retained in the codebase for engine stability and potential future expansions, but the active mod experience is single-player.

### 10.2 Local Map Leaderboards (High Scores)
* **Persistent Records**: Each map retains a local high-score record file on disk (`gladder/scores/<mapname>.dat` or JSON).
* **Tracked Metrics**: Records the Top 10 runs per map, logging Date, Final Score, Waves Cleared, Total Frags, Total Time, and Earned Grade.
* **Column-Aligned Tabular Grid**: The local leaderboard overlay is rendered as an orderly, column-aligned tabular grid centered in the viewport. Rather than drawing each run record as a single space-padded text line (which misaligns due to proportional character widths in engine fonts), each column (Rank, Score, Waves, Frags, Time, Grade, Status) renders its entries along dedicated vertical coordinate axes with fixed column offsets, ensuring clean, legible column alignment across all screen resolutions.
* **Future-Proof Baseline**: Designed as the local data layer that can later interface with external web-based leaderboards.

### 10.3 Save / Load Restrictions & Death Flow
To maintain authentic arcade tension, competitive scoring integrity, and fluid game pacing, saving and loading functionality is eliminated entirely:
* **No Save Functionality**: All save mechanisms (quick-save, manual console/menu save, autosave triggers) are disabled at the root level.
* **No Load Functionality**: Loading existing save files during a session (quick-load or menu load) is disabled.
* **Session Finality & Seamless Death Transition**: Each session represents a single, self-contained run. Player death bypasses original save reload and main menu prompts, seamlessly launching the end-of-run summary screen with full score calculation and high score recording.
* **Corpse Orientation Lock**: Upon player death, the orientation angles (yaw, pitch, roll) of the 3D player corpse model are permanently locked at the exact moment and position of death. Moving the mouse or looking around in third-person view / death cameras must not pivot or rotate the dead player model on the floor.

### 10.4 In-Game Match Restart (`gladder_restart` & `GameMenu` Integration)
* **On-Demand Match Restart**: Players can restart their active gauntlet run on demand via the in-game escape menu (`resource/GameMenu.res`) or the console.
* **Full Map Reload vs. Engine Restart Bug**:
  * In standard GoldSrc, executing the engine `restart` command after match conclusion, player death, or state invalidation suffers from an engine entity restoration bug: destroyed or modified brush entities (`func_breakable`, `func_wall`, interactive brushes) fail to reload, leaving map brush geometry missing and world structures broken.
  * To ensure a complete, reliable level reset, the mod introduces the dedicated `gladder_restart` command (available as both a client console command and server command).
  * `gladder_restart` automatically resolves the current active map name (via `pfnGetLevelName` on client or `gpGlobals->mapname` on server) and executes a full level load (`map <mapname>`).
* **GameMenu Entry**: The "Restart Match" option in `resource/GameMenu.res` invokes `gladder_restart` (instead of legacy `engine restart`), triggering an immediate, clean full map reload from Wave 1.
* **State Reset**: Reloading the match cleanly purges all accumulated session metrics, resets combo streaks, restores procedural spawner state, recreates all world brush entities, and returns the player to Point A with the default staging loadout (`item_suit` and `weapon_crowbar`).

### 10.5 New Game Gauntlet Map Selection (Map Selector vs. Legacy Difficulty)
* **Map Selection Architecture**: Configured via declarative GameUI resource scripts (`resource/GameMenu.res` and `Resource/NewGameDialog.res`). Replaces the legacy Half-Life difficulty selection dialog (`#GameUI_Difficulty` ComboBox in `NewGameDialog`) with a Gauntlet Map Selector.
* **Obsolete Manual Difficulty**: Because Half-Life: Gladder enforces automatic wave-progressive engine skill scaling (`skill 1` -> `skill 2` -> `skill 3` as defined in Section 3), manual difficulty selection is obsolete.
* **Gauntlet Arena Selection**: When selecting "New Game" from the main menu, the player is presented with a map list interface allowing selection among available gauntlet maps (e.g. `gladder_crossfire`, `gladder_bootcamp`, or custom community maps), initiating the chosen level directly.

---

## 11. Engine Resource Management & Wave Garbage Collection

Because GoldSrc enforces a strict maximum entity limit (`MAX_EDICTS`, typically 512 to 900+ entities), strict resource purging occurs at each wave reset:

### 11.1 Transient Entity Clearing & Wave Purge
* **Active Monster Cleanup**: Any monsters surviving from the previous wave are eradicated immediately upon wave completion.
* **Dropped Item Purge**: Uncollected weapons, ammunition boxes, and medical kits scattered across the map are removed to prevent entity buildup.
* **Transient Entity Clearing & Orphaned Effects**:
  To prevent floating visual artifacts, broken physics links, or edict leaks across wave resets, all transient combat entities and attached visual effects are comprehensively purged:
  * **Monster & Player Projectiles**: Rockets (`rpg_rocket`, `hvr_rocket`), grenades (`grenade`), Alien Grunt hornets (`hornet`), live snarks (`monster_snark`), crossbow bolts (`bolt`), Bullsquid acid spitballs (`squidspit`), Gonarch mortar spit (`bmortar`), Alien Controller attack balls (`controller_head_ball`, `controller_energy_ball`), Nihilanth spheres (`nihilanth_energy_ball`), planted satchels (`monster_satchel`), and planted tripmines (`monster_tripmine`).
  * **Beams & Continuous Effects**: Active laser and lightning beams (`beam` / `CBeam`, including Vortigaunt lightning, Gargantua flame beams, tripmine lasers, and Egon beams) and laser targeting spots (`laser_spot`).
  * **Orphaned Attached Sprites**: Dynamic effect sprites (`env_sprite`) attached to monsters via `MOVETYPE_FOLLOW` (such as Alien Controller charging head balls `sprites/xspark4.spr`, Gargantua eye glows, Turret eye glows, Nihilanth tele balls) whose parent entity was removed or freed.
  * **Debris & Corpses**: Monster gibs (`gib`), player corpses, and temporary impact decals.
* **Reliable Spawn Pool**: Guarantees that the incoming wave has a full allocation of free entity slots for procedural spawning without triggering engine exhaustion (`ED_Alloc: no free edicts`).

### 11.2 Interactive World Objects Lifecycle & Wave Regeneration (`gladder_breakable`)
To maintain consistent level interactivity and route geometry without entity degradation or corruption across wave resets:
* **Avoidance of Pushables (`func_pushable`)**: Movable pushable objects (`func_pushable`) are excluded from general mod design to prevent physics desynchronization and blocking geometry across repeated laps.
* **Breakable Object Wave Regeneration (`gladder_breakable`)**:
  * Breakable environmental objects (such as wooden crates, glass panes, or barricades) are mapped with or subclassed as `gladder_breakable`, retained across waves, and regenerated cleanly at the start of each wave reset.
  * **Non-Destructive Break Simulation**: When a `gladder_breakable` takes lethal damage during an active wave, it triggers its standard breaking visual/audio effects, debris particles, and collision removal, but the underlying server entity is preserved in memory rather than permanently deleted (`UTIL_Remove`).
  * **State Hiding & Regeneration Protocol**:
    * Upon destruction, the object enters a hidden, non-solid state (`EF_NODRAW`, solid state set to `SOLID_NOT`).
    * During wave reset (`ResetWave`), all broken `gladder_breakable` entities are restored to their original visual appearance, collision bounds (`SOLID_BSP`), and full hit points.
    * **Engine World Relinking (`UTIL_SetOrigin`)**: Upon restoring solid state, the entity MUST be actively relinked into the engine's BSP spatial partitioning tree via `UTIL_SetOrigin(pev, m_vecOriginalOrigin)`. Simply updating `pev->origin` without invoking engine set-origin leaves the entity unlinked in the spatial leaf hierarchy, rendering it non-solid to physics hull traces.
  * **Spawner Collision Exclusion & Pre-Spawn Validation**:
    * Procedural item and supply generation (`SpawnPickups`) must execute after breakable regeneration has fully restored and relinked all `gladder_breakable` entities in the world.
    * In addition to hull traces, pickup spawning must explicitly verify that candidate spawn coordinates do not overlap or intersect solid brush entity bounding volumes (`GladderIsPositionSolidOrBrush`), strictly preventing items, weapons, or ammo from spawning embedded inside crates or breakable geometry.

### 11.3 Combat Decal Purging & Environmental Decal Preservation
Intense combat across successive waves rapidly accumulates hundreds of dynamic surface decals (bullet impacts, monster blood splatters, explosion burns). If left uncleared, dynamic decals degrade rendering performance, clutter map navigation, and eventually cause erratic decal replacement as the engine hits internal decal limits.

* **Separation of Dynamic vs. Environmental Decals**:
  * **Dynamic Combat Decals**: Decals produced by projectile impacts (`UTIL_DecalTrace`, `TE_DECAL`, `TE_WORLDDECAL`), monster blood splatters (`UTIL_BloodDrips`), and explosions (`scorch1`). These decals lack the engine's `FDECAL_PERMANENT` flag.
  * **Static Environmental Decals**: Pre-placed map decals created by level designers (`infodecal` entities: blood stains, hazard markings, radioactive warning signs, grime). These are instantiated at map initialization via `g_engfuncs.pfnStaticDecal()` with the `FDECAL_PERMANENT` flag set.
* **Wave Transition Decal Purge**:
  * Upon wave completion (when the player hits Point B `trigger_gladder_end`), the client receives the wave completion transition event.
  * During the subsequent screen fade-to-black before the player is teleported back to Point A, the client executes an engine decal purge (`GladderPurgeCombatDecals` / `R_DecalRemoveAll`, also aliased to `r_cleardecals`).
  * Because GoldSrc's `R_DecalRemoveAll` strictly unlinks decals lacking `FDECAL_PERMANENT`, all transient combat impacts and blood are completely removed, while all level-authored `infodecal`s remain intact.
* **Upstream Server-Side Static Decal Tracking (`OnStaticDecal`)**:
  * To guard against any edge cases where surface decals are globally invalidated or need deterministic replay across wave resets, upstream `urgorri/halflife-refactored` provides the `CGameRules::OnStaticDecal(...)` lifecycle hook called from `CDecal::StaticDecal()`.
  * `CGladderRules` captures and caches all static decal origins, textures, and parent entities at map spawn, allowing instant reconstruction if required.

---

## 12. Wave Transition UI Feedback & Dynamic Screen Notifications

To provide immediate, unmissable feedback upon entering and completing a wave, centered on-screen overlays notify the player of match progression:

### 12.1 Wave Start Overlay
* **Trigger Event**: Fired immediately when the player crosses the wave start trigger (`trigger_gladder_start`) leaving Point A.
* **Wave Title / Number**: The active wave number and name are displayed prominently, centered in the player's viewport (e.g. `WAVE 5 STARTED` or custom designated wave title).
* **Special Wave Mutator Subtitle**:
  * If the active wave features a special mutator (e.g. `Low Gravity`, `Blackout`, `Swarm: Headcrabs`, `Elite Champion Surge`), the modifier name and tactical alert are rendered centered **immediately beneath** the wave title.
  * Ensures the player is alerted to altered physics or hostile conditions before engaging enemies.

### 12.2 Wave Completion Overlay
* **Trigger Event**: Fired the instant the player enters the extraction zone at Point B (`trigger_gladder_end`).
* **Wave Completion Text**: Renders `WAVE <N> COMPLETED` centered on screen (where `<N>` corresponds strictly to the wave number just completed, e.g. `WAVE 1 COMPLETED` upon completing Wave 1, rather than the incremented incoming wave number) directly before screen fade and Point A teleportation.

### 12.3 Overlay Presentation & Timing
* **Screen Placement**: Centered horizontally (`ScreenWidth() / 2`) and placed in the upper-middle third of the screen (`ScreenHeight() / 3`) to ensure high visibility without obstructing the player's primary weapon reticle.
* **Display Duration**: 3.0 seconds solid hold, followed by a smooth 1.0-second alpha fade-out (total 4.0 seconds), doubling the on-screen presentation time to ensure players have ample time to read wave numbers and mutator alerts during intense combat and movement.
* **Aesthetic Consistency**: Rendered using classic Half-Life green/amber HUD typography, perfectly synchronized with the corresponding audio stings (Section 8).

