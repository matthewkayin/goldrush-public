#include "squad.h"

#ifdef GOLD_DEBUG

#include "editor/state.h"
#include "match/bot/bot.h"
#include "editor/ui_helpers.h"

static const Rect MENU_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (300 / 2),
    .y = 64,
    .w = 300,
    .h = 256
};

static const BotSquadType SQUAD_TYPE_VALUES[] = {
    BOT_SQUAD_TYPE_DEFEND,
    BOT_SQUAD_TYPE_LANDMINES,
    BOT_SQUAD_TYPE_PATROL
};
static const size_t squad_type_values_count = sizeof(SQUAD_TYPE_VALUES) / sizeof(SQUAD_TYPE_VALUES[0]);

EditorMenuSquad::EditorMenuSquad(const ScenarioSquad& squad) {
    squad_name = std::string(squad.name);
    squad_player = squad.player_id - 1;

    for (squad_type_index = 0; squad_type_index < squad_type_values_count; squad_type_index++) {
        if (squad.type == SQUAD_TYPE_VALUES[squad_type_index]) {
            break;
        }
    }
    GOLD_ASSERT(squad_type_index != squad_type_values_count);
}

const char* EditorMenuSquad::get_header_text() const {
    return "Edit Squad";
}

Rect EditorMenuSquad::get_rect() const {
    return MENU_RECT;
}

void EditorMenuSquad::child_update(EditorState* state) {
    ui_begin_column(state->ui_context, ivec2(MENU_RECT.x + 8, MENU_RECT.y + 30), 4);
        // Name
        ui_text_input(state->ui_context, "Name: ", ivec2(MENU_RECT.w - 32, 24), &squad_name, MAX_USERNAME_LENGTH);

        // Player dropdown
        std::vector<std::string> squad_player_items;
        for (uint8_t player_id = 1; player_id < MAX_PLAYERS; player_id++) {
            char player_text[64];
            sprintf(player_text, "%u: %s", player_id, state->scenario->players[player_id].name);
            squad_player_items.push_back(std::string(player_text));
        }
        editor_ui_dropdown(state->ui_context, "Player:", &squad_player, squad_player_items, MENU_RECT);

        // Squad type
        std::vector<std::string> squad_type_items;
        for (uint32_t index = 0; index < squad_type_values_count; index++) {
            BotSquadType squad_type = SQUAD_TYPE_VALUES[index];
            squad_type_items.push_back(std::string(bot_squad_type_str(squad_type)));
        }
        editor_ui_dropdown(state->ui_context, "Type:", &squad_type_index, squad_type_items, MENU_RECT);
    ui_end_container(state->ui_context);
}

void EditorMenuSquad::on_submit(EditorState* state) {
    const ScenarioSquad previous_squad = state->scenario->squads[state->tool.squads.squad_index];
    ScenarioSquad edited_squad = previous_squad;
    strncpy(edited_squad.name, squad_name.c_str(), MAX_USERNAME_LENGTH);
    edited_squad.player_id = squad_player + 1;
    edited_squad.type = SQUAD_TYPE_VALUES[squad_type_index];
    if (edited_squad.player_id != previous_squad.player_id) {
        edited_squad.entity_count = 0;
    }
    if (edited_squad.type != previous_squad.type) {
        edited_squad.patrol_cell = ivec2(-1, -1);
    }

    if (scenario_squads_are_equal(edited_squad, previous_squad)) {
        return;
    }

    editor_state_do_action(state, (EditorActionEditSquad) {
        .index = state->tool.squads.squad_index,
        .previous_value = previous_squad,
        .new_value = edited_squad
    });
}

#endif
