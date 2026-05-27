#include "item_list.h"

#include "core/input.h"
#include "network/types.h"
#include "render/render.h"

static const uint32_t ITEM_LIST_PAGE_SIZE = 9U;
static const uint32_t ITEM_LIST_SEARCH_QUERY_MAX_LENGTH = NETWORK_LOBBY_NAME_BUFFER_SIZE - 1;

MenuItemList menu_item_list_init(const char* no_items_message) {
    MenuItemList item_list;
    item_list.no_items_message = no_items_message;
    item_list.item_selected = MENU_ITEM_NONE;
    item_list.page = 0;

    return item_list;
}

void menu_item_list_clear(MenuItemList& item_list) {
    item_list.item_selected = MENU_ITEM_NONE;
    item_list.page = 0;
    item_list.items.clear();
}

void menu_item_list_update(MenuItemList& item_list, UiContext& ui_context) {
    item_list.user_requests_search = false;

    ui_frame_rect(ui_context, ITEM_LIST_RECT);

    if (item_list.items.empty()) {
        const int message_width = render_get_text_size(FONT_HACK_GOLD, item_list.no_items_message.c_str()).x;
        ui_element_position(ui_context, ivec2(
            ITEM_LIST_RECT.x + (ITEM_LIST_RECT.w / 2) - (message_width / 2),
            ITEM_LIST_RECT.y + 8
        ));
        ui_text(ui_context, FONT_HACK_GOLD, item_list.no_items_message.c_str());
    }

    // Search and top buttons
    ui_begin_row(ui_context, ivec2(44, 4), 4);
        // Search
        ui_text_input(ui_context, "Search: ", ivec2(300, 24), &item_list.search_query, ITEM_LIST_SEARCH_QUERY_MAX_LENGTH);

        // Refresh button
        const bool pressed_enter_on_search =
            input_is_action_just_pressed(INPUT_ACTION_ENTER) &&
            input_is_text_input_active();
        if (ui_sprite_button(ui_context, SPRITE_UI_BUTTON_REFRESH, false, false) || pressed_enter_on_search) {
            item_list.user_requests_search = true;
            item_list.page = 0;
            item_list.item_selected = MENU_ITEM_NONE;
            item_list.items.clear();
        }

        // Arrow buttons
        if (ui_sprite_button(ui_context, SPRITE_UI_BUTTON_ARROW, item_list.page == 0, true)) {
            item_list.page--;
        }
        if (ui_sprite_button(ui_context, SPRITE_UI_BUTTON_ARROW, item_list.page == menu_item_list_get_page_count(item_list) - 1, false)) {
            item_list.page++;
        }
    ui_end_container(ui_context);

    // Item list
    uint32_t base_index = (item_list.page * ITEM_LIST_PAGE_SIZE);
    for (uint32_t item_index = base_index; item_index < std::min(base_index + ITEM_LIST_PAGE_SIZE, (uint32_t)item_list.items.size()); item_index++) {
        char item_text[128];
        if (item_index == item_list.item_selected) {
            sprintf(item_text, "* %s *", item_list.items[item_index].c_str());
        } else {
            sprintf(item_text, "  %s  ", item_list.items[item_index].c_str());
        }

        ivec2 text_frame_size = ui_text_frame_size(item_text);
        ui_element_position(ui_context, ivec2(
            ITEM_LIST_RECT.x + (ITEM_LIST_RECT.w / 2) - (text_frame_size.x / 2),
            ITEM_LIST_RECT.y + 12 + (20 * (int)(item_index - base_index))
        ));
        if (ui_text_frame(ui_context, item_text, false)) {
            item_list.item_selected = item_index;
        }
    }
}

uint32_t menu_item_list_get_page_count(const MenuItemList& item_list) {
    if (item_list.items.size() == 0) {
        return 1;
    }

    uint32_t page_count = item_list.items.size() / ITEM_LIST_PAGE_SIZE;
    if (item_list.items.size() % ITEM_LIST_PAGE_SIZE != 0) {
        page_count++;
    }

    return page_count;
}
