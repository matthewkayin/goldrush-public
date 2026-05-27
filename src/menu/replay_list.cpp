#include "menu.h"

#include "core/filesystem.h"
#include "util/util.h"

static const char* REPLAY_LIST_NO_ITEMS_MESSAGE = "No replay files found.";

static const Rect REPLAY_CONFIRM_RECT = {
    .x = (SCREEN_WIDTH / 2) - (206 / 2),
    .y = 84,
    .w = 206,
    .h = 66
};

SDL_EnumerationResult menu_replay_list_on_replay_found(void* state_ptr, const char* /*dirname*/, const char* filename) {
    MenuItemList* item_list = (MenuItemList*)state_ptr;

    // Ignore non-replay files
    if (!string_ends_with(filename, ".rep")) {
        return SDL_ENUM_CONTINUE;
    }

    // Ignore files which don't match the search query
    if (strstr(filename, item_list->search_query.c_str()) == NULL) {
        return SDL_ENUM_CONTINUE;
    }

    item_list->items.push_back(std::string(filename));

    return SDL_ENUM_CONTINUE;
}


void menu_replay_list_init(MenuState* state) {
    state->item_list = menu_item_list_init(REPLAY_LIST_NO_ITEMS_MESSAGE);
    menu_replay_list_search(state);
}

void menu_replay_list_search(MenuState* state) {
    menu_item_list_clear(state->item_list);
    std::string replay_folder_path = filesystem_get_data_path() + "replays";
    SDL_EnumerateDirectory(replay_folder_path.c_str(), menu_replay_list_on_replay_found, (void*)&state->item_list);
}

void menu_replay_list_update(MenuState* state) {
    menu_item_list_update(state->item_list, state->ui_context);

    // Search
    if (state->item_list.user_requests_search) {
        menu_replay_list_search(state);
    }

    // Button row
    ui_begin_row(state->ui_context, ivec2(BUTTON_X, ITEM_LIST_RECT.y + ITEM_LIST_RECT.h + 4), 4);
        if (ui_button(state->ui_context, "Back")) {
            menu_set_mode(state, MENU_MODE_MAIN);
        }
        if (ui_button(state->ui_context, "Clear Autosaves")) {
            state->mode = MENU_MODE_REPLAY_CONFIRM_CLEAR;
        }
        if (state->item_list.item_selected != MENU_ITEM_NONE) {
            if (ui_button(state->ui_context, "Watch")) {
                menu_set_mode(state, MENU_MODE_LOAD_REPLAY);
            }
            if (ui_button(state->ui_context, "Rename")) {
                state->mode = MENU_MODE_REPLAY_RENAME;
                state->item_list_item_to_rename = std::string(menu_get_selected_replay_filename(state));
            }
            if (ui_button(state->ui_context, "Delete")) {
                std::string replay_path = filesystem_get_data_path() + FILESYSTEM_REPLAY_FOLDER_NAME + menu_get_selected_replay_filename(state);
                state->item_list.items.erase(state->item_list.items.begin() + state->item_list.item_selected);
                state->item_list.item_selected = MENU_ITEM_NONE;
                if (state->item_list.page > menu_item_list_get_page_count(state->item_list) - 1) {
                    state->item_list.page--;
                }
                SDL_RemovePath(replay_path.c_str());
            }
        }
    ui_end_container(state->ui_context);

    // Replay rename dialog
    if (state->mode == MENU_MODE_REPLAY_RENAME) {
        menu_replay_list_rename_dialog(state);
    }

    // Replay confirm dialog
    if (state->mode == MENU_MODE_REPLAY_CONFIRM_CLEAR) {
        menu_replay_list_confirm_dialog(state);
    }
}

void menu_replay_list_rename_dialog(MenuState* state) {
    state->ui_context.input_enabled = true;
    ui_screen_shade(state->ui_context);
    ui_begin_column(state->ui_context, ivec2((SCREEN_WIDTH / 2) - 150, (SCREEN_HEIGHT / 2) - 64), 4);
        ui_text_input(state->ui_context, "Rename: ", ivec2(300, 24), &state->item_list_item_to_rename, NETWORK_LOBBY_NAME_BUFFER_SIZE - 1);

        ui_begin_row(state->ui_context, ivec2(0, 0), 4);
            if (ui_button(state->ui_context, "Back")) {
                state->mode = MENU_MODE_REPLAYS;
            }
            if (ui_button(state->ui_context, "OK")) {
                if (state->item_list_item_to_rename.empty()) {
                    menu_show_status(state, "Replay name cannot be empty.");
                } else {
                    if (!string_ends_with(state->item_list_item_to_rename, ".rep")) {
                        state->item_list_item_to_rename += ".rep";
                    }
                    std::string old_filename = filesystem_get_data_path() + "replays/" + menu_get_selected_replay_filename(state);
                    std::string new_filename = filesystem_get_data_path() + "replays/" + state->item_list_item_to_rename;
                    rename(old_filename.c_str(), new_filename.c_str());
                    state->item_list.items[state->item_list.item_selected] = state->item_list_item_to_rename;
                    state->mode = MENU_MODE_REPLAYS;
                }
            }
        ui_end_container(state->ui_context);
    ui_end_container(state->ui_context);
}

void menu_replay_list_confirm_dialog(MenuState* state) {
    state->ui_context.input_enabled = true;
    ui_screen_shade(state->ui_context);

    ui_frame_rect(state->ui_context, REPLAY_CONFIRM_RECT);
    const char* confirm_text = "Delete all \"autosave\" replays?";
    ivec2 header_text_size = render_get_text_size(FONT_HACK_GOLD, confirm_text);

    ui_element_position(state->ui_context, ivec2(REPLAY_CONFIRM_RECT.x + (REPLAY_CONFIRM_RECT.w / 2) - (header_text_size.x / 2), REPLAY_CONFIRM_RECT.y + 12));
    ui_text(state->ui_context, FONT_HACK_GOLD, confirm_text);

    ui_element_position(state->ui_context, ui_button_position_frame_bottom_left(REPLAY_CONFIRM_RECT));
    if (ui_button(state->ui_context, "Back")) {
        state->mode = MENU_MODE_REPLAYS;
    }

    ui_element_position(state->ui_context, ui_button_position_frame_bottom_right(REPLAY_CONFIRM_RECT, "Yes"));
    if (ui_button(state->ui_context, "Yes")) {
        for (const std::string& replay_filename : state->item_list.items) {
            if (replay_filename.rfind(FILESYSTEM_REPLAY_AUTOSAVE_PREFIX, 0) == 0) {
                std::string replay_path = filesystem_get_data_path() + FILESYSTEM_REPLAY_FOLDER_NAME + replay_filename;
                SDL_RemovePath(replay_path.c_str());
            }
        }
        menu_set_mode(state, MENU_MODE_REPLAYS);
    }
}
