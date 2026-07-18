#include "input.h"

#include "defines.h"
#include "core/logger.h"
#include "core/filesystem.h"
#include "util/json.h"
#include <cstring>
#include <algorithm>
#include <functional>

struct InputState {
    SDL_Window* window;
    ivec2 scaled_screen_size;
    ivec2 scaled_screen_position;

    ivec2 mouse_position;
    int mouse_scroll;
    bool action_is_pressed[INPUT_ACTION_COUNT];
    bool action_was_pressed[INPUT_ACTION_COUNT];
    bool hotkey_is_pressed[INPUT_HOTKEY_COUNT];
    bool hotkey_was_pressed[INPUT_HOTKEY_COUNT];
    bool user_requests_exit;
    bool should_capture_mouse;

    std::string* text_input_str;
    size_t text_input_max_length;

    SDL_Scancode hotkey_mapping[INPUT_HOTKEY_COUNT];

    SDL_Scancode key_just_pressed;
};
static InputState state;

void input_update_screen_scale();

void input_init(SDL_Window* window) {
    memset(&state, 0, sizeof(state));
    state.window = window;
    input_update_screen_scale();
    input_stop_text_input();

    state.should_capture_mouse = true;
    state.mouse_scroll = 0;

    #ifndef GOLD_DEBUG
        SDL_SetWindowMouseGrab(state.window, true);
    #endif

    // Init hotkey mapping
    input_set_hotkey_mapping_to_default(state.hotkey_mapping);
    input_load_hotkey_mapping();
}

void input_save_hotkey_mapping() {
    Json* hotkey_json = json_object();
    for (uint32_t hotkey_index = 0; hotkey_index < INPUT_HOTKEY_COUNT; hotkey_index++) {
        InputHotkey hotkey = (InputHotkey)hotkey_index;
        json_object_set_number(hotkey_json, input_get_hotkey_str(hotkey), input_get_hotkey_mapping(hotkey));
    }
    if (!json_write(hotkey_json, input_get_saved_hotkeys_path().c_str())) {
        log_error("Could not save hotkeys file.");
    }
}

void input_set_hotkey_mapping_from_json(Json* hotkey_json) {
    for (size_t index = 0; index < hotkey_json->object.length; index++) {
        const char* key = hotkey_json->object.keys[index];
        int hotkey;
        for (hotkey = 0; hotkey < INPUT_HOTKEY_COUNT; hotkey++) {
            if (strcmp(key, input_get_hotkey_str((InputHotkey)hotkey)) == 0) {
                break;
            }
        }
        if (hotkey == INPUT_HOTKEY_COUNT) {
            log_warn("Hotkey %s not recognized.", key);
            continue;
        }

        SDL_Scancode value = (SDL_Scancode)((int)json_object_get_number(hotkey_json, key));
        if (!input_is_key_valid_hotkey_mapping(value)) {
            log_warn("Hotkey value %i for key %s is not valid.", value, key);
            continue;
        }

        input_set_hotkey_mapping((InputHotkey)hotkey, value);
    }
}

void input_load_hotkey_mapping() {
    // Check in saves folder
    std::string hotkey_path = input_get_saved_hotkeys_path();
    Json* hotkey_json = json_read(hotkey_path.c_str());
    if (hotkey_json) {
        input_set_hotkey_mapping_from_json(hotkey_json);
        log_info("Loaded hotkey mapping from saves folder.");
        json_free(hotkey_json);
        return;
    }

    // If that didn't work, check the old path
    hotkey_path = filesystem_get_data_path() + "hotkeys.json";
    hotkey_json = json_read(hotkey_path.c_str());
    if (hotkey_json) {
        input_set_hotkey_mapping_from_json(hotkey_json);
        input_save_hotkey_mapping();
        log_info("Loaded hotkey mapping from old path. Saved to saves folder.");
        json_free(hotkey_json);
        return;
    }

    // If not found in either path, save defaults
    input_save_hotkey_mapping();
    log_info("hotkeys.json does not exist. Saved defaults.");
}

const std::string input_get_saved_hotkeys_path() {
    return filesystem_get_saves_folder_path() + "hotkeys.json";
}

const char* input_get_hotkey_str(InputHotkey hotkey) {
    switch (hotkey) {
        case INPUT_HOTKEY_ATTACK:
            return "attack";
        case INPUT_HOTKEY_STOP:
            return "stop";
        case INPUT_HOTKEY_DEFEND:
            return "defend";
        case INPUT_HOTKEY_BUILD:
            return "build";
        case INPUT_HOTKEY_BUILD2:
            return "build2";
        case INPUT_HOTKEY_REPAIR:
            return "repair";
        case INPUT_HOTKEY_UNLOAD:
            return "unload";
        case INPUT_HOTKEY_CANCEL:
            return "cancel";
        case INPUT_HOTKEY_HALL:
            return "hall";
        case INPUT_HOTKEY_HOUSE:
            return "house";
        case INPUT_HOTKEY_SALOON:
            return "saloon";
        case INPUT_HOTKEY_WORKSHOP:
            return "workshop";
        case INPUT_HOTKEY_SMITH:
            return "smith";
        case INPUT_HOTKEY_COOP:
            return "coop";
        case INPUT_HOTKEY_BARRACKS:
            return "barracks";
        case INPUT_HOTKEY_SHERIFFS:
            return "sheriffs";
        case INPUT_HOTKEY_BUNKER:
            return "bunker";
        case INPUT_HOTKEY_MINER:
            return "miner";
        case INPUT_HOTKEY_COWBOY:
            return "cowboy";
        case INPUT_HOTKEY_BANDIT:
            return "bandit";
        case INPUT_HOTKEY_SAPPER:
            return "sapper";
        case INPUT_HOTKEY_PYRO:
            return "pyro";
        case INPUT_HOTKEY_WAGON:
            return "wagon";
        case INPUT_HOTKEY_JOCKEY:
            return "jockey";
        case INPUT_HOTKEY_SOLDIER:
            return "soldier";
        case INPUT_HOTKEY_CANNON:
            return "cannon";
        case INPUT_HOTKEY_DETECTIVE:
            return "detective";
        case INPUT_HOTKEY_BALLOON:
            return "balloon";
        case INPUT_HOTKEY_MOLOTOV:
            return "molotov";
        case INPUT_HOTKEY_CAMO:
            return "camo";
        case INPUT_HOTKEY_LANDMINE:
            return "landmine";
        case INPUT_HOTKEY_RESEARCH_LANDMINES:
            return "landmines";
        case INPUT_HOTKEY_RESEARCH_WAGON_ARMOR:
            return "wagon_armor";
        case INPUT_HOTKEY_RESEARCH_BAYONETS:
            return "bayonets";
        case INPUT_HOTKEY_RESEARCH_PRIVATE_EYE:
            return "private_eye";
        case INPUT_HOTKEY_RESEARCH_GETAWAY_BOOTS:
            return "getaway_boots";
        case INPUT_HOTKEY_RESEARCH_IRON_SIGHTS:
            return "iron_sights";
        case INPUT_HOTKEY_IDLE_MINER:
            return "idle_miner";
        default: {
            GOLD_ASSERT(false);
            return "";
        }
    }
}

void input_update_screen_scale() {
    ivec2 window_size;
    SDL_GetWindowSize(state.window, &window_size.x, &window_size.y);

    int scale_width = window_size.x / SCREEN_WIDTH;
    int scale_height = window_size.y / SCREEN_HEIGHT;
    int scale = std::min(scale_width, scale_height);

    state.scaled_screen_size = ivec2(SCREEN_WIDTH, SCREEN_HEIGHT) * scale;
    state.scaled_screen_position = ivec2(
        (window_size.x / 2) - (state.scaled_screen_size.x / 2),
        (window_size.y / 2) - (state.scaled_screen_size.y / 2));
}

void input_set_mouse_capture_enabled(bool value) {
    state.should_capture_mouse = value;
}

void input_poll_events() {
    memcpy(&state.action_was_pressed, &state.action_is_pressed, sizeof(state.action_is_pressed));
    memcpy(&state.hotkey_was_pressed, &state.hotkey_is_pressed, sizeof(state.hotkey_is_pressed));
    state.key_just_pressed = INPUT_KEY_NONE;
    state.mouse_scroll = 0;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // Handle quit
        if (event.type == SDL_EVENT_QUIT) {
            state.user_requests_exit = true;
            break;
        }

        if (event.type == SDL_EVENT_WINDOW_RESIZED ||
                event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            input_update_screen_scale();
        }

        // Capture mouse
        if (state.should_capture_mouse && !SDL_GetWindowMouseGrab(state.window)) {
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
                SDL_SetWindowMouseGrab(state.window, true);
                continue;
            }
            // If the mouse is not captured, don't handle any other input
            // Breaking here also prevents the initial click into the window from triggering any action
            break;
        }

        #ifdef GOLD_DEBUG
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_TAB) {
                SDL_SetWindowMouseGrab(state.window, false);
                break;
            }
        #endif

        switch (event.type) {
            case SDL_EVENT_WINDOW_FOCUS_LOST:
            case SDL_EVENT_WINDOW_FOCUS_GAINED: {
                memset(state.action_is_pressed, 0, sizeof(state.action_is_pressed));
                memset(state.hotkey_is_pressed, 0, sizeof(state.hotkey_is_pressed));
                break;
            }
            case SDL_EVENT_MOUSE_MOTION: {
                state.mouse_position = ivec2((int)event.motion.x - state.scaled_screen_position.x, (int)event.motion.y - state.scaled_screen_position.y);
                state.mouse_position = ivec2((state.mouse_position.x * SCREEN_WIDTH) / state.scaled_screen_size.x, (state.mouse_position.y * SCREEN_HEIGHT) / state.scaled_screen_size.y);
                state.mouse_position.x = std::clamp(state.mouse_position.x, 0, SCREEN_WIDTH);
                state.mouse_position.y = std::clamp(state.mouse_position.y, 0, SCREEN_HEIGHT);
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                switch (event.button.button) {
                    case SDL_BUTTON_LEFT:
                        state.action_is_pressed[INPUT_ACTION_LEFT_CLICK] = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                        break;
                    case SDL_BUTTON_RIGHT:
                        state.action_is_pressed[INPUT_ACTION_RIGHT_CLICK] = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                        break;
                    default:
                        break;
                }
                break;
            }
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP: {
                // Map editor shortcuts
                #ifdef GOLD_DEBUG
                    // Only capture these shortcuts when mouse capture is disabled (i.e. when we are in the editor)
                    // This ensures that any colliding hotkeys still work in debug (such as S for stop)
                    if (!state.should_capture_mouse) {
                        const bool* key_state = SDL_GetKeyboardState(NULL);
                        // Use CMD instead of CTRL on Mac version of the editor
                        #ifdef PLATFORM_MACOS
                            const SDL_Scancode CTRL = SDL_SCANCODE_LGUI;
                            const SDL_Scancode DELETE = SDL_SCANCODE_BACKSPACE;
                        #else
                            const SDL_Scancode CTRL = SDL_SCANCODE_LCTRL;
                            const SDL_Scancode DELETE = SDL_SCANCODE_DELETE;
                        #endif
                        std::function<bool(InputAction, SDL_Scancode)> handle_ctrl_action = [&key_state, &event](InputAction action, SDL_Scancode action_scancode) {
                            if (!key_state[CTRL]) {
                                state.action_is_pressed[action] = false;
                                return false;
                            }
                            if (event.key.scancode != action_scancode) {
                                return false;
                            }
                            state.action_is_pressed[action] = event.type == SDL_EVENT_KEY_DOWN;
                            return true;
                        };
                        if (handle_ctrl_action(INPUT_ACTION_EDITOR_SAVE, SDL_SCANCODE_S)) { break; }
                        if (handle_ctrl_action(INPUT_ACTION_EDITOR_UNDO, SDL_SCANCODE_Z)) { break; }
                        if (handle_ctrl_action(INPUT_ACTION_EDITOR_UNDO, SDL_SCANCODE_Z)) { break; }
                        if (handle_ctrl_action(INPUT_ACTION_EDITOR_REDO, SDL_SCANCODE_R)) { break; }
                        if (handle_ctrl_action(INPUT_ACTION_EDITOR_COPY, SDL_SCANCODE_C)) { break; }
                        if (handle_ctrl_action(INPUT_ACTION_EDITOR_CUT, SDL_SCANCODE_X)) { break; }
                        if (handle_ctrl_action(INPUT_ACTION_EDITOR_PASTE, SDL_SCANCODE_V)) { break; }
                        if (!key_state[CTRL]) {
                            switch (event.key.scancode) {
                                case SDL_SCANCODE_B:
                                    state.action_is_pressed[INPUT_ACTION_EDITOR_TOOL_BRUSH] = event.type == SDL_EVENT_KEY_DOWN;
                                    break;
                                case SDL_SCANCODE_F:
                                    state.action_is_pressed[INPUT_ACTION_EDITOR_TOOL_FILL] = event.type == SDL_EVENT_KEY_DOWN;
                                    break;
                                case SDL_SCANCODE_R:
                                    state.action_is_pressed[INPUT_ACTION_EDITOR_TOOL_RECT] = event.type == SDL_EVENT_KEY_DOWN;
                                    break;
                                case SDL_SCANCODE_S:
                                    state.action_is_pressed[INPUT_ACTION_EDITOR_TOOL_SELECT] = event.type == SDL_EVENT_KEY_DOWN;
                                    break;
                                case SDL_SCANCODE_D:
                                    state.action_is_pressed[INPUT_ACTION_EDITOR_TOOL_DECORATE] = event.type == SDL_EVENT_KEY_DOWN;
                                    break;
                                case SDL_SCANCODE_A:
                                    state.action_is_pressed[INPUT_ACTION_EDITOR_TOOL_ADD_ENTITY] = event.type == SDL_EVENT_KEY_DOWN;
                                    break;
                                case SDL_SCANCODE_E:
                                    state.action_is_pressed[INPUT_ACTION_EDITOR_TOOL_EDIT_ENTITY] = event.type == SDL_EVENT_KEY_DOWN;
                                    break;
                                case SDL_SCANCODE_Q:
                                    state.action_is_pressed[INPUT_ACTION_EDITOR_TOOL_SQUADS] = event.type == SDL_EVENT_KEY_DOWN;
                                    break;
                                case DELETE:
                                    state.action_is_pressed[INPUT_ACTION_EDITOR_DELETE] = event.type == SDL_EVENT_KEY_DOWN;
                                    break;
                                default:
                                    break;
                            }
                        }
                    }
                #endif

                if (event.type == SDL_EVENT_KEY_DOWN) {
                    state.key_just_pressed = event.key.scancode;
                }
                switch (event.key.scancode) {
                    case SDL_SCANCODE_LSHIFT:
                    case SDL_SCANCODE_RSHIFT:
                        state.action_is_pressed[INPUT_ACTION_SHIFT] = event.type == SDL_EVENT_KEY_DOWN;
                        break;
                    case SDL_SCANCODE_LCTRL:
                    case SDL_SCANCODE_RCTRL:
                #ifdef PLATFORM_MACOS
                    case SDL_SCANCODE_LGUI:
                    case SDL_SCANCODE_RGUI:
                #endif
                        state.action_is_pressed[INPUT_ACTION_CTRL] = event.type == SDL_EVENT_KEY_DOWN;
                        break;
                    case SDL_SCANCODE_SPACE:
                        state.action_is_pressed[INPUT_ACTION_SPACE] = event.type == SDL_EVENT_KEY_DOWN;
                        break;
                #ifdef GOLD_DEBUG
                    case SDL_SCANCODE_F9:
                        state.action_is_pressed[INPUT_ACTION_TURBO] = event.type == SDL_EVENT_KEY_DOWN;
                        break;
                #endif
                    case SDL_SCANCODE_F10:
                        state.action_is_pressed[INPUT_ACTION_MATCH_MENU] = event.type == SDL_EVENT_KEY_DOWN;
                        break;
                    case SDL_SCANCODE_RETURN:
                        state.action_is_pressed[INPUT_ACTION_ENTER] = event.type == SDL_EVENT_KEY_DOWN;
                        break;
                    case SDL_SCANCODE_BACKSPACE: {
                        if (event.type == SDL_EVENT_KEY_DOWN && SDL_TextInputActive(state.window) && state.text_input_str->length() > 0) {
                            state.text_input_str->pop_back();
                        }
                        break;
                    }
                    default: {
                        if (event.key.scancode >= SDL_SCANCODE_1 && event.key.scancode <= SDL_SCANCODE_0) {
                            int key_index = event.key.scancode - SDL_SCANCODE_1;
                            state.action_is_pressed[INPUT_ACTION_NUM1 + key_index] = event.type == SDL_EVENT_KEY_DOWN;
                        } else if (event.key.scancode >= SDL_SCANCODE_F1 && event.key.scancode <= SDL_SCANCODE_F6) {
                            int key_index = event.key.scancode - SDL_SCANCODE_F1;
                            state.action_is_pressed[INPUT_ACTION_F1 + key_index] = event.type == SDL_EVENT_KEY_DOWN;
                        } else {
                            for (uint32_t hotkey = 0; hotkey < INPUT_HOTKEY_COUNT; hotkey++) {
                                if (event.key.scancode == state.hotkey_mapping[hotkey]) {
                                    state.hotkey_is_pressed[hotkey] = event.type == SDL_EVENT_KEY_DOWN;
                                }
                            }
                        }
                        break;
                    }
                }
                break;
            }
            case SDL_EVENT_TEXT_INPUT: {
                (*state.text_input_str) += std::string(event.text.text);
                if (state.text_input_str->length() > state.text_input_max_length) {
                    (*state.text_input_str) = state.text_input_str->substr(0, state.text_input_max_length);
                }
                break;
            }
            case SDL_EVENT_MOUSE_WHEEL: {
                state.mouse_scroll = event.wheel.integer_y;
                break;
            }
            default:
                break;
        }
    }
}

void input_start_text_input(std::string* str, size_t max_length) {
    SDL_StartTextInput(state.window);
    state.text_input_str = str;
    state.text_input_max_length = max_length;
}

void input_stop_text_input() {
    SDL_StopTextInput(state.window);
    state.text_input_str = NULL;
}

bool input_is_text_input_active() {
    return SDL_TextInputActive(state.window);
}

ivec2 input_get_mouse_position() {
    return state.mouse_position;
}

bool input_user_requests_exit() {
    return state.user_requests_exit;
}

int input_get_mouse_scroll() {
    return state.mouse_scroll;
}

bool input_is_action_pressed(InputAction action) {
    return state.action_is_pressed[action];
}

bool input_is_action_just_pressed(InputAction action) {
    return state.action_is_pressed[action] && !state.action_was_pressed[action];
}

bool input_is_action_just_released(InputAction action) {
    return state.action_was_pressed[action] && !state.action_is_pressed[action];
}

bool input_is_hotkey_pressed(InputHotkey hotkey) {
    return state.hotkey_is_pressed[hotkey];
}

bool input_is_hotkey_just_pressed(InputHotkey hotkey) {
    return state.hotkey_is_pressed[hotkey] && !state.hotkey_was_pressed[hotkey];
}

bool input_is_hotkey_just_released(InputHotkey hotkey) {
    return state.hotkey_was_pressed[hotkey] && !state.hotkey_is_pressed[hotkey];
}

int input_sprintf_sdl_scancode_str(char* str_ptr, SDL_Scancode scancode) {
    if (scancode == SDL_SCANCODE_ESCAPE) {
        return sprintf(str_ptr, "ESC");
    }
    if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z) {
        uint8_t letter_index = (uint8_t)scancode - (uint8_t)SDL_SCANCODE_A;
        return sprintf(str_ptr, "%c", (char)((uint8_t)'A' + letter_index));
    }
    if (scancode >= SDL_SCANCODE_MINUS && scancode <= SDL_SCANCODE_SLASH) {
        static const char* scancode_chars = "-=[]\\\\;'~,./";
        return sprintf(str_ptr, "%c", scancode_chars[scancode - SDL_SCANCODE_MINUS]);
    }
    return 0;
}

SDL_Scancode input_get_hotkey_mapping(InputHotkey hotkey) {
    return state.hotkey_mapping[hotkey];
}

void input_set_hotkey_mapping(InputHotkey hotkey, SDL_Scancode key) {
    state.hotkey_mapping[hotkey] = key;
}

void input_set_hotkey_mapping_to_default(SDL_Scancode* hotkey_mapping) {
    // Unit
    hotkey_mapping[INPUT_HOTKEY_ATTACK] = SDL_SCANCODE_A;
    hotkey_mapping[INPUT_HOTKEY_STOP] = SDL_SCANCODE_S;
    hotkey_mapping[INPUT_HOTKEY_DEFEND] = SDL_SCANCODE_D;

    // Miner
    hotkey_mapping[INPUT_HOTKEY_BUILD] = SDL_SCANCODE_B;
    hotkey_mapping[INPUT_HOTKEY_BUILD2] = SDL_SCANCODE_V;
    hotkey_mapping[INPUT_HOTKEY_REPAIR] = SDL_SCANCODE_R;

    // Unload
    hotkey_mapping[INPUT_HOTKEY_UNLOAD] = SDL_SCANCODE_X;

    // Esc
    hotkey_mapping[INPUT_HOTKEY_CANCEL] = SDL_SCANCODE_ESCAPE;

    // Build 1
    hotkey_mapping[INPUT_HOTKEY_HALL] = SDL_SCANCODE_T;
    hotkey_mapping[INPUT_HOTKEY_HOUSE] = SDL_SCANCODE_E;
    hotkey_mapping[INPUT_HOTKEY_SALOON] = SDL_SCANCODE_S;
    hotkey_mapping[INPUT_HOTKEY_BUNKER] = SDL_SCANCODE_B;
    hotkey_mapping[INPUT_HOTKEY_WORKSHOP] = SDL_SCANCODE_W;

    // Build 2
    hotkey_mapping[INPUT_HOTKEY_SMITH] = SDL_SCANCODE_S;
    hotkey_mapping[INPUT_HOTKEY_COOP] = SDL_SCANCODE_C;
    hotkey_mapping[INPUT_HOTKEY_BARRACKS] = SDL_SCANCODE_B;
    hotkey_mapping[INPUT_HOTKEY_SHERIFFS] = SDL_SCANCODE_E;

    // Hall
    hotkey_mapping[INPUT_HOTKEY_MINER] = SDL_SCANCODE_E;

    // Saloon
    hotkey_mapping[INPUT_HOTKEY_COWBOY] = SDL_SCANCODE_C;
    hotkey_mapping[INPUT_HOTKEY_BANDIT] = SDL_SCANCODE_B;

    // Workshop
    hotkey_mapping[INPUT_HOTKEY_SAPPER] = SDL_SCANCODE_S;
    hotkey_mapping[INPUT_HOTKEY_PYRO] = SDL_SCANCODE_R;
    hotkey_mapping[INPUT_HOTKEY_BALLOON] = SDL_SCANCODE_B;
    hotkey_mapping[INPUT_HOTKEY_RESEARCH_LANDMINES] = SDL_SCANCODE_E;

    // Coop
    hotkey_mapping[INPUT_HOTKEY_WAGON] = SDL_SCANCODE_W;
    hotkey_mapping[INPUT_HOTKEY_JOCKEY] = SDL_SCANCODE_C;

    // Smith
    hotkey_mapping[INPUT_HOTKEY_RESEARCH_WAGON_ARMOR] = SDL_SCANCODE_W;
    hotkey_mapping[INPUT_HOTKEY_RESEARCH_BAYONETS] = SDL_SCANCODE_B;
    hotkey_mapping[INPUT_HOTKEY_RESEARCH_GETAWAY_BOOTS] = SDL_SCANCODE_G;
    hotkey_mapping[INPUT_HOTKEY_RESEARCH_IRON_SIGHTS] = SDL_SCANCODE_S;

    // Barracks
    hotkey_mapping[INPUT_HOTKEY_SOLDIER] = SDL_SCANCODE_S;
    hotkey_mapping[INPUT_HOTKEY_CANNON] = SDL_SCANCODE_C;

    // Sheriff's Office
    hotkey_mapping[INPUT_HOTKEY_DETECTIVE] = SDL_SCANCODE_D;
    hotkey_mapping[INPUT_HOTKEY_RESEARCH_PRIVATE_EYE] = SDL_SCANCODE_E;

    // Pyro
    hotkey_mapping[INPUT_HOTKEY_MOLOTOV] = SDL_SCANCODE_V;
    hotkey_mapping[INPUT_HOTKEY_LANDMINE] = SDL_SCANCODE_E;

    // Detective
    hotkey_mapping[INPUT_HOTKEY_CAMO] = SDL_SCANCODE_C;

    // Misc
    hotkey_mapping[INPUT_HOTKEY_IDLE_MINER] = SDL_SCANCODE_GRAVE;
}

SDL_Scancode input_get_key_just_pressed() {
    return state.key_just_pressed;
}

bool input_is_key_valid_hotkey_mapping(SDL_Scancode key) {
    return key == SDL_SCANCODE_ESCAPE ||
                (key >= SDL_SCANCODE_A && key <= SDL_SCANCODE_Z) ||
                (key >= SDL_SCANCODE_MINUS && key <= SDL_SCANCODE_SLASH);
}
