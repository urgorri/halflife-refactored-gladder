# AGENTS.md

## Repository Overview & Identity

This repository (`halflife-refactored-gladder`) is the downstream repository implementing **Half-Life: Gladder**, an arcade-style, wave-based gauntlet runner mod for the Half-Life (GoldSrc) engine.
The non-technical gameplay design, rules, and telemetry requirements are specified in [`SPEC.md`](SPEC.md).

The codebase is built on top of the upstream modern SDK repository **[`urgorri/halflife-refactored`](https://github.com/urgorri/halflife-refactored)**, which is also maintained by the project owner.

---

## The Prime Directive: Upstream Synchronization & Zero-Pollution Policy

**All AI agents and developers working in this repository must preserve seamless synchronization with upstream at all times.**

### 1. Zero Core Pollution
- Do not introduce ad-hoc modifications, dirty hacks, or direct alterations to core engine/SDK files (such as `dlls/core/`, `dlls/ai/`, `cl_dll/core/`, `cl_dll/entities/`, `cl_dll/hl/`).
- Core files in this repository should remain identical to `upstream/master`. Any divergence in shared core files creates merge conflicts and breaks seamless rebases when upstream receives bugfixes, code smell refactorings, or build improvements.

### 2. The "Stop, Analyze & Upstream First" Protocol
Whenever a feature, trigger, entity, or mechanic from [`SPEC.md`](SPEC.md) appears to require modifying an existing core file because upstream lacks an appropriate hook, virtual method, registry, or extension point:

1. **STOP:** Do not implement an in-place patch or invasive workaround in core vanilla files downstream.
2. **ANALYZE:** Identify the exact architectural coupling or missing extension point in upstream. Determine what agnostic interface, virtual delegation, or registry upstream needs.
3. **VERIFY BEHAVIORAL EQUIVALENCE:** Ensure that the proposed upstream abstraction is 100% backward-compatible and preserves vanilla Half-Life gameplay without altering a single bit of original behavior (e.g. passive default fallbacks).
4. **REPORT TO MAINTAINER:** Document the finding and propose the concrete upstream refactoring. The project maintainer will implement or dispatch the change in `urgorri/halflife-refactored` first.
5. **DOWNSTREAM CONSUMPTION:** Once the upstream change is merged and synchronized into this repository, implement the Gladder feature purely as an external consumer of the new decoupled extension point.

### 3. Downstream Architectural Isolation
All Gladder-specific functionality must reside in dedicated downstream directories:
- `dlls/gladder/`: Gamemodes, wave managers, procedural spawner, spatial grid indexer, custom entities (`trigger_gladder_*`), scoring, and leaderboards.
- `cl_dll/gladder/`: Custom HUD elements, combo counter, match stopwatch, end-of-run summary screens, and client visual modifiers.

Integrations with engine lifecycle events must exclusively use upstream's extensible registries:
- **Game Rules:** Subclass `CGameRules` (e.g. `CGladderRules`) and register via `REGISTER_GAMERULES`.
- **HUD Elements:** Subclass `CHudBase` and register via `REGISTER_HUD_ELEMENT` / `HudRegistry`.
- **User Messages:** Declare network messages via `REGISTER_USER_MESSAGE` / `UserMessageRegistry`.
- **Custom Items & Weapons:** Register via `ItemRegistry` and `ClientWeaponManager`.
- **Client Visual Effects:** Register client-side entity modifiers via `EntityVisualRegistry`.

---

## Build Systems & File Synchronization

The repository maintains multiple build systems that must remain strictly synchronized:

* **CMake**: `CMakeLists.txt` (used for local builds, unit tests, and CI CMake targets).
* **Visual Studio 2019 Projects**: `projects/vs2019/*.vcxproj` and `*.vcxproj.filters` (`hldll`, `hl_cdll`, `hl_tests`, `smoke_test_client`).
* **Linux Makefiles**: `linux/Makefile.*` (`Makefile.hldll`, `Makefile.hl_cdll`, `Makefile.tests`).

### Rules for File Additions, Moves, and Deletions:

* **Always synchronize all build targets**: When adding, renaming, or removing any `.cpp` or `.c` source or test file, update all three build systems (`CMakeLists.txt`, `projects/vs2019/`, and `linux/Makefile.*`) simultaneously.
* **Update filters**: Keep Visual Studio `.vcxproj.filters` aligned with the folder structure under `dlls/`, `cl_dll/`, and `tests/`.
* **Verify symbol exports**: Ensure entity factories and exported functions (`tests/verify_symbols.py`, `dlls/hl.def`, `tests/golden/symbols_*`) compile and export properly across both Windows DLLs and Linux `.so` shared libraries.
* **Synchronize test suites**: Ensure all unit test files added under `tests/` are included in `CMakeLists.txt` (`TESTS_SOURCES`), `projects/vs2019/hl_tests.vcxproj`, and `linux/Makefile.tests`.
* **Synchronize map editor definitions (`gladder.fgd`)**: Whenever an entity is added, modified, or removed in `dlls/gladder/` (via `LINK_ENTITY_TO_CLASS`), update `gladder.fgd` at the root of the repository so level designers can place and configure the entity in map editors (Valve Hammer Editor, J.A.C.K., TrenchBroom).

### Cross-Platform Build Discipline & Pre-PR Verification (Windows & Linux):

The codebase must compile and pass tests across both Windows and Linux CI environments. **Agents and developers do not need to install or run Docker, virtual machines, or local Linux toolchains.** Instead, code and build configurations must strictly adhere to static cross-platform discipline so that dual-platform compatibility is guaranteed by design:

1. **Static Code Discipline (Guaranteed Dual-Platform Compilation)**:
   - **C++11 Standard Compliance (`-std=gnu++11`)**: While Windows MSVC and CMake use C++14, the legacy Linux toolchains (`linux/Makefile.*`) compile with `-std=gnu++11`. Code must remain strictly compliant with C++11.
   - **No C++14-only Aggregate Initialization**: Never use brace-enclosed aggregate initialization (`{...}`) on structs or classes that contain default member initializers (e.g. `int foo = 0;`). In C++11, default member initializers prevent a type from being an aggregate. Always provide explicit constructors (`Foo() : foo(0) {}`, `Foo(int f) : foo(f) {}`), or instantiate the struct and assign its fields explicitly.
   - **Path Separators & Case Sensitivity**: Linux filesystems are strictly case-sensitive. All `#include` paths must match disk filename casing exactly (e.g. `#include "hud.h"`, not `#include "HUD.h"`). In source code and file operations, always use forward slashes (`/`), never Windows-only backslashes (`\`).
   - **Platform-Specific Headers**: Never include Windows-specific headers (`<windows.h>`, `<direct.h>`) without appropriate `#ifdef _WIN32` preprocessor guards, providing POSIX equivalents (`<unistd.h>`, `<sys/stat.h>`) for Linux.

2. **Linux Makefile Directory Hygiene (`linux/Makefile.*`)**:
   - Whenever source or object files are added under a subfolder (e.g. `$(HL1_SERVER_OBJ_DIR)/gladder/...`), the makefile must ensure the directory exists before compilation:
     1. Add the directory to the `dirs:` target with `mkdir -p <dir>`.
     2. Declare an explicit target rule for the directory (e.g. `$(HL1_SERVER_OBJ_DIR)/gladder: \n\t mkdir -p $(HL1_SERVER_OBJ_DIR)/gladder`) when used as a prerequisite in pattern rules.
     3. Ensure the top-level `all:` target depends on `dirs`.

3. **Pre-PR Local Verification (Windows)**:
   Before opening or updating a pull request, verify all local Windows targets:
   - Build both `projects/vs2019/hl_cdll.vcxproj` and `hldll.vcxproj` in Release/Win32 configuration.
   - Build CMake targets (`cmake -B build -A Win32` && `cmake --build build --config Release`).
   - Run the full Catch2 test suite: `hl_tests.exe` (must pass 100% of assertions).
   - Run the client smoke test: `smoke_test_client.exe <path/to/client.dll>`.
   - Verify exported symbols against golden baselines (`verify_symbols.py`).

By strictly enforcing these static code and makefile invariants, code is guaranteed to build cleanly in Linux CI without requiring local Linux simulation or container tooling.

---

## Level Design & Map Editor Compatibility (`gladder.fgd`)

The repository maintains **`gladder.fgd`** at the repository root. Forge Game Data (.fgd) files provide level editors (Valve Hammer Editor 3.4/3.5, J.A.C.K., TrenchBroom, Sledge) with definitions, property dialogs, spawnflag checkboxes, and bounding hulls for all entities available in the mod.

### Rules for Entity Authoring:
- **Mandatory FGD Registration**: Every new map-placeable entity introduced in downstream code (`dlls/gladder/`) MUST be registered in `gladder.fgd`. Never introduce or merge a map entity PR without its corresponding FGD entry.
- **Entity Classification**:
  - Point entities (relays, markers, ambient logic) must be declared with `@PointClass` and appropriate base classes (e.g. `base(Targetname, Targetx)`).
  - Brush entities (triggers, area volumes, breakables) must be declared with `@SolidClass` and appropriate base classes (e.g. `base(Trigger)`, `base(Breakable, RenderFields)`).
- **KeyValues & Spawnflags Completeness**: Every `KeyValue` handled in C++ (such as `areaname`, `areaid`, `wave_event`) and every spawnflag bitmask must be documented with clear labels, default values, and dropdown choices.
- **Base Class Hygiene**: Gladder's FGD extends standard `halflife.fgd`. It must not redefine standard Half-Life base classes (`Targetname`, `Target`, `Trigger`, `Breakable`, etc.) to avoid duplicate symbol collision in editors that load both files.

---

## Technical Constraints & Guidelines

### GoldSrc Engine Architecture
- **MAX_WEAPONS Limit (32):** Weapon IDs must not exceed 31 (`WEAPON_SUIT = 31`). Custom weapons (e.g. Opposing Force arsenal) use available slots in IDs 16..24.
- **Entity Limits (`MAX_EDICTS`):** GoldSrc strictly caps concurrent entities (typically 512–900). Wave transitions must enforce comprehensive garbage collection of monsters, dropped items, and temporary decals.
- **Client vs. Server Separation:** Visual animations, levitating/bobbing pickups, and dynamic lights must be processed on the client to avoid saturating network bandwidth and to maintain smooth rendering at high framerates.
- **Deterministic Math & Movement:** Do not alter shared movement physics routines (`pm_shared/`) or prediction states unless explicitly required by an isolated mutator.

### Development Environment & Standards
- **Build System:** CMake (targeting Win32 / x86 architecture, C++14 standard).
- **Language Policy:** All code, comments, commit messages, PR descriptions, documentation, logs, and error strings must be written in **English**.
- **Testing:** Ensure build targets (`hl`, `client`, `hl_tests`) compile cleanly without warnings or ABI layout regressions.
