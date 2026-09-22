# Half-Life: Gladder - Non-Technical Game Specification

## 1. Overview & Vision

**Half-Life: Gladder** is an arcade-style, wave-based gauntlet runner mod for the Half-Life (GoldSrc) engine. The mod delivers high-replayability, procedural arcade action while preserving the classic atmospheric look, feel, and universe of the original game.

Instead of progressing linearly through a series of maps, players run through a single map repeatedly across sequential "waves" or "laps". The environment remains constant, but each wave dramatically transforms the gameplay by procedurally populating the map with new configurations of enemies, hazards, weapons, and pickups with escalating difficulty.

---

## 2. Core Gameplay Loop

1. **Spawn at Point A (Start)**: The player begins the wave at the designated start point (Point A) equipped with starting equipment.
2. **Traversal & Combat**: The player navigates through the map toward Point B while battling procedurally spawned enemies and scavenging randomized supplies.
3. **Reaching Point B (Finish)**: Entering the designated extraction / completion zone (Point B) immediately completes the current wave.
4. **Teleport & Reset**: The player is instantly teleported back to Point A.
5. **Wave Increment & Respawn**: The wave counter increases (Wave 1 → Wave 2 → ...), uncollected items or remaining enemies are reset/cleared, and a fresh wave of threats and supplies is generated across the map.
6. **Persistence**: The loop repeats continuously without map reloads or level transitions.

---

## 3. Wave Progression & Difficulty Scaling

* **Dynamic Spawning**: Every wave procedurally generates:
  * **Monsters**: Ground units, airborne threats, and ambushes.
  * **Weapons**: Classic Half-Life weapons alongside extended expansion weapons.
  * **Pickups**: Health kits, HEV battery chargers/cells, and ammunition boxes.
* **Escalating Challenge**: With each wave:
  * Enemy encounter density and variety increase.
  * More dangerous and higher-tier enemy types appear.
  * Resource availability (health, armor, ammo) tightens to emphasize route efficiency and target prioritization.
* **Time-Driven Match Objective**:
  * The core goal of each run is **time-based**: players must survive, navigate the gauntlet, and clear as many waves as possible within a configured session duration / time limit (e.g., 10, 15, or 20 minutes) or survival clock.
  * Wave completions are continuously counted and logged as the primary scoring metric, but the match concludes when the time limit expires (or upon player death).


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
* **Wave Start Marker / Trigger (Point A)**: Defines the player's initial spawn point and the return destination upon wave reset.
* **Wave Completion Trigger (Point B)**: Brush or point trigger placed at the end of the run that detects player arrival, triggers wave completion feedback, and coordinates the teleportation back to Point A.
* **Wave State Relays**: Input/output events that fire on wave start, wave victory, and game over, enabling mappers to trigger map-specific environmental events (doors opening, lights flickering, sirens, hazards).
* **End-Game Backdrop Camera**: Support for associating an in-map static camera entity (such as a designated `trigger_camera`) that provides the scenic background view during the post-game summary.
* **Area Definition Volumes**: Bounding box entities with designer parameters for indexing scope and zoning.

---

## 7. Extended Arsenal & Aesthetic Fidelity

* **Authentic Half-Life Universe**: Retains original GoldSrc art assets, audio, HUD styling, and movement mechanics.
* **Expanded Weapon Roster**: Integrates weapons from official expansions (such as *Half-Life: Opposing Force*), including:
  * Pipe Wrench and Combat Knife.
  * Desert Eagle (.357 caliber sidearm with laser sight).
  * M40A1 Sniper Rifle.
  * M249 SAW (Squad Automatic Weapon).
  * Spore Launcher.
  * Shock Rifle.
  * Displacer Cannon.
  * Barnacle Grapple (where map geometry allows).

---

## 8. Real-Time HUD & Player Statistics Tracking

The mod continuously aggregates gameplay statistics across waves and displays them permanently on the player's heads-up display (HUD).

### 8.1 Persistent HUD Elements
A dedicated on-screen text overlay is rendered at all times (matching classic Half-Life HUD green/amber typography and aesthetic):
* **Match Clock / Time Remaining**: Displays the active session timer (either a countdown against the time limit, e.g., `TIME LEFT: 05:24`, or total elapsed run time).
* **Current Wave Counter**: Displays the active wave number (e.g., `WAVE: 14`).
* **Current Wave Timer**: Stopwatch tracking elapsed time spent in the active wave (`WAVE TIME: 00:38`).
* **Total Frags / Kills**: Real-time counter of total monsters eliminated during the match (`FRAGS: 187`).
* **Average Wave Duration**: Dynamically recalculated average completion time across all finished waves (`AVG WAVE: 00:46`).

### 8.2 End-of-Run Summary & Grading
Upon match conclusion (time limit expiration or player death), a comprehensive summary screen appears displaying detailed performance telemetry:
* **Static Camera Backdrop**:
  * The player's viewport switches immediately to an in-level static camera (activating a designated `trigger_camera` placed in the map).
  * The living environment remains visible and active in the background while the statistical results, kill breakdown, and final rank grade are rendered clearly overlaid on screen.
* **Total Waves Cleared**: Total number of completed wave laps achieved within the allotted time.
* **Overall Time & Lap Pacing**:
  * Total match time played.
  * Fastest single wave lap time vs. slowest wave lap time.
  * Average lap duration across the entire run.
* **Granular Per-Enemy-Type Kill Log**:
  * An itemized kill log breaking down exact frags for each distinct enemy species encountered (e.g., `Headcrabs: 42`, `Zombies: 28`, `Houndeyes: 16`, `Bullsquids: 9`, `Vortigaunts: 14`, `Alien Grunts: 6`, `HECU Grunts: 19`, etc.).
* **Arcade Performance Rating (S / A / B / C / D / F)**:
  * An arcade grade calculated via a composite performance formula evaluating:
    * Total waves cleared within the time limit (primary weight).
    * Total combat frags and kill diversity.
    * Average wave completion speed (pace / aggressiveness).
    * Survival efficiency (penalties for excessive damage taken or deaths).
  * Grade scale:
    * **S Rank**: Exceptional mastery—maximum wave clearance speed, high frag count, near-flawless route execution.
    * **A Rank**: Excellent performance—high wave count and strong combat throughput.
    * **B Rank**: Solid run—consistent pacing with moderate wave completion.
    * **C Rank**: Average performance—slower wave pacing or cautious play.
    * **D / F Rank**: Subpar performance—early death, low wave count, or failing to maintain gauntlet momentum.

---

## 9. Arcade Integrity & Save/Load Restrictions

To maintain authentic arcade tension, competitive scoring integrity, and fluid game pacing, saving and loading functionality is eliminated entirely:
* **No Save Functionality**: All save mechanisms (quick-save, manual console/menu save, autosave triggers) are disabled at the root level.
* **No Load Functionality**: Loading existing save files during a session (quick-load or menu load) is disabled.
* **Session Finality & Permadeath**: Each session represents a single, self-contained run. Player death or timer expiration concludes the run and redirects to the final grading screen without checkpoint reloading.
