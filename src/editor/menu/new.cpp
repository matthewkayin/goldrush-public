#include "new.h"
#include "match/state/map_gen.h"

#ifdef GOLD_DEBUG

#include "core/ui.h"
#include "shared/match_setting.h"
#include "editor/state.h"
#include "editor/ui_helpers.h"

static const Rect MENU_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (300 / 2),
    .y = 48,
    .w = 300,
    .h = 256
};

static const uint32_t MAP_INIT_STYLE_BLANK = 0U;
static const uint32_t MAP_INIT_STYLE_GENERATED = 1U;

EditorMenuNew::EditorMenuNew() {
    map_type = MAP_TYPE_BOULDER;
    map_size = MAP_SIZE_SMALL;
    map_init_style = MAP_INIT_STYLE_BLANK;
}

const char* EditorMenuNew::get_header_text() const {
    return "New Map";
}

Rect EditorMenuNew::get_rect() const {
    return MENU_RECT;
}

void EditorMenuNew::child_update(EditorState* state) {
    ui_begin_column(state->ui_context, ivec2(MENU_RECT.x + 8, MENU_RECT.y + 30), 4);
        // Map type
        editor_ui_dropdown(state->ui_context, "Map Type:", &map_type, match_setting_data(MATCH_SETTING_MAP_TYPE).values, MENU_RECT);

        // Map size
        editor_ui_dropdown(state->ui_context, "Map Size:", &map_size, match_setting_data(MATCH_SETTING_MAP_SIZE).values, MENU_RECT);

        // Use noise gen params
        editor_ui_dropdown(state->ui_context, "Generation Style:", &map_init_style, { "Blank", "Noise" }, MENU_RECT);
    ui_end_container(state->ui_context);
}

void EditorMenuNew::on_submit(EditorState* state) {
    editor_state_free_document(state);
    state->scenario = scenario_init((MapType)map_type, (MapSize)map_size);

    if (map_init_style == MAP_INIT_STYLE_GENERATED) {
        free(state->scenario->raw_map);
        int map_tile_size = map_get_tile_size((MapSize)map_size);
        state->scenario->raw_map = raw_map_generate((MapType)map_type, map_tile_size, map_tile_size, rand(), MAP_GEN_OPTION_GENERATE_DECORATIONS);
    }

    int lcg_seed = state->scenario->map_bake_lcg_seed;
    map_init(state->map, state->scenario->map_type, state->scenario->raw_map, &lcg_seed);
}

#endif
