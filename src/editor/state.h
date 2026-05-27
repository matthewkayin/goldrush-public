#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "match/scenario/scenario.h"
#include "match/state/map.h"
#include "core/ui.h"
#include "render/ysort.h"
#include "editor/action.h"
#include "editor/menu/interface.h"
#include <SDL3/SDL.h>
#include <string>

const Rect TOOLBAR_RECT = (Rect) {
    .x = 0, .y = 0,
    .w = SCREEN_WIDTH, .h = 22
};
const Rect STATUS_RECT = (Rect) {
    .x = 0, .y = SCREEN_HEIGHT - 18,
    .w = SCREEN_WIDTH, .h = 18
};
const Rect SIDEBAR_RECT = (Rect) {
    .x = 0, .y = TOOLBAR_RECT.h,
    .w = 144,
    .h = SCREEN_HEIGHT - TOOLBAR_RECT.h - STATUS_RECT.h
};
const Rect CANVAS_RECT = (Rect) {
    .x = SIDEBAR_RECT.w,
    .y = TOOLBAR_RECT.y + TOOLBAR_RECT.h,
    .w = SCREEN_WIDTH - SIDEBAR_RECT.w,
    .h = SCREEN_HEIGHT - STATUS_RECT.h - TOOLBAR_RECT.h
};
const Rect MINIMAP_RECT = (Rect) {
    .x = 8, .y = SCREEN_HEIGHT - 132 - 22,
    .w = 128, .h = 128
};

enum EditorToolType {
    EDITOR_TOOL_BRUSH,
    EDITOR_TOOL_FILL,
    EDITOR_TOOL_RECT,
    EDITOR_TOOL_SELECT,
    EDITOR_TOOL_DECORATE,
    EDITOR_TOOL_ADD_ENTITY,
    EDITOR_TOOL_EDIT_ENTITY,
    EDITOR_TOOL_SQUADS,
    EDITOR_TOOL_PLAYER_SPAWN,
    EDITOR_TOOL_CONSTANTS
};

struct EditorToolBrush {
    uint32_t value;
    bool is_painting;
};

struct EditorToolFill {
    uint32_t value;
};

struct EditorToolRect {
    uint32_t value;
    bool is_painting;
    ivec2 origin;
    ivec2 end;
};

struct EditorToolSelect {
    ivec2 origin;
    ivec2 end;
    bool is_selecting;
    bool is_pasting;
};

struct EditorToolAddEntity {
    EntityType entity_type;
    uint32_t player_id;
    int scroll_offset;
};

struct EditorToolEditEntity {
    uint32_t entity_index;
    uint32_t gold_held;
    uint32_t gold_held_slider_scale;
    ivec2 drag_offset;
    bool is_dragging;
};

struct EditorToolSquads {
    uint32_t squad_index;
    int scroll_offset;
};

struct EditorToolConstants {
    uint32_t constant_index;
    int scroll_offset;
};

struct EditorTool {
    EditorToolType type;
    union {
        EditorToolBrush brush;
        EditorToolFill fill;
        EditorToolRect rect;
        EditorToolSelect select;
        EditorToolAddEntity add_entity;
        EditorToolEditEntity edit_entity;
        EditorToolSquads squads;
        EditorToolConstants constants;
    };
};

struct EditorClipboard {
    int width;
    int height;
    std::vector<uint8_t> values;
};

struct EditorState {
    // Window
    SDL_Window* window;
    bool is_requesting_playtest;

    // Document
    Scenario* scenario;
    std::string scenario_path;
    bool scenario_is_saved;

    // UI
    Map map;
    IEditorMenu* menu;
    UiContext ui_context;
    int ui_toolbar_id;
    bool is_in_file_menu;

    // Tools
    EditorTool tool;
    std::vector<EditorActionBrushStroke> tool_brush_stroke;

    // Actions
    size_t action_head;
    std::vector<EditorAction> actions;

    // Camera
    ivec2 camera_offset;
    ivec2 camera_drag_previous_offset;
    ivec2 camera_drag_mouse_position;
    bool is_minimap_dragging;

    // Clipboard
    EditorClipboard clipboard;
};

// Init
EditorState* editor_state_init(SDL_Window* window);
void editor_state_free_document(EditorState* state);
void editor_state_free(EditorState* state);
void editor_state_update_map(EditorState* state);

// Update
void editor_state_update(EditorState* state);
void editor_state_update_toolbar(EditorState* state);
void editor_state_handle_toolbar_action(EditorState* state, const std::string& column, const std::string& action);
bool editor_state_is_idle(const EditorState* state);
void editor_state_update_status_bar(EditorState* state);
const char* editor_state_get_raw_map_value_str(uint8_t value);
void editor_state_begin_playtest(EditorState* state);
void editor_state_end_playtest(EditorState* state);

// Sidebar
void editor_state_update_sidebar(EditorState* state);
void editor_state_clear_decorations(EditorState* state);
void editor_state_generate_decorations(EditorState* state);
void editor_state_sidebar_add_entity(EditorState* state);
void editor_state_sidebar_edit_entity(EditorState* state);
void editor_state_sidebar_squads(EditorState* state);
void editor_state_sidebar_constants(EditorState* state);
std::vector<std::string> editor_state_get_player_name_dropdown_items(const EditorState* state);

// Camera
void editor_state_update_camera(EditorState* state);
bool editor_state_is_camera_dragging(const EditorState* state);
void editor_state_clamp_camera(EditorState* state);
void editor_state_center_camera_on_cell(EditorState* state, ivec2 cell);

// Tool
void editor_state_set_tool(EditorState* state, EditorToolType type);
void editor_state_update_tool(EditorState* state);
void editor_state_update_tool_brush(EditorState* state);
bool editor_state_tool_brush_should_paint(const EditorState* state);
void editor_state_update_tool_fill(EditorState* state);
void editor_state_update_tool_rect(EditorState* state);
void editor_state_update_tool_select(EditorState* state);
void editor_state_update_tool_decorate(EditorState* state);
uint8_t editor_state_tool_decorate_toggle_value(uint8_t previous_value);
void editor_state_update_tool_add_entity(EditorState* state);
void editor_state_update_tool_edit_entity(EditorState* state);
void editor_state_update_tool_squads(EditorState* state);
void editor_state_update_tool_player_spawn(EditorState* state);
void editor_state_update_tool_constants(EditorState* state);

// Tool helpers
void editor_state_ensure_tool_value_is_in_bounds(EditorState* state);
uint32_t editor_state_get_tool_map_value(const EditorState* state);
bool editor_state_is_tool_active(const EditorState* state);
ivec2 editor_state_canvas_to_world_space(const EditorState* state, ivec2 point);
Rect editor_state_cell_rect_to_world_space(const EditorState* state, const Rect& rect);
ivec2 editor_state_get_hovered_cell(const EditorState* state);
uint32_t editor_state_get_hovered_entity(const EditorState* state);
bool editor_state_is_hovered_cell_valid(const EditorState* state);
bool editor_state_can_tool_be_used(const EditorState* state);
std::vector<uint32_t> editor_state_get_selected_entities(const EditorState* state);
bool editor_state_is_dragging_entity(const EditorState* state, uint32_t entity_index);
uint32_t editor_state_get_entity_squad(const EditorState* state, uint32_t entity_index);
Rect editor_state_tool_rect_get_rect(const EditorState* state);
bool editor_state_tool_select_has_select_rect(const EditorState* state);
Rect editor_state_tool_select_get_select_rect(const EditorState* state);
void editor_state_tool_select_clear_selection(EditorState* state);
void editor_state_flatten_rect(EditorState* state, const Rect& rect);
void editor_state_tool_edit_entity_delete_entity(EditorState* state, uint32_t entity_index);
void editor_state_remove_entity_from_squad(EditorState* state, uint32_t squad_index, uint32_t entity_index);

// Clipboard
void editor_state_clipboard_clear(EditorState* state);
bool editor_state_clipboard_is_empty(EditorState* state);
void editor_state_clipboard_copy(EditorState* state);
void editor_state_clipboard_paste(EditorState* state, ivec2 top_left_cell);

// Actions
void editor_state_push_action(EditorState* state, const EditorAction& action);
void editor_state_do_action(EditorState* state, const EditorAction& action);
void editor_state_undo_action(EditorState* state);
void editor_state_redo_action(EditorState* state);
void editor_state_clear_actions(EditorState* state);

// Menu
bool editor_state_is_in_menu(const EditorState* state);
bool editor_state_is_toolbar_open(const EditorState* state);

// File
void editor_state_open_file_save_dialog(EditorState* state);
void editor_state_open_file_open_dialog(EditorState* state);
void editor_state_save_document(EditorState* state, const char* path);
void editor_state_open_file_import_dialog(EditorState* state);
void editor_state_open_file_export_dialog(EditorState* state);

// Render
void editor_state_render(const EditorState* state);
void editor_state_render_scenario(const EditorState* state);
RenderSpriteParams editor_state_create_entity_render_params(const EditorState* state, const ScenarioEntity& entity);
ivec2 editor_state_get_entity_animation_frame(EntityType type);
ivec2 editor_state_get_entity_render_position(const EditorState* state, EntityType type, ivec2 cell);
MinimapPixel editor_state_get_minimap_pixel_for_tile_sprite(SpriteName sprite);
MinimapPixel editor_state_get_minimap_pixel_for_cell(const EditorState* state, ivec2 cell);
MinimapPixel editor_state_get_minimap_pixel_for_entity(const EditorState* state, const std::vector<uint32_t>& selection, uint32_t entity_index);
bool editor_state_should_render_mouse_hover_rect(const EditorState* state);
void editor_state_render_mouse_hover_rect(const EditorState* state);
bool editor_state_should_render_entity_preview(const EditorState* state);
void editor_state_render_entity_preview(const EditorState* state);
bool editor_state_should_render_constant_cell_value(const EditorState* state);
bool editor_state_should_render_paste_preview(const EditorState* state);
void editor_state_render_paste_preview(const EditorState* state);
SpriteName editor_state_get_raw_map_preview_sprite(const EditorState* state, uint8_t value);
bool editor_state_should_render_squad_patrol_cell(const EditorState* state);
std::vector<ivec2> editor_state_get_entity_vision_cells(const EditorState* state, ivec2 cell, int cell_size, int sight);
void editor_state_render_camera_rect(const EditorState* state, ivec2 camera_cell);
ivec2 editor_state_get_player_spawn_camera_offset(const EditorState* state, ivec2 cell);
bool editor_state_should_render_tool_constants_camera_preview_rect(const EditorState* state);

#endif
