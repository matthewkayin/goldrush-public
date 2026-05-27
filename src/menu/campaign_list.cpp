#include "core/ui.h"
#include "menu.h"

#include "core/logger.h"
#include "core/filesystem.h"
#include "network/network.h"
#include "menu/item_list.h"
#include "menu/types.h"
#include "util/json.h"
#include <cstdint>

static const char* CAMPAIGN_LIST_NO_ITEMS_MESSAGE = "Click 'New' to begin a new campaign.";

static const Rect CAMPAIGN_INFO_RECT = {
    .x = ITEM_LIST_RECT.x + ITEM_LIST_RECT.w + 2,
    .y = ITEM_LIST_RECT.y,
    .w = 172 - 22,
    .h = 56
};

std::string menu_get_campaign_saves_path() {
    return filesystem_get_data_path() + "campaign_saves.json";
}

void menu_load_campaign_saves(MenuState* state) {
    // Start by clearing the campaign saves
    state->campaign_saves.clear();

    // Open campaign saves file
    Json* campaign_saves_json = json_read(menu_get_campaign_saves_path().c_str());
    if (campaign_saves_json == NULL) {
        log_info("No campaign saves file found.");
        return;
    }

    // Get the entries list
    Json* campaign_saves_entries_json = json_object_get(campaign_saves_json, "entries");
    if (campaign_saves_entries_json == NULL || campaign_saves_entries_json->type != JSON_TYPE_ARRAY) {
        log_warn("Campaign saves file invalid.");
        json_free(campaign_saves_json);
        return;
    }

    // Parse each entry and add it to the saves list
    for (uint32_t entry_index = 0; entry_index < campaign_saves_entries_json->array.length; entry_index++) {
        Json* entry_json = campaign_saves_entries_json->array.values[entry_index];
        Json* entry_name_json = json_object_get(entry_json, "name");
        Json* entry_missions_completed_json = json_object_get(entry_json, "missions_completed");
        Json* entry_playtime_seconds_json = json_object_get(entry_json, "playtime_seconds");
        const bool is_valid_entry =
            entry_name_json != NULL && entry_name_json->type == JSON_TYPE_STRING &&
            entry_missions_completed_json != NULL && entry_missions_completed_json->type == JSON_TYPE_NUMBER &&
            entry_playtime_seconds_json != NULL && entry_playtime_seconds_json-> type == JSON_TYPE_NUMBER;
        if (!is_valid_entry) {
            log_warn("Found invalid campaign saves entry. Skipping...");
            continue;
        }

        CampaignSaveEntry entry;
        strncpy(entry.name, entry_name_json->string.value, sizeof(entry.name));
        entry.missions_completed = (uint32_t)entry_missions_completed_json->number.value;
        entry.playtime_seconds = (uint32_t)entry_playtime_seconds_json->number.value;
        state->campaign_saves.push_back(entry);
        log_debug("Loaded campaign save %s missions completed %u.", entry.name, entry.missions_completed);
    }

    json_free(campaign_saves_json);
    log_info("Loaded campaign saves.");
}

void menu_save_campaign_saves(const MenuState* state) {
    Json* campaign_saves_json = json_object();

    Json* campaign_saves_entries_json = json_array();
    for (const CampaignSaveEntry& entry : state->campaign_saves) {
        Json* entry_json = json_object();
        json_object_set_string(entry_json, "name", entry.name);
        json_object_set_number(entry_json, "missions_completed", entry.missions_completed);
        json_object_set_number(entry_json, "playtime_seconds", entry.playtime_seconds);
        json_array_push(campaign_saves_entries_json, entry_json);
    }
    json_object_set(campaign_saves_json, "entries", campaign_saves_entries_json);

    bool success = json_write(campaign_saves_json, menu_get_campaign_saves_path().c_str());
    json_free(campaign_saves_json);

    if (success) {
        log_info("Saved campaign saves.");
    } else {
        log_error("Failed to save campaign saves.");
    }
}

void menu_campaign_list_init(MenuState* state) {
    menu_load_campaign_saves(state);
    state->item_list = menu_item_list_init(CAMPAIGN_LIST_NO_ITEMS_MESSAGE);
    menu_campaign_list_search(state);
}

void menu_campaign_list_search(MenuState* state) {
    state->item_list_index_to_campaign_save_index.clear();

    for (uint32_t entry_index = 0; entry_index < state->campaign_saves.size(); entry_index++) {
        const CampaignSaveEntry& entry = state->campaign_saves[entry_index];

        if (strstr(entry.name, state->item_list.search_query.c_str()) != NULL) {
            state->item_list.items.push_back(menu_campaign_list_item_str(entry));
            state->item_list_index_to_campaign_save_index.push_back(entry_index);
        }
    }
}

void menu_campaign_list_update(MenuState* state) {
    menu_item_list_update(state->item_list, state->ui_context);

    // Search
    if (state->item_list.user_requests_search) {
        menu_campaign_list_search(state);
    }

    // Sidebar
    if (state->item_list.item_selected != MENU_ITEM_NONE) {
        const uint32_t selected_save_index = menu_campaign_list_get_selected_campaign_save(state);
        const CampaignSaveEntry& entry = state->campaign_saves[selected_save_index];

        ui_frame_rect(state->ui_context, CAMPAIGN_INFO_RECT);

        ui_begin_column(state->ui_context, ivec2(CAMPAIGN_INFO_RECT.x + 8, CAMPAIGN_INFO_RECT.y + 14), 4);
            // Missions completed
            char missions_completed_text[128];
            if (entry.missions_completed == CAMPAIGN_MISSION_COUNT) {
                sprintf(missions_completed_text, "Complete!");
            } else {
                sprintf(missions_completed_text, "Missions Completed: %u", entry.missions_completed);
            }
            ui_text(state->ui_context, FONT_HACK_GOLD, missions_completed_text);

            // Playtime
            Time playtime = Time::from_seconds(entry.playtime_seconds);
            char playtime_text[128];
            sprintf(playtime_text, "Playtime: %i:%02i:%02i", playtime.hours, playtime.minutes, playtime.seconds);
            ui_text(state->ui_context, FONT_HACK_GOLD, playtime_text);
        ui_end_container(state->ui_context);
    }

    // Button row
    ui_begin_row(state->ui_context, ivec2(BUTTON_X, ITEM_LIST_RECT.y + ITEM_LIST_RECT.h + 4), 4);
        if (ui_button(state->ui_context, "Back")) {
            menu_set_mode(state, MENU_MODE_SINGLEPLAYER);
        }
        if (ui_button(state->ui_context, "New")) {
            state->campaign_name = std::string(network_get_username()) + "'s Campaign";
            menu_set_mode(state, MENU_MODE_CAMPAIGN_LIST_NEW);
        }
        if (state->item_list.item_selected != MENU_ITEM_NONE) {
            if (ui_button(state->ui_context, "Continue")) {
                menu_set_mode(state, MENU_MODE_CAMPAIGN);
            }
            if (ui_button(state->ui_context, "Rename")) {
                state->mode = MENU_MODE_CAMPAIGN_LIST_RENAME;
                uint32_t selected_save_index = menu_campaign_list_get_selected_campaign_save(state);
                state->item_list_item_to_rename = state->campaign_saves[selected_save_index].name;
            }
            if (ui_button(state->ui_context, "Delete")) {
                state->mode = MENU_MODE_CAMPAIGN_LIST_DELETE;
            }
        }
    ui_end_container(state->ui_context);

    // Campaign new dialog
    if (state->mode == MENU_MODE_CAMPAIGN_LIST_NEW) {
        menu_campaign_list_new_dialog(state);
    }

    // Campaign rename dialog
    if (state->mode == MENU_MODE_CAMPAIGN_LIST_RENAME) {
        menu_campaign_list_rename_dialog(state);
    }

    // Cmapaign delete confirm dialog
    if (state->mode == MENU_MODE_CAMPAIGN_LIST_DELETE) {
        menu_campaign_list_delete_dialog(state);
    }
}

std::string menu_campaign_list_item_str(const CampaignSaveEntry& entry) {
    return std::string(entry.name);
}

uint32_t menu_campaign_list_get_selected_campaign_save(const MenuState* state) {
    return state->item_list_index_to_campaign_save_index[state->item_list.item_selected];
}

void menu_campaign_list_new_dialog(MenuState* state) {
    static const Rect DIALOG_RECT = {
        .x = (SCREEN_WIDTH / 2) - (348 / 2),
        .y = 64,
        .w = 348,
        .h = 96
    };

    state->ui_context.input_enabled = true;
    ui_screen_shade(state->ui_context);

    // Frame
    ui_frame_rect(state->ui_context, DIALOG_RECT);

    // Header text
    ivec2 header_text_size = render_get_text_size(FONT_HACK_GOLD, "New Campaign");
    ui_element_position(state->ui_context, ivec2(DIALOG_RECT.x + (DIALOG_RECT.w / 2) - (header_text_size.x / 2), DIALOG_RECT.y + 6));
    ui_text(state->ui_context, FONT_HACK_GOLD, "New Campaign");

    // Content
    ui_begin_column(state->ui_context, ivec2(DIALOG_RECT.x + 8, DIALOG_RECT.y + 26), 6);
        ui_text_input(state->ui_context, "Name: ", ivec2(332, 24), &state->campaign_name, CAMPAIGN_SAVE_ENTRY_NAME_BUFFER_SIZE - 1);
        menu_ui_lobby_privacy(state);
    ui_end_container(state->ui_context);

    // Back button
    ui_element_position(state->ui_context, ui_button_position_frame_bottom_left(DIALOG_RECT));
    if (ui_button(state->ui_context, "Back")) {
        state->mode = MENU_MODE_CAMPAIGN_LIST;
    }

    // Create button
    ui_element_position(state->ui_context, ui_button_position_frame_bottom_right(DIALOG_RECT, "Create"));
    if (ui_button(state->ui_context, "Create")) {
        if (state->campaign_name.length() == 0) {
            menu_show_status(state, "Please enter a campaign name.");
        } else {
            // Create a new campaign saves entry
            CampaignSaveEntry entry;
            strncpy(entry.name, state->campaign_name.c_str(), sizeof(entry.name));
            entry.missions_completed = 0;
            entry.playtime_seconds = 0;
            state->campaign_saves.push_back(entry);

            // Save the campaign file
            menu_save_campaign_saves(state);

            // Add the campaign to the item list and select it
            state->item_list.items.push_back(menu_campaign_list_item_str(entry));
            state->item_list_index_to_campaign_save_index.push_back(state->campaign_saves.size() - 1);
            state->item_list.item_selected = state->item_list.items.size() - 1;

            // TODO: go into campaign mode instead
            menu_set_mode(state, MENU_MODE_CAMPAIGN);
        }
    }
}

void menu_campaign_list_rename_dialog(MenuState* state) {
    state->ui_context.input_enabled = true;
    ui_screen_shade(state->ui_context);
    ui_begin_column(state->ui_context, ivec2((SCREEN_WIDTH / 2) - 150, (SCREEN_HEIGHT / 2) - 64), 4);
        ui_text_input(state->ui_context, "Rename: ", ivec2(300, 24), &state->item_list_item_to_rename, CAMPAIGN_SAVE_ENTRY_NAME_BUFFER_SIZE - 1);

        ui_begin_row(state->ui_context, ivec2(0, 0), 4);
            if (ui_button(state->ui_context, "Back")) {
                state->mode = MENU_MODE_CAMPAIGN_LIST;
            }
            if (ui_button(state->ui_context, "OK")) {
                if (state->item_list_item_to_rename.empty()) {
                    menu_show_status(state, "Campaign name cannot be empty.");
                } else {
                    uint32_t selected_save_index = menu_campaign_list_get_selected_campaign_save(state);
                    strncpy(state->campaign_saves[selected_save_index].name, state->item_list_item_to_rename.c_str(), CAMPAIGN_SAVE_ENTRY_NAME_BUFFER_SIZE);
                    state->item_list.items[state->item_list.item_selected] = menu_campaign_list_item_str(state->campaign_saves[selected_save_index]);
                    menu_save_campaign_saves(state);
                    state->mode = MENU_MODE_CAMPAIGN_LIST;
                }
            }
        ui_end_container(state->ui_context);
    ui_end_container(state->ui_context);
}

void menu_campaign_list_delete_dialog(MenuState* state) {
    // Determine header text size
    const uint32_t selected_save_index = menu_campaign_list_get_selected_campaign_save(state);
    std::string delete_text = "Delete the campaign '" + std::string(state->campaign_saves[selected_save_index].name) + "'?";
    ivec2 header_text_size = render_get_text_size(FONT_HACK_GOLD, delete_text.c_str());

    // Delete rect
    const int delete_rect_width = header_text_size.x + 32;
    const Rect DELETE_RECT = {
        .x = (SCREEN_WIDTH / 2) - (delete_rect_width / 2),
        .y = 64,
        .w = delete_rect_width,
        .h = 64
    };

    // Create dialog rect
    state->ui_context.input_enabled = true;
    ui_screen_shade(state->ui_context);
    ui_frame_rect(state->ui_context, DELETE_RECT);

    // Header text
    ui_element_position(state->ui_context, ivec2(DELETE_RECT.x + (DELETE_RECT.w / 2) - (header_text_size.x / 2), DELETE_RECT.y + 12));
    ui_text(state->ui_context, FONT_HACK_GOLD, delete_text.c_str());

    // Back button
    ui_element_position(state->ui_context, ui_button_position_frame_bottom_left(DELETE_RECT));
    if (ui_button(state->ui_context, "Back")) {
        state->mode = MENU_MODE_CAMPAIGN_LIST;
    }

    // Delete button
    ui_element_position(state->ui_context, ui_button_position_frame_bottom_right(DELETE_RECT, "Delete"));
    if (ui_button(state->ui_context, "Delete")) {
        state->campaign_saves.erase(state->campaign_saves.begin() + selected_save_index);
        menu_save_campaign_saves(state);
        menu_set_mode(state, MENU_MODE_CAMPAIGN_LIST);
    }
}
