#pragma once

#include "menu/menu.h"
#include "match/shell/shell.h"

#include <SDL3/SDL.h>
#include <cstdint>
#include <string>

enum LaunchMode {
    LAUNCH_MODE_MENU,
    LAUNCH_MODE_LOBBY_INVITE,
    LAUNCH_MODE_TEST_HOST,
    LAUNCH_MODE_TEST_JOIN,
    LAUNCH_MODE_EDITOR,
    LAUNCH_MODE_LUA_DOC,
    LAUNCH_MODE_ROAD_DATA,
    LAUNCH_MODE_SCENARIO_EXPORT
};

struct LaunchOptions {
    LaunchMode mode;
    std::string log_prefix;
    uint64_t steam_invite_id;
    bool log_prefix_is_specified;
};

enum TestMode {
    TEST_MODE_OFF,
    TEST_MODE_HOST,
    TEST_MODE_JOIN
};

enum GameMode {
    GAME_MODE_MENU,
    GAME_MODE_MATCH,
    GAME_MODE_EDITOR
};

struct GameState {
    SDL_Window* window;
    LaunchMode launch_mode;
    GameMode mode;

    // Game states
    MenuState* menu_state;
    MatchShell* match_shell;

    // Timekeeping
    uint64_t last_time;
    uint64_t last_second;
    uint64_t update_accumulator;
    uint32_t frames;
    uint32_t fps;

    // Debug
    Bot test_bot;
    uint32_t playback_speed;
    bool debug_should_render_info;
};

// Main
int gold_main(int argc, char** argv);

// Launch options
LaunchOptions gold_get_launch_options(int argc, char** argv);
bool gold_get_argv(int argc, char** argv, const char* key, const char** result);

// Engine init / quit
bool gold_init(const LaunchOptions& launch_options);
bool gold_create_window();
void gold_quit();

// Game loop
bool gold_is_running();
void gold_timekeep();
void gold_set_mode_match(RawMap* raw_map, int lcg_seed);
void gold_set_mode_scenario();
void gold_set_mode_replay();
void gold_leave_match();
RawMap* gold_generate_map(int* lcg_seed);

// Steam
bool gold_steam_restart_app_if_necessary();
bool gold_steam_api_init();
void gold_steam_api_shutdown();
void gold_steam_run_callbacks();

// Debug
void gold_debug_handle_input();
void gold_debug_render_info();
void gold_debug_begin_editor_playtest();

// Profile
void gold_debug_render_profiler_info();

// Test mode
void gold_test_mode_init();
void gold_test_mode_update();
