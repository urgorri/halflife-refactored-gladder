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
