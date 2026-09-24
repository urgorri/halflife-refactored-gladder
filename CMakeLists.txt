cmake_minimum_required(VERSION 3.16)
project(halflife-refactored LANGUAGES C CXX)

# Require C++14 and C99 standards
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Options to selectively build subsystems
option(BUILD_SERVER "Build Half-Life server game library (hl)" ON)
option(BUILD_CLIENT "Build Half-Life client game library (client)" ON)
option(BUILD_TESTS "Build Behavioral Equivalence test suite (hl_tests)" ON)

# Default output directory layout
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()
if(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()
if(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
endif()

# Global Architecture and Compiler Configuration
if(MSVC)
    # Ensure Win32 (x86 32-bit) architecture
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(WARNING "Target architecture is 64-bit. Half-Life GoldSrc is a 32-bit engine; please configure CMake with -A Win32.")
    endif()

    # Enable RTTI and align runtime library to static CRT (/MT for Release, /MTd for Debug)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR /W3")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_definitions(
        WIN32
        _WINDOWS
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_DEPRECATE
    )
else()
    # Enforce 32-bit compilation on Linux / POSIX systems
    add_compile_options(-m32)
    add_link_options(-m32)

    # Align compiler definitions with Linux Makefiles
    add_compile_definitions(
        POSIX
        _POSIX
        LINUX
        _LINUX
        GNUC
        HL1
        stricmp=strcasecmp
        _strnicmp=strncasecmp
        strnicmp=strncasecmp
        _snprintf=snprintf
        _alloca=alloca
        _vsnprintf=vsnprintf
        NO_MALLOC_OVERRIDE
    )
    add_compile_options(
        -fpermissive
        -fno-strict-aliasing
        -Werror=return-type
        -w
    )
endif()

# Shared Physical Movement Sources (pm_shared)
set(PM_SHARED_SOURCES
    pm_shared/pm_debug.c
    pm_shared/pm_duck.c
    pm_shared/pm_hull.c
    pm_shared/pm_ladders.c
    pm_shared/pm_math.c
    pm_shared/pm_move_air.c
    pm_shared/pm_move_ground.c
    pm_shared/pm_move_water.c
    pm_shared/pm_shared.c
    pm_shared/pm_step.c
)

# ==============================================================================
# Server Target: hl (hl.dll / hl.so)
# ==============================================================================
if(BUILD_SERVER)
    # Collect all server sources dynamically, supporting downstream extension directories
    file(GLOB_RECURSE SERVER_SOURCES CONFIGURE_DEPENDS
        "dlls/*.cpp"
        "dlls/*.c"
    )

    # wxdebug.cpp is Windows-only (requires tchar.h)
    if(NOT WIN32)
        list(FILTER SERVER_SOURCES EXCLUDE REGEX "dlls/core/wxdebug\\.cpp$")
    endif()

    # Optional downstream custom hooks
    file(GLOB_RECURSE SERVER_CUSTOM_SOURCES CONFIGURE_DEPENDS
        "dlls/custom/*.cpp"
        "dlls/custom/*.c"
    )

    set(SERVER_ALL_SOURCES
        ${SERVER_SOURCES}
        ${SERVER_CUSTOM_SOURCES}
        ${PM_SHARED_SOURCES}
        game_shared/voice_gamemgr.cpp
    )

    # Windows module definition file for export table alignment
    if(MSVC)
        list(APPEND SERVER_ALL_SOURCES dlls/hl.def)
    endif()

    add_library(hl SHARED ${SERVER_ALL_SOURCES})

    set_target_properties(hl PROPERTIES
        PREFIX ""
        OUTPUT_NAME "hl"
    )

    target_compile_definitions(hl PRIVATE
        QUIVER
        VOXEL
        QUAKE2
        VALVE_DLL
        CLIENT_WEAPONS
    )

    target_include_directories(hl PRIVATE
        dlls
        dlls/core
        dlls/weapons
        engine
        common
        pm_shared
        game_shared
        public
    )

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dlls/custom")
        target_include_directories(hl PRIVATE dlls/custom)
    endif()

    if(WIN32)
        target_link_libraries(hl PRIVATE dbghelp)
    endif()

    if(NOT MSVC)
        target_link_libraries(hl PRIVATE m dl pthread stdc++)
    endif()
endif()

# ==============================================================================
# Client Target: client (client.dll / client.so)
# ==============================================================================
if(BUILD_CLIENT)
    # Collect client sources while excluding standalone / legacy overview.cpp
    file(GLOB_RECURSE CLIENT_SOURCES CONFIGURE_DEPENDS
        "cl_dll/*.cpp"
        "cl_dll/*.c"
    )
    list(FILTER CLIENT_SOURCES EXCLUDE REGEX "cl_dll/render/overview\\.cpp$")

    # Optional downstream custom hooks
    file(GLOB_RECURSE CLIENT_CUSTOM_SOURCES CONFIGURE_DEPENDS
        "cl_dll/custom/*.cpp"
        "cl_dll/custom/*.c"
    )

    # Shared weapon sources included directly in client prediction
    set(CLIENT_SHARED_WEAPON_SOURCES
        dlls/weapons/weapon_crossbow.cpp
        dlls/weapons/weapon_crowbar.cpp
        dlls/weapons/weapon_egon.cpp
        dlls/weapons/weapon_gauss.cpp
        dlls/weapons/weapon_glock.cpp
        dlls/weapons/weapon_handgrenade.cpp
        dlls/weapons/weapon_hornetgun.cpp
        dlls/weapons/weapon_mp5.cpp
        dlls/weapons/weapon_python.cpp
        dlls/weapons/weapon_rpg.cpp
        dlls/weapons/weapon_satchel.cpp
        dlls/weapons/weapon_shotgun.cpp
        dlls/weapons/weapon_snark.cpp
        dlls/weapons/weapon_tripmine.cpp
    )

    # Shared common/vgui client sources
    set(CLIENT_SHARED_SOURCES
        common/parsemsg.cpp
        public/interface.cpp
        game_shared/vgui_checkbutton2.cpp
        game_shared/vgui_grid.cpp
        game_shared/vgui_helpers.cpp
        game_shared/vgui_listbox.cpp
        game_shared/vgui_loadtga.cpp
        game_shared/vgui_scrollbar2.cpp
        game_shared/vgui_slider2.cpp
        game_shared/voice_banmgr.cpp
    )

    set(CLIENT_ALL_SOURCES
        ${CLIENT_SOURCES}
        ${CLIENT_CUSTOM_SOURCES}
        ${CLIENT_SHARED_WEAPON_SOURCES}
        ${CLIENT_SHARED_SOURCES}
        ${PM_SHARED_SOURCES}
    )

    add_library(client SHARED ${CLIENT_ALL_SOURCES})

    set_target_properties(client PROPERTIES
        PREFIX ""
        OUTPUT_NAME "client"
    )

    target_compile_definitions(client PRIVATE
        CLIENT_DLL
        CLIENT_WEAPONS
        HL_DLL
    )

    if(NOT MSVC)
        target_compile_definitions(client PRIVATE
            _snwprintf=swprintf
            DISABLE_JUMP_ORIGIN
            DISABLE_VEC_ORIGIN
        )
    endif()

    target_include_directories(client PRIVATE
        dlls
        dlls/core
        dlls/weapons
        cl_dll
        cl_dll/hud
        cl_dll/studio
        cl_dll/input
        cl_dll/core
        cl_dll/render
        cl_dll/entities
        cl_dll/hl
        cl_dll/systems
        cl_dll/events
        public
        common
        pm_shared
        engine
        utils/vgui/include
        game_shared
        external
    )

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cl_dll/custom")
        target_include_directories(client PRIVATE cl_dll/custom)
    endif()

    if(MSVC)
        target_link_libraries(client PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/utils/vgui/lib/win32_vc16/vgui.lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/lib/public/sdl2.lib"
            wsock32
        )
        add_custom_command(TARGET client POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/lib/public/vgui.dll"
                "$<TARGET_FILE_DIR:client>/"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/lib/public/SDL2.dll"
                "$<TARGET_FILE_DIR:client>/"
        )
    else()
        add_library(vgui_bundled SHARED IMPORTED)
        set_target_properties(vgui_bundled PROPERTIES
            IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/linux/vgui.so"
        )
        add_library(sdl2_bundled SHARED IMPORTED)
        set_target_properties(sdl2_bundled PROPERTIES
            IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/linux/libSDL2.so"
        )
        target_link_libraries(client PRIVATE
            vgui_bundled
            sdl2_bundled
            dl
            pthread
            m
            stdc++
        )
    endif()
endif()

# ==============================================================================
# Tests Target: hl_tests (hl_tests.exe / hl_tests)
# ==============================================================================
if(BUILD_TESTS)
    set(TESTS_SOURCES
        external/catch2/catch_amalgamated.cpp
        tests/mock_engine.cpp
        tests/test_main.cpp
        tests/test_pm_movement.cpp
        tests/test_combat.cpp
        tests/test_abi_layout.cpp
        tests/test_saverestore.cpp
        tests/test_weapon_pipeline.cpp
        tests/test_item_registry.cpp
        tests/test_ai_factions.cpp
        tests/test_gamerules_skill.cpp
        tests/test_gameplay_hooks.cpp
        tests/test_hud_registry.cpp
        tests/mock_client_engine.cpp
        tests/test_client_lifecycle.cpp
        tests/test_client_studio.cpp
        tests/test_studio_transform.cpp
        tests/test_level_transition.cpp
        tests/test_effects_beams.cpp
        tests/test_crash_handler.cpp
        tests/test_triggers.cpp
        tests/test_player_customization.cpp
        tests/test_ai_navigation.cpp
        dlls/core/client_customization.cpp
        dlls/systems/effects_beams.cpp
        dlls/systems/chargers.cpp
        dlls/systems/crash_handler.cpp
        dlls/systems/triggers_audio.cpp
        dlls/systems/sound_sentences.cpp
        dlls/weapons/weapon_registry.cpp
        dlls/items/item_registry.cpp
        dlls/ai/monster_relationship_manager.cpp
        dlls/ai/talk_companion_registry.cpp
        dlls/ai/nodes_graph.cpp
        dlls/ai/nodes_routing.cpp
        dlls/ai/nodes_links.cpp
        dlls/gameplay/gamerules_factory.cpp
        dlls/core/skill_manager.cpp
        dlls/core/skill.cpp
        dlls/core/user_message_registry.cpp
        cl_dll/hl/client_weapon_manager.cpp
        cl_dll/events/event_registry.cpp
        cl_dll/hud/hud_registry.cpp
        cl_dll/entities/entity_visual_registry.cpp
        ${PM_SHARED_SOURCES}
    )

    add_executable(hl_tests ${TESTS_SOURCES})

    set_target_properties(hl_tests PROPERTIES
        OUTPUT_NAME "hl_tests"
    )

    target_compile_definitions(hl_tests PRIVATE
        VALVE_DLL
        HL_TESTS
        CATCH_AMALGAMATED_CUSTOM_MAIN
    )

    if(MSVC)
        target_compile_definitions(hl_tests PRIVATE _CONSOLE)
    endif()

    target_include_directories(hl_tests PRIVATE
        .
        tests
        external/catch2
        dlls
        dlls/core
        dlls/weapons
        dlls/ai
        dlls/gameplay
        cl_dll
        engine
        common
        pm_shared
        game_shared
        public
    )

    if(WIN32)
        target_link_libraries(hl_tests PRIVATE dbghelp)
    endif()

    if(NOT MSVC)
        target_link_libraries(hl_tests PRIVATE m dl pthread stdc++)
    endif()

    enable_testing()
    add_test(NAME behavioral_equivalence COMMAND hl_tests)

    # ==============================================================================
    # Smoke Test Target: smoke_test_client (smoke_test_client.exe / smoke_test_client)
    # ==============================================================================
    add_executable(smoke_test_client tests/smoke_test_client.cpp)

    set_target_properties(smoke_test_client PROPERTIES
        OUTPUT_NAME "smoke_test_client"
    )

    if(MSVC)
        target_compile_definitions(smoke_test_client PRIVATE _CONSOLE)
    endif()

    target_include_directories(smoke_test_client PRIVATE
        .
        tests
        engine
        common
        pm_shared
        public
    )

    if(NOT MSVC)
        target_link_libraries(smoke_test_client PRIVATE dl m pthread)
    endif()
endif()
