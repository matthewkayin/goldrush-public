#include "core/input.h"
#include "shell.h"

static const int MENU_WIDTH = 150;
static const int DESYNC_MENU_WIDTH = 166;

bool match_shell_menu_button_can_be_clicked(const MatchShell* shell) {
    if (shell->camera_mode == CAMERA_MODE_MINIMAP_DRAG) {
        return false;
    }
    if (match_shell_is_selecting(shell)) {
        return false;
    }
    if (shell->mode == MATCH_SHELL_MODE_MATCH_OVER_DEFEAT ||
            shell->mode == MATCH_SHELL_MODE_MATCH_OVER_VICTORY ||
            shell->mode == MATCH_SHELL_MODE_DESYNC) {
        return false;
    }
    return true;
}

bool match_shell_menu_button_is_clicked(const MatchShell* shell) {
    if (!match_shell_menu_button_can_be_clicked(shell)) {
        return false;
    }

    const SpriteInfo& menu_button_sprite_info = render_get_sprite_info(SPRITE_UI_BUTTON_BURGER);
    const Rect menu_button_rect = (Rect) {
        .x = MENU_BUTTON_POSITION.x, .y = MENU_BUTTON_POSITION.y,
        .w = menu_button_sprite_info.frame_width, .h = menu_button_sprite_info.frame_height
    };

    return input_is_action_just_pressed(INPUT_ACTION_MATCH_MENU) ||
        (input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK) &&
            menu_button_rect.has_point(input_get_mouse_position()));
}

void match_shell_menu_update(MatchShell* shell) {
    if (match_shell_menu_button_is_clicked(shell)) {
        if (match_shell_is_in_menu(shell)) {
            shell->options_menu.mode = OPTIONS_MENU_CLOSED;
            shell->mode = MATCH_SHELL_MODE_NONE;
        } else {
            shell->mode = MATCH_SHELL_MODE_MENU;
            input_stop_text_input();
        }
        sound_play(SOUND_UI_CLICK);
    }

    ui_begin(shell->ui_context);
    const ivec2 button_size = ui_button_size("Return to Menu");
    switch (shell->mode) {
        case MATCH_SHELL_MODE_MENU: {
            shell->ui_context.input_enabled = shell->options_menu.mode == OPTIONS_MENU_CLOSED;
            const bool is_in_scenario = shell->scenario_lua_state != NULL;
            const uint32_t button_count = is_in_scenario ? 5 : 4;
            match_shell_begin_menu(shell, "Game Menu", button_count);
                if (is_in_scenario && ui_button(shell->ui_context, "Restart", button_size, true)) {
                    shell->mode = MATCH_SHELL_MODE_MENU_SURRENDER_RESTART;
                }
                if (ui_button(shell->ui_context, "Leave Match", button_size, true)) {
                    if (match_shell_is_surrender_required_to_leave(shell)) {
                        shell->mode = MATCH_SHELL_MODE_MENU_SURRENDER;
                    } else {
                        match_shell_leave_match(shell, MATCH_SHELL_MODE_LEAVE_MATCH);
                    }
                }
                if (ui_button(shell->ui_context, "Exit Program", button_size, true)) {
                    if (match_shell_is_surrender_required_to_leave(shell)) {
                        shell->mode = MATCH_SHELL_MODE_MENU_SURRENDER_TO_DESKTOP;
                    } else {
                        match_shell_leave_match(shell, MATCH_SHELL_MODE_EXIT_PROGRAM);
                    }
                }
                if (ui_button(shell->ui_context, "Options", button_size, true)) {
                    shell->options_menu = options_menu_open();
                }
                if (ui_button(shell->ui_context, "Back", button_size, true)) {
                    shell->mode = MATCH_SHELL_MODE_NONE;
                }
            ui_end_container(shell->ui_context);

            if (shell->options_menu.mode != OPTIONS_MENU_CLOSED) {
                options_menu_update(shell->options_menu, shell->ui_context);
            }

            break;
        }
        case MATCH_SHELL_MODE_MENU_SURRENDER:
        case MATCH_SHELL_MODE_MENU_SURRENDER_TO_DESKTOP:
        case MATCH_SHELL_MODE_MENU_SURRENDER_RESTART: {
            const char* header_text = shell->mode == MATCH_SHELL_MODE_MENU_SURRENDER_RESTART
                ? "Restart?"
                : "Surrender?";
            match_shell_begin_menu(shell, header_text, 2);
                if (ui_button(shell->ui_context, "Yes", button_size, true)) {
                    MatchShellMode next_mode;
                    if (shell->mode == MATCH_SHELL_MODE_MENU_SURRENDER_TO_DESKTOP) {
                        next_mode = MATCH_SHELL_MODE_EXIT_PROGRAM;
                    } else if (shell->mode == MATCH_SHELL_MODE_MENU_SURRENDER_RESTART) {
                        next_mode = MATCH_SHELL_MODE_LEAVE_SCENARIO_RESTART;
                    } else if (shell->scenario_lua_state != NULL) {
                        next_mode = MATCH_SHELL_MODE_LEAVE_SCENARIO_DEFEAT;
                    } else {
                        next_mode = MATCH_SHELL_MODE_LEAVE_MATCH;
                    }
                    match_shell_leave_match(shell, next_mode);
                }
                if (ui_button(shell->ui_context, "Back", button_size, true)) {
                    shell->mode = MATCH_SHELL_MODE_MENU;
                }
            ui_end_container(shell->ui_context);
            break;
        }
        case MATCH_SHELL_MODE_MATCH_OVER_VICTORY:
        case MATCH_SHELL_MODE_MATCH_OVER_DEFEAT: {
            const char* header_text = shell->mode == MATCH_SHELL_MODE_MATCH_OVER_VICTORY
                ? "Victory!"
                : "Defeat!";
            match_shell_begin_menu(shell, header_text, 3);
                if (ui_button(shell->ui_context, "Keep Playing", button_size, true)) {
                    shell->mode = MATCH_SHELL_MODE_NONE;
                }
                if (ui_button(shell->ui_context, "Return to Menu", button_size, true)) {
                    match_shell_leave_match(shell, MATCH_SHELL_MODE_LEAVE_MATCH);
                }
                if (ui_button(shell->ui_context, "Exit Program", button_size, true)) {
                    match_shell_leave_match(shell, MATCH_SHELL_MODE_EXIT_PROGRAM);
                }
            ui_end_container(shell->ui_context);
            break;
        }
        case MATCH_SHELL_MODE_SCENARIO_VICTORY: {
            match_shell_begin_menu(shell, "Victory!", 1);
                if (ui_button(shell->ui_context, "Continue", button_size, true)) {
                    match_shell_leave_match(shell, MATCH_SHELL_MODE_LEAVE_SCENARIO_VICTORY);
                }
            ui_end_container(shell->ui_context);
            break;
        }
        case MATCH_SHELL_MODE_SCENARIO_DEFEAT: {
            match_shell_begin_menu(shell, "Defeat!", 3);
                if (ui_button(shell->ui_context, "Restart", button_size, true)) {
                    match_shell_leave_match(shell, MATCH_SHELL_MODE_LEAVE_SCENARIO_RESTART);
                }
                if (ui_button(shell->ui_context, "Return to Menu", button_size, true)) {
                    match_shell_leave_match(shell, MATCH_SHELL_MODE_LEAVE_SCENARIO_DEFEAT);
                }
                if (ui_button(shell->ui_context, "Exit Program", button_size, true)) {
                    match_shell_leave_match(shell, MATCH_SHELL_MODE_EXIT_PROGRAM);
                }
            ui_end_container(shell->ui_context);
            break;
        }
        case MATCH_SHELL_MODE_DESYNC: {
            const Rect MENU_RECT = (Rect) {
                .x = (SCREEN_WIDTH / 2) - (DESYNC_MENU_WIDTH / 2),
                .y = 64,
                .w = DESYNC_MENU_WIDTH,
                .h = 56 + (26 * 2)
            };
            ui_frame_rect(shell->ui_context, MENU_RECT);

            // Header
            const char* header_text = "Desync Detected";
            ivec2 text_size = render_get_text_size(FONT_WESTERN8_GOLD, header_text);
            ivec2 text_pos = ivec2(MENU_RECT.x + (MENU_RECT.w / 2) - (text_size.x / 2), MENU_RECT.y + 10);
            ui_element_position(shell->ui_context, text_pos);
            ui_text(shell->ui_context, FONT_WESTERN8_GOLD, header_text);

            // Desync message
            ivec2 text_column_position = ivec2(MENU_RECT.x + (MENU_RECT.w / 2), MENU_RECT.y + 32);
            ui_begin_column(shell->ui_context, text_column_position, 5);
                ui_text(shell->ui_context, FONT_HACK_WHITE, "Your game is out of", true);
                ui_text(shell->ui_context, FONT_HACK_WHITE, "sync with your opponents.", true);
            ui_end_container(shell->ui_context);

            ivec2 column_position = ivec2(MENU_RECT.x + (MENU_RECT.w / 2), MENU_RECT.y + 32 + 47);
            ui_begin_column(shell->ui_context, column_position, 5);
                if (ui_button(shell->ui_context, "Leave Match", button_size, true)) {
                    match_shell_leave_match(shell, MATCH_SHELL_MODE_LEAVE_MATCH);
                }
                if (ui_button(shell->ui_context, "Exit Program", button_size, true)) {
                    match_shell_leave_match(shell, MATCH_SHELL_MODE_EXIT_PROGRAM);
                }
            ui_end_container(shell->ui_context);
            break;
        }
        default:
            break;
    }
}

void match_shell_begin_menu(MatchShell* shell, const char* header_text, uint32_t button_count) {
    const Rect MENU_RECT = (Rect) {
        .x = (SCREEN_WIDTH / 2) - (MENU_WIDTH / 2),
        .y = button_count < 3 ? 64 : 48,
        .w = MENU_WIDTH,
        .h = 38 + (26 * (int)button_count)
    };
    ui_frame_rect(shell->ui_context, MENU_RECT);

    ivec2 text_size = render_get_text_size(FONT_WESTERN8_GOLD, header_text);
    ivec2 text_pos = ivec2(MENU_RECT.x + (MENU_RECT.w / 2) - (text_size.x / 2), MENU_RECT.y + 10);
    ui_element_position(shell->ui_context, text_pos);
    ui_text(shell->ui_context, FONT_WESTERN8_GOLD, header_text);

    const ivec2 column_position = ivec2(MENU_RECT.x + (MENU_RECT.w / 2), MENU_RECT.y + 32);
    ui_begin_column(shell->ui_context, column_position, 5);
}
