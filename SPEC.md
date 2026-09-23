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
5. **Teleport & Reset**: The player is instantly teleported back to Point A.
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
* **Time-Driven Match Objective**:
  * The core goal of each run is **time-based**: players must survive, navigate the gauntlet, and clear as many waves as possible within a configured session duration / time limit (e.g., 10, 15, or 20 minutes) or survival clock.
  * Wave completions are continuously counted and logged as the primary scoring metric, but the match concludes when the time limit expires (or upon player death).
* **Arcade Scoring & Combo Multiplier**:
  * **End-of-Match Score Calculation**: To preserve clean screen visibility and immersion, the cumulative score is **calculated and revealed exclusively at the end of the match** on the summary screen.
  * **In-Game Combo Meter**: During active combat, rapid consecutive kills within a decaying window build a streak multiplier (`x2`, `x3`, `x4`, etc.). The game logs every combo event, streak count, and the peak maximum combo attained for the final score calculation.
* **Elite Monster Variants (Champions)**:
  * Enemies have a procedurally rolled chance to spawn as **Elite Champions**, with the spawn probability increasing as wave numbers climb.
  * **Visual Identification**: Rendered using GoldSrc's engine glow shell (`kRenderFxGlowShell` with red RGB color).
  * **Attributes**: Enhanced durability, heightened aggression, and elevated point payouts upon defeat.
* **Special Waves & Random Mutators**:
  * Every few waves (at configurable intervals, e.g. every 5th wave), the mod rolls a random gameplay mutator:
    * **Blackout**: Level lights are extinguished into pitch darkness; navigation relies heavily on the HEV flashlight.
    * **Low Gravity**: Xen-like reduced gravity physics applied to both the player and physical debris.
    * **Swarm**: Standard enemy distribution is replaced by a massive horde composed exclusively of a single randomly selected monster species (e.g., all Houndeyes, all Headcrabs, or all Alien Grunts).
* **Diminishing Wall Charger Capacity**:
  * Wall-mounted medical stations (`func_healthcharger`) and HEV suit rechargers (`func_recharge`) are manually placed in map architecture.
  * At the start of each wave, the code re-energizes these stations, but their total restorative capacity ("juice") degrades incrementally per wave, creating heightened tension around health conservation in later waves.



---

## 4. Map Spatial Indexing & Grid Generation

To spawn entities intelligently without hand-placing thousands of spawn markers, the mod indexes map geometry into a reusable spatial cache file.

### 4.1 Indexing Workflow
* When a map is loaded for the first time, the mod checks whether an index file already exists for it (e.g., in a dedicated map data folder such as `gladder/maps/<mapname>.grid.dat`).
* If not present, the mod performs an automatic spatial scan of the level before gameplay begins and saves the resulting index to disk.
* On subsequent playthroughs, the saved index is loaded instantly from disk.

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
  * **Clearance Verification**: An upward check or clearance hull trace verifies that the space between the detected surface and any overhead ceiling/obstruction provides adequate height clearance for players, monsters, or item bounding boxes.
* Cells identified with valid supporting surfaces and adequate vertical clearance are recorded into the grid database, ready to receive randomized entity spawns during waves.

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
* **Wave Completion Trigger (Point B)**: Brush or point trigger placed at the end of the run that detects player arrival, triggers wave completion feedback, and coordinates the teleportation back to Point A.
* **Wave State Relays**: Input/output events that fire on wave start, wave victory, and game over, enabling mappers to trigger map-specific environmental events (doors opening, lights flickering, sirens, hazards).
* **End-Game Backdrop Camera**: Support for associating an in-map static camera entity (such as a designated `trigger_camera`) that provides the scenic background view during the post-game summary.
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
  * **Health & Medical**: Vibrant Green glow and lighting.
  * **Armor & HEV Batteries**: Cyan / Bright Blue glow and lighting.
  * **Ammunition**: Amber / Golden-Orange glow and lighting.
  * **Weapons & Ordnance**: Magenta / Violet or fiery Red glow and lighting (tiered by weapon potency).
### 7.3 Iconic Lambda Collectible Item
* **Unique Per-Wave Spawn**: In each wave, exactly **one single collectible item** spawns procedurally at a randomly selected valid grid location in the map.
* **Custom Model & Presentation**: Features a custom 3D model of the classic Half-Life Lambda (`λ`) insignia. It follows the same arcade visual language: floating suspended in mid-air, rotating on its vertical axis, gently bobbing up and down, and emitting a vibrant golden-orange glow and light.
* **Score & Stat Progression**:
  * Collecting the Lambda grants a substantial score point bonus.
  * Increments the player's total match collectible counter.
* **Acoustic Feedback**: Immediately triggers a dedicated, distinct pickup sound cue upon collection.
* **Wave Lifecycle**: If missed or left behind when Point B is reached, the collectible is wiped during wave garbage collection, and a single new Lambda collectible spawns at a newly randomized grid coordinate for the next wave.

---

## 8. Arcade Audio Cues & Soundscapes

Distinct audio cues provide immediate acoustic feedback for critical match state transitions (specific sound asset files to be designated during implementation):
* **Wave Start Cue**: A punchy, energizing audio sting triggered when the wave commences from Point A.
* **Wave Completion Cue**: A rewarding, triumphant sound effect played the instant Point B is touched and the wave loop finishes.
* **Special Wave / Mutator Alert Cue**: An ominous, siren-like warning sound indicating that a special wave modifier (Blackout, Low Gravity, Swarm) is active.
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
* **Total Frags / Kills**: Real-time counter of total monsters eliminated during the match (`FRAGS: 187`).
* **Average Wave Duration**: Dynamically recalculated average completion time across all finished waves (`AVG WAVE: 00:46`).

### 9.2 End-of-Run Summary, Score Calculation & Grading
Upon match conclusion—whether through **time limit expiration** or **player death**—the mod overrides the standard engine reload/menu routine and immediately activates the summary screen over the static camera backdrop:

* **Static Camera Backdrop**:
  * The player's viewport switches immediately to an in-level static camera (activating a designated `trigger_camera` placed in the map).
  * The living environment remains visible and active in the background while the statistical results, animated score tally, and final rank grade are rendered clearly overlaid on screen.
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
* **Future-Proof Baseline**: Designed as the local data layer that can later interface with external web-based leaderboards.

### 10.3 Save / Load Restrictions & Death Flow
To maintain authentic arcade tension, competitive scoring integrity, and fluid game pacing, saving and loading functionality is eliminated entirely:
* **No Save Functionality**: All save mechanisms (quick-save, manual console/menu save, autosave triggers) are disabled at the root level.
* **No Load Functionality**: Loading existing save files during a session (quick-load or menu load) is disabled.
* **Session Finality & Seamless Death Transition**: Each session represents a single, self-contained run. Player death bypasses original save reload and main menu prompts, seamlessly launching the end-of-run summary screen with full score calculation and high score recording.

---

## 11. Engine Resource Management & Wave Garbage Collection

Because GoldSrc enforces a strict maximum entity limit (`MAX_EDICTS`, typically 512 to 900+ entities), strict resource purging occurs at each wave reset:
* **Active Monster Cleanup**: Any monsters surviving from the previous wave are eradicated immediately upon wave completion.
* **Dropped Item Purge**: Uncollected weapons, ammunition boxes, and medical kits scattered across the map are removed to prevent entity buildup.
* **Transient Entity Clearing**: Lingering projectiles, gibs, corpses, and temporary decal effects are purged.
* **Reliable Spawn Pool**: Guarantees that the incoming wave has a full allocation of free entity slots for procedural spawning without triggering engine exhaustion (`ED_Alloc: no free edicts`).
