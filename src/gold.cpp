#include "gold.h"

#include "core/sound.h"
#include "core/cursor.h"
#include "core/options.h"
#include "core/logger.h"
#include "core/filesystem.h"
#include "core/resource.h"
#include "core/input.h"
#include "defines.h"
#include "match/bot/config.h"
#include "match/shell/shell.h"
#include "match/state/map_gen.h"
#include "menu/animation.h"
#include "menu/menu.h"
#include "menu/types.h"
#include "render/road_data.h"
#include "network/network.h"
#include "network/types.h"
#include "render/render.h"
#include "match/shell/desync.h"
#include "match/shell/script/doc.h"
#include "editor/editor.h"
#include "debug/env.h"
#include "profile/profile.h"

#ifdef GOLD_STEAM
    #include <steam/steam_api.h>
#endif

#include <SDL3/SDL_ttf.h>
#include <string>
#include <ctime>

static const uint64_t UPDATE_DURATION = SDL_NS_PER_SECOND / UPDATES_PER_SECOND;
static GameState state;

int gold_main(int argc, char** argv) {
    // Steam restart app if necessary
    if (gold_steam_restart_app_if_necessary()) {
        return 1;
    }

    // Get launch options
    LaunchOptions launch_options = gold_get_launch_options(argc, argv);

    // Dispatch special launch options
    if (launch_options.mode == LAUNCH_MODE_LUA_DOC) {
        script_generate_doc();
        return 0;
    }

    if (launch_options.mode == LAUNCH_MODE_RESOURCE_PACK) {
        resource_create_pack();
        return 0;
    }

    if (launch_options.mode == LAUNCH_MODE_ROAD_DATA) {
        render_generate_road_data();
        return 0;
    }

    if (launch_options.mode == LAUNCH_MODE_SCENARIO_EXPORT) {
        scenario_export_all();
        return 0;
    }

    // Init
    if (!gold_init(launch_options)) {
        return 1;
    }

    // Set initial game mode
    switch (state.launch_mode) {
        case LAUNCH_MODE_MENU:
        case LAUNCH_MODE_TEST_HOST:
        case LAUNCH_MODE_TEST_JOIN: {
            state.menu_state = menu_init();
            state.mode = GAME_MODE_MENU;
            break;
        }
        case LAUNCH_MODE_EDITOR: {
            log_info("Launching as scenario editor.");
            editor_init(state.window);
            state.mode = GAME_MODE_EDITOR;
            break;
        }
        default: {
            GOLD_ASSERT(false);
            break;
        }
    }

    // Game loop
    while (gold_is_running()) {
        gold_timekeep();
        gold_steam_run_callbacks();

        while (state.update_accumulator >= UPDATE_DURATION) {
            state.update_accumulator -= UPDATE_DURATION;

            // Input
            input_poll_events();
            gold_debug_handle_input();

            // Network
            network_service();
            NetworkEvent event;
            while (network_poll_events(&event)) {
                switch (state.mode) {
                    case GAME_MODE_MENU: {
                        if (event.type == NETWORK_EVENT_MATCH_LOAD) {
                            gold_set_mode_match(event.match_load.raw_map, event.match_load.lcg_seed);
                            break;
                        }

                        menu_handle_network_event(state.menu_state, event);
                        break;
                    }
                    case GAME_MODE_MATCH: {
                        match_shell_handle_network_event(state.match_shell, event);
                        break;
                    }
                    case GAME_MODE_EDITOR:
                        break;
                }

                network_cleanup_event(&event);
            }

            // Test mode update
            if (state.launch_mode == LAUNCH_MODE_TEST_HOST || state.launch_mode == LAUNCH_MODE_TEST_JOIN) {
                gold_test_mode_update();
            }

            // Update
            switch (state.mode) {
                case GAME_MODE_MENU: {
                    menu_update(state.menu_state);

                    // Host load match
                    if (state.menu_state->mode == MENU_MODE_LOAD_MATCH) {
                        int lcg_seed;
                        RawMap* raw_map = gold_generate_map(&lcg_seed);
                        if (raw_map == NULL) {
                            menu_show_status(state.menu_state, "Failed to generate map. Try again.");
                            break;
                        }

                        network_begin_loading_match(raw_map, lcg_seed);
                        gold_set_mode_match(raw_map, lcg_seed);

                        free(raw_map);
                        break;
                    }

                    // Load scenario
                    if (state.menu_state->mode == MENU_MODE_LOAD_SCENARIO) {
                        gold_set_mode_scenario();
                        break;
                    }

                    // Load replay
                    if (state.menu_state->mode == MENU_MODE_LOAD_REPLAY) {
                        gold_set_mode_replay();
                        break;
                    }

                    break;
                }
                case GAME_MODE_MATCH: {
                    match_shell_update(state.match_shell);

                    // gold_is_running() will handle exiting the program
                    if (state.match_shell->mode == MATCH_SHELL_MODE_EXIT_PROGRAM) {
                        break;
                    }

                    if (match_shell_is_in_leave_match_mode(state.match_shell)) {
                        gold_leave_match();
                    }

                    break;
                }
                case GAME_MODE_EDITOR: {
                    editor_update();
                    if (editor_requests_playtest()) {
                        gold_debug_begin_editor_playtest();
                    }
                    break;
                }
            }
        }

        // Render
        render_prepare_frame();

        // Render gaem mode
        switch (state.mode) {
            case GAME_MODE_MENU: {
                menu_render(state.menu_state);
                break;
            }
            case GAME_MODE_MATCH: {
                match_shell_render(state.match_shell);
                break;
            }
            case GAME_MODE_EDITOR: {
                editor_render();
                break;
            }
        }

        // Debug text
        gold_debug_render_info();
        gold_debug_render_profiler_info();

        render_present_frame();

        FrameMark;
    } // End game loop

    // Quit
    gold_quit();

    return 0;
}

// LAUNCH OPTIONS

LaunchOptions gold_get_launch_options(int argc, char** argv) {
    LaunchOptions launch_options;
    launch_options.mode = LAUNCH_MODE_MENU;
    launch_options.log_prefix = "latest";
    launch_options.steam_invite_id = 0;
    launch_options.log_prefix_is_specified = false;

    #ifdef GOLD_DEBUG
        launch_options.log_prefix = filesystem_get_timestamp_str();

        const char* log_prefix_value;
        if (gold_get_argv(argc, argv, "--log-prefix", &log_prefix_value)) {
            launch_options.log_prefix = std::string(log_prefix_value);
            launch_options.log_prefix_is_specified = true;
        }

        if (gold_get_argv(argc, argv, "--test-host", NULL)) {
            launch_options.mode = LAUNCH_MODE_TEST_HOST;
        }

        if (gold_get_argv(argc, argv, "--test-join", NULL)) {
            launch_options.mode = LAUNCH_MODE_TEST_JOIN;
        }

        if (gold_get_argv(argc, argv, "--editor", NULL)) {
            launch_options.mode = LAUNCH_MODE_EDITOR;
        }

        if (gold_get_argv(argc, argv, "--lua-doc", NULL)) {
            launch_options.mode = LAUNCH_MODE_LUA_DOC;
        }

        if (gold_get_argv(argc, argv, "--resource-pack", NULL)) {
            launch_options.mode = LAUNCH_MODE_RESOURCE_PACK;
        }

        if (gold_get_argv(argc, argv, "--road-data", NULL)) {
            launch_options.mode = LAUNCH_MODE_ROAD_DATA;
        }

        if (gold_get_argv(argc, argv, "--scenario-export", NULL)) {
            launch_options.mode = LAUNCH_MODE_SCENARIO_EXPORT;
        }
    #endif
    #ifdef GOLD_STEAM
        const char* steam_invite_id_str;
        if (gold_get_argv(argc, argv, "+connect_lobby", &steam_invite_id_str)) {
            launch_options.mode = LAUNCH_MODE_LOBBY_INVITE;
            launch_options.steam_invite_id = std::stoull(steam_invite_id_str);
        }
    #endif

    return launch_options;
}

bool gold_get_argv(int argc, char** argv, const char* key, const char** result) {
    for (int argn = 1; argn < argc; argn++) {
        if (strcmp(argv[argn], key) == 0) {
            if (result != NULL && argn + 1 == argc) {
                return false;
            }
            if (result != NULL) {
                *result = argv[argn + 1];
            }
            return true;
        }
    }

    return false;
}

// ENGINE INIT / QUIT

bool gold_init(const LaunchOptions& launch_options) {
    // Init logger
    filesystem_create_required_folders();
    const std::string logfile_path = launch_options.log_prefix + ".log";
    if (!logger_init(logfile_path.c_str())) {
        return false;
    }

    // Log initialization messages
    log_info("Initializing %s %s.", APP_NAME, APP_VERSION);
    log_info("Detected platform %s.", GOLD_PLATFORM_STR);
    log_info("%s build.", GOLD_BUILD_TYPE_STR);

    // Load options
    options_load();

    // Debug variables
    state.playback_speed = 1;
    env_init();

    // Init SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        log_error("Failed to initialize SDL: %s", SDL_GetError());
        return false;
    }
    if (!TTF_Init()) {
        log_error("Failed to initialize SDL_ttf: %s", SDL_GetError());
        return false;
    }

    // Init Steam API
    if (!gold_steam_api_init()) {
        return false;
    }

    // Create window
    if (!gold_create_window()) {
        return false;
    }

    // Init subsystems
    if (!resource_init()) {
        logger_quit();
        return false;
    }
    resource_open_pack();
    if (!render_init(state.window)) {
        logger_quit();
        return false;
    }
    if (!cursor_init()) {
        logger_quit();
        return false;
    }
    if (!sound_init()) {
        logger_quit();
        return false;
    }
    if (!network_init()) {
        logger_quit();
        return false;
    }
    if (!desync_init(launch_options.log_prefix.c_str())) {
        logger_quit();
        return false;
    }
    resource_close_pack();

    input_init(state.window);
    // TODO
    // achievements_init();
    srand((uint32_t)time(0));

    // Init game state
    state.launch_mode = launch_options.mode;
    state.mode = GAME_MODE_MENU;

    // Init timekeeping
    state.last_time = SDL_GetTicksNS();
    state.last_second = state.last_time;
    state.update_accumulator = 0;
    state.frames = 0;
    state.fps = 0;

    // Game states
    state.menu_state = nullptr;
    state.match_shell = nullptr;

    return true;
}

bool gold_create_window() {
    int window_width, window_height;
    SDL_WindowFlags flags = SDL_WINDOW_OPENGL;

    if (option_get_value(OPTION_DISPLAY) == RENDER_DISPLAY_FULLSCREEN) {
        SDL_DisplayID* display_id = SDL_GetDisplays(NULL);
        const SDL_DisplayMode* display_mode = SDL_GetDesktopDisplayMode(display_id[0]);
        window_width = display_mode->w;
        window_height = display_mode->h;
        flags |= SDL_WINDOW_FULLSCREEN;
    } else {
        window_width = WINDOWED_WIDTH;
        window_height = WINDOWED_HEIGHT;
    }

    state.window = SDL_CreateWindow(APP_NAME, window_width, window_height, flags);
    if (state.window == NULL) {
        log_error("Error creating window: %s", SDL_GetError());
        return false;
    }

    return true;
}

void gold_quit() {
    // Network disconnect
    if (network_get_status() == NETWORK_STATUS_CONNECTED || network_get_status() == NETWORK_STATUS_HOST) {
        network_disconnect();
    }

    // Save options
    options_save();

    // Cleanup game state
    if (state.menu_state != nullptr) {
        menu_free(state.menu_state);
    }
    if (state.match_shell != nullptr) {
        match_shell_free(state.match_shell);
    }
    if (state.launch_mode == LAUNCH_MODE_EDITOR) {
        editor_quit();
    }

    // Quit subsystems
    desync_quit();
    network_quit();
    sound_quit();
    cursor_quit();
    render_quit();
    logger_quit();

    gold_steam_api_shutdown();

    SDL_DestroyWindow(state.window);
    TTF_Quit();
    SDL_Quit();

    log_info("%s quit gracefully.", APP_NAME);
}

// GAME LOOP

bool gold_is_running() {
    if (input_user_requests_exit()) {
        return false;
    }

    if (state.mode == GAME_MODE_MENU && state.menu_state->mode == MENU_MODE_EXIT) {
        return false;
    }

    if (state.mode == GAME_MODE_MATCH && state.match_shell->mode == MATCH_SHELL_MODE_EXIT_PROGRAM) {
        return false;
    }


    return true;
}

void gold_timekeep() {
    // Timekeep
    uint64_t current_time = SDL_GetTicksNS();
    state.update_accumulator += (current_time - state.last_time) * state.playback_speed;
    state.last_time = current_time;

    if (current_time - state.last_second >= SDL_NS_PER_SECOND) {
        state.fps = state.frames;
        state.frames = 0;
        state.last_second += SDL_NS_PER_SECOND;
    }
    state.frames++;
}

void gold_set_mode_match(RawMap* raw_map, int lcg_seed) {
    state.match_shell = match_shell_init_match(raw_map, lcg_seed);
    state.mode = GAME_MODE_MATCH;

    if (state.launch_mode == LAUNCH_MODE_TEST_HOST || state.launch_mode == LAUNCH_MODE_TEST_JOIN) {
        gold_test_mode_init();
    }
}

void gold_set_mode_scenario() {
    char scenario_path[256];
    char script_path[256];
    Scenario* scenario;

    // Init network
    network_set_backend(NETWORK_BACKEND_LAN);
    network_open_lobby("Campaign Scenario", NETWORK_LOBBY_PRIVACY_SINGLEPLAYER);

    // Set scenario and script path
    uint32_t selected_scenario = state.menu_state->campaign_mission_selected + 1;
    sprintf(scenario_path, "%sscenario%u/scenario%u.scn", filesystem_get_scenario_path().c_str(), selected_scenario, selected_scenario);
    sprintf(script_path, "%sscenario%u/scenario%u.lua", filesystem_get_scenario_path().c_str(), selected_scenario, selected_scenario);

    // Load scenario
    scenario = scenario_import(scenario_path);
    if (!scenario) {
        goto error;
    }

    // Init match shell
    state.match_shell = match_shell_init_scenario(scenario, script_path);
    scenario_free(scenario);
    if (state.match_shell == nullptr || state.match_shell->mode == MATCH_SHELL_MODE_LEAVE_MATCH) {
        goto error;
    }

    // Add bots
    for (uint8_t player_id = 1; player_id < MAX_PLAYERS; player_id++) {
        if (state.match_shell->match_state.players[player_id].mode == PLAYER_MODE_ACTIVE) {
            network_add_bot();
        }
    }

    state.mode = GAME_MODE_MATCH;
    return;

error:
    network_disconnect();
    menu_show_status(state.menu_state, "Failed to begin mission.");
    menu_set_mode(state.menu_state, MENU_MODE_CAMPAIGN);
}

void gold_set_mode_replay() {
    const char* replay_path = menu_get_selected_replay_filename(state.menu_state);
    state.match_shell = match_shell_init_replay(replay_path);
    if (state.match_shell == nullptr) {
        menu_set_mode(state.menu_state, MENU_MODE_REPLAYS);
        menu_show_status(state.menu_state, "Error opening replay file.");
        return;
    }
    state.mode = GAME_MODE_MATCH;
}

void gold_leave_match() {
    log_info("Leaving match mode.");

    // Save the shell mode so that we can still access it after freeing the shell
    MatchShellMode shell_mode = state.match_shell->mode;
    uint32_t shell_seconds_elapsed = state.match_shell->match_timer / UPDATES_PER_SECOND;

    // Free the shell
    sound_stop_all();
    match_shell_free(state.match_shell);
    state.match_shell = nullptr;

    // Return to editor
    if (state.launch_mode == LAUNCH_MODE_EDITOR) {
        editor_end_playtest();
        state.mode = GAME_MODE_EDITOR;
        return;
    }

    // Restart scenario
    if (shell_mode == MATCH_SHELL_MODE_LEAVE_SCENARIO_RESTART) {
        gold_set_mode_scenario();
        return;
    }

    // Return to menu
    state.mode = GAME_MODE_MENU;
    state.menu_state->menu_animation = menu_animation_init();

    // Return to campaign menu
    if (state.menu_state->mode == MENU_MODE_LOAD_SCENARIO) {
        bool victory = shell_mode == MATCH_SHELL_MODE_LEAVE_SCENARIO_VICTORY;
        menu_campaign_on_scenario_finished(state.menu_state, shell_seconds_elapsed, victory);
        return;
    }

    menu_set_mode(state.menu_state, MENU_MODE_MAIN);
}

RawMap* gold_generate_map(int* lcg_seed) {
    const uint32_t MAP_GEN_MAX_ATTEMPTS = 3U;

    RawMap* raw_map = NULL;
    MapSize map_size = (MapSize)network_get_match_setting(MATCH_SETTING_MAP_SIZE);
    MapType map_type = (MapType)network_get_match_setting(MATCH_SETTING_MAP_TYPE);
    int map_tile_size = map_get_tile_size(map_size);

    for (uint32_t attempts = 0; attempts < MAP_GEN_MAX_ATTEMPTS; attempts++) {
        log_debug("Map gen attempt %u / %u", attempts, MAP_GEN_MAX_ATTEMPTS);

        // Generate LCG seed
        *lcg_seed = (int)time(NULL);
        env_get_rand_seed_override(lcg_seed);

        raw_map = raw_map_generate(map_type, map_tile_size, map_tile_size, *lcg_seed, MAP_GEN_OPTION_SAVE_GOLDMINES | MAP_GEN_OPTION_GENERATE_DECORATIONS);
        if (!raw_map_is_valid(raw_map)) {
            free(raw_map);
            raw_map = NULL;
        } else {
            break;
        }
    }

    if (raw_map != NULL) {
        log_info("Map generation successful.");
    } else {
        log_error("Map generation reached max attempts.");
    }

    return raw_map;
}

// STEAM

#ifdef GOLD_STEAM

bool gold_steam_restart_app_if_necessary() {
    return SteamAPI_RestartAppIfNecessary(GOLD_STEAM_APP_ID));
}

bool gold_steam_api_init() {
    if (!SteamAPI_Init()) {
        log_error("Error initializing Steam API.");
        return false;
    }
    log_info("Initialized Steam API. App ID %u. User logged on? %i", SteamUtils()->GetAppID(), (int)SteamUser()->BLoggedOn());
    return true;
}

void gold_steam_api_shutdown() {
    SteamAPI_Shutdown();
}

void gold_steam_run_callbacks() {
    SteamAPI_RunCallbacks();
}

#else

bool gold_steam_restart_app_if_necessary() {
    return false;
}

bool gold_steam_api_init() {
    return true;
}

void gold_steam_api_shutdown() {}
void gold_steam_run_callbacks() {}

#endif

// DEBUG

#ifdef GOLD_DEBUG

void gold_debug_handle_input() {
    if (input_is_action_just_pressed(INPUT_ACTION_F3)) {
        state.debug_should_render_info = !state.debug_should_render_info;
    }
    if (input_is_action_just_pressed(INPUT_ACTION_TURBO)) {
        state.playback_speed = state.playback_speed == 1 ? 4 : 1;
    }
}

void gold_debug_render_info() {
    if (!state.debug_should_render_info) {
        return;
    }

    int render_y = 0;
    char debug_text[256];
    sprintf(debug_text, "FPS: %u", state.fps);
    render_text(FONT_HACK_WHITE, debug_text, ivec2(0, render_y));
    render_y += 10;

    if (state.mode == GAME_MODE_MATCH) {
        sprintf(debug_text, "Paused ? %i", (int)state.match_shell->is_paused);
        render_text(FONT_HACK_WHITE, debug_text, ivec2(0, render_y));
        render_y += 10;
    }

    if (state.mode == GAME_MODE_MATCH && !match_shell_is_mouse_in_ui()) {
        ivec2 cell = (input_get_mouse_position() + state.match_shell->camera_offset) / TILE_SIZE;
        Tile tile = map_get_tile(state.match_shell->match_state.map, cell);
        sprintf(debug_text, "Cell <%i, %i> Elevation %u Tile <%u, %u> Region %i Minimap Pixel %u", cell.x, cell.y, tile.elevation, tile.frame_x, tile.frame_y, map_get_region(state.match_shell->match_state.map, cell), match_shell_get_minimap_pixel_for_cell(state.match_shell, cell));
        render_text(FONT_HACK_WHITE, debug_text, ivec2(0, render_y));
        render_y += 10;

        render_draw_rect((Rect) {
            .x = (cell.x * TILE_SIZE) - state.match_shell->camera_offset.x,
            .y = (cell.y * TILE_SIZE) - state.match_shell->camera_offset.y,
            .w = TILE_SIZE,
            .h = TILE_SIZE
        }, RENDER_COLOR_WHITE);

        Cell map_cell = map_get_cell(state.match_shell->match_state.map, CELL_LAYER_GROUND, cell);
        if (map_cell.type == CELL_UNIT || map_cell.type == CELL_BUILDING || map_cell.type == CELL_MINER || map_cell.type == CELL_GOLDMINE) {
            const Entity& entity = state.match_shell->match_state.entities.get_by_id(map_cell.id);
            ivec2 target_cell = entity_is_target_invalid(state.match_shell->match_state, entity) ? ivec2(-1, -1) : entity_get_target_cell(state.match_shell->match_state, entity);
            sprintf(debug_text, "Entity %u %s mode %u target %u cell <%i, %i> is mining %i goldmine id %u pathfind attempts %u", map_cell.id, entity_get_data(entity.type).name, entity.mode, entity.target.type, target_cell.x, target_cell.y, (int)entity_is_mining(state.match_shell->match_state, entity), entity.goldmine_id, entity.pathfind_attempts);
            render_text(FONT_HACK_WHITE, debug_text, ivec2(0, render_y));
            render_y += 10;
        }
    }
}

void gold_debug_begin_editor_playtest() {
    // Setup network
    network_set_backend(NETWORK_BACKEND_LAN);
    network_open_lobby("Test Game", NETWORK_LOBBY_PRIVACY_SINGLEPLAYER);
    network_set_username("Player");

    // Init shell
    state.match_shell = match_shell_init_scenario(editor_get_scenario(), editor_get_scenario_script_path().c_str());
    if (state.match_shell == nullptr) {
        network_disconnect();
        editor_end_playtest();
        return;
    }

    // Add bots
    if (state.match_shell->mode != MATCH_SHELL_MODE_LEAVE_MATCH) {
        for (uint8_t player_id = 1; player_id < MAX_PLAYERS; player_id++) {
            if (state.match_shell->match_state.players[player_id].mode == PLAYER_MODE_ACTIVE) {
                network_add_bot();
            }
        }
    }

    state.mode = GAME_MODE_MATCH;
}

#else

void gold_debug_read_env() {}
void gold_debug_get_rand_seed_override(int* /*lcg_seed*/) {}
void gold_debug_handle_input() {}
void gold_debug_render_info() {}
bool gold_debug_init_desync(const char* /*desync_foldername*/) { return true; }
void gold_debug_begin_editor_playtest();

#endif

// PROFILE

#ifdef TRACY_ENABLE

void gold_debug_render_profiler_info() {
    char fps_text[128];
    sprintf(fps_text, "Tracy Enabled | FPS: %u", state.fps);
    render_text(FONT_HACK_WHITE, fps_text, ivec2(0, 0));
}

#else

void gold_debug_render_profiler_info() {}

#endif

// TEST MODE

#ifdef GOLD_DEBUG

void gold_test_mode_init() {
    log_debug("Initializing test mode bot.");

    int test_seed = rand();
    BotConfig bot_config = bot_config_init_from_difficulty(DIFFICULTY_HARD);
    bot_config.opener = BOT_OPENER_TECH_FIRST;
    bot_config.preferred_unit_comp = env_get().test_mode_unit_comp == BOT_UNIT_COMP_NONE
        ? bot_config_roll_preferred_unit_comp(&test_seed)
        : env_get().test_mode_unit_comp;

    state.test_bot = bot_init(state.match_shell->match_state, network_get_player_id(), bot_config);
}

void gold_test_mode_update() {
    if (state.mode == GAME_MODE_MENU) {
        switch (state.menu_state->mode) {
            case MENU_MODE_USERNAME: {
                state.menu_state->username = state.launch_mode == LAUNCH_MODE_TEST_HOST
                    ? "Burr"
                    : "Hamilton";
                network_set_username(state.menu_state->username.c_str());
                menu_set_mode(state.menu_state, MENU_MODE_MAIN);
                break;
            }
            case MENU_MODE_MAIN: {
                #ifdef GOLD_STEAM
                    menu_set_mode_lobbylist(state.menu_state, NETWORK_BACKEND_STEAM);
                #else
                    menu_set_mode_lobbylist(state.menu_state, NETWORK_BACKEND_LAN);
                #endif
                break;
            }
            case MENU_MODE_LOBBYLIST: {
                if (state.launch_mode == LAUNCH_MODE_TEST_HOST) {
                    menu_set_mode(state.menu_state, MENU_MODE_CREATE_LOBBY);
                } else if (state.launch_mode == LAUNCH_MODE_TEST_JOIN) {
                    if (state.menu_state->lobbies.empty()) {
                        network_search_lobbies("");
                    } else {
                        network_join_lobby(state.menu_state->lobbies[0].connection_info);
                        menu_set_mode(state.menu_state, MENU_MODE_CONNECTING);
                    }
                }
                break;
            }
            case MENU_MODE_CREATE_LOBBY: {
                network_open_lobby(state.menu_state->lobby_name.c_str(), (NetworkLobbyPrivacy)state.menu_state->lobby_privacy);
                menu_set_mode(state.menu_state, MENU_MODE_CONNECTING);
                break;
            }
            case MENU_MODE_LOBBY: {
                if (state.launch_mode == LAUNCH_MODE_TEST_HOST) {
                    if (network_get_match_setting((uint8_t)MATCH_SETTING_MAP_SIZE) != MAP_SIZE_MEDIUM) {
                        network_set_match_setting((uint8_t)MATCH_SETTING_MAP_SIZE, (uint8_t)MAP_SIZE_MEDIUM);
                        break;
                    }
                    if (network_get_match_setting((uint8_t)MATCH_SETTING_TEAMS) != TEAMS_ENABLED) {
                        network_set_match_setting((uint8_t)MATCH_SETTING_TEAMS, (uint8_t)TEAMS_ENABLED);
                        break;
                    }
                    if (network_get_match_setting((uint8_t)MATCH_SETTING_DIFFICULTY) != DIFFICULTY_HARD) {
                        network_set_match_setting((uint8_t)MATCH_SETTING_DIFFICULTY, (uint8_t)DIFFICULTY_HARD);
                        break;
                    }
                    if (network_get_player_count() == 1) {
                        // wait for player 2
                        break;
                    }
                    if (network_get_player_count() < MAX_PLAYERS) {
                        network_add_bot();
                        break;
                    }
                    if (network_get_player(2).team != 0) {
                        network_set_player_team(2, 0);
                        break;
                    }
                    if (network_get_player(3).team != 1) {
                        network_set_player_team(3, 1);
                        break;
                    }
                    if (network_get_player(1).status == NETWORK_PLAYER_STATUS_READY) {
                        menu_set_mode(state.menu_state, MENU_MODE_LOAD_MATCH);
                    }
                } else if (state.launch_mode == LAUNCH_MODE_TEST_JOIN) {
                    if (network_get_player(network_get_player_id()).team == 0 &&
                            network_get_match_setting((uint8_t)MATCH_SETTING_TEAMS) == TEAMS_ENABLED) {
                        network_set_player_team(network_get_player_id(), 1);
                        break;
                    }
                    if (network_get_player(network_get_player_id()).status == NETWORK_PLAYER_STATUS_NOT_READY) {
                        network_set_player_ready(true);
                    }
                }
                break;
            }
            default:
                break;
        }
    } else if (state.mode == GAME_MODE_MATCH) {
        if (match_shell_is_in_leave_match_mode(state.match_shell)) {
            return;
        }

        if (state.match_shell->mode == MATCH_SHELL_MODE_MATCH_OVER_VICTORY ||
                state.match_shell->mode == MATCH_SHELL_MODE_MATCH_OVER_DEFEAT) {
            match_shell_leave_match(state.match_shell, MATCH_SHELL_MODE_LEAVE_MATCH);
        } else if (state.match_shell->match_timer % TURN_DURATION == 0 &&
                state.match_shell->match_state.players[network_get_player_id()].mode == PLAYER_MODE_ACTIVE &&
                state.match_shell->input_queue.empty()) {
            uint32_t turn_number = state.match_shell->match_timer / TURN_DURATION;
            if (turn_number % TURN_OFFSET == 0) {
                MatchInput input;
                input = bot_get_turn_input(state.match_shell->match_state, state.test_bot, state.match_shell->match_timer);
                state.match_shell->input_queue.push_back(input);
            }

            // Check for surrender
            if (bot_should_surrender(state.match_shell->match_state, state.test_bot, state.match_shell->match_timer)) {
                network_send_chat("gg");
                match_shell_leave_match(state.match_shell, MATCH_SHELL_MODE_LEAVE_MATCH);
            }
        }
    }
}

#else

void gold_test_mode_init() {}
void gold_test_mode_update() {}

#endif
