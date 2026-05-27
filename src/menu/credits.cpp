#include "menu.h"

#include "render/font.h"

static const uint32_t CREDITS_TICK_DURATION = 4U;
static const uint32_t CREDITS_SECTION_PADDING = 64U;

void menu_credits_init(MenuState* state) {
    state->menu_animation.wagon_x = -32;
    state->credits_yoffset = SCREEN_HEIGHT;
    state->credits_timer = CREDITS_TICK_DURATION;

    void* credits_resource = resource_load(RESOURCE_CREDITS);
    char* credits_resource_ptr = (char*)credits_resource;

    state->credits_entries.clear();
    char line[CREDITS_ENTRY_BUFFER_SIZE];
    while (*credits_resource_ptr != '\0') {
        size_t line_size = 0;
        while (credits_resource_ptr[line_size] != '\0' && credits_resource_ptr[line_size] != '\n' && credits_resource_ptr[line_size] != '\r') {
            line[line_size] = credits_resource_ptr[line_size];
            line_size++;
        }
        line[line_size] = '\0';
        credits_resource_ptr += line_size + 2;

        CreditsEntry entry;
        if (line_size == 0) {
            entry.type = CREDITS_ENTRY_BREAK;
        } else if (line[0] == '#') {
            entry.type = CREDITS_ENTRY_HEADER;
            strncpy(entry.text, (char*)line + 2, CREDITS_ENTRY_BUFFER_SIZE);
        } else {
            entry.type = CREDITS_ENTRY_TEXT;
            strncpy(entry.text, line, CREDITS_ENTRY_BUFFER_SIZE);
        }
        state->credits_entries.push_back(entry);
    }

    free(credits_resource);
}

void menu_credits_update(MenuState* state) {
    state->credits_timer--;
    if (state->credits_timer == 0) {
        state->credits_yoffset--;
        state->credits_timer = CREDITS_TICK_DURATION;
    }

    ivec2 button_size = ui_button_size("Skip");
    ui_element_position(state->ui_context, ivec2(SCREEN_WIDTH - button_size.x - 4, 4));
    if (ui_button(state->ui_context, "Skip") || menu_credits_are_over(state)) {
        menu_set_mode(state, MENU_MODE_MAIN);
    }
}

void menu_credits_render(const MenuState* state) {
    int render_y = state->credits_yoffset;
    int header_height = menu_credits_header_height();
    int text_height = menu_credits_text_height();

    // Title
    const SpriteInfo& title_sprite_info = render_get_sprite_info(SPRITE_UI_TITLE);
    render_sprite_frame(SPRITE_UI_TITLE, ivec2(0, 0), ivec2(21, render_y), 0, 0);
    render_y += title_sprite_info.frame_height + CREDITS_SECTION_PADDING;

    for (const CreditsEntry& entry : state->credits_entries) {
        switch (entry.type) {
            case CREDITS_ENTRY_HEADER: {
                render_text(FONT_WESTERN16_OFFBLACK, entry.text, ivec2(25, render_y));
                render_y += header_height;
                break;
            }
            case CREDITS_ENTRY_TEXT: {
                render_text(FONT_WESTERN8_OFFBLACK, entry.text, ivec2(25, render_y));
                render_y += text_height;
                break;
            }
            case CREDITS_ENTRY_BREAK: {
                render_y += CREDITS_SECTION_PADDING;
                break;
            }
        }
    }
}

bool menu_credits_are_over(const MenuState* state) {
    int header_height = menu_credits_header_height();
    int text_height = menu_credits_text_height();

    const SpriteInfo& title_sprite_info = render_get_sprite_info(SPRITE_UI_TITLE);
    int render_y = state->credits_yoffset + title_sprite_info.frame_height + CREDITS_SECTION_PADDING;

    for (const CreditsEntry& entry : state->credits_entries) {
        switch (entry.type) {
            case CREDITS_ENTRY_HEADER: {
                render_y += header_height;
                break;
            }
            case CREDITS_ENTRY_TEXT: {
                render_y += text_height;
                break;
            }
            case CREDITS_ENTRY_BREAK: {
                render_y += CREDITS_SECTION_PADDING;
                break;
            }
        }
    }

    return render_y <= -header_height;
}

int menu_credits_header_height() {
    return render_get_text_size(FONT_WESTERN16_OFFBLACK, "Matthew Madden").y + 4;
}

int menu_credits_text_height() {
    return render_get_text_size(FONT_WESTERN8_OFFBLACK, "Matthew Madden").y;
}
