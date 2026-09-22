# Half-Life: Gladder

An arcade wave-based gauntlet runner mod for the Half-Life (GoldSrc) engine.

`Half-Life: Gladder` transforms the classic Half-Life experience into a fast-paced, high-replayability arcade challenge. Players navigate single-map gauntlets across escalating waves, fighting procedural enemy encounters and scavenging randomized supplies under tight time constraints.

---

## Upstream & Lineage

This project is a downstream fork of [**urgorri/halflife-refactored**](https://github.com/urgorri/halflife-refactored), which provides a modernized, modular, and behavior-preserving refactor of the official **Half-Life 1 SDK** (GoldSrc) released by Valve Corporation.

* **Upstream**: [`urgorri/halflife-refactored`](https://github.com/urgorri/halflife-refactored)
* **Root Origin**: Valve Software Half-Life 1 SDK
* **Architecture Base**: Leverages the refactored subsystem architecture (`dlls/monsters/`, `dlls/systems/`, `dlls/items/`, `dlls/weapons/`, `dlls/gameplay/`) and extensible mod hooks (`dlls/custom/`, `cl_dll/custom/`).

---

## Core Features

* **Gauntlet Run Loop**: Players run from **Point A** (spawn) to **Point B** (finish). Reaching Point B immediately loops the player back to Point A, increments the wave counter, and regenerates threats and pickups without level transitions or map reloads.
* **Procedural Dynamic Spawning**: Each wave populates the map with a randomized distribution of monsters, weapons, ammunition, and health/armor pickups calibrated to an escalating difficulty curve.
* **Map Spatial Indexing (32-Unit Grid)**:
  * Maps are automatically indexed into persistent cache files (`gladder/maps/<mapname>.grid.dat`).
  * Scans bounding area volumes with vertical raycasts (sampled every 32 engine units) to identify valid supporting surfaces (including tops of crates, platforms, and structures) and ensure adequate vertical clearance.
* **Time-Driven Match Objective**: The core goal is time-based—survive, maintain momentum, and complete as many wave laps as possible before the session clock expires.
* **Real-Time HUD Telemetry**:
  * Persistent on-screen display tracking: Match Clock / Time Remaining, Active Wave, Active Wave Stopwatch, Total Frags, and Average Wave Duration.
* **Post-Game Summary & Grading**:
  * Shows total waves completed, pacing breakdown, and itemized kill log per enemy species.
  * Awards an arcade performance rating (**S / A / B / C / D / F**) calculated from wave clearance speed, combat throughput, and survival efficiency.
  * Displays the results over a live cinematic static camera backdrop (`trigger_camera`).
* **Arcade Integrity**: All mid-game saving and loading (quick-save, autosave, quick-load) are completely disabled. Runs are permadeath sessions.
* **Expanded Arsenal**: Features weapons from official expansions, including *Half-Life: Opposing Force* (e.g., Desert Eagle, M40A1 Sniper Rifle, M249 SAW, Shock Rifle, Spore Launcher, Displacer Cannon).

For full details on gameplay rules, entity definitions, and systems design, see [**`SPEC.md`**](./SPEC.md).

---

## Building

The project inherits the cross-platform CMake build system from `halflife-refactored`, compiling the server library (`hl`), client library (`client`), and behavioral test suite (`hl_tests`).

### Windows (Visual Studio 2019 / 2022 / BuildTools)

Configure for 32-bit x86 architecture and compile:

```cmd
cmake -B build -A Win32
cmake --build build --config Release
```

Output binaries are placed in `build/bin/Release/`:
* `hl.dll` (Server DLL)
* `client.dll` (Client DLL)
* `hl_tests.exe` (Behavioral equivalence test suite)

To run the test suite:

```cmd
ctest --test-dir build -C Release --output-on-failure
```

### Linux (GCC / Clang x86)

Ensure 32-bit multilib packages and CMake are installed:

```bash
sudo dpkg --add-architecture i386
sudo apt-get update && sudo apt-get install -y gcc-multilib g++-multilib make cmake
```

Configure and compile:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Output binaries are placed in `build/bin/`:
* `hl.so` (Server shared library)
* `client.so` (Client shared library)
* `hl_tests` (Behavioral equivalence test suite)

To run the test suite:

```bash
ctest --test-dir build --output-on-failure
```

---

## Mapping & Level Design Integration

`Half-Life: Gladder` provides mapper-friendly entities to adapt existing maps or build custom gauntlet arenas:
* **Point A Spawn**: Player spawn origin and wave reset destination.
* **Point B Trigger**: Trigger brush or entity marking the lap finish line.
* **Area Definition Volumes**: Bounding box entities with min/max bounds and custom IDs to guide spatial grid generation.
* **End-Game Camera**: In-map static camera (`trigger_camera`) for the final score screen backdrop.
* **Wave Relays**: Logic relays triggered on wave start, wave victory, and game over.

---

## License

This project is derived from the **Half-Life 1 SDK** originally released by Valve Corporation.

The original SDK license is retained in this repository:
* [`LICENSE`](./LICENSE)

Copyright © Valve Corp.

See [`LICENSE`](./LICENSE) for full terms governing the SDK and derivative works.

## Disclaimer

This project is an independent community modification based on the Half-Life 1 SDK. It is not an official Valve project and is not affiliated with or endorsed by Valve Corporation.
