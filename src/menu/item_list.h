#pragma once

#include "core/ui.h"
#include <cstdint>
#include <vector>
#include <string>

const Rect ITEM_LIST_RECT = {
    .x = 36, .y = 32, .w = 432, .h = 200
};
const uint32_t MENU_ITEM_NONE = UINT32_MAX;

struct MenuItemList {
    std::string no_items_message;
    std::string search_query;
    std::vector<std::string> items;
    uint32_t item_selected;
    uint32_t page;
    bool user_requests_search;
};

MenuItemList menu_item_list_init(const char* no_items_message);
void menu_item_list_clear(MenuItemList& item_list);
void menu_item_list_update(MenuItemList& item_list, UiContext& ui_context);
uint32_t menu_item_list_get_page_count(const MenuItemList& item_list);
