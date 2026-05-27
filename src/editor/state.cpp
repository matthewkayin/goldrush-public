#include "state.h"

#include "defines.h"

#ifdef GOLD_DEBUG

#include "core/ui.h"
#include "editor/action.h"
#include "match/scenario/scenario.h"
#include "core/input.h"
#include "shared/match_setting.h"
#include "match/bot/bot.h"
#include "match/state/map.h"
#include "match/state/map_gen.h"
#include "editor/menu/new.h"
#include "editor/menu/constant.h"
#include "editor/menu/players.h"
#include "editor/menu/squad.h"
#include "editor/ui_helpers.h"
#include <algorithm>

static const uint32_t EDITOR_ENTITY_HAS_NO_SQUAD = UINT32_MAX;

const std::vector<std::vector<std::string>> TOOLBAR_OPTIONS = {
    { "File", "New", "Open", "Save", "Save As", "Export", "Import" },
    { "Edit", "Undo", "Redo", "Copy", "Cut", "Paste", "Players" },
    { "Tool", "Brush", "Fill", "Rect", "Select", "Decorate", "Add Entity", "Edit Entity", "Squads", "Player Spawn", "Constants" },
};

static const std::unordered_map<InputAction, std::vector<std::string>> TOOLBAR_SHORTCUTS = {
    { INPUT_ACTION_EDITOR_SAVE, { "File", "Save" }},
    { INPUT_ACTION_EDITOR_UNDO, { "Edit", "Undo" }},
    { INPUT_ACTION_EDITOR_REDO, { "Edit", "Redo" }},
    { INPUT_ACTION_EDITOR_COPY, { "Edit", "Copy" }},
    { INPUT_ACTION_EDITOR_CUT, { "Edit", "Cut" }},
    { INPUT_ACTION_EDITOR_PASTE, { "Edit", "Paste" }},
    { INPUT_ACTION_EDITOR_TOOL_BRUSH, { "Tool", "Brush" }},
    { INPUT_ACTION_EDITOR_TOOL_FILL, { "Tool", "Fill" }},
    { INPUT_ACTION_EDITOR_TOOL_RECT, { "Tool", "Rect" }},
    { INPUT_ACTION_EDITOR_TOOL_SELECT, { "Tool", "Select" }},
    { INPUT_ACTION_EDITOR_TOOL_DECORATE, { "Tool", "Decorate" }},
    { INPUT_ACTION_EDITOR_TOOL_ADD_ENTITY, { "Tool", "Add Entity" }},
    { INPUT_ACTION_EDITOR_TOOL_EDIT_ENTITY, { "Tool", "Edit Entity" }},
    { INPUT_ACTION_EDITOR_TOOL_SQUADS, { "Tool", "Squads" }},
};

static const uint32_t GOLD_HELD_PARAMS_MAX_VALUES[] = { 100, 1000, 10000, 100000 };
static const uint32_t GOLD_HELD_PARAMS_STEP_VALUES[] = { 5, 50, 500, 10000 };

// INIT

EditorState* editor_state_init(SDL_Window* window) {
    EditorState* state = new EditorState();

    // Window
    state->window = window;
    state->is_requesting_playtest = false;

    // Scenario
    state->scenario = scenario_init(MAP_TYPE_TOMBSTONE, MAP_SIZE_SMALL);
    state->scenario_path = "";
    state->scenario_is_saved = false;

    // Map
    int lcg_seed = rand();
    map_init(state->map, MAP_TYPE_TOMBSTONE, state->scenario->raw_map, &lcg_seed);
    editor_state_update_map(state);

    // UI
    state->menu = nullptr;
    state->ui_context = ui_init();
    state->ui_toolbar_id = -1;
    state->is_in_file_menu = false;

    // Tool
    state->tool = (EditorTool) {
        .type = EDITOR_TOOL_BRUSH,
        .brush = (EditorToolBrush) {
            .value = MAP_VALUE_LOWGROUND,
            .is_painting = false
        }
    };

    // Actions
    state->action_head = 0;

    // Camera
    state->camera_offset = ivec2(0, 0);
    state->camera_drag_previous_offset = ivec2(0, 0);
    state->camera_drag_mouse_position = ivec2(-1, -1);
    state->is_minimap_dragging = false;

    return state;
}

void editor_state_free_document(EditorState* state) {
    if (state->scenario != NULL) {
        scenario_free(state->scenario);
        state->scenario = NULL;
    }
    if (state->tool.type == EDITOR_TOOL_SELECT) {
        editor_state_tool_select_clear_selection(state);
    }
    editor_state_clipboard_clear(state);
    editor_state_clear_actions(state);
    state->scenario_path = "";
    state->camera_offset = ivec2(0, 0);
}

void editor_state_free(EditorState* state) {
    editor_state_free_document(state);
    if (state->menu != nullptr) {
        delete state->menu;
    }
    delete state;
}

void editor_state_update_map(EditorState* state) {
    // Copy the lcg seed so that we do not modify it
    int lcg_seed = state->scenario->map_bake_lcg_seed;
    map_bake(state->map, state->scenario->raw_map, &lcg_seed);

    // Clear the map cell grid
    map_clear_cells(state->map);
    map_block_walls_and_water(state->map);

    // Place decorations on the cell grid
    std::vector<ivec2> decorations = raw_map_get_decorations(state->scenario->raw_map);
    for (ivec2 cell : decorations) {
        map_create_decoration_at_cell(state->map, cell, &lcg_seed);
    }

    // Place entities on the cell grid
    for (uint32_t entity_index = 0; entity_index < state->scenario->entity_count; entity_index++) {
        const ScenarioEntity& entity = state->scenario->entities[entity_index];
        const EntityData& entity_data = entity_get_data(entity.type);
        map_set_cell_rect(state->map, entity_data.cell_layer, entity.cell, entity_data.cell_size, (Cell) {
            .type = CELL_UNIT,
            .id = (uint16_t)entity_index
        });
    }
}

// UPDATE

void editor_state_update(EditorState* state) {
    // UI Begin
    ui_begin(state->ui_context);
    state->ui_context.input_enabled =
        !state->is_minimap_dragging &&
        !editor_state_is_camera_dragging(state) &&
        !editor_state_is_in_menu(state);

    editor_state_update_toolbar(state);
    editor_state_update_sidebar(state);
    editor_state_update_status_bar(state);

    // Update menu
    if (state->menu != nullptr) {
        state->menu->update(state);
        if (!state->menu->is_open()) {
            delete state->menu;
            state->menu = nullptr;
            return;
        }
    }

    editor_state_update_camera(state);
    editor_state_update_tool(state);

    // Launch test run
    const bool can_launch_test_run =
        !editor_state_is_in_menu(state) &&
        !state->is_minimap_dragging &&
        !editor_state_is_camera_dragging(state) &&
        !editor_state_is_toolbar_open(state) &&
        !input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK) &&
        !input_is_action_just_pressed(INPUT_ACTION_RIGHT_CLICK);
    if (can_launch_test_run && input_is_action_just_pressed(INPUT_ACTION_F5)) {
        editor_state_begin_playtest(state);
    }
}

void editor_state_update_toolbar(EditorState* state) {
    // Toolbar
    ui_small_frame_rect(state->ui_context, TOOLBAR_RECT);
    ui_element_position(state->ui_context, ivec2(3, 3));
    state->ui_toolbar_id = state->ui_context.next_element_id + 1;

    std::string toolbar_column, toolbar_action;
    if (ui_toolbar(state->ui_context, &toolbar_column, &toolbar_action, TOOLBAR_OPTIONS, 2) && editor_state_is_idle(state)) {
        editor_state_handle_toolbar_action(state, toolbar_column, toolbar_action);
    }

    // Editor shortcuts
    if (editor_state_is_idle(state)) {
        for (auto it : TOOLBAR_SHORTCUTS) {
            InputAction shortcut = it.first;
            if (input_is_action_just_pressed(shortcut)) {
                editor_state_handle_toolbar_action(state, it.second[0], it.second[1]);
                return;
            }
        }
    }
}

void editor_state_handle_toolbar_action(EditorState* state, const std::string& column, const std::string& action) {
    if (column == "File" && action == "New") {
        state->menu = new EditorMenuNew();
    }

    if (column == "File" && action == "Save") {
        if (state->scenario_path == "") {
            editor_state_open_file_save_dialog(state);
        } else {
            editor_state_save_document(state, state->scenario_path.c_str());
        }
    }

    if (column == "File" && action == "Save As") {
        editor_state_open_file_save_dialog(state);
    }

    if (column == "File" && action == "Export") {
        editor_state_open_file_export_dialog(state);
    }

    if (column == "File" && action == "Import") {
        editor_state_open_file_import_dialog(state);
    }

    if (column == "File" && action == "Open") {
        editor_state_open_file_open_dialog(state);
    }

    if (column == "Edit" && action == "Undo") {
        editor_state_undo_action(state);
    }

    if (column == "Edit" && action == "Redo") {
        editor_state_redo_action(state);
    }

    if (column == "Edit" && action == "Copy") {
        if (state->tool.type != EDITOR_TOOL_SELECT || !editor_state_tool_select_has_select_rect(state)) {
            return;
        }
        editor_state_clipboard_copy(state);
    }

    if (column == "Edit" && action == "Cut") {
        if (state->tool.type != EDITOR_TOOL_SELECT || !editor_state_tool_select_has_select_rect(state)) {
            return;
        }
        editor_state_clipboard_copy(state);
        editor_state_flatten_rect(state, editor_state_tool_select_get_select_rect(state));
        editor_state_tool_select_clear_selection(state);
    }

    if (column == "Edit" && action == "Paste") {
        if (state->tool.type != EDITOR_TOOL_SELECT || editor_state_clipboard_is_empty(state)) {
            return;
        }
        editor_state_tool_select_clear_selection(state);
        state->tool.select.is_pasting = true;
    }

    if (column == "Edit" && action == "Players") {
        state->menu = new EditorMenuPlayers(state->scenario);
    }

    if (column == "Tool") {
        for (uint32_t index = 1; index < TOOLBAR_OPTIONS[2].size(); index++) {
            if (action == TOOLBAR_OPTIONS[2][index]) {
                editor_state_set_tool(state, (EditorToolType)(index - 1));
            }
        }
    }
}

bool editor_state_is_idle(const EditorState* state) {
    return !editor_state_is_in_menu(state) &&
        !state->is_minimap_dragging &&
        !editor_state_is_tool_active(state) &&
        !editor_state_is_camera_dragging(state);
}

void editor_state_update_status_bar(EditorState* state) {
    ui_small_frame_rect(state->ui_context, STATUS_RECT);
    char status_text[512];
    status_text[0] = '\0';
    char* status_text_ptr = status_text;
    if (CANVAS_RECT.has_point(input_get_mouse_position()) &&
            !state->is_minimap_dragging &&
            !editor_state_is_camera_dragging(state) &&
            !editor_state_is_in_menu(state)) {
        const ivec2 cell = editor_state_get_hovered_cell(state);
        status_text_ptr += sprintf(status_text_ptr, "Cell: <%i, %i> Value: %s ", cell.x, cell.y, editor_state_get_raw_map_value_str(state->scenario->raw_map->data[cell.x + (cell.y * state->scenario->raw_map->width)]));

        const uint32_t entity_index = editor_state_get_hovered_entity(state);
        if (entity_index != INDEX_INVALID) {
            status_text_ptr += sprintf(status_text_ptr, "Entity: %u", entity_index);
        }
    } else if (!CANVAS_RECT.has_point(input_get_mouse_position())) {
        status_text_ptr += sprintf(status_text_ptr, "%s", state->scenario_path.empty() ? "Untitled" : state->scenario_path.c_str());
        if (!state->scenario_is_saved) {
            status_text_ptr += sprintf(status_text_ptr, "*");
        }
    }
    if (status_text[0] != '\0') {
        ui_element_position(state->ui_context, ivec2(4, SCREEN_HEIGHT - 15));
        ui_text(state->ui_context, FONT_HACK_GOLD, status_text);
    }
}

const char* editor_state_get_raw_map_value_str(uint8_t value) {
    switch (value) {
        case MAP_VALUE_WATER:
            return "Water";
        case MAP_VALUE_LOWGROUND:
            return "Lowground";
        case MAP_VALUE_HIGHGROUND:
            return "Highground";
        case MAP_VALUE_STAIR:
            return "Stair";
        case MAP_VALUE_GOLDMINE_LOWGROUND:
            return "Goldmine Lowground";
        case MAP_VALUE_GOLDMINE_HIGHGROUND:
            return "Goldmine Highground";
        case MAP_VALUE_DECORATION_LOWGROUND:
            return "Decoration Lowground";
        case MAP_VALUE_DECORATION_HIGHGROUND:
            return "Decoration Highground";
        default:
            return "Unknown";
    }
}

void editor_state_begin_playtest(EditorState* state) {
    if (!state->scenario_path.empty() && !state->scenario_is_saved) {
        editor_state_save_document(state, state->scenario_path.c_str());
    }
    input_set_mouse_capture_enabled(true);
    SDL_SetWindowMouseGrab(state->window, true);
    state->is_requesting_playtest = true;
}

void editor_state_end_playtest(EditorState* state) {
    input_set_mouse_capture_enabled(false);
    SDL_SetWindowMouseGrab(state->window, false);
    state->is_requesting_playtest = false;
}

// SIDEBAR

void editor_state_update_sidebar(EditorState* state) {
    ui_small_frame_rect(state->ui_context, SIDEBAR_RECT);

    ui_begin_column(state->ui_context, ivec2(SIDEBAR_RECT.x + 4, SIDEBAR_RECT.y + 4), 4);
        // Tool text
        char tool_text[64];
        sprintf(tool_text, "%s Tool", TOOLBAR_OPTIONS[2][state->tool.type + 1].c_str());
        ui_text(state->ui_context, FONT_HACK_GOLD, tool_text);

        switch (state->tool.type) {
            case EDITOR_TOOL_BRUSH: {
                editor_ui_dropdown(state->ui_context, "Value:", &state->tool.brush.value, { "Water", "Lowground", "Highground", "Ramp" }, SIDEBAR_RECT);
                break;
            }
            case EDITOR_TOOL_FILL: {
                editor_ui_dropdown(state->ui_context, "Value:", &state->tool.fill.value, { "Water", "Lowground", "Highground" }, SIDEBAR_RECT);
                break;
            }
            case EDITOR_TOOL_RECT: {
                editor_ui_dropdown(state->ui_context, "Value:", &state->tool.rect.value, { "Water", "Lowground", "Highground" }, SIDEBAR_RECT);
                break;
            }
            case EDITOR_TOOL_SELECT: {
                if (state->tool.select.is_pasting) {
                    char paste_text[64];
                    sprintf(paste_text, "Pasting %ux%u", state->clipboard.width, state->clipboard.height);
                    ui_text(state->ui_context, FONT_HACK_GOLD, paste_text);
                }
                break;
            }
            case EDITOR_TOOL_DECORATE: {
                if (ui_slim_button(state->ui_context, "Clear")) {
                    editor_state_clear_decorations(state);
                }
                if (ui_slim_button(state->ui_context, "Generate")) {
                    editor_state_generate_decorations(state);
                }
                break;
            }
            case EDITOR_TOOL_ADD_ENTITY: {
                editor_state_sidebar_add_entity(state);
                break;
            }
            case EDITOR_TOOL_EDIT_ENTITY: {
                editor_state_sidebar_edit_entity(state);
                break;
            }
            case EDITOR_TOOL_SQUADS: {
                editor_state_sidebar_squads(state);
                break;
            }
            case EDITOR_TOOL_CONSTANTS: {
                editor_state_sidebar_constants(state);
                break;
            }
            case EDITOR_TOOL_PLAYER_SPAWN: {
                char player_spawn_text[128];
                sprintf(player_spawn_text, "Spawn: <%i, %i>", state->scenario->player_spawn.x, state->scenario->player_spawn.y);
                ui_text(state->ui_context, FONT_HACK_GOLD, player_spawn_text);
                break;
            }
        }
    ui_end_container(state->ui_context);
}

void editor_state_clear_decorations(EditorState* state) {
    std::vector<EditorActionBrushStroke> stroke;
    for (int index = 0; index < state->map.width * state->map.height; index++) {
        const uint8_t map_value = state->scenario->raw_map->data[index];
        const bool is_decoration =
            map_value == MAP_VALUE_DECORATION_LOWGROUND ||
            map_value == MAP_VALUE_DECORATION_HIGHGROUND;
        if (is_decoration) {
            stroke.push_back((EditorActionBrushStroke) {
                .index = index,
                .previous_value = map_value,
                .new_value = map_value == MAP_VALUE_DECORATION_LOWGROUND
                    ? MAP_VALUE_LOWGROUND
                    : MAP_VALUE_HIGHGROUND
            });
        }
    }

    editor_state_do_action(state, (EditorActionBrush) {
        .stroke = stroke
    });
}

void editor_state_generate_decorations(EditorState* state) {
    editor_state_clear_decorations(state);

    // Place goldmines into the map so that the decorations avoid them
    for (uint32_t entity_index = 0; entity_index < state->scenario->entity_count; entity_index++) {
        const ScenarioEntity& entity = state->scenario->entities[entity_index];
        if (entity.type == ENTITY_GOLDMINE) {
            int entity_cell_index = entity.cell.x + (entity.cell.y * state->map.width);
            state->scenario->raw_map->data[entity_cell_index] =
                state->scenario->raw_map->data[entity_cell_index] == MAP_VALUE_LOWGROUND
                    ? MAP_VALUE_GOLDMINE_LOWGROUND
                    : MAP_VALUE_GOLDMINE_HIGHGROUND;
        }
    }

    // Generate decorations
    int lcg_seed = rand();
    raw_map_generate_decorations(state->scenario->raw_map, state->scenario->map_type, &lcg_seed);

    // Remove the goldmines from the map now that we have generated decorations
    raw_map_extract_goldmines(state->scenario->raw_map);

    // Create a brush stroke with the generated decoration cells
    std::vector<ivec2> decoration_cells = raw_map_extract_decorations(state->scenario->raw_map);
    std::vector<EditorActionBrushStroke> stroke;
    for (ivec2 cell : decoration_cells) {
        int index = cell.x + (cell.y * state->map.width);
        stroke.push_back((EditorActionBrushStroke) {
            .index = index,
            .previous_value = state->scenario->raw_map->data[index],
            .new_value = state->scenario->raw_map->data[index] == MAP_VALUE_LOWGROUND
                ? MAP_VALUE_DECORATION_LOWGROUND
                : MAP_VALUE_DECORATION_HIGHGROUND
        });
    }

    editor_state_do_action(state, (EditorActionBrush) {
        .stroke = stroke
    });
}

void editor_state_sidebar_add_entity(EditorState* state) {
    const uint32_t TOOL_ENTITY_ROW_SIZE = 4;
    const uint32_t TOOL_ADD_ENTITY_VISIBLE_ROW_COUNT = 4;

    std::vector<std::string> items = editor_state_get_player_name_dropdown_items(state);
    bool was_dropdown_clicked = ui_dropdown(state->ui_context, UI_DROPDOWN_MINI, &state->tool.add_entity.player_id, items, false);

    for (uint32_t row = state->tool.add_entity.scroll_offset; row < state->tool.add_entity.scroll_offset + TOOL_ADD_ENTITY_VISIBLE_ROW_COUNT; row++) {
        ui_begin_row(state->ui_context, ivec2(0, 0), 2);
            for (uint32_t col = 0; col < TOOL_ENTITY_ROW_SIZE; col++) {
                if ((row * TOOL_ENTITY_ROW_SIZE) + col >= ENTITY_TYPE_COUNT) {
                    continue;
                }
                EntityType entity_type = (EntityType)((row * TOOL_ENTITY_ROW_SIZE) + col);
                if (ui_icon_button(state->ui_context, entity_get_data(entity_type).icon, state->tool.add_entity.entity_type == entity_type) && !was_dropdown_clicked) {
                    state->tool.add_entity.entity_type = entity_type;
                }
            }
        ui_end_container(state->ui_context);
    }

    // Scroll
    if (SIDEBAR_RECT.has_point(input_get_mouse_position())) {
        int row_count = ENTITY_TYPE_COUNT / TOOL_ENTITY_ROW_SIZE;
        if (ENTITY_TYPE_COUNT % TOOL_ENTITY_ROW_SIZE != 0) {
            row_count++;
        }
        const int scroll_max = row_count - (int)TOOL_ADD_ENTITY_VISIBLE_ROW_COUNT;
        state->tool.add_entity.scroll_offset = std::clamp(state->tool.add_entity.scroll_offset - input_get_mouse_scroll(), 0, scroll_max);
    }
}

void editor_state_sidebar_edit_entity(EditorState* state) {
    if (state->tool.edit_entity.entity_index == INDEX_INVALID) {
        return;
    }

    const ScenarioEntity& entity = state->scenario->entities[state->tool.edit_entity.entity_index];

    ui_icon_button(state->ui_context, entity_get_data(entity.type).icon, true);

    if (entity_is_misc(entity.type)) {
        char gold_text[16];
        sprintf(gold_text, "Gold: %u", state->tool.edit_entity.gold_held);

        ui_text(state->ui_context, FONT_HACK_GOLD, gold_text);

        UiSliderParams params = (UiSliderParams) {
            .display = UI_SLIDER_DISPLAY_NO_VALUE,
            .size = UI_SLIDER_SIZE_MINI,
            .min = 0,
            .max = 1,
            .step = 1
        };

        if (entity.type == ENTITY_GOLDMINE) {
            editor_ui_dropdown(state->ui_context, "Scale: ", &state->tool.edit_entity.gold_held_slider_scale, { "Hundred", "Thousand", "10k", "100k" }, SIDEBAR_RECT);
            params.max = GOLD_HELD_PARAMS_MAX_VALUES[state->tool.edit_entity.gold_held_slider_scale];
            params.step = GOLD_HELD_PARAMS_STEP_VALUES[state->tool.edit_entity.gold_held_slider_scale];
        } else if (entity.type == ENTITY_CRATE) {
            params.max = 400;
            params.step = 25;
        }

        ui_slider(state->ui_context, &state->tool.edit_entity.gold_held, NULL, params);
        if (input_is_action_just_released(INPUT_ACTION_LEFT_CLICK) && state->tool.edit_entity.gold_held != entity.gold_held) {
            ScenarioEntity edited_entity = entity;
            edited_entity.gold_held = state->tool.edit_entity.gold_held;
            editor_state_do_action(state, (EditorActionEditEntity) {
                .index = state->tool.edit_entity.entity_index,
                .previous_value = entity,
                .new_value = edited_entity
            });
        }

        if (entity.type == ENTITY_GOLDMINE) {
            uint32_t goldmine_is_rigged = (uint32_t)(entity.is_rigged);
            if (editor_ui_dropdown(state->ui_context, "Rigged?", &goldmine_is_rigged, { "No", "Yes" }, SIDEBAR_RECT)) {
                ScenarioEntity edited_entity = entity;
                edited_entity.is_rigged = (bool)goldmine_is_rigged;
                editor_state_do_action(state, (EditorActionEditEntity) {
                    .index = state->tool.edit_entity.entity_index,
                    .previous_value = entity,
                    .new_value = edited_entity
                });
            }
        }
    } else {
        std::vector<std::string> items = editor_state_get_player_name_dropdown_items(state);
        uint32_t entity_player_id = entity.player_id;
        if (ui_dropdown(state->ui_context, UI_DROPDOWN_MINI, &entity_player_id, items, false)) {
            ScenarioEntity edited_entity = entity;
            edited_entity.player_id = (uint8_t)entity_player_id;
            editor_state_do_action(state, (EditorActionEditEntity) {
                .index = state->tool.edit_entity.entity_index,
                .previous_value = entity,
                .new_value = edited_entity
            });
        }
    }

    if (ui_button(state->ui_context, "Delete") || input_is_action_just_pressed(INPUT_ACTION_EDITOR_DELETE)) {
        editor_state_tool_edit_entity_delete_entity(state, state->tool.edit_entity.entity_index);
    }
}

void editor_state_sidebar_squads(EditorState* state) {
    const uint32_t TOOL_SQUADS_ROW_SIZE = 4;
    const uint32_t TOOL_SQUADS_VISIBLE_ROW_COUNT = 3;

    bool already_clicked = false;

    // Squad dropdown
    ui_begin_row(state->ui_context, ivec2(0, 0), 2);
        std::vector<std::string> squad_dropdown_items;
        for (uint32_t squad_index = 0; squad_index < state->scenario->squads.size(); squad_index++) {
            squad_dropdown_items.push_back(std::string(state->scenario->squads[squad_index].name));
        }
        if (ui_dropdown(state->ui_context, UI_DROPDOWN_MINI, &state->tool.squads.squad_index, squad_dropdown_items, false, 9)) {
            already_clicked = true;
        }
        if (ui_sprite_button(state->ui_context, SPRITE_UI_EDITOR_PLUS, false, false)) {
            editor_state_do_action(state, (EditorActionAddSquad) {});
            state->tool.squads.squad_index = state->scenario->squads.size() - 1;
        }
        if (ui_sprite_button(state->ui_context, SPRITE_UI_EDITOR_EDIT, state->scenario->squads.empty(), false)) {
            state->menu = new EditorMenuSquad(state->scenario->squads[state->tool.squads.squad_index]);
        }
        if (ui_sprite_button(state->ui_context, SPRITE_UI_EDITOR_TRASH, state->scenario->squads.empty(), false)) {
            editor_state_do_action(state, (EditorActionRemoveSquad) {
                .index = state->tool.squads.squad_index,
                .value = state->scenario->squads[state->tool.squads.squad_index]
            });
            state->tool.squads.squad_index = 0;
        }
    ui_end_container(state->ui_context);

    // Squad info
    if (!state->scenario->squads.empty()) {
        const ScenarioSquad& squad = state->scenario->squads[state->tool.squads.squad_index];
        char squad_info_text[64];
        sprintf(squad_info_text, "%s / %s", state->scenario->players[squad.player_id].name, bot_squad_type_str(squad.type));
        ui_text(state->ui_context, FONT_HACK_GOLD, squad_info_text);

        if (squad.type == BOT_SQUAD_TYPE_PATROL) {
            char patrol_cell_text[64];
            sprintf(patrol_cell_text, "Patrol Cell: <%i, %i>", squad.patrol_cell.x, squad.patrol_cell.y);
            ui_text(state->ui_context, FONT_HACK_GOLD, patrol_cell_text);
        }

        ui_text(state->ui_context, FONT_HACK_GOLD, "Entities");

        for (uint32_t row = state->tool.squads.scroll_offset; row < state->tool.squads.scroll_offset + TOOL_SQUADS_VISIBLE_ROW_COUNT; row++) {
            ui_begin_row(state->ui_context, ivec2(0, 0), 2);
                for (uint32_t col = 0; col < TOOL_SQUADS_ROW_SIZE; col++) {
                    uint32_t squad_entity_index = col + (row * TOOL_SQUADS_ROW_SIZE);
                    if (squad_entity_index >= squad.entity_count) {
                        continue;
                    }
                    uint32_t entity_index = squad.entities[squad_entity_index];
                    EntityType entity_type = state->scenario->entities[entity_index].type;
                    if (ui_icon_button(state->ui_context, entity_get_data(entity_type).icon, true) && !already_clicked) {
                        editor_state_remove_entity_from_squad(state, state->tool.squads.squad_index, entity_index);
                    }
                }
            ui_end_container(state->ui_context);
        }
    }

    // Scroll
    if (SIDEBAR_RECT.has_point(input_get_mouse_position()) && !state->scenario->squads.empty()) {
        const int squad_entity_count = (int)state->scenario->squads[state->tool.squads.squad_index].entity_count;
        int row_count = squad_entity_count / (int)TOOL_SQUADS_ROW_SIZE;
        if (row_count != 0 && squad_entity_count % TOOL_SQUADS_ROW_SIZE != 0) {
            row_count++;
        }
        const int scroll_max = row_count = (int)TOOL_SQUADS_VISIBLE_ROW_COUNT;
        state->tool.squads.scroll_offset = std::clamp(state->tool.squads.scroll_offset - input_get_mouse_scroll(), 0, scroll_max);
    }
}

void editor_state_sidebar_constants(EditorState* state) {
    const uint32_t TOOL_CONSTANTS_ROW_SIZE = 4;
    const uint32_t TOOL_CONSTANTS_VISIBLE_ROW_COUNT = 3;

    bool already_clicked = false;

    // Constant selector row
    ui_begin_row(state->ui_context, ivec2(0, 0), 2);
        // Build items list
        std::vector<std::string> constant_dropdown_items;
        for (uint32_t constant_index = 0; constant_index < state->scenario->constants.size(); constant_index++) {
            constant_dropdown_items.push_back(std::string(state->scenario->constants[constant_index].name));
        }

        // Dropdown
        if (ui_dropdown(state->ui_context, UI_DROPDOWN_MINI, &state->tool.constants.constant_index, constant_dropdown_items, false, 9)) {
            already_clicked = true;
        }

        // Add button
        if (ui_sprite_button(state->ui_context, SPRITE_UI_EDITOR_PLUS, false, false)) {
            editor_state_do_action(state, (EditorActionAddConstant) {});
            state->tool.constants.constant_index = state->scenario->constants.size() - 1;
        }

        // Edit button
        if (ui_sprite_button(state->ui_context, SPRITE_UI_EDITOR_EDIT, state->scenario->constants.empty(), false)) {
            state->menu = new EditorMenuConstant(state->scenario->constants[state->tool.constants.constant_index].name);
        }

        // Delete button
        if (ui_sprite_button(state->ui_context, SPRITE_UI_EDITOR_TRASH, state->scenario->constants.empty(), false)) {
            editor_state_do_action(state, (EditorActionRemoveConstant) {
                .index = state->tool.constants.constant_index,
                .value = state->scenario->constants[state->tool.constants.constant_index]
            });
            state->tool.constants.constant_index = std::clamp(state->tool.constants.constant_index, 0U, (uint32_t)state->scenario->constants.size() - 1U);
        }
    ui_end_container(state->ui_context);

    // Constant info
    if (!state->scenario->constants.empty()) {
        const ScenarioConstant& constant = state->scenario->constants[state->tool.constants.constant_index];

        // Populate constant type dropdown items
        std::vector<std::string> constant_type_items;
        for (uint32_t constant_type = 0; constant_type < SCENARIO_CONSTANT_TYPE_COUNT; constant_type++) {
            constant_type_items.push_back(scenario_constant_type_str((ScenarioConstantType)constant_type));
        }

        // Constant type dropdown
        uint32_t constant_type = constant.type;
        if (editor_ui_dropdown(state->ui_context, "Type:", &constant_type, constant_type_items, SIDEBAR_RECT, 8)) {
            if (constant_type != constant.type) {
                ScenarioConstant edited_constant = constant;
                scenario_constant_set_type(edited_constant, (ScenarioConstantType)constant_type);
                editor_state_do_action(state, (EditorActionEditConstant) {
                    .index = state->tool.constants.constant_index,
                    .previous_value = constant,
                    .new_value = edited_constant
                });
            }
            already_clicked = true;
        }

        switch (constant.type) {
            case SCENARIO_CONSTANT_TYPE_ENTITY: {
                char value_str[32];
                if (constant.entity_index >= state->scenario->entity_count) {
                    sprintf(value_str, "Entity: INVALID");
                } else {
                    sprintf(value_str, "Entity: %u", constant.entity_index);
                }
                ui_text(state->ui_context, FONT_HACK_GOLD, value_str);
                break;
            }
            case SCENARIO_CONSTANT_TYPE_CELL: {
                char value_str[32];
                sprintf(value_str, "Cell: <%i, %i>", constant.cell.x, constant.cell.y);
                ui_text(state->ui_context, FONT_HACK_GOLD, value_str);
                break;
            }
            case SCENARIO_CONSTANT_TYPE_ENTITY_LIST: {
                for (uint32_t row = state->tool.constants.scroll_offset; row < state->tool.constants.scroll_offset + TOOL_CONSTANTS_VISIBLE_ROW_COUNT; row++) {
                    ui_begin_row(state->ui_context, ivec2(0, 0), 2);
                        for (uint32_t col = 0; col < TOOL_CONSTANTS_ROW_SIZE; col++) {
                            uint32_t constant_entity_index = col + (row * TOOL_CONSTANTS_ROW_SIZE);
                            if (constant_entity_index >= constant.entity_list.entity_count) {
                                continue;
                            }
                            uint32_t entity_index = constant.entity_list.entity_ids[constant_entity_index];
                            EntityType entity_type = state->scenario->entities[entity_index].type;
                            if (ui_icon_button(state->ui_context, entity_get_data(entity_type).icon, true) && !already_clicked) {
                                ScenarioConstant edited_constant = constant;
                                edited_constant.entity_list.entity_ids[constant_entity_index] = edited_constant.entity_list.entity_ids[edited_constant.entity_list.entity_count - 1];
                                edited_constant.entity_list.entity_count--;

                                editor_state_do_action(state, (EditorActionEditConstant) {
                                    .index = state->tool.constants.constant_index,
                                    .previous_value = constant,
                                    .new_value = edited_constant
                                });
                            }
                        }
                    ui_end_container(state->ui_context);
                }
                break;
            }
            case SCENARIO_CONSTANT_TYPE_COUNT: {
                GOLD_ASSERT(false);
                break;
            }
        }
    }

    // Scroll
    const bool can_scroll =
        !state->scenario->constants.empty() &&
        state->scenario->constants[state->tool.constants.constant_index].type == SCENARIO_CONSTANT_TYPE_ENTITY_LIST;
    if (can_scroll && SIDEBAR_RECT.has_point(input_get_mouse_position())) {
        const int entity_list_count = (int)state->scenario->constants[state->tool.constants.constant_index].entity_list.entity_count;
        int row_count = entity_list_count / (int)TOOL_CONSTANTS_ROW_SIZE;
        if (row_count != 0 && entity_list_count % TOOL_CONSTANTS_ROW_SIZE != 0) {
            row_count++;
        }
        const int scroll_max = row_count = (int)TOOL_CONSTANTS_VISIBLE_ROW_COUNT;
        state->tool.constants.scroll_offset = std::clamp(state->tool.constants.scroll_offset - input_get_mouse_scroll(), 0, scroll_max);
    }
}

std::vector<std::string> editor_state_get_player_name_dropdown_items(const EditorState* state) {
    std::vector<std::string> items;

    char player_text[64];
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        sprintf(player_text, "%u: %s", player_id, state->scenario->players[player_id].name);
        items.push_back(std::string(player_text));
    }

    return items;
}

// CAMERA

void editor_state_update_camera(EditorState* state) {
    // Camera drag
    if (input_is_action_just_pressed(INPUT_ACTION_RIGHT_CLICK) &&
            !state->is_minimap_dragging &&
            !editor_state_is_in_menu(state) &&
            !editor_state_is_tool_active(state) &&
            CANVAS_RECT.has_point(input_get_mouse_position())) {
        state->camera_drag_previous_offset = state->camera_offset;
        state->camera_drag_mouse_position = input_get_mouse_position();
    }
    if (input_is_action_just_released(INPUT_ACTION_RIGHT_CLICK)) {
        state->camera_drag_mouse_position = ivec2(-1, -1);
        return;
    }
    if (editor_state_is_camera_dragging(state)) {
        ivec2 mouse_position_difference = input_get_mouse_position() - state->camera_drag_mouse_position;
        state->camera_offset = state->camera_drag_previous_offset - mouse_position_difference;
        editor_state_clamp_camera(state);
    }

    // Minimap drag
    if (MINIMAP_RECT.has_point(input_get_mouse_position()) &&
            !editor_state_is_camera_dragging(state) &&
            !editor_state_is_in_menu(state) &&
            input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        state->is_minimap_dragging = true;
    }
    if (state->is_minimap_dragging && input_is_action_just_released(INPUT_ACTION_LEFT_CLICK)) {
        state->is_minimap_dragging = false;
        return;
    }
    if (state->is_minimap_dragging) {
        ivec2 minimap_pos = ivec2(
            std::clamp(input_get_mouse_position().x - MINIMAP_RECT.x, 0, MINIMAP_RECT.w),
            std::clamp(input_get_mouse_position().y - MINIMAP_RECT.y, 0, MINIMAP_RECT.h));
        ivec2 map_pos = ivec2(
            (state->map.width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.w,
            (state->map.height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.h);
        editor_state_center_camera_on_cell(state, map_pos / TILE_SIZE);
    }
}

bool editor_state_is_camera_dragging(const EditorState* state) {
    return state->camera_drag_mouse_position.x != -1;
}

void editor_state_clamp_camera(EditorState* state) {
    state->camera_offset.x = std::clamp(state->camera_offset.x, 0, (state->map.width * TILE_SIZE) - CANVAS_RECT.w);
    state->camera_offset.y = std::clamp(state->camera_offset.y, 0, (state->map.height * TILE_SIZE) - CANVAS_RECT.h);
}

void editor_state_center_camera_on_cell(EditorState* state, ivec2 cell) {
    state->camera_offset.x = (cell.x * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_WIDTH / 2);
    state->camera_offset.y = (cell.y * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_HEIGHT / 2);
    editor_state_clamp_camera(state);
}

// TOOLS

void editor_state_set_tool(EditorState* state, EditorToolType type) {
    if (editor_state_is_tool_active(state)) {
        log_warn("Editor tried to switch tools while current tool is active.");
        return;
    }

    switch (type) {
        case EDITOR_TOOL_BRUSH: {
            state->tool = (EditorTool) {
                .type = EDITOR_TOOL_BRUSH,
                .brush = (EditorToolBrush) {
                    .value = editor_state_get_tool_map_value(state),
                    .is_painting = false
                }
            };
            break;
        }
        case EDITOR_TOOL_FILL: {
            uint8_t tool_value = editor_state_get_tool_map_value(state);
            if (tool_value >= MAP_VALUE_STAIR) {
                tool_value = MAP_VALUE_LOWGROUND;
            }
            state->tool = (EditorTool) {
                .type = EDITOR_TOOL_FILL,
                .fill = (EditorToolFill) {
                    .value = tool_value
                }
            };
            break;
        }
        case EDITOR_TOOL_RECT: {
            uint8_t tool_value = editor_state_get_tool_map_value(state);
            if (tool_value >= MAP_VALUE_STAIR) {
                tool_value = MAP_VALUE_LOWGROUND;
            }
            state->tool = (EditorTool) {
                .type = EDITOR_TOOL_RECT,
                .rect = (EditorToolRect) {
                    .value = tool_value,
                    .is_painting = false,
                    .origin = ivec2(-1, -1),
                    .end = ivec2(-1, -1)
                }
            };
            break;
        }
        case EDITOR_TOOL_SELECT: {
            state->tool = (EditorTool) {
                .type = EDITOR_TOOL_SELECT,
                .select = (EditorToolSelect) {
                    .origin = ivec2(-1, -1),
                    .end = ivec2(-1, -1),
                    .is_selecting = false,
                    .is_pasting = false
                }
            };
            break;
        }
        case EDITOR_TOOL_DECORATE: {
            state->tool = (EditorTool) {
                .type = EDITOR_TOOL_DECORATE
            };
            break;
        }
        case EDITOR_TOOL_ADD_ENTITY: {
            state->tool = (EditorTool) {
                .type = EDITOR_TOOL_ADD_ENTITY,
                .add_entity = (EditorToolAddEntity) {
                    .entity_type = ENTITY_MINER,
                    .player_id = 0,
                    .scroll_offset = 0
                }
            };
            break;
        }
        case EDITOR_TOOL_EDIT_ENTITY: {
            state->tool = (EditorTool) {
                .type = EDITOR_TOOL_EDIT_ENTITY,
                .edit_entity = (EditorToolEditEntity) {
                    .entity_index = INDEX_INVALID,
                    .gold_held = 0,
                    .gold_held_slider_scale = 0,
                    .drag_offset = ivec2(-1, -1),
                    .is_dragging = false
                }
            };
            break;
        }
        case EDITOR_TOOL_SQUADS: {
            state->tool = (EditorTool) {
                .type = EDITOR_TOOL_SQUADS,
                .squads = (EditorToolSquads) {
                    .squad_index = 0,
                    .scroll_offset = 0
                }
            };
            break;
        }
        case EDITOR_TOOL_PLAYER_SPAWN: {
            state->tool = (EditorTool) {
                .type = EDITOR_TOOL_PLAYER_SPAWN
            };
            break;
        }
        case EDITOR_TOOL_CONSTANTS: {
            state->tool = (EditorTool) {
                .type = EDITOR_TOOL_CONSTANTS,
                .constants = (EditorToolConstants) {
                    .constant_index = 0,
                    .scroll_offset = 0
                }
            };
            break;
        }
    }
}

void editor_state_update_tool(EditorState* state) {
    switch (state->tool.type) {
        case EDITOR_TOOL_BRUSH: {
            editor_state_update_tool_brush(state);
            break;
        }
        case EDITOR_TOOL_FILL: {
            editor_state_update_tool_fill(state);
            break;
        }
        case EDITOR_TOOL_RECT: {
            editor_state_update_tool_rect(state);
            break;
        }
        case EDITOR_TOOL_SELECT: {
            editor_state_update_tool_select(state);
            break;
        }
        case EDITOR_TOOL_DECORATE: {
            editor_state_update_tool_decorate(state);
            break;
        }
        case EDITOR_TOOL_ADD_ENTITY: {
            editor_state_update_tool_add_entity(state);
            break;
        }
        case EDITOR_TOOL_EDIT_ENTITY: {
            editor_state_update_tool_edit_entity(state);
            break;
        }
        case EDITOR_TOOL_SQUADS: {
            editor_state_update_tool_squads(state);
            break;
        }
        case EDITOR_TOOL_PLAYER_SPAWN: {
            editor_state_update_tool_player_spawn(state);
            break;
        }
        case EDITOR_TOOL_CONSTANTS: {
            editor_state_update_tool_constants(state);
            break;
        }
    }
}

void editor_state_update_tool_brush(EditorState* state) {
    // Brush begin painting
    if (!state->tool.brush.is_painting && editor_state_can_tool_be_used(state) && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        state->tool.brush.is_painting = true;
        state->tool_brush_stroke.clear();
    }

    // Brush paint
    if (editor_state_tool_brush_should_paint(state)) {
        const ivec2 cell = editor_state_get_hovered_cell(state);
        const int index = cell.x + (cell.y * state->scenario->raw_map->width);
        state->tool_brush_stroke.push_back((EditorActionBrushStroke) {
            .index = index,
            .previous_value = state->scenario->raw_map->data[index],
            .new_value = (uint8_t)state->tool.brush.value
        });
        state->scenario->raw_map->data[index] = (uint8_t)state->tool.brush.value;
        editor_state_update_map(state);
    }

    // Brush end painting
    if (state->tool.brush.is_painting && input_is_action_just_released(INPUT_ACTION_LEFT_CLICK)) {
        state->tool.brush.is_painting = false;
        if (!state->tool_brush_stroke.empty()) {
            editor_state_push_action(state, (EditorActionBrush) {
                .stroke = state->tool_brush_stroke
            });
        }
    }
}

bool editor_state_tool_brush_should_paint(const EditorState* state) {
    // Don't paint if not painting
    if (!state->tool.brush.is_painting) {
        return false;
    }

    // Don't paint if our mouse is not in the canvas
    if (!CANVAS_RECT.has_point(input_get_mouse_position())) {
        return false;
    }

    // Don't paint if we can't place a stair here
    const ivec2 cell = editor_state_get_hovered_cell(state);
    if (state->tool.brush.value == MAP_VALUE_STAIR && !raw_map_can_stair_be_placed_at_cell(state->scenario->raw_map, cell)) {
        return false;
    }

    // Don't paint if it wouldn't change the map value
    if (state->tool.brush.value == state->scenario->raw_map->data[cell.x + (cell.y * state->scenario->raw_map->width)]) {
        return false;
    }

    return true;
}

void editor_state_update_tool_fill(EditorState* state) {
    // Don't fill if we haven't clicked
    if (!editor_state_can_tool_be_used(state) || !input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        return;
    }

    // Don't fill if the clicked cell already matches the tool value
    const ivec2 cell = editor_state_get_hovered_cell(state);
    const uint8_t previous_value = state->scenario->raw_map->data[cell.x + (cell.y * state->scenario->raw_map->width)];
    if (previous_value == (uint8_t)state->tool.fill.value) {
        return;
    }

    std::vector<int> fill_indices;
    const uint8_t new_value = (uint8_t)state->tool.fill.value;

    std::vector<ivec2> frontier;
    std::vector<bool> is_explored(state->map.width * state->map.height, false);
    frontier.push_back(cell);

    while (!frontier.empty()) {
        ivec2 next = frontier.back();
        frontier.pop_back();

        if (is_explored[next.x + (next.y * state->map.width)]) {
            continue;
        }

        fill_indices.push_back(next.x + (next.y * state->map.width));
        is_explored[next.x + (next.y * state->map.width)] = true;

        for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
            ivec2 child = next + DIRECTION_IVEC2[direction];
            if (!map_is_cell_in_bounds(state->map, child)) {
                continue;
            }
            if (is_explored[child.x + (child.y * state->map.width)]) {
                continue;
            }
            if (state->scenario->raw_map->data[child.x + (child.y * state->scenario->raw_map->width)] != previous_value) {
                continue;
            }
            frontier.push_back(child);
        }
    }

    std::sort(fill_indices.begin(), fill_indices.end());

    std::vector<EditorActionBrushStroke> stroke;
    for (int index : fill_indices) {
        stroke.push_back((EditorActionBrushStroke) {
            .index = index,
            .previous_value = previous_value,
            .new_value = new_value
        });
    }
    editor_state_do_action(state, (EditorActionBrush) {
        .stroke = stroke
    });
}

void editor_state_update_tool_rect(EditorState* state) {
    // Rect begin painting
    if (!state->tool.rect.is_painting && editor_state_can_tool_be_used(state) && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        state->tool.rect.is_painting = true;
        state->tool.rect.origin = editor_state_get_hovered_cell(state);
        state->tool.rect.end = state->tool.rect.origin;
    }

    // Rect paint
    if (state->tool.rect.is_painting && CANVAS_RECT.has_point(input_get_mouse_position())) {
        state->tool.rect.end = editor_state_get_hovered_cell(state);
    }

    // Rect end painting
    if (state->tool.rect.is_painting && input_is_action_just_released(INPUT_ACTION_LEFT_CLICK)) {
        Rect rect = editor_state_tool_rect_get_rect(state);
        std::vector<EditorActionBrushStroke> stroke;
        for (int y = rect.y; y < rect.y + rect.h; y++) {
            for (int x = rect.x; x < rect.x + rect.w; x++) {
                stroke.push_back((EditorActionBrushStroke) {
                    .index = x + (y * state->map.width),
                    .previous_value = state->scenario->raw_map->data[x + (y * state->map.width)],
                    .new_value = (uint8_t)state->tool.rect.value
                });
            }
        }

        editor_state_do_action(state, (EditorActionBrush) {
            .stroke = stroke
        });

        state->tool.rect.is_painting = false;
    }
}

void editor_state_update_tool_select(EditorState* state) {
    // Begin selecting
    const bool can_begin_selecting =
        !state->tool.select.is_selecting &&
        !state->tool.select.is_pasting &&
        editor_state_can_tool_be_used(state);
    if (can_begin_selecting && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        const ivec2 world_space_mouse_pos = editor_state_canvas_to_world_space(state, input_get_mouse_position());
        state->tool.select.origin = world_space_mouse_pos;
        state->tool.select.end = world_space_mouse_pos;
        state->tool.select.is_selecting = true;
    }

    // Update selection
    if (state->tool.select.is_selecting && CANVAS_RECT.has_point(input_get_mouse_position())) {
        state->tool.select.end = editor_state_canvas_to_world_space(state, input_get_mouse_position());
    }

    // Select end
    if (state->tool.select.is_selecting && input_is_action_just_released(INPUT_ACTION_LEFT_CLICK)) {
        state->tool.select.is_selecting = false;
    }

    // Paste
    const bool can_paste =
        state->tool.select.is_pasting &&
        editor_state_can_tool_be_used(state);
    if (can_paste && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        const ivec2 paste_cell = editor_state_get_hovered_cell(state);
        editor_state_clipboard_paste(state, paste_cell);
        state->tool.select.is_pasting = false;
    }

    // Camera drag during selection
    if (state->tool.select.is_selecting) {
        const int CAMERA_DRAG_MARGIN = 16;

        ivec2 mouse_in_canvas_position = input_get_mouse_position() - ivec2(CANVAS_RECT.x, CANVAS_RECT.y);
        ivec2 drag_direction = ivec2(0, 0);
        if (mouse_in_canvas_position.x < CAMERA_DRAG_MARGIN) {
            drag_direction.x = -1;
        } else if (mouse_in_canvas_position.x >= CANVAS_RECT.w - CAMERA_DRAG_MARGIN) {
            drag_direction.x = 1;
        }
        if (mouse_in_canvas_position.y < CAMERA_DRAG_MARGIN) {
            drag_direction.y = -1;
        } else if (mouse_in_canvas_position.y >= CANVAS_RECT.h - CAMERA_DRAG_MARGIN) {
            drag_direction.y = 1;
        }
        state->camera_offset += drag_direction * 8;
        editor_state_clamp_camera(state);
    }
}

void editor_state_update_tool_decorate(EditorState* state) {
    const bool can_decorate = editor_state_can_tool_be_used(state) && editor_state_is_hovered_cell_valid(state);
    if (can_decorate && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        const ivec2 cell = editor_state_get_hovered_cell(state);
        const int index = cell.x + (cell.y * state->map.width) ;
        const uint8_t previous_value = state->scenario->raw_map->data[index];
        editor_state_do_action(state, (EditorActionBrush) {
            .stroke = {
                (EditorActionBrushStroke) {
                    .index = index,
                    .previous_value = previous_value,
                    .new_value = editor_state_tool_decorate_toggle_value(previous_value)
                }
            }
        });
    }
}

uint8_t editor_state_tool_decorate_toggle_value(uint8_t previous_value) {
    switch (previous_value) {
        case MAP_VALUE_LOWGROUND:
            return MAP_VALUE_DECORATION_LOWGROUND;
        case MAP_VALUE_HIGHGROUND:
            return MAP_VALUE_DECORATION_HIGHGROUND;
        case MAP_VALUE_DECORATION_LOWGROUND:
            return MAP_VALUE_LOWGROUND;
        case MAP_VALUE_DECORATION_HIGHGROUND:
            return MAP_VALUE_HIGHGROUND;
        default:
            GOLD_ASSERT(false);
            return MAP_VALUE_LOWGROUND;
    }
}

void editor_state_update_tool_add_entity(EditorState* state) {
    const bool can_add_entity = editor_state_can_tool_be_used(state) && editor_state_is_hovered_cell_valid(state);
    if (can_add_entity && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        editor_state_do_action(state, (EditorActionAddEntity) {
            .type = state->tool.add_entity.entity_type,
            .player_id = entity_is_misc(state->tool.add_entity.entity_type)
                ? (uint8_t)PLAYER_NONE
                : (uint8_t)state->tool.add_entity.player_id,
            .cell = editor_state_get_hovered_cell(state)
        });
    }
}

void editor_state_update_tool_edit_entity(EditorState* state) {
    // Select entity
    const uint32_t hovered_entity_index = editor_state_get_hovered_entity(state);
    const bool can_select_entity =
        editor_state_can_tool_be_used(state) &&
        editor_state_is_hovered_cell_valid(state) &&
        hovered_entity_index != INDEX_INVALID &&
        hovered_entity_index != state->tool.edit_entity.entity_index &&
        !state->tool.edit_entity.is_dragging;
    if (can_select_entity && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        state->tool.edit_entity.entity_index = hovered_entity_index;
        state->tool.edit_entity.gold_held = state->scenario->entities[state->tool.edit_entity.entity_index].gold_held;

        const uint32_t scale_count = sizeof(GOLD_HELD_PARAMS_MAX_VALUES) / sizeof(GOLD_HELD_PARAMS_MAX_VALUES[0]);
        state->tool.edit_entity.gold_held_slider_scale = 0;
        while (state->tool.edit_entity.gold_held_slider_scale < scale_count - 1 &&
                    state->scenario->entities[hovered_entity_index].gold_held > GOLD_HELD_PARAMS_MAX_VALUES[state->tool.edit_entity.gold_held_slider_scale]) {
            state->tool.edit_entity.gold_held_slider_scale++;
        }
    }

    // Begin dragging
    const bool can_drag_entity =
        editor_state_can_tool_be_used(state) &&
        editor_state_is_hovered_cell_valid(state) &&
        hovered_entity_index != INDEX_INVALID &&
        hovered_entity_index == state->tool.edit_entity.entity_index &&
        !state->tool.edit_entity.is_dragging;
    if (can_drag_entity && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        state->tool.edit_entity.is_dragging = true;
        state->tool.edit_entity.drag_offset =
            editor_state_get_hovered_cell(state) -
            state->scenario->entities[state->tool.edit_entity.entity_index].cell;
    }

    // End dragging
    const bool can_release_entity =
        state->tool.edit_entity.is_dragging &&
        editor_state_can_tool_be_used(state);
    if (can_release_entity && input_is_action_just_released(INPUT_ACTION_LEFT_CLICK)) {
        state->tool.edit_entity.is_dragging = false;

        if (CANVAS_RECT.has_point(input_get_mouse_position()) && editor_state_is_hovered_cell_valid(state)) {
            const ivec2 new_cell = editor_state_get_hovered_cell(state) - state->tool.edit_entity.drag_offset;
            const ScenarioEntity selected_entity = state->scenario->entities[state->tool.edit_entity.entity_index];
            if (new_cell != selected_entity.cell) {
                ScenarioEntity edited_entity = selected_entity;
                edited_entity.cell = new_cell;

                editor_state_do_action(state, (EditorActionEditEntity) {
                    .index = state->tool.edit_entity.entity_index,
                    .previous_value = selected_entity,
                    .new_value = edited_entity
                });
            }
        }
    }
}

void editor_state_update_tool_squads(EditorState* state) {
    if (state->scenario->squads.empty() || !editor_state_can_tool_be_used(state)) {
        return;
    }

    const uint32_t hovered_entity_index = editor_state_get_hovered_entity(state);
    if (hovered_entity_index != INDEX_INVALID && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        uint32_t entity_squad_index = editor_state_get_entity_squad(state, hovered_entity_index);

        // Select entity squad
        if (input_is_action_pressed(INPUT_ACTION_CTRL) && entity_squad_index != EDITOR_ENTITY_HAS_NO_SQUAD) {
            state->tool.squads.squad_index = entity_squad_index;
            return;
        }

        // If entity is part of selected squad, remove it from the squad
        if (input_is_action_pressed(INPUT_ACTION_SHIFT) && entity_squad_index == state->tool.squads.squad_index) {
            editor_state_remove_entity_from_squad(state, entity_squad_index, hovered_entity_index);
            return;
        }

        // Otherwise, add entity to squad
        const ScenarioSquad& selected_squad = state->scenario->squads[state->tool.squads.squad_index];
        const bool can_add_to_squad =
            entity_squad_index == EDITOR_ENTITY_HAS_NO_SQUAD &&
            selected_squad.entity_count < SCENARIO_SQUAD_MAX_ENTITIES &&
            state->scenario->entities[hovered_entity_index].player_id == selected_squad.player_id;
        if (can_add_to_squad && !input_is_action_pressed(INPUT_ACTION_SHIFT)) {
            ScenarioSquad edited_squad = selected_squad;
            edited_squad.entities[edited_squad.entity_count] = hovered_entity_index;
            edited_squad.entity_count++;

            editor_state_do_action(state, (EditorActionEditSquad) {
                .index = state->tool.squads.squad_index,
                .previous_value = selected_squad,
                .new_value = edited_squad
            });
            return;
        }
    }

    // Set patrol cell
    const ScenarioSquad& selected_squad = state->scenario->squads[state->tool.squads.squad_index];
    const bool can_set_patrol_cell =
        editor_state_is_hovered_cell_valid(state) &&
        selected_squad.type == BOT_SQUAD_TYPE_PATROL;
    if (can_set_patrol_cell && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK) && input_is_action_pressed(INPUT_ACTION_SHIFT)) {
        ScenarioSquad edited_squad = selected_squad;
        edited_squad.patrol_cell = editor_state_get_hovered_cell(state);

        editor_state_do_action(state, (EditorActionEditSquad) {
            .index = state->tool.squads.squad_index,
            .previous_value = selected_squad,
            .new_value = edited_squad
        });
    }
}

void editor_state_update_tool_player_spawn(EditorState* state) {
    const bool can_set_player_spawn =
        editor_state_can_tool_be_used(state) &&
        editor_state_is_hovered_cell_valid(state);
    if (can_set_player_spawn && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        editor_state_do_action(state, (EditorActionSetPlayerSpawn) {
            .previous_value = state->scenario->player_spawn,
            .new_value = editor_state_get_hovered_cell(state)
        });
    }
}

void editor_state_update_tool_constants(EditorState* state) {
    if (state->scenario->constants.empty() || !editor_state_can_tool_be_used(state) || !editor_state_is_hovered_cell_valid(state)) {
        return;
    }

    // Set constant value
    if (input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        const ScenarioConstant& selected_constant = state->scenario->constants[state->tool.constants.constant_index];
        ScenarioConstant edited_constant = selected_constant;

        switch (selected_constant.type) {
            case SCENARIO_CONSTANT_TYPE_ENTITY: {
                edited_constant.entity_index = editor_state_get_hovered_entity(state);
                break;
            }
            case SCENARIO_CONSTANT_TYPE_CELL: {
                edited_constant.cell = editor_state_get_hovered_cell(state);
                break;
            }
            case SCENARIO_CONSTANT_TYPE_ENTITY_LIST: {
                uint32_t entity_index = editor_state_get_hovered_entity(state);

                uint32_t constant_entity_index;
                for (constant_entity_index = 0; constant_entity_index < edited_constant.entity_list.entity_count; constant_entity_index++) {
                    if (edited_constant.entity_list.entity_ids[constant_entity_index] == entity_index) {
                        break;
                    }
                }

                // Add entity to list
                if (constant_entity_index == edited_constant.entity_list.entity_count) {
                    edited_constant.entity_list.entity_ids[edited_constant.entity_list.entity_count] = entity_index;
                    edited_constant.entity_list.entity_count++;
                } else {
                // Remove entity from list
                    edited_constant.entity_list.entity_ids[constant_entity_index] = edited_constant.entity_list.entity_ids[edited_constant.entity_list.entity_count - 1];
                    edited_constant.entity_list.entity_count--;
                }
                break;
            }
            case SCENARIO_CONSTANT_TYPE_COUNT: {
                GOLD_ASSERT(false);
                break;
            }
        }

        editor_state_do_action(state, (EditorActionEditConstant) {
            .index = state->tool.constants.constant_index,
            .previous_value = selected_constant,
            .new_value = edited_constant
        });
    }
}

// TOOL HELPERS

void editor_state_ensure_tool_value_is_in_bounds(EditorState* state) {
    switch (state->tool.type) {
        case EDITOR_TOOL_EDIT_ENTITY: {
            if (state->tool.edit_entity.entity_index >= state->scenario->entity_count) {
                state->tool.edit_entity.entity_index = INDEX_INVALID;
            }
            break;
        }
        case EDITOR_TOOL_SQUADS: {
            if (state->tool.squads.squad_index >= state->scenario->squads.size()) {
                state->tool.squads.squad_index = 0;
            }
            break;
        }
        case EDITOR_TOOL_CONSTANTS: {
            if (state->tool.constants.constant_index >= state->scenario->constants.size()) {
                state->tool.constants.constant_index = 0;
            }
            break;
        }
        default: {
            break;
        }
    }
}

uint32_t editor_state_get_tool_map_value(const EditorState* state) {
    switch (state->tool.type) {
        case EDITOR_TOOL_BRUSH:
            return state->tool.brush.value;
        case EDITOR_TOOL_FILL:
            return state->tool.fill.value;
        case EDITOR_TOOL_RECT:
            return state->tool.rect.value;
        default:
            return MAP_VALUE_LOWGROUND;
    }
}

bool editor_state_is_tool_active(const EditorState* state) {
    switch (state->tool.type) {
        case EDITOR_TOOL_BRUSH:
            return state->tool.brush.is_painting;
        case EDITOR_TOOL_RECT:
            return state->tool.rect.is_painting;
        case EDITOR_TOOL_SELECT:
            return state->tool.select.is_selecting && !state->tool.select.is_pasting;
        case EDITOR_TOOL_EDIT_ENTITY:
            return state->tool.edit_entity.is_dragging;
        default:
            return false;
    }
}

ivec2 editor_state_canvas_to_world_space(const EditorState* state, ivec2 point) {
    return point - ivec2(CANVAS_RECT.x, CANVAS_RECT.y) + state->camera_offset;
}

Rect editor_state_cell_rect_to_world_space(const EditorState* state, const Rect& rect) {
    return (Rect) {
        .x = (rect.x * TILE_SIZE) - state->camera_offset.x,
        .y = (rect.y * TILE_SIZE) - state->camera_offset.y,
        .w = rect.w * TILE_SIZE,
        .h = rect.h * TILE_SIZE
    };
}

ivec2 editor_state_get_hovered_cell(const EditorState* state) {
    return editor_state_canvas_to_world_space(state, input_get_mouse_position()) / TILE_SIZE;
}

uint32_t editor_state_get_hovered_entity(const EditorState* state) {
    ivec2 cell = editor_state_get_hovered_cell(state);

    for (uint32_t entity_index = 0; entity_index < state->scenario->entity_count; entity_index++) {
        const ScenarioEntity& entity = state->scenario->entities[entity_index];
        const int entity_cell_size = entity_get_data(entity.type).cell_size;
        Rect entity_cell_rect = (Rect) {
            .x = entity.cell.x, .y = entity.cell.y,
            .w = entity_cell_size, .h = entity_cell_size
        };
        if (entity_cell_rect.has_point(cell)) {
            return entity_index;
        }
    }

    return INDEX_INVALID;
}

bool editor_state_is_hovered_cell_valid(const EditorState* state) {
    switch (state->tool.type) {
        case EDITOR_TOOL_DECORATE: {
            const ivec2 hovered_cell = editor_state_get_hovered_cell(state);
            Cell map_cell = map_get_cell(state->map, CELL_LAYER_GROUND, hovered_cell);
            return map_cell.type == CELL_DECORATION || map_cell.type == CELL_EMPTY;
        }
        case EDITOR_TOOL_ADD_ENTITY: {
            const ivec2 hovered_cell = editor_state_get_hovered_cell(state);
            const EntityData& entity_data = entity_get_data(state->tool.add_entity.entity_type);
            return !map_is_cell_rect_occupied(state->map, entity_data.cell_layer, hovered_cell, entity_data.cell_size);
        }
        case EDITOR_TOOL_EDIT_ENTITY: {
            if (!state->tool.edit_entity.is_dragging) {
                return true;
            }

            const ivec2 hovered_cell = editor_state_get_hovered_cell(state) - state->tool.edit_entity.drag_offset;
            const EntityType selected_entity_type = state->scenario->entities[state->tool.edit_entity.entity_index].type;
            const EntityData& entity_data = entity_get_data(selected_entity_type);

            for (int y = hovered_cell.y; y < hovered_cell.y + entity_data.cell_size; y++) {
                for (int x = hovered_cell.x; x < hovered_cell.x + entity_data.cell_size; x++) {
                    Cell map_cell = map_get_cell(state->map, entity_data.cell_layer, ivec2(x, y));
                    if (map_cell.type == CELL_BLOCKED) {
                        return false;
                    }
                    if (map_cell.type == CELL_UNIT && map_cell.id != state->tool.edit_entity.entity_index) {
                        return false;
                    }
                }
            }

            return true;
        }
        case EDITOR_TOOL_SQUADS: {
            if (state->scenario->squads.empty()) {
                return true;
            }

            const ScenarioSquad& squad = state->scenario->squads[state->tool.squads.squad_index];
            const uint32_t hovered_entity_index = editor_state_get_hovered_entity(state);

            // Valid if placing squad patrol point
            if (squad.type == BOT_SQUAD_TYPE_PATROL && input_is_action_pressed(INPUT_ACTION_SHIFT)) {
                return true;
            }

            // Not valid if not hovering an entity
            if (hovered_entity_index == INDEX_INVALID) {
                return false;
            }

            // Not valid if squad is full
            if (squad.entity_count == SCENARIO_SQUAD_MAX_ENTITIES) {
                return false;
            }

            const uint32_t hovered_entity_squad_index = editor_state_get_entity_squad(state, hovered_entity_index);

            // Not valid if the hovered entity belongs to a different squad
            if (hovered_entity_squad_index != EDITOR_ENTITY_HAS_NO_SQUAD &&
                    hovered_entity_squad_index != state->tool.squads.squad_index) {
                return false;
            }

            // Not valid if the hovered entity belongs to a player who does not own this squad
            if (state->scenario->entities[hovered_entity_index].player_id !=
                    state->scenario->squads[state->tool.squads.squad_index].player_id) {
                return false;
            }

            // Otherwise, valid
            return true;
        }
        case EDITOR_TOOL_CONSTANTS: {
            if (state->scenario->constants.empty()) {
                return true;
            }

            const ScenarioConstant& constant = state->scenario->constants[state->tool.constants.constant_index];
            switch (constant.type) {
                case SCENARIO_CONSTANT_TYPE_ENTITY: {
                    const uint32_t hovered_entity_index = editor_state_get_hovered_entity(state);
                    return hovered_entity_index != INDEX_INVALID;
                }
                case SCENARIO_CONSTANT_TYPE_CELL: {
                    return true;
                }
                case SCENARIO_CONSTANT_TYPE_ENTITY_LIST: {
                    // We have to be hovering an entity
                    const uint32_t hovered_entity_index = editor_state_get_hovered_entity(state);
                    if (hovered_entity_index == INDEX_INVALID) {
                        return false;
                    }

                    // If we are hovering an entity,
                    // then the cell is valid because we can remove it
                    bool entity_is_in_list = false;
                    for (uint32_t index = 0; index < constant.entity_list.entity_count; index++) {
                        if (constant.entity_list.entity_ids[index] == hovered_entity_index) {
                            entity_is_in_list = true;
                            break;
                        }
                    }
                    if (entity_is_in_list) {
                        return true;
                    }

                    // Otherwise, the cell represents us adding an entity
                    // and is only valid if we have space in the list for it
                    return constant.entity_list.entity_count < SELECTION_LIMIT;
                }
                case SCENARIO_CONSTANT_TYPE_COUNT: {
                    GOLD_ASSERT(false);
                    return false;
                }
            }
        }
        default: {
            return true;
        }
    }
}

bool editor_state_can_tool_be_used(const EditorState* state) {
    return !editor_state_is_in_menu(state) &&
        !editor_state_is_toolbar_open(state) &&
        !state->is_minimap_dragging &&
        !editor_state_is_camera_dragging(state) &&
        CANVAS_RECT.has_point(input_get_mouse_position());
}

std::vector<uint32_t> editor_state_get_selected_entities(const EditorState* state) {
    std::vector<uint32_t> selection;

    if (state->tool.type == EDITOR_TOOL_EDIT_ENTITY && state->tool.edit_entity.entity_index != INDEX_INVALID) {
        selection.push_back(state->tool.edit_entity.entity_index);
    }

    if (state->tool.type == EDITOR_TOOL_SQUADS && !state->scenario->squads.empty()) {
        const ScenarioSquad& squad = state->scenario->squads[state->tool.squads.squad_index];
        for (uint32_t squad_entity_index = 0; squad_entity_index < squad.entity_count; squad_entity_index++) {
            selection.push_back(squad.entities[squad_entity_index]);
        }
    }

    if (state->tool.type == EDITOR_TOOL_CONSTANTS && !state->scenario->constants.empty()) {
        const ScenarioConstant& constant = state->scenario->constants[state->tool.constants.constant_index];
        if (constant.type == SCENARIO_CONSTANT_TYPE_ENTITY && constant.entity_index < state->scenario->entity_count) {
            selection.push_back(constant.entity_index);
        }
        if (constant.type == SCENARIO_CONSTANT_TYPE_ENTITY_LIST) {
            for (uint32_t constant_entity_index = 0; constant_entity_index < constant.entity_list.entity_count; constant_entity_index++) {
                uint32_t entity_index = constant.entity_list.entity_ids[constant_entity_index];
                if (entity_index < state->scenario->entity_count) {
                    selection.push_back(entity_index);
                }
            }
        }
    }

    return selection;
}

bool editor_state_is_dragging_entity(const EditorState* state, uint32_t entity_index) {
    return state->tool.type == EDITOR_TOOL_EDIT_ENTITY &&
        state->tool.edit_entity.is_dragging &&
        state->tool.edit_entity.entity_index == entity_index;
}

uint32_t editor_state_get_entity_squad(const EditorState* state, uint32_t entity_index) {
    for (uint32_t squad_index = 0; squad_index < state->scenario->squads.size(); squad_index++) {
        const ScenarioSquad& squad = state->scenario->squads[squad_index];
        for (uint32_t squad_entity_index = 0; squad_entity_index < squad.entity_count; squad_entity_index++) {
            if (squad.entities[squad_entity_index] == entity_index) {
                return squad_index;
            }
        }
    }

    return EDITOR_ENTITY_HAS_NO_SQUAD;
}

Rect editor_state_tool_rect_get_rect(const EditorState* state) {
    GOLD_ASSERT(state->tool.type == EDITOR_TOOL_RECT);

    // The +1 on the width and height is to ensure that the rect is inclusive of the end cell
    return (Rect) {
        .x = std::min(state->tool.rect.origin.x, state->tool.rect.end.x),
        .y = std::min(state->tool.rect.origin.y, state->tool.rect.end.y),
        .w = std::abs(state->tool.rect.origin.x - state->tool.rect.end.x) + 1,
        .h = std::abs(state->tool.rect.origin.y - state->tool.rect.end.y) + 1
    };
}

bool editor_state_tool_select_has_select_rect(const EditorState* state) {
    GOLD_ASSERT(state->tool.type == EDITOR_TOOL_SELECT);
    return state->tool.select.origin.x != -1;
}

Rect editor_state_tool_select_get_select_rect(const EditorState* state) {
    GOLD_ASSERT(state->tool.type == EDITOR_TOOL_SELECT);

    ivec2 origin_cell = state->tool.select.origin / TILE_SIZE;
    ivec2 end_cell = state->tool.select.end / TILE_SIZE;
    return (Rect) {
        .x = std::min(origin_cell.x, end_cell.x),
        .y = std::min(origin_cell.y, end_cell.y),
        .w = std::abs(origin_cell.x - end_cell.x) + 1,
        .h = std::abs(origin_cell.y - end_cell.y) + 1
    };
}

void editor_state_tool_select_clear_selection(EditorState* state) {
    GOLD_ASSERT(state->tool.type == EDITOR_TOOL_SELECT);

    state->tool.select.origin = ivec2(-1, -1);
    state->tool.select.end = ivec2(-1, -1);
}

void editor_state_flatten_rect(EditorState* state, const Rect& rect) {
    std::vector<EditorActionBrushStroke> stroke;

    for (int y = rect.y; y < rect.y + rect.h; y++) {
        for (int x = rect.x; x < rect.x + rect.w; x++) {
            stroke.push_back((EditorActionBrushStroke) {
                .index = x + (y * state->map.width),
                .previous_value = state->scenario->raw_map->data[x + (y * state->map.width)],
                .new_value = MAP_VALUE_LOWGROUND
            });
        }
    }

    editor_state_do_action(state, (EditorActionBrush) {
        .stroke = stroke
    });
}

void editor_state_tool_edit_entity_delete_entity(EditorState* state, uint32_t entity_index) {
    uint32_t entity_squad_index = INDEX_INVALID;
    for (uint32_t squad_index = 0; squad_index < state->scenario->squads.size(); squad_index++) {
        for (uint32_t squad_entity_index = 0; squad_entity_index < state->scenario->squads[squad_index].entity_count; squad_entity_index++) {
            if (state->scenario->squads[squad_index].entities[squad_entity_index] == entity_index) {
                entity_squad_index = squad_index;
                break;
            }
        }
    }

    std::vector<uint32_t> constant_indices;
    for (uint32_t constant_index = 0; constant_index < state->scenario->constants.size(); constant_index++) {
        if (state->scenario->constants[constant_index].type == SCENARIO_CONSTANT_TYPE_ENTITY && state->scenario->constants[constant_index].entity_index == entity_index) {
            constant_indices.push_back(constant_index);
        }
    }

    editor_state_do_action(state, (EditorActionRemoveEntity) {
        .index = entity_index,
        .value = state->scenario->entities[entity_index],
        .squad_index = entity_squad_index,
            .constant_indices = constant_indices
    });
    state->tool.edit_entity.entity_index = INDEX_INVALID;
}

void editor_state_remove_entity_from_squad(EditorState* state, uint32_t squad_index, uint32_t entity_index) {
    ScenarioSquad edited_squad = state->scenario->squads[squad_index];

    uint32_t squad_entity_index;
    for (squad_entity_index = 0; squad_entity_index < edited_squad.entity_count; squad_entity_index++) {
        if (edited_squad.entities[squad_entity_index] == entity_index) {
            break;
        }
    }
    GOLD_ASSERT(squad_entity_index != edited_squad.entity_count);

    edited_squad.entities[squad_entity_index] = edited_squad.entities[edited_squad.entity_count - 1];
    edited_squad.entity_count--;

    editor_state_do_action(state, (EditorActionEditSquad) {
        .index = squad_index,
        .previous_value = state->scenario->squads[squad_index],
        .new_value = edited_squad
    });
}

// CLIPBOARD

void editor_state_clipboard_clear(EditorState* state) {
    state->clipboard.width = 0;
    state->clipboard.height = 0;
    state->clipboard.values.clear();
}

bool editor_state_clipboard_is_empty(EditorState* state) {
    return state->clipboard.width == 0;
}

void editor_state_clipboard_copy(EditorState* state) {
    if (state->tool.type != EDITOR_TOOL_SELECT) {
        log_warn("Tried to copy when not in select tool.");
        return;
    }

    if (!editor_state_tool_select_has_select_rect(state)) {
        return;
    }

    editor_state_clipboard_clear(state);

    const Rect select_rect = editor_state_tool_select_get_select_rect(state);
    state->clipboard.width = select_rect.w;
    state->clipboard.height = select_rect.h;
    state->clipboard.values.reserve(state->clipboard.width * state->clipboard.height);
    for (int y = 0; y < state->clipboard.height; y++) {
        for (int x = 0; x < state->clipboard.width; x++) {
            const ivec2 map_cell = ivec2(select_rect.x + x, select_rect.y + y);
            const int map_index = map_cell.x + (map_cell.y * state->map.width);
            state->clipboard.values.push_back(state->scenario->raw_map->data[map_index]);
        }
    }
}

void editor_state_clipboard_paste(EditorState* state, ivec2 top_left_cell) {
    std::vector<EditorActionBrushStroke> stroke;

    for (int y = 0; y < state->clipboard.height; y++) {
        for (int x = 0; x < state->clipboard.width; x++) {
            ivec2 cell = top_left_cell + ivec2(x, y);
            if (!map_is_cell_in_bounds(state->map, cell)) {
                continue;
            }

            int index = cell.x + (cell.y * state->map.width);
            stroke.push_back((EditorActionBrushStroke) {
                .index = index,
                .previous_value = state->scenario->raw_map->data[index],
                .new_value = state->clipboard.values[x + (y * state->clipboard.width)]
            });
        }
    }

    editor_state_do_action(state, (EditorActionBrush) {
        .stroke = stroke
    });
}

// ACTIONS

// Adds an action to the stack but does not execute the action
void editor_state_push_action(EditorState* state, const EditorAction& action) {
    while (state->actions.size() > state->action_head) {
        state->actions.pop_back();
    }
    state->actions.push_back(action);
    state->action_head++;
}

// Adds an action to the stack and also executes the action
void editor_state_do_action(EditorState* state, const EditorAction& action) {
    editor_state_push_action(state, action);
    editor_action_execute(state->scenario, action, EDITOR_ACTION_MODE_DO);
    editor_state_update_map(state);
}

void editor_state_undo_action(EditorState* state) {
    if (state->action_head == 0) {
        return;
    }

    state->action_head--;
    editor_action_execute(state->scenario, state->actions[state->action_head], EDITOR_ACTION_MODE_UNDO);
    editor_state_update_map(state);

    editor_state_ensure_tool_value_is_in_bounds(state);
}

void editor_state_redo_action(EditorState* state) {
    if (state->action_head == state->actions.size()) {
        return;
    }

    editor_action_execute(state->scenario, state->actions[state->action_head], EDITOR_ACTION_MODE_DO);
    editor_state_update_map(state);
    state->action_head++;

    editor_state_ensure_tool_value_is_in_bounds(state);
}

void editor_state_clear_actions(EditorState* state) {
    state->actions.clear();
    state->action_head = 0;
}

// MENU

bool editor_state_is_in_menu(const EditorState* state) {
    return state->menu != nullptr && !state->is_in_file_menu;
}

bool editor_state_is_toolbar_open(const EditorState* state) {
    return state->ui_context.element_selected == state->ui_toolbar_id;
}

#endif
