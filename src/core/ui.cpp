#include "ui.h"

#include "render/render.h"
#include "core/input.h"
#include "core/sound.h"
#include <algorithm>

static const uint32_t UI_TEXT_INPUT_BLINK_DURATION = 30;
static const int UI_ELEMENT_NONE = -1;

void ui_update_container(UiContext& context, ivec2 size);
int ui_get_next_element_id(UiContext& context);
int ui_get_next_text_input_id(UiContext& context);
void ui_queue_text(UiContext& context, FontName font, const char* text, ivec2 position, int z_index);
void ui_queue_sprite(UiContext& context, SpriteName sprite, ivec2 frame, ivec2 position, int z_index, bool flip_h = false);
void ui_queue_ninepatch(UiContext& context, SpriteName sprite, Rect rect, int z_index);
void ui_queue_draw_rect(UiContext& context, Rect rect, RenderColor color, int z_index);
void ui_queue_fill_rect(UiContext& context, Rect rect, RenderColor color, int z_index);

UiContext ui_init() {
    UiContext context;

    context.text_input_cursor_blink_timer = UI_TEXT_INPUT_BLINK_DURATION;
    context.text_input_show_cursor = false;
    context.element_selected = UI_ELEMENT_NONE;
    context.element_selected_future = UI_ELEMENT_NONE;

    return context;
}

void ui_begin(UiContext& context) {
    context.container_stack.clear();
    for (int z_index = 0; z_index < UI_Z_INDEX_COUNT; z_index++) {
        context.render_queue[z_index].clear();
    }

    // Update of timers and such can go here since ui_begin() is called every frame
    context.next_element_id = -1;
    context.next_text_input_id = -1;
    context.element_selected = context.element_selected_future;
    context.input_enabled = true;
}

void ui_element_position(UiContext& context, ivec2 position) {
    context.container_stack.push_back({
        .type = UI_CONTAINER_ELEMENT_POSITION,
        .origin = position,
        .spacing = 0,
        .size = ivec2(0, 0)
    });
}

void ui_element_size(UiContext& context, ivec2 size) {
    context.container_stack.push_back({
        .type = UI_CONTAINER_ELEMENT_SIZE,
        .origin = ivec2(0, 0),
        .spacing = 0,
        .size = size
    });
}

void ui_insert_padding(UiContext& context, ivec2 size) {
    context.container_stack[context.container_stack.size() - 1].origin += size;
}

void ui_begin_row(UiContext& context, ivec2 position, int spacing) {
    context.container_stack.push_back({
        .type = UI_CONTAINER_ROW,
        .origin = position,
        .spacing = spacing,
        .size = ivec2(0, 0)
    });
}

void ui_begin_column(UiContext& context, ivec2 position, int spacing) {
    context.container_stack.push_back({
        .type = UI_CONTAINER_COLUMN,
        .origin = position,
        .spacing = spacing,
        .size = ivec2(0, 0)
    });
}

void ui_end_container(UiContext& context) {
    ivec2 container_size = context.container_stack.back().size;
    context.container_stack.pop_back();
    ui_update_container(context, container_size);
}

ivec2 ui_button_size(const char* text) {
    ivec2 text_size = render_get_text_size(FONT_WESTERN8_OFFBLACK, text);
    if (text_size.x % 8 != 0) {
        text_size.x = ((text_size.x / 8) + 2) * 8;
    }
    text_size.x += 16;

    return ivec2(text_size.x, 21);
}

ivec2 ui_button_position_frame_bottom_left(Rect rect) {
    return ivec2(rect.x + 16, rect.y + rect.h - 21 - 8);
}

ivec2 ui_button_position_frame_bottom_right(Rect rect, const char* text) {
    ivec2 button_size = ui_button_size(text);
    return ivec2(rect.x + rect.w - 16 - button_size.x, rect.y + rect.h - 21 - 8);
}

bool ui_button(UiContext& context, const char* text, ivec2 size, bool center_horizontally) {
    ivec2 origin = ui_get_container_origin(context);
    ivec2 button_size = size.x == -1 ? ui_button_size(text) : size;
    Rect button_rect = (Rect) {
        .x = origin.x, .y = origin.y,
        .w = button_size.x, .h = button_size.y
    };
    ui_update_container(context, ivec2(button_rect.w, button_rect.h));

    if (center_horizontally) {
        button_rect.x -= button_size.x / 2;
    }
    bool hovered = context.input_enabled && context.element_selected == UI_ELEMENT_NONE && button_rect.has_point(input_get_mouse_position());

    int frame_count = button_rect.w / 8;
    for (int frame = 0; frame < frame_count; frame++) {
        // Determine hframe
        int hframe = 1;
        if (frame == 0) {
            hframe = 0;
        } else if (frame == frame_count - 1) {
            hframe = 2;
        }
        ui_queue_sprite(context, SPRITE_UI_MENU_BUTTON, ivec2(hframe, (int)hovered), ivec2(button_rect.x + (frame * 8), button_rect.y - (int)hovered), 0);
    }

    ivec2 text_pos = ivec2(button_rect.x + 5, button_rect.y + 3 - (int)hovered);
    if (center_horizontally) {
        ivec2 text_size = render_get_text_size(FONT_WESTERN8_OFFBLACK, text);
        text_pos.x = button_rect.x + (button_rect.w / 2) - (text_size.x / 2);
    }
    ui_queue_text(context, hovered ? FONT_WESTERN8_WHITE : FONT_WESTERN8_OFFBLACK, text, text_pos, 0);

    bool clicked = hovered && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK);
    if (clicked) {
        sound_play(SOUND_UI_CLICK);
    }
    return clicked;
}

bool ui_slim_button(UiContext& context, const char* text, bool disabled) {
    ivec2 origin = ui_get_container_origin(context);
    const SpriteInfo& sprite_info = render_get_sprite_info(SPRITE_UI_DROPDOWN_MINI);
    ivec2 button_size = ivec2(sprite_info.frame_width, sprite_info.frame_height);
    Rect button_rect = (Rect) {
        .x = origin.x, .y = origin.y,
        .w = button_size.x, .h = button_size.y
    };
    ui_update_container(context, ivec2(button_rect.w, button_rect.h));
    bool hovered = context.input_enabled && !disabled && context.element_selected == UI_ELEMENT_NONE && button_rect.has_point(input_get_mouse_position());

    ivec2 render_pos = ivec2(button_rect.x, button_rect.y - (int)hovered);
    ivec2 text_pos = ivec2(render_pos.x + 5, render_pos.y + 2);
    ui_queue_sprite(context, SPRITE_UI_DROPDOWN_MINI, hovered ? ivec2(0, 4) : ivec2(0, 3), render_pos, 0);
    ui_queue_text(context, hovered ? FONT_HACK_WHITE : FONT_HACK_OFFBLACK, text, text_pos, 0);

    bool clicked = hovered && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK);
    if (clicked) {
        sound_play(SOUND_UI_CLICK);
    }
    return clicked;
}

bool ui_sprite_button(UiContext& context, SpriteName sprite, bool disabled, bool flip_h) {
    ivec2 origin = ui_get_container_origin(context);
    const SpriteInfo& sprite_info = render_get_sprite_info(sprite);
    ui_update_container(context, ivec2(sprite_info.frame_width, sprite_info.frame_height));

    Rect sprite_rect = (Rect) {
        .x = origin.x, .y = origin.y,
        .w = sprite_info.frame_width, .h = sprite_info.frame_height
    };
    int hframe = 0;
    if (disabled) {
        hframe = sprite_info.hframes == 2 ? 0 : 2;
    } else if (context.input_enabled && sprite_rect.has_point(input_get_mouse_position())) {
        hframe = 1;
    }

    ui_queue_sprite(context, sprite, ivec2(hframe, 0), origin, 0, flip_h);

    bool clicked = context.input_enabled && context.element_selected == UI_ELEMENT_NONE && !disabled && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK) && sprite_rect.has_point(input_get_mouse_position());
    if (clicked) {
        sound_play(SOUND_UI_CLICK);
    }
    return clicked;
}

bool ui_campaign_orb(UiContext& context, SpriteName sprite, bool disabled, bool is_selected) {
    ivec2 origin = ui_get_container_origin(context);
    const SpriteInfo& sprite_info = render_get_sprite_info(sprite);
    ui_update_container(context, ivec2(sprite_info.frame_width, sprite_info.frame_height));

    Rect sprite_rect = (Rect) {
        .x = origin.x - 2, .y = origin.y - 2,
        .w = sprite_info.frame_width + 4, .h = sprite_info.frame_height + 4
    };
    int hframe = 0;
    if (disabled) {
        hframe = 0;
    } else if (context.input_enabled && sprite_rect.has_point(input_get_mouse_position())) {
        hframe = 1;
    } else if (is_selected) {
        hframe = 1;
    }

    ui_queue_sprite(context, sprite, ivec2(hframe, 0), origin, 0, false);

    bool clicked = context.input_enabled && context.element_selected == UI_ELEMENT_NONE && !disabled && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK) && sprite_rect.has_point(input_get_mouse_position());
    if (clicked) {
        sound_play(SOUND_UI_CLICK);
    }
    return clicked;
}

bool ui_icon_button(UiContext& context, SpriteName sprite, bool selected) {
    const SpriteInfo& button_sprite_info = render_get_sprite_info(SPRITE_UI_ICON_BUTTON);

    ivec2 origin = ui_get_container_origin(context);
    ui_update_container(context, ivec2(button_sprite_info.frame_width, button_sprite_info.frame_height));

    Rect button_rect = (Rect) {
        .x = origin.x, .y = origin.y,
        .w = button_sprite_info.frame_width, .h = button_sprite_info.frame_height
    };

    bool hovered = context.input_enabled && sprite != UI_ICON_BUTTON_EMPTY &&
                        button_rect.has_point(input_get_mouse_position());
    int hframe = selected ? 0 : 2;
    if (hovered) {
        hframe = 1;
    }

    ui_queue_sprite(context, SPRITE_UI_ICON_BUTTON, ivec2(hframe, 0), origin - ivec2(0, (int)hovered), 0);
    if (sprite != UI_ICON_BUTTON_EMPTY) {
        ui_queue_sprite(context, sprite, ivec2(hframe, 0), origin - ivec2(0, (int)hovered), 0);
    }

    bool clicked = hovered && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK);
    if (clicked) {
        sound_play(SOUND_UI_CLICK);
    }

    return clicked;
}

void ui_text(UiContext& context, FontName font, const char* text, bool center_horizontally) {
    ivec2 text_size = render_get_text_size(font, text);
    ivec2 origin = ui_get_container_origin(context);
        ui_update_container(context, text_size);
    if (center_horizontally) {
        origin.x -= text_size.x / 2;
    }
    ui_queue_text(context, font, text, origin, 0);
}

ivec2 ui_text_frame_size(const char* text) {
    const SpriteInfo& sprite_info = render_get_sprite_info(SPRITE_UI_TEXT_FRAME);
    ivec2 text_size = render_get_text_size(FONT_HACK_OFFBLACK, text);
    int frame_count = (text_size.x / sprite_info.frame_width) + 1;
    if (text_size.x % sprite_info.frame_width != 0) {
        frame_count++;
    }

    return ivec2(sprite_info.frame_width * frame_count, sprite_info.frame_height);
}

bool ui_text_frame(UiContext& context, const char* text, bool disabled) {
    ivec2 origin = ui_get_container_origin(context);

    const SpriteInfo& sprite_info = render_get_sprite_info(SPRITE_UI_TEXT_FRAME);
    ivec2 text_size = render_get_text_size(FONT_HACK_OFFBLACK, text);
    int frame_count = (text_size.x / sprite_info.frame_width) + 1;
    if (text_size.x % sprite_info.frame_width != 0) {
        frame_count++;
    }
    Rect rect = (Rect) {
        .x = origin.x, .y = origin.y,
        .w = frame_count * sprite_info.frame_width, .h = sprite_info.frame_height
    };
    ui_update_container(context, ivec2(rect.w, rect.h));
    bool hovered = context.input_enabled && context.element_selected == UI_ELEMENT_NONE && !disabled && rect.has_point(input_get_mouse_position());

    for (int frame = 0; frame < frame_count; frame++) {
        int hframe = 1;
        if (frame == 0) {
            hframe = 0;
        } else if (frame == frame_count - 1) {
            hframe = 2;
        }
        ui_queue_sprite(context, SPRITE_UI_TEXT_FRAME, ivec2(hframe, 0), ivec2(origin.x + (frame * sprite_info.frame_width), origin.y - (int)hovered), 0);
    }

    ui_queue_text(context, hovered ? FONT_HACK_WHITE : FONT_HACK_OFFBLACK, text, ivec2(rect.x + (rect.w / 2) - (text_size.x / 2), rect.y + 1 - (int)hovered), 0);

    bool clicked = hovered && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK);
    if (clicked) {
        sound_play(SOUND_UI_CLICK);
    }
    return clicked;
}

void ui_frame(UiContext& context, ivec2 size) {
    ivec2 origin = ui_get_container_origin(context);
    ui_update_container(context, size);
    Rect frame_rect = (Rect) {
        .x = origin.x, .y = origin.y,
        .w = size.x, .h = size.y
    };
    ui_queue_ninepatch(context, SPRITE_UI_FRAME, frame_rect, 0);
}

void ui_frame_rect(UiContext& context, Rect rect) {
    ui_element_position(context, ivec2(rect.x, rect.y));
    ui_frame(context, ivec2(rect.w, rect.h));
}

void ui_small_frame_rect(UiContext& context, Rect rect) {
    ui_element_position(context, ivec2(rect.x, rect.y));
    ui_update_container(context, ivec2(rect.x, rect.y));
    ui_queue_ninepatch(context, SPRITE_UI_FRAME_SMALL, rect, 0);
}

void ui_screen_shade(UiContext& context) {
    ui_queue_fill_rect(context, {
        .x = 0, .y = 0,
        .w = SCREEN_WIDTH, .h = SCREEN_HEIGHT},
    RENDER_COLOR_OFFBLACK_A128, 0);
}

void ui_text_input(UiContext& context, const char* prompt, ivec2 size, std::string* value, size_t max_length) {
    int id = ui_get_next_text_input_id(context);
    ivec2 origin = ui_get_container_origin(context);
    ui_update_container(context, size);

    Rect text_input_rect = (Rect) {
        .x = origin.x, .y = origin.y,
        .w = size.x, .h = size.y
    };
    ui_queue_ninepatch(context, SPRITE_UI_FRAME_SMALL, text_input_rect, 0);
    ui_queue_text(context, FONT_HACK_GOLD, prompt, ivec2(text_input_rect.x + 5, text_input_rect.y + 6), 0);
    ivec2 prompt_size = render_get_text_size(FONT_HACK_GOLD, prompt);
    ivec2 text_pos = ivec2(text_input_rect.x + prompt_size.x, text_input_rect.y + 6);

    ui_queue_text(context, FONT_HACK_GOLD, value->c_str(), text_pos, 0);
    text_pos.x += render_get_text_size(FONT_HACK_GOLD, value->c_str()).x;

    if (input_is_text_input_active() && context.text_input_selected == id) {
        if (!context.input_enabled) {
            input_stop_text_input();
        } else {
            context.text_input_cursor_blink_timer--;
            if (context.text_input_cursor_blink_timer == 0) {
                context.text_input_show_cursor = !context.text_input_show_cursor;
                context.text_input_cursor_blink_timer = UI_TEXT_INPUT_BLINK_DURATION;
            }
            if (context.text_input_show_cursor) {
                ivec2 cursor_pos = ivec2(text_pos.x - 2, text_pos.y - 1);
                ui_queue_text(context, FONT_HACK_GOLD, "|", cursor_pos, 0);
            }
        }
    }

    if (input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK) && context.input_enabled) {
        if (text_input_rect.has_point(input_get_mouse_position())) {
            sound_play(SOUND_UI_CLICK);
            input_start_text_input(value, max_length);
            context.text_input_selected = id;
            context.text_input_cursor_blink_timer = UI_TEXT_INPUT_BLINK_DURATION;
        }
    }
}

bool ui_team_picker(UiContext& context, char value, bool disabled) {
    ivec2 origin = ui_get_container_origin(context);
    const SpriteInfo& sprite_info = render_get_sprite_info(SPRITE_UI_TEAM_PICKER);
    ivec2 size = ivec2(sprite_info.frame_width, sprite_info.frame_height);
    ui_update_container(context, size);

    Rect rect = (Rect) {
        .x = origin.x, .y = origin.y,
        .w = size.x, .h = size.y
    };

    bool is_hovered = context.input_enabled &&
            context.element_selected == UI_ELEMENT_NONE &&
            !disabled &&
            rect.has_point(input_get_mouse_position());

    ui_queue_sprite(context, SPRITE_UI_TEAM_PICKER, ivec2((int)is_hovered, 0), origin, 0);
    char text[2] = { value, '\0' };
    ui_queue_text(context, is_hovered ? FONT_HACK_WHITE : FONT_HACK_OFFBLACK, text, ivec2(rect.x + 5, rect.y + 2), 0);

    bool clicked = is_hovered && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK);
    if (clicked) {
        sound_play(SOUND_UI_CLICK);
    }
    return clicked;
}

int ui_dropdown_clamp_scroll_offset(int scroll_offset, size_t selected_items_size, int scroll_max_visible_items) {
    if (scroll_max_visible_items >= (int)selected_items_size) {
        return 0;
    }
    return std::clamp(scroll_offset, 0, (int)selected_items_size - scroll_max_visible_items);
}

bool ui_dropdown(UiContext& context, UiDropdownType type, uint32_t* selected_item, const std::vector<std::string>& items, bool disabled, int scroll_max_visible_items) {
    int dropdown_id = ui_get_next_element_id(context);
    ivec2 origin = ui_get_container_origin(context);
    SpriteName sprite = type == UI_DROPDOWN ? SPRITE_UI_DROPDOWN : SPRITE_UI_DROPDOWN_MINI;
    const SpriteInfo& sprite_info = render_get_sprite_info(sprite);
    ivec2 size = ivec2(sprite_info.frame_width, sprite_info.frame_height);
    ui_update_container(context, size);

    Rect rect = (Rect) {
        .x = origin.x, .y = origin.y,
        .w = size.x, .h = size.y
    };

    if (items.empty()) {
        disabled = true;
    }
    bool hovered = context.input_enabled && context.element_selected == UI_ELEMENT_NONE && rect.has_point(input_get_mouse_position());

    int vframe = 0;
    if (disabled) {
        vframe = 3;
    } else if (context.element_selected == dropdown_id) {
        vframe = 2;
    } else if (hovered) {
        vframe = 1;
    }

    FontName font = type == UI_DROPDOWN ? FONT_WESTERN8_OFFBLACK : FONT_HACK_OFFBLACK;
    FontName hovered_font = type == UI_DROPDOWN ? FONT_WESTERN8_WHITE : FONT_HACK_WHITE;

    int text_yoffset = type == UI_DROPDOWN ? 3 : 2;

    ui_queue_sprite(context, sprite, ivec2(0, vframe), origin, 0);

    char item_text[32];
    if (!items.empty()) {
        if (type == UI_DROPDOWN_MINI) {
            strncpy(item_text, items.at(*selected_item).c_str(), 9);
            item_text[9] = '\0';
        } else {
            strcpy(item_text, items.at(*selected_item).c_str());
        }
        ui_queue_text(context, vframe == 1 ? hovered_font : font, item_text, ivec2(origin.x + 5, origin.y + text_yoffset), 0);
    }

    if (context.element_selected == dropdown_id) {
        // Dropdown item scroll
        if (scroll_max_visible_items != UI_DROPDOWN_SCROLL_DISABLED) {
            context.dropdown_scroll_offset = ui_dropdown_clamp_scroll_offset(context.dropdown_scroll_offset - input_get_mouse_scroll(), items.size(), scroll_max_visible_items);
        }

        // Render all the dropdown items
        int item_hovered = -1;
        const int visible_item_count = scroll_max_visible_items == UI_DROPDOWN_SCROLL_DISABLED
            ? (int)items.size()
            : std::min((int)items.size(), scroll_max_visible_items);
        for (int index = 0; index < visible_item_count; index++) {
            int dropdown_direction = origin.y + (size.y * (visible_item_count + 1)) > SCREEN_HEIGHT ? -1 : 1;

            Rect item_rect = (Rect) {
                .x = origin.x, .y = origin.y + ((size.y * (index + 1)) * dropdown_direction),
                .w = size.x, .h = size.y
            };
            bool item_is_hovered = context.input_enabled && item_rect.has_point(input_get_mouse_position());

            ui_queue_sprite(context, sprite, ivec2(0, 3 + (int)item_is_hovered), ivec2(item_rect.x, item_rect.y), 1);

            if (type == UI_DROPDOWN_MINI) {
                strncpy(item_text, items.at((size_t)(context.dropdown_scroll_offset + index)).c_str(), 12);
                item_text[12] = '\0';
            } else {
                strcpy(item_text, items.at((size_t)(context.dropdown_scroll_offset + index)).c_str());
            }
            ui_queue_text(context, item_is_hovered ? hovered_font : font, item_text, ivec2(item_rect.x + 5, item_rect.y + text_yoffset), 1);

            if (item_is_hovered) {
                item_hovered = index;
            }
        }

        // If the user left clicked, close the dropdown
        if (context.input_enabled && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
            context.element_selected_future = UI_ELEMENT_NONE;
            // If they happened to be selecting an item, update the value
            if (item_hovered != -1) {
                *selected_item = (uint32_t)(context.dropdown_scroll_offset + item_hovered);
            }
            sound_play(SOUND_UI_CLICK);
            // Return true if the item was changed
            return item_hovered != -1;
        }
    } else if (!disabled && hovered && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        context.element_selected_future = dropdown_id;
        context.dropdown_scroll_offset = scroll_max_visible_items == UI_DROPDOWN_SCROLL_DISABLED ? 0
            : ui_dropdown_clamp_scroll_offset((int)*selected_item, items.size(), scroll_max_visible_items);
        sound_play(SOUND_UI_CLICK);
    }

    return false;
}

bool ui_toolbar(UiContext& context, std::string* column, std::string* action, const std::vector<std::vector<std::string>>& items, int spacing) {
    const int toolbar_id = ui_get_next_element_id(context);
    const ivec2 origin = ui_get_container_origin(context);

    const SpriteInfo& dropdown_sprite_info = render_get_sprite_info(SPRITE_UI_DROPDOWN_MINI);
    const ivec2 size = ivec2((dropdown_sprite_info.frame_width + spacing) * items.size(), dropdown_sprite_info.frame_height);
    ui_update_container(context, size);

    const Rect toolbar_rect = (Rect) {
        .x = origin.x, .y = origin.y,
        .w = size.x, .h = size.y
    };

    const FontName font = FONT_HACK_OFFBLACK;
    const FontName hovered_font = FONT_HACK_WHITE;
    const int text_yoffset = 2;

    const bool hovered =
        context.input_enabled &&
        context.element_selected == UI_ELEMENT_NONE &&
        toolbar_rect.has_point(input_get_mouse_position());

    *column = "";
    *action = "";
    for (int column_index = 0; column_index < (int)items.size(); column_index++) {
        const int column_x = toolbar_rect.x + (column_index * (dropdown_sprite_info.frame_width + spacing));
        const bool mouse_is_in_column =
            input_get_mouse_position().x >= column_x &&
            input_get_mouse_position().x < column_x + dropdown_sprite_info.frame_width;

        int vframe = 0;
        if (hovered && mouse_is_in_column) {
            vframe = 1;
        } else if (context.element_selected == toolbar_id && context.toolbar_column_selected == column_index) {
            vframe = 2;
        }

        ui_queue_sprite(context, SPRITE_UI_DROPDOWN_MINI, ivec2(0, vframe), ivec2(column_x, origin.y), 0);
        ui_queue_text(context, vframe == 1 ? hovered_font : font, items[column_index][0].c_str(), ivec2(column_x + 5, origin.y + text_yoffset), 0);

        const Rect column_header_rect = (Rect) {
            .x = column_x, .y = origin.y,
            .w = dropdown_sprite_info.frame_width,
            .h = dropdown_sprite_info.frame_height
        };
        if (context.element_selected == toolbar_id && column_header_rect.has_point(input_get_mouse_position())) {
            context.toolbar_column_selected = column_index;
        }

        if (context.element_selected == toolbar_id && context.toolbar_column_selected == column_index) {
            int item_hovered = -1;
            for (int row_index = 0; row_index < (int)items[column_index].size() - 1; row_index++) {
                const Rect item_rect = (Rect) {
                    .x = column_x,
                    .y = origin.y + ((dropdown_sprite_info.frame_height * (row_index + 1))),
                    .w = dropdown_sprite_info.frame_width,
                    .h = dropdown_sprite_info.frame_height
                };
                bool item_is_hovered = context.input_enabled && item_rect.has_point(input_get_mouse_position());

                ui_queue_sprite(context, SPRITE_UI_DROPDOWN_MINI, ivec2(0, 3 + (int)item_is_hovered), ivec2(item_rect.x, item_rect.y), 1);
                ui_queue_text(context, item_is_hovered ? hovered_font : font, items[column_index][row_index + 1].c_str(), ivec2(item_rect.x + 5, item_rect.y + text_yoffset), 1);

                if (item_is_hovered) {
                    item_hovered = row_index;
                }
            }

            if (context.input_enabled && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK) && item_hovered != -1) {
                *column = items[column_index][0];
                *action = items[column_index][item_hovered + 1];
                sound_play(SOUND_UI_CLICK);
            }
        }
    }

    if (context.element_selected == toolbar_id && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        context.element_selected_future = UI_ELEMENT_NONE;
        sound_play(SOUND_UI_CLICK);
    } else if (context.element_selected == UI_ELEMENT_NONE && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK) && hovered) {
        context.element_selected_future = toolbar_id;
        context.toolbar_column_selected = (input_get_mouse_position().x - origin.x) / (dropdown_sprite_info.frame_width + spacing);
        sound_play(SOUND_UI_CLICK);
    }

    return *action != "";
}

bool ui_slider(UiContext& context, uint32_t* value, uint32_t* buffered_value, UiSliderParams params) {
    static const int VALUE_STR_PADDING = 4;
    static const int SLIDER_HEIGHT = 5;
    static const int NOTCH_WIDTH = 5;
    static const int NOTCH_HEIGHT = 14;

    int slider_id = ui_get_next_element_id(context);
    ivec2 origin = ui_get_container_origin(context);
    uint32_t old_value = *value;

    SpriteName dropdown_sprite_name = params.size == UI_SLIDER_SIZE_NORMAL ? SPRITE_UI_DROPDOWN : SPRITE_UI_DROPDOWN_MINI;
    FontName font_name = params.size == UI_SLIDER_SIZE_NORMAL ? FONT_WESTERN8_GOLD : FONT_HACK_GOLD;

    // Slider width will be based on the dropdown sprite's width
    const SpriteInfo& dropdown_sprite_info = render_get_sprite_info(dropdown_sprite_name);
    ivec2 size = ivec2(dropdown_sprite_info.frame_width, dropdown_sprite_info.frame_height);
    if (params.display != UI_SLIDER_DISPLAY_NO_VALUE) {
        char option_value_str[8];
        if (params.display == UI_SLIDER_DISPLAY_RAW_VALUE) {
            sprintf(option_value_str, "%i", *value);
        } else if (params.display == UI_SLIDER_DISPLAY_PERCENT) {
            float percentage = (float)(*value) / (float)params.max;
            sprintf(option_value_str, "%i%%", (int)(percentage * 100.0f));
        }
        ivec2 value_str_text_size = render_get_text_size(font_name, option_value_str);
        size.x += VALUE_STR_PADDING + value_str_text_size.x;

        ui_queue_text(context, font_name, option_value_str, ivec2(origin.x - VALUE_STR_PADDING - value_str_text_size.x, origin.y + 3), 0);
    }

    ui_update_container(context, size);

    Rect slider_rect = (Rect) {
        .x = origin.x,
        .y = origin.y + (dropdown_sprite_info.frame_height / 2) - (SLIDER_HEIGHT / 2),
        .w = dropdown_sprite_info.frame_width,
        .h = SLIDER_HEIGHT
    };
    Rect slider_subrect = (Rect) {
        .x = slider_rect.x + 1,
        .y = slider_rect.y,
        .w = ((slider_rect.w - 2) * ((int)*value - (int)params.min)) / (int)params.max,
        .h = slider_rect.h
    };
    Rect notch_rect = (Rect) {
        .x = slider_subrect.x + slider_subrect.w - (NOTCH_WIDTH / 2),
        .y = slider_rect.y + (slider_rect.h / 2) - (NOTCH_HEIGHT / 2),
        .w = NOTCH_WIDTH, .h = NOTCH_HEIGHT
    };
    Rect notch_subrect = (Rect) {
        .x = notch_rect.x + 1,
        .y = notch_rect.y + 1,
        .w = notch_rect.w - 2,
        .h = notch_rect.h - 2
    };

    if (buffered_value != NULL) {
        Rect slider_buffered_subrect = (Rect) {
            .x = slider_rect.x + 1,
            .y = slider_rect.y,
            .w = ((slider_rect.w - 2) * ((int)*buffered_value - (int)params.min)) / (int)params.max,
            .h = slider_rect.h
        };
        ui_queue_fill_rect(context, slider_buffered_subrect, RENDER_COLOR_WHITE, 0);
    }
    ui_queue_fill_rect(context, slider_subrect, RENDER_COLOR_GOLD, 0);
    ui_queue_draw_rect(context, slider_rect, RENDER_COLOR_OFFBLACK, 0);
    ui_queue_fill_rect(context, notch_rect, RENDER_COLOR_OFFBLACK, 0);
    ui_queue_fill_rect(context, notch_subrect, RENDER_COLOR_GOLD, 0);

    Rect input_rect = (Rect) {
        .x = origin.x, .y = origin.y,
        .w = dropdown_sprite_info.frame_width,
        .h = dropdown_sprite_info.frame_height
    };
    bool hovered = context.input_enabled && context.element_selected == UI_ELEMENT_NONE &&
                        input_rect.has_point(input_get_mouse_position());

    if (hovered && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        context.element_selected = slider_id;
        context.element_selected_future = slider_id;
    }

    if (context.element_selected == slider_id) {
        int mouse_x = input_get_mouse_position().x;
        if (mouse_x < input_rect.x) {
            mouse_x = input_rect.x;
        } else if (mouse_x > input_rect.x + input_rect.w) {
            mouse_x = input_rect.x + input_rect.w;
        }
        mouse_x -= input_rect.x;

        uint32_t scaled_min = params.min / params.step;
        uint32_t scaled_max = params.max / params.step;
        uint32_t scaled_value = scaled_min + (uint32_t)((mouse_x * (int)scaled_max) / input_rect.w);
        *value = scaled_value * params.step;

        if (input_is_action_just_released(INPUT_ACTION_LEFT_CLICK)) {
            context.element_selected = UI_ELEMENT_NONE;
            context.element_selected_future = UI_ELEMENT_NONE;
        }
    }

    return *value != old_value;
}

void ui_sprite(UiContext& context, SpriteName sprite, ivec2 frame) {
    ivec2 origin = ui_get_container_origin(context);
    const SpriteInfo& sprite_info = render_get_sprite_info(sprite);
    ui_update_container(context, ivec2(sprite_info.frame_width, sprite_info.frame_height));

    ui_queue_sprite(context, sprite, frame, origin, 0, false);
}

bool ui_screenshot_frame(UiContext& context, ivec2 size) {
    ivec2 origin = ui_get_container_origin(context);
    ui_update_container(context, size);

    Rect frame_rect = (Rect) {
        .x = origin.x, .y = origin.y,
        .w = size.x, .h = size.y
    };

    bool hovered = context.input_enabled && frame_rect.has_point(input_get_mouse_position());
    if (hovered) {
        ui_queue_draw_rect(context, frame_rect, RENDER_COLOR_WHITE, 0);
    }

    bool clicked = hovered && context.element_selected == UI_ELEMENT_NONE && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK);
    if (clicked) {
        sound_play(SOUND_UI_CLICK);
    }
    return clicked;
}

void ui_render(const UiContext& context) {
    for (int z_index = 0; z_index < UI_Z_INDEX_COUNT; z_index++) {
        for (const UiRender& render : context.render_queue[z_index]) {
            switch (render.type) {
                case UI_RENDER_TEXT: {
                    render_text(render.text.font, render.text.text, render.text.position);
                    break;
                }
                case UI_RENDER_SPRITE: {
                    uint32_t sprite_options = RENDER_SPRITE_NO_CULL;
                    if (render.sprite.flip_h) {
                        sprite_options |= RENDER_SPRITE_FLIP_H;
                    }
                    render_sprite_frame(render.sprite.sprite, render.sprite.frame, render.sprite.position, sprite_options, 0);
                    break;
                }
                case UI_RENDER_NINEPATCH: {
                    render_ninepatch(render.ninepatch.sprite, render.ninepatch.rect);
                    break;
                }
                case UI_RENDER_DRAW_RECT: {
                    render_draw_rect(render.rect.rect, render.rect.color);
                    break;
                }
                case UI_RENDER_FILL_RECT: {
                    render_fill_rect(render.rect.rect, render.rect.color);
                    break;
                }
            }
        }
    }
}

ivec2 ui_get_container_origin(const UiContext& context) {
    ivec2 origin = ivec2(0, 0);
    for (const UiContainer& container : context.container_stack) {
        origin += container.origin;
    }

    return origin;
}

void ui_update_container(UiContext& context, ivec2 size) {
    if (!context.container_stack.empty()) {
        UiContainer& container = context.container_stack.back();
        switch (container.type) {
            case UI_CONTAINER_ROW: {
                container.origin.x += size.x + container.spacing;
                container.size.x += size.x + container.spacing;
                container.size.y = std::max(container.size.y, size.y);
                break;
            }
            case UI_CONTAINER_COLUMN: {
                container.origin.y += size.y + container.spacing;
                container.size.y += size.y + container.spacing;
                container.size.x = std::max(container.size.x, size.x);
                break;
            }
            case UI_CONTAINER_ELEMENT_POSITION: {
                ui_end_container(context);
                break;
            }
            case UI_CONTAINER_ELEMENT_SIZE: {
                ui_end_container(context);
                break;
            }
        }
    }
}

int ui_get_next_element_id(UiContext& context) {
    context.next_element_id++;
    return context.next_element_id;
}

int ui_get_next_text_input_id(UiContext& context) {
    context.next_text_input_id++;
    return context.next_text_input_id;
}

void ui_queue_text(UiContext& context, FontName font, const char* text, ivec2 position, int z_index) {
    UiRender render;
    render.type = UI_RENDER_TEXT;
    render.text.font = font;
    render.text.position = position;
    strcpy(render.text.text, text);
    context.render_queue[z_index].push_back(render);
}

void ui_queue_sprite(UiContext& context, SpriteName sprite, ivec2 frame, ivec2 position, int z_index, bool flip_h) {
    context.render_queue[z_index].push_back((UiRender) {
        .type = UI_RENDER_SPRITE,
        .sprite = (UiRenderSprite) {
            .sprite = sprite,
            .frame = frame,
            .position = position,
            .flip_h = flip_h
        }
    });
}

void ui_queue_ninepatch(UiContext& context, SpriteName sprite, Rect rect, int z_index) {
    context.render_queue[z_index].push_back((UiRender) {
        .type = UI_RENDER_NINEPATCH,
        .ninepatch = (UiRenderNinepatch) {
            .sprite = sprite,
            .rect = rect
        }
    });
}

void ui_queue_draw_rect(UiContext& context, Rect rect, RenderColor color, int z_index) {
    context.render_queue[z_index].push_back((UiRender) {
        .type = UI_RENDER_DRAW_RECT,
        .rect = (UiRenderRect) {
            .rect = rect,
            .color = color
        }
    });
}

void ui_queue_fill_rect(UiContext& context, Rect rect, RenderColor color, int z_index) {
    context.render_queue[z_index].push_back((UiRender) {
        .type = UI_RENDER_FILL_RECT,
        .rect = (UiRenderRect) {
            .rect = rect,
            .color = color
        }
    });
}
