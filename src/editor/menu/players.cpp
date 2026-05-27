#include "players.h"

#ifdef GOLD_DEBUG

#include "core/input.h"
#include "editor/state.h"
#include "editor/ui_helpers.h"
#include "util/bitflag.h"
#include "match/state/upgrade.h"
#include <algorithm>

static const Rect MENU_RECT = (Rect) {
     .x = (SCREEN_WIDTH / 2) - (300 / 2),
     .y = 32,
     .w = 300,
     .h = 296
 };

static const int ICON_ROW_SIZE = 8;
static const int MAX_VISIBLE_SCROLL_ITEMS = 8;

static const UiSliderParams STARTING_GOLD_SLIDER_PARAMS = (UiSliderParams) {
    .display = UI_SLIDER_DISPLAY_RAW_VALUE,
    .size = UI_SLIDER_SIZE_NORMAL,
    .min = 0,
    .max = 1500,
    .step = 50
};

static const UiSliderParams MACRO_CYCLE_COOLDOWN_SLIDER_PARAMS = (UiSliderParams) {
    .display = UI_SLIDER_DISPLAY_RAW_VALUE,
    .size = UI_SLIDER_SIZE_NORMAL,
    .min = 0,
    .max = 180,
    .step = 30
};

static const std::vector<std::string> YES_NO_DROPDOWN_OPTIONS = { "No", "Yes" };
static const std::vector<std::string> TARGET_BASE_COUNT_DROPDOWN_OPTIONS = { "0", "1", "2", "3", "Match Enemy" };

EditorMenuPlayers::EditorMenuPlayers(const Scenario* scenario) {
    mode = EDITOR_MENU_PLAYERS_MODE_PLAYERS;
    selected_player_id = 0;
    selected_player_name_string = std::string(scenario->players[selected_player_id].name);
    scroll = 0;
}

const char* EditorMenuPlayers::get_header_text() const {
    return "Edit Players";
}

Rect EditorMenuPlayers::get_rect() const {
    return MENU_RECT;
}

void EditorMenuPlayers::child_update(EditorState* state) {
    switch (mode) {
        case EDITOR_MENU_PLAYERS_MODE_PLAYERS: {
            mode_players_update(state);
            break;
        }
        case EDITOR_MENU_PLAYERS_MODE_TECH: {
            mode_tech_update(state);
            break;
        }
    }
}

void EditorMenuPlayers::mode_players_update(EditorState* state) {
    ui_begin_column(state->ui_context, ivec2(MENU_RECT.x + 8, MENU_RECT.y + 30), 4);
        // Selection dropdown items
        std::vector<std::string> player_dropdown_items;
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            char item_text[64];
            sprintf(item_text, "%u: %s", player_id, state->scenario->players[player_id].name);
            player_dropdown_items.push_back(item_text);
        }

        // Selection dropdown
        uint32_t previous_selected_player_id = selected_player_id;
        if (editor_ui_dropdown(state->ui_context, "Player: ", &selected_player_id, player_dropdown_items, MENU_RECT)) {
            if (input_is_text_input_active()) {
                input_stop_text_input();
                strncpy(state->scenario->players[previous_selected_player_id].name, selected_player_name_string.c_str(), MAX_USERNAME_LENGTH);
            }

            selected_player_name_string = std::string(state->scenario->players[selected_player_id].name);
            scroll = 0;
        }

        // Name
        if (selected_player_id != 0) {
            ui_text_input(state->ui_context, "Name: ", ivec2(MENU_RECT.w - 32, 24), &selected_player_name_string, MAX_USERNAME_LENGTH);
        }

        std::vector<std::function<void()>> scroll_items;

        // Team
        scroll_items.push_back([this, state]() {
            uint32_t selected_value = state->scenario->players[selected_player_id].team;
            if (editor_ui_dropdown(state->ui_context, "Team: ", &selected_value, { "1", "2", "3", "4" }, MENU_RECT)) {
                state->scenario->players[selected_player_id].team = (uint8_t)selected_value;
            }
        });

        // Recolor ID
        scroll_items.push_back([this, state]() {
            const std::vector<std::string> PLAYER_COLOR_STRS = { "Blue", "Red", "Green", "Purple" };
            uint32_t selected_value = state->scenario->players[selected_player_id].recolor_id;
            if (editor_ui_dropdown(state->ui_context, "Color: ", &selected_value, PLAYER_COLOR_STRS, MENU_RECT)) {
                state->scenario->players[selected_player_id].recolor_id = (uint8_t)selected_value;
            }
        });

        // Starting gold
        scroll_items.push_back([this, state]() {
            editor_ui_slider(state->ui_context, "Starting Gold:", &state->scenario->players[selected_player_id].starting_gold, STARTING_GOLD_SLIDER_PARAMS, MENU_RECT);
        });

        // Bot config options
        if (selected_player_id != 0) {
            BotConfig& bot_config = state->scenario->bot_config[selected_player_id - 1];

            scroll_items.push_back([state, &bot_config]() {
                std::vector<std::string> opener_items;
                for (uint32_t opener = 0; opener < BOT_OPENER_COUNT; opener++) {
                    opener_items.push_back(std::string(bot_config_opener_str((BotOpener)opener)));
                }
                editor_ui_dropdown(state->ui_context, "Opener:", (uint32_t*)&bot_config.opener, opener_items, MENU_RECT);
            });

            scroll_items.push_back([state, &bot_config]() {
                std::vector<std::string> unit_comp_items;
                for (uint32_t unit_comp = BOT_UNIT_COMP_NONE + 1; unit_comp < BOT_UNIT_COMP_COUNT; unit_comp++) {
                    unit_comp_items.push_back(std::string(bot_config_unit_comp_str((BotUnitComp)unit_comp)));
                }
                uint32_t selection = bot_config.preferred_unit_comp - 1;
                if (editor_ui_dropdown(state->ui_context, "Unit Comp:", &selection, unit_comp_items, MENU_RECT)) {
                    bot_config.preferred_unit_comp = (BotUnitComp)(selection + 1);
                }
            });

            for (uint32_t flag_index = 0; flag_index < BOT_CONFIG_FLAG_COUNT; flag_index++) {
                scroll_items.push_back([this, state, &bot_config, flag_index]() {
                    uint32_t flag = 1U << flag_index;
                    mode_players_bot_config_dropdown(state->ui_context, bot_config, flag);
                });
            }

            scroll_items.push_back([state, &bot_config]() {
                uint32_t target_base_count_selection = bot_config.target_base_count == BOT_TARGET_BASE_COUNT_MATCH_ENEMY
                    ? TARGET_BASE_COUNT_DROPDOWN_OPTIONS.size() - 1
                    : bot_config.target_base_count;
                if (editor_ui_dropdown(state->ui_context, "Target Base Count:", &target_base_count_selection, TARGET_BASE_COUNT_DROPDOWN_OPTIONS, MENU_RECT)) {
                    bot_config.target_base_count = target_base_count_selection == TARGET_BASE_COUNT_DROPDOWN_OPTIONS.size() - 1
                        ? BOT_TARGET_BASE_COUNT_MATCH_ENEMY
                        : target_base_count_selection;
                }
            });

            scroll_items.push_back([state, &bot_config]() {
                uint32_t macro_cycle_cooldown = bot_config.macro_cycle_cooldown / UPDATES_PER_SECOND;
                if (editor_ui_slider(state->ui_context, "Macro Cooldown:", &macro_cycle_cooldown, MACRO_CYCLE_COOLDOWN_SLIDER_PARAMS, MENU_RECT)) {
                    bot_config.macro_cycle_cooldown = macro_cycle_cooldown * UPDATES_PER_SECOND;
                }
            });
        }

        scroll_items.push_back([this, state]() {
            // Edit tech button
            if (editor_ui_prompt_and_button(state->ui_context, "Allowed Tech:", "Edit", MENU_RECT)) {
                mode = EDITOR_MENU_PLAYERS_MODE_TECH;
            }
        });

        for (int index = scroll; index < std::min(scroll + MAX_VISIBLE_SCROLL_ITEMS, (int)scroll_items.size()); index++) {
            scroll_items[index]();
        }

        const int scroll_max = std::max(0, (int)scroll_items.size() - MAX_VISIBLE_SCROLL_ITEMS);
        scroll = std::clamp(scroll - input_get_mouse_scroll(), 0, scroll_max);
    ui_end_container(state->ui_context);
}

void EditorMenuPlayers::mode_players_bot_config_dropdown(UiContext& ui_context, BotConfig& bot_config, uint32_t flag) const {
    char prompt[64];
    sprintf(prompt, "%s", bot_config_flag_str(flag));
    uint32_t value = (uint32_t)bitflag_check(bot_config.flags, flag);
    if (editor_ui_dropdown(ui_context, prompt, &value, YES_NO_DROPDOWN_OPTIONS, MENU_RECT)) {
        bitflag_set(&bot_config.flags, flag, (bool)value);
    }
}

void EditorMenuPlayers::mode_tech_update(EditorState* state) {
    ui_begin_column(state->ui_context, ivec2(MENU_RECT.x + 8, MENU_RECT.y + 30), 4);
        // Player name text
        char player_name_text[128];
        sprintf(player_name_text, "Edit Tech for %u: %s", selected_player_id, state->scenario->players[selected_player_id].name);
        ui_text(state->ui_context, FONT_HACK_GOLD, player_name_text);

        // Allowed tech
        const uint32_t TECH_COUNT = ENTITY_TYPE_COUNT + UPGRADE_COUNT;
        uint32_t row_count = TECH_COUNT / ICON_ROW_SIZE;
        if (TECH_COUNT % ICON_ROW_SIZE != 0) {
            row_count++;
        }

        for (uint32_t row = 0; row < row_count; row++) {
            ui_begin_row(state->ui_context, ivec2(4, 0), 2);
                for (uint32_t col = 0; col < ICON_ROW_SIZE; col++) {
                    uint32_t tech_index = col + (row * ICON_ROW_SIZE);
                    if (tech_index >= TECH_COUNT) {
                        continue;
                    }

                    const SpriteName icon = tech_index < ENTITY_TYPE_COUNT
                        ? entity_get_data((EntityType)tech_index).icon
                        : upgrade_get_data(1U << (tech_index - ENTITY_TYPE_COUNT)).icon;
                    bool is_tech_allowed = mode_tech_is_tech_allowed(state->scenario, tech_index);
                    if (ui_icon_button(state->ui_context, icon, is_tech_allowed)) {
                        mode_tech_set_tech_allowed(state->scenario, tech_index, !is_tech_allowed);
                    }
                }
            ui_end_container(state->ui_context);
        }
    ui_end_container(state->ui_context);
}

bool EditorMenuPlayers::mode_tech_is_tech_allowed(const Scenario* scenario, uint32_t tech_index) const {
    if (tech_index < ENTITY_TYPE_COUNT) {
        return selected_player_id == 0
            ? scenario->player_allowed_entities[tech_index]
            : scenario->bot_config[selected_player_id - 1].is_entity_allowed[tech_index];
    } else {
        uint32_t upgrade_index = tech_index - ENTITY_TYPE_COUNT;
        uint32_t upgrade = (1U << upgrade_index);
        return selected_player_id == 0
            ? (scenario->player_allowed_upgrades & upgrade) == upgrade
            : (scenario->bot_config[selected_player_id - 1].allowed_upgrades & upgrade) == upgrade;
    }
}

void EditorMenuPlayers::mode_tech_set_tech_allowed(Scenario* scenario, uint32_t tech_index, bool value) {
    if (tech_index < ENTITY_TYPE_COUNT) {
        bool* tech_allowed_ptr = selected_player_id == 0
            ? scenario->player_allowed_entities
            : scenario->bot_config[selected_player_id - 1].is_entity_allowed;
        tech_allowed_ptr[tech_index] = value;
    } else {
        uint32_t upgrade_index = tech_index - ENTITY_TYPE_COUNT;
        uint32_t upgrade = (1U << upgrade_index);
        uint32_t* allowed_upgrades_ptr = selected_player_id == 0
            ? &scenario->player_allowed_upgrades
            : &scenario->bot_config[selected_player_id - 1].allowed_upgrades;

        bitflag_set(allowed_upgrades_ptr, upgrade, value);
    }
}

void EditorMenuPlayers::on_submit(EditorState* state) {
    if (mode == EDITOR_MENU_PLAYERS_MODE_TECH) {
        mode = EDITOR_MENU_PLAYERS_MODE_PLAYERS;
        _is_open = true;
        return;
    }

    strncpy(state->scenario->players[selected_player_id].name, selected_player_name_string.c_str(), MAX_USERNAME_LENGTH);
}

#endif
