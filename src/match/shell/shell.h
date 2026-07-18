#pragma once

#include "core/input.h"
#include "core/animation.h"
#include "core/ui.h"
#include "shared/options_menu.h"
#include "shared/hotkey.h"
#include "match/state/match.h"
#include "match/state/input.h"
#include "match/bot/bot.h"
#include "render/render.h"
#include "render/ysort.h"
#include "network/types.h"
#include "match/scenario/scenario.h"
#include "match/shell/achievements.h"
#include <luajit/lua.hpp>
#include <vector>
#include <queue>

#define MATCH_SHELL_CONTROL_GROUP_COUNT 10U
#define MATCH_SHELL_CONTROL_GROUP_NONE UINT32_MAX
#define MATCH_SHELL_CAMERA_HOTKEY_COUNT 6U

#define GLOBAL_OBJECTIVE_COUNTER_HEADER_TEXT_BUFFER_SIZE 32

#define MATCH_SHELL_CHAT_PREFIX_BUFFER_SIZE (MAX_USERNAME_LENGTH + 4)
#define MATCH_SHELL_CHAT_MESSAGE_BUFFER_SIZE NETWORK_CHAT_BUFFER_SIZE

// Rects
const Rect SCREEN_RECT = (Rect) { .x = 0, .y = 0, .w = SCREEN_WIDTH, .h = SCREEN_HEIGHT };
const Rect MINIMAP_RECT = (Rect) { .x = 4, .y = SCREEN_HEIGHT - 132, .w = 128, .h = 128 };

// UI
const Rect BOTTOM_PANEL_RECT = (Rect) {
    .x = 136, .y = SCREEN_HEIGHT - MATCH_SHELL_UI_HEIGHT,
    .w = 372, .h = MATCH_SHELL_UI_HEIGHT
};
const Rect BUTTON_PANEL_RECT = (Rect) {
    .x = BOTTOM_PANEL_RECT.x + BOTTOM_PANEL_RECT.w,
    .y = SCREEN_HEIGHT - 106,
    .w = 132, .h = 106
};
const Rect REPLAY_PANEL_RECT = (Rect) {
    .x = BOTTOM_PANEL_RECT.x + BOTTOM_PANEL_RECT.w,
    .y = SCREEN_HEIGHT - 116,
    .w = 132, .h = 116
};
const ivec2 MENU_BUTTON_POSITION = ivec2(1, 1);

// Wanted sign position
const ivec2 WANTED_SIGN_POSITION = ivec2(BUTTON_PANEL_RECT.x + 31, BUTTON_PANEL_RECT.y + 9);

// Hotkey button positions
const int HOTKEY_BUTTON_X = SCREEN_WIDTH - 132 + 14;
const int HOTKEY_BUTTON_Y = SCREEN_HEIGHT - MATCH_SHELL_UI_HEIGHT + 10;
const int HOTKEY_BUTTON_PADDING_X = 32 + 4;
const int HOTKEY_BUTTON_PADDING_Y = 32 + 6;
const ivec2 HOTKEY_BUTTON_POSITIONS[HOTKEY_GROUP_SIZE] = {
    ivec2(HOTKEY_BUTTON_X, HOTKEY_BUTTON_Y),
    ivec2(HOTKEY_BUTTON_X + HOTKEY_BUTTON_PADDING_X, HOTKEY_BUTTON_Y),
    ivec2(HOTKEY_BUTTON_X + (2 * HOTKEY_BUTTON_PADDING_X), HOTKEY_BUTTON_Y),
    ivec2(HOTKEY_BUTTON_X, HOTKEY_BUTTON_Y + HOTKEY_BUTTON_PADDING_Y),
    ivec2(HOTKEY_BUTTON_X + HOTKEY_BUTTON_PADDING_X, HOTKEY_BUTTON_Y + HOTKEY_BUTTON_PADDING_Y),
    ivec2(HOTKEY_BUTTON_X + (2 * HOTKEY_BUTTON_PADDING_X), HOTKEY_BUTTON_Y + HOTKEY_BUTTON_PADDING_Y)
};

// Bottom panel positions
static const ivec2 SELECTION_LIST_TOP_LEFT = ivec2(164, 284);
static const ivec2 BUILDING_QUEUE_TOP_LEFT = ivec2(320, 284);
static const ivec2 BUILDING_QUEUE_POSITIONS[BUILDING_QUEUE_MAX] = {
    BUILDING_QUEUE_TOP_LEFT,
    BUILDING_QUEUE_TOP_LEFT + ivec2(0, 35),
    BUILDING_QUEUE_TOP_LEFT + ivec2(36, 35),
    BUILDING_QUEUE_TOP_LEFT + ivec2(36 * 2, 35),
    BUILDING_QUEUE_TOP_LEFT + ivec2(36 * 3, 35)
};
static const Rect BUILDING_QUEUE_PROGRESS_BAR_RECT = (Rect) {
    .x = 320 + 36,
    .y = 284 + 24,
    .w = 104, .h = 6
};
static const ivec2 GARRISON_ICON_TOP_LEFT = ivec2(320, 284 + 18);
static const ivec2 GARRISON_ICON_POSITIONS[4] = {
    GARRISON_ICON_TOP_LEFT,
    GARRISON_ICON_TOP_LEFT + ivec2(36, 0),
    GARRISON_ICON_TOP_LEFT + ivec2(36 * 2, 0),
    GARRISON_ICON_TOP_LEFT + ivec2(36 * 3, 0)
};

// Timing
const uint32_t TURN_OFFSET = 4;
const uint32_t TURN_DURATION = 4;
const uint32_t DISCONNECT_GRACE = 10;

// Chat
const uint32_t CHAT_MESSAGE_DURATION = 3U * 60U;
const uint32_t CHAT_MESSAGE_HINT_DURATION = 5U * 60U;
const uint32_t CHAT_MAX_LINES = 8U;

// Alerts
const uint32_t ALERT_DURATION = 90;
const uint32_t ALERT_LINGER_DURATION = 60 * 20;
const uint32_t ALERT_TOTAL_DURATION = ALERT_DURATION + ALERT_LINGER_DURATION;

// Replay
const uint32_t REPLAY_FOG_NONE = 0U;
const uint32_t REPLAY_FOG_EVERYONE = 1U;

// Music
const uint32_t MATCH_SHELL_MUSIC_TRACK_COUNT = 5U;

enum MatchShellMode {
    MATCH_SHELL_MODE_NOT_STARTED,
    MATCH_SHELL_MODE_NONE,
    MATCH_SHELL_MODE_BUILD,
    MATCH_SHELL_MODE_BUILD2,
    MATCH_SHELL_MODE_BUILDING_PLACE,
    MATCH_SHELL_MODE_TARGET_ATTACK,
    MATCH_SHELL_MODE_TARGET_UNLOAD,
    MATCH_SHELL_MODE_TARGET_REPAIR,
    MATCH_SHELL_MODE_TARGET_MOLOTOV,
    MATCH_SHELL_MODE_CHAT,
    MATCH_SHELL_MODE_MENU,
    MATCH_SHELL_MODE_MENU_SURRENDER,
    MATCH_SHELL_MODE_MENU_SURRENDER_TO_DESKTOP,
    MATCH_SHELL_MODE_MENU_SURRENDER_RESTART,
    MATCH_SHELL_MODE_MATCH_OVER_VICTORY,
    MATCH_SHELL_MODE_MATCH_OVER_DEFEAT,
    MATCH_SHELL_MODE_SCENARIO_VICTORY,
    MATCH_SHELL_MODE_SCENARIO_DEFEAT,
    MATCH_SHELL_MODE_LEAVE_MATCH,
    MATCH_SHELL_MODE_LEAVE_SCENARIO_VICTORY,
    MATCH_SHELL_MODE_LEAVE_SCENARIO_DEFEAT,
    MATCH_SHELL_MODE_LEAVE_SCENARIO_RESTART,
    MATCH_SHELL_MODE_EXIT_PROGRAM,
    MATCH_SHELL_MODE_DESYNC
};

enum MatchShellSelectionType {
    MATCH_SHELL_SELECTION_NONE,
    MATCH_SHELL_SELECTION_UNITS,
    MATCH_SHELL_SELECTION_ENEMY_UNIT,
    MATCH_SHELL_SELECTION_BUILDINGS,
    MATCH_SHELL_SELECTION_ENEMY_BUILDING,
    MATCH_SHELL_SELECTION_GOLD
};

// Camera

enum CameraMode {
    CAMERA_MODE_FREE,
    CAMERA_MODE_MINIMAP_DRAG,
    CAMERA_MODE_PAN,
    CAMERA_MODE_HELD
};

struct CameraPanState {
};

struct CameraShakeState {
    ivec2 start_offset;
    int seed;
    uint32_t timer;
    uint32_t duration;
};

struct CameraState {
    CameraPanState pan;
    CameraShakeState shake;
};

// Alert

struct Alert {
    MinimapPixel pixel;
    ivec2 cell;
    int cell_size;
    uint32_t timer;
};

// Chat

struct ChatMessage {
    FontName prefix_font;
    uint32_t timer;
    char prefix[MATCH_SHELL_CHAT_PREFIX_BUFFER_SIZE];
    char message[MATCH_SHELL_CHAT_MESSAGE_BUFFER_SIZE];
};

// Replay

enum ReplayEntryType: uint8_t {
    REPLAY_ENTRY_INPUT,
    REPLAY_ENTRY_CHAT,
    REPLAY_ENTRY_DISCONNECT,
    REPLAY_ENTRY_NEW_TURN
};

struct ReplayEntry {
    ReplayEntryType type;
    union {
        MatchInput input;
        ChatMessage chat_message;
        uint8_t disconnect_player_id;
    };
};

struct ReplayChatMessage {
    uint32_t turn;
    ChatMessage chat;
};

// Objective

enum ObjectiveCounterType {
    OBJECTIVE_COUNTER_TYPE_NONE,
    OBJECTIVE_COUNTER_TYPE_VARIABLE,
    OBJECTIVE_COUNTER_TYPE_ENTITY
};

struct Objective {
    std::string description;
    bool is_complete;
    ObjectiveCounterType counter_type;
    uint32_t counter_value;
    uint32_t counter_target;
};

enum GlobalObjectiveCounterType {
    GLOBAL_OBJECTIVE_COUNTER_OFF,
    GLOBAL_OBJECTIVE_COUNTER_GOLD,
    GLOBAL_OBJECTIVE_COUNTER_COUNTDOWN,
    GLOBAL_OBJECTIVE_COUNTER_VARIABLE,
    GLOBAL_OBJECTIVE_COUNTER_TYPE_COUNT
};

struct GlobalObjectiveCounterGold {
    uint32_t values[MAX_PLAYERS];
    uint32_t max_value;
};

struct GlobalObjectiveCounterCountdown {
    char header_text[GLOBAL_OBJECTIVE_COUNTER_HEADER_TEXT_BUFFER_SIZE];
    uint32_t end_frame;
};

struct GlobalObjectiveCounterVariable {
    char header_text[GLOBAL_OBJECTIVE_COUNTER_HEADER_TEXT_BUFFER_SIZE];
    uint32_t value;
};

struct GlobalObjectiveCounter {
    GlobalObjectiveCounterType type;
    union {
        GlobalObjectiveCounterGold gold;
        GlobalObjectiveCounterCountdown countdown;
        GlobalObjectiveCounterVariable variable;
    };
};

// Scenario

struct EntityHighlight {
    Animation animation;
    EntityId entity_id;
};

struct AvalancheColumn {
    Animation animation;
    ivec2 position;
    ivec2 destination;
};

// Fog

enum MatchShellFogLevel {
    MATCH_SHELL_FOG_ENABLED,
    MATCH_SHELL_FOG_BOT_VISION,
    MATCH_SHELL_FOG_DISABLED
};

// Render

enum RenderHealthbarType {
    RENDER_HEALTHBAR,
    RENDER_GARRISON_BAR,
    RENDER_ENERGY_BAR
};

enum FireCellRender {
    FIRE_CELL_DO_NOT_RENDER,
    FIRE_CELL_RENDER_BELOW,
    FIRE_CELL_RENDER_ABOVE
};

struct MatchShell {
    MatchShellMode mode;
    UiContext ui_context;
    OptionsMenuState options_menu;
    uint32_t menu_help_page;

    // Simulation timers
    bool match_over_is_victory;
    bool is_paused;
    uint32_t match_timer;
    uint32_t disconnect_timer;
    uint32_t match_over_timer;

    // Match state and bots (synced state)
    MatchState match_state;
    Bot bots[MAX_PLAYERS];

    // Inputs
    std::queue<std::vector<MatchInput>> inputs[MAX_PLAYERS];
    std::vector<MatchInput> input_queue;

    // Camera
    CameraMode camera_mode;
    ivec2 camera_offset;
    ivec2 camera_pan_start_offset;
    ivec2 camera_pan_end_offset;
    uint32_t camera_pan_timer;
    uint32_t camera_pan_duration;
    int camera_shake_seed;
    ivec2 camera_hotkeys[MATCH_SHELL_CAMERA_HOTKEY_COUNT];

    // Selection
    ivec2 select_origin;
    uint32_t double_click_timer;
    uint32_t control_group_selected;
    uint32_t control_group_double_tap_timer;
    std::vector<EntityId> control_groups[MATCH_SHELL_CONTROL_GROUP_COUNT];
    std::vector<EntityId> selection;

    // Status
    std::string status_message;
    uint32_t status_timer;

    // For building placement
    InputHotkey hotkey_group[HOTKEY_GROUP_SIZE];
    EntityType building_type;

    // Chat
    std::string chat_message;
    CircularVector<ChatMessage, CHAT_MAX_LINES> chat;
    bool chat_cursor_visible;
    uint32_t chat_cursor_blink_timer;

    // Alerts
    std::vector<Alert> alerts;
    ivec2 latest_alert_cell;

    // Sound
    uint32_t sound_cooldown_timers[SOUND_COUNT];
    uint32_t sound_fire_voice_index;
    uint32_t music_begin_timer;
    uint32_t music_next_track;

    // Animations
    Animation rally_flag_animation;
    Animation move_animation;
    Animation building_fire_animation;
    ivec2 move_animation_position;
    EntityId move_animation_entity_id;
    std::vector<EntityHighlight> entity_highlights;

    // Gold amounts
    uint32_t displayed_gold_amounts[MAX_PLAYERS];

    // Scenario
    uint32_t scenario_allowed_upgrades;
    bool scenario_allowed_entities[ENTITY_TYPE_COUNT];
    lua_State* scenario_lua_state;
    std::vector<Objective> scenario_objectives;
    GlobalObjectiveCounter scenario_global_objective_counter;
    std::vector<AvalancheColumn> scenario_avalanche_columns;

    // Replay file (write)
    FILE* replay_file;

    // Replay data (read)
    bool replay_mode;
    UiContext replay_ui_context;
    std::vector<MatchState> replay_checkpoints;
    std::vector<std::vector<ReplayEntry>> replay_entries;

    // Replay fog
    uint32_t replay_fog_index;
    std::vector<std::string> replay_fog_texts;
    std::vector<uint8_t> replay_fog_player_ids;

    // Replay loading thread
    SDL_Thread* replay_loading_thread;
    MatchState replay_loading_match_state;
    uint32_t replay_loading_match_timer;

    SDL_Mutex* replay_loading_mutex;
    uint32_t replay_loaded_match_timer;

    SDL_Mutex* replay_loading_early_exit_mutex;
    bool replay_loading_early_exit;

    // Checksum
    uint32_t next_checksum_frame;
    std::queue<uint32_t> checksums[MAX_PLAYERS];

    // Achievements
    AchievementsTracker achievements_tracker;

    // Debug
    MatchShellFogLevel debug_fog_level;
    bool debug_show_region_lines;
};

// Init
MatchShell* match_shell_init_base();
MatchShell* match_shell_init_match(RawMap* raw_map, int32_t lcg_seed);
MatchShell* match_shell_init_scenario(const Scenario* scenario, const char* script_path);
MatchShell* match_shell_init_replay(const char* replay_path);
void match_shell_init_input_queues(MatchShell* shell);
void match_shell_free(MatchShell* shell);

// Network event
void match_shell_handle_network_event(MatchShell* shell, NetworkEvent event);

// Update
void match_shell_update(MatchShell* shell);
bool match_shell_begin_turn(MatchShell* shell);
void match_shell_handle_input(MatchShell* shell);

// Helpers
void match_shell_show_status(MatchShell* shell, const char* message);
void match_shell_order_move(MatchShell* shell);
bool match_shell_is_entity_an_enemy_of_player(const MatchShell* shell, EntityId entity_id);
bool match_shell_does_player_meet_hotkey_requirements(const MatchState& state, InputHotkey hotkey);
bool match_shell_is_hotkey_available(const MatchShell* shell, const HotkeyButtonInfo& info);
uint32_t match_shell_get_player_entity_count(const MatchShell* shell, uint8_t player_id, EntityType entity_type);
uint32_t match_shell_update_displayed_gold_amount(uint32_t current_displayed_value, uint32_t current_actual_value);
void match_shell_set_match_over_victory(MatchShell* shell);
void match_shell_set_match_over_defeat(MatchShell* shell);
void match_shell_leave_match(MatchShell* shell, MatchShellMode mode);

// State queries
bool match_shell_is_mouse_in_ui();
bool match_shell_is_selecting(const MatchShell* shell);
bool match_shell_is_in_menu(const MatchShell* shell);
bool match_shell_is_in_hotkey_submenu(const MatchShell* shell);
bool match_shell_is_targeting(const MatchShell* shell);
bool match_shell_is_in_leave_match_mode(const MatchShell* shell);
bool match_shell_is_at_least_one_opponent_in_match(const MatchShell* shell);
bool match_shell_is_in_single_player_game();
bool match_shell_is_surrender_required_to_leave(const MatchShell* shell);

// Camera
void match_shell_camera_update(MatchShell* shell);
void match_shell_clamp_camera(MatchShell* shell);
void match_shell_center_camera_on_cell(MatchShell* shell, ivec2 cell);
bool match_shell_is_camera_free(const MatchShell* shell);
void match_shell_begin_camera_pan(MatchShell* shell, ivec2 end_cell, double duration, bool shake);

// Selection
void match_shell_selection_update(MatchShell* shell);
std::vector<EntityId> match_shell_create_selection(const MatchShell* shell, Rect select_rect);
void match_shell_set_selection(MatchShell* shell, std::vector<EntityId>& selection);
MatchShellSelectionType match_shell_get_selection_type(const MatchShell* shell, const std::vector<EntityId>& selection);
bool match_shell_selection_has_enough_energy(const MatchShell* shell, const std::vector<EntityId>& selection, uint32_t cost);
bool match_shell_is_entity_player_controlled_and_in_goldmine(const MatchShell* shell, const Entity& entity);
bool match_shell_is_entity_selectable(const MatchShell* shell, const Entity& entity);
bool match_shell_can_keep_selecting_entity(const MatchShell* shell, const Entity& entity);

// Idle miners button
EntityList match_shell_find_idle_miners(const MatchShell* shell);
bool match_shell_is_idle_miner_button_pressed(const MatchShell* shell, const EntityList& idle_miners);
void match_shell_select_idle_miner(MatchShell* shell, const EntityList& idle_miners);

// Vision
bool match_shell_is_entity_visible(const MatchShell* shell, const Entity& entity);
bool match_shell_is_cell_rect_revealed(const MatchShell* shell, ivec2 cell, int cell_size);
int match_shell_get_fog(const MatchShell* shell, ivec2 cell);

// Chat
void match_shell_get_player_prefix(const MatchShell* shell, uint8_t player_id, char* prefix);
FontName match_shell_get_player_font(uint8_t player_id);
void match_shell_add_chat_message(MatchShell* shell, FontName prefix_font, const char* prefix, const char* message, uint32_t duration);
void match_shell_handle_player_disconnect(MatchShell* shell, uint8_t player_id);

// Building placement
ivec2 match_shell_get_building_cell(int building_size, ivec2 camera_offset);
bool match_shell_building_can_be_placed(const MatchShell* shell);
bool match_shell_is_building_place_cell_valid(const MatchShell* shell, ivec2 miner_cell, ivec2 cell);

// Hotkey Menu
Rect match_shell_get_selection_list_item_rect(uint32_t selection_index);
Rect match_shell_get_idle_miner_button_rect();
bool match_shell_has_pressed_idle_miner_button();

// Fire
bool match_shell_is_fire_on_screen(const MatchShell* shell);

// Menu
bool match_shell_menu_button_can_be_clicked(const MatchShell* shell);
bool match_shell_menu_button_is_clicked(const MatchShell* shell);
void match_shell_menu_update(MatchShell* shell);
void match_shell_begin_menu(MatchShell* shell, const char* header_text, uint32_t button_count);

// Debug
void match_shell_debug_handle_chat_message(MatchShell* shell, const std::string& message);
bool match_shell_debug_has_next_checksums(MatchShell* shell);
bool match_shell_debug_are_next_checksums_out_of_sync(MatchShell* shell);

// Replay file
void match_shell_replay_set_filename(const char* filename);
FILE* match_shell_replay_file_open(MatchPlayer players[MAX_PLAYERS], MapType map_type, const RawMap* raw_map, int lcg_seed);
void match_shell_replay_file_close(FILE* file);
void match_shell_replay_file_write_entry(FILE* file, const ReplayEntry& entry);
bool match_shell_replay_file_read(const char* path, MatchState& state, std::vector<std::vector<ReplayEntry>>* replay_entries);

// Replay UI
void match_shell_replay_ui_update(MatchShell* shell);

// Replay playback
void match_shell_replay_handle_entries_for_turn(const std::vector<std::vector<ReplayEntry>>& replay_entries, MatchState& match_state, CircularVector<ChatMessage, CHAT_MAX_LINES>* chat, uint32_t turn);
void match_shell_replay_scrub(MatchShell* state, uint32_t position);
size_t match_shell_replay_end_of_tape(const MatchShell* shell);

// Render
void match_shell_render(const MatchShell* shell);

// Render helpers
bool match_shell_should_render_remembered_entities_for_team(const MatchShell* shell, uint8_t team);
bool match_shell_use_yellow_rings(const MatchShell* shell);
SpriteName match_shell_get_entity_select_ring(EntityType type, bool attacking);
SpriteName match_shell_hotkey_get_sprite(const MatchShell* shell, InputHotkey hotkey, bool show_toggled);
void match_shell_render_healthbar(RenderHealthbarType type, ivec2 position, ivec2 size, int amount, int max);
void match_shell_render_target_build(const MatchShell* shell, const Target& target, uint8_t player_id);
RenderSpriteParams match_shell_create_entity_render_params(const MatchShell* shell, const Entity& entity);
void match_shell_render_entity_select_rings_and_healthbars(const MatchShell* shell, const Entity& entity);
void match_shell_render_entity_icon(const MatchShell* shell, const Entity& entity, Rect icon_rect);
void match_shell_render_entity_move_animation(const MatchShell* shell, const Entity& entity, Animation move_animation);
void match_shell_render_particle(const MatchShell* shell, const Particle& particle);
bool match_shell_should_render_hotkey_toggled(const MatchShell* shell, InputHotkey hotkey);
const char* match_shell_render_get_stat_tooltip(SpriteName sprite);
void match_shell_render_tooltip(const MatchShell* shell, InputHotkey hotkey);
ivec2 match_shell_get_queued_target_position(const MatchShell* shell, const Target& target);
FireCellRender match_shell_get_fire_cell_render(const MatchShell* shell, const Fire& fire);
MinimapPixel match_shell_get_minimap_pixel_for_cell(const MatchShell* shell, ivec2 cell);
MinimapPixel match_shell_get_minimap_pixel_for_entity(const MatchShell* shell, const Entity& entity);

// Debug render
void match_shell_debug_render_cell_region_lines(const MatchShell* shell, ivec2 base_coords, ivec2 base_pos, uint32_t render_elevation, ivec2 cell);
