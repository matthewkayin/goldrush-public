#include "editor.h"

#include "defines.h"

#ifdef GOLD_DEBUG

#include "core/input.h"
#include "core/options.h"
#include "render/render.h"
#include "editor/state.h"

static EditorState* state;

void editor_init(SDL_Window* window) {
    input_set_mouse_capture_enabled(false);
    if (option_get_value(OPTION_DISPLAY) != RENDER_DISPLAY_WINDOWED) {
        option_set_value(OPTION_DISPLAY, RENDER_DISPLAY_WINDOWED);
    }
    state = editor_state_init(window);

    log_info("Initialized map editor.");
}

void editor_quit() {
    editor_state_free(state);
}

void editor_update() {
    editor_state_update(state);
}

bool editor_requests_playtest() {
    return state->is_requesting_playtest;
}

void editor_end_playtest() {
    editor_state_end_playtest(state);
}

const Scenario* editor_get_scenario() {
    return state->scenario;
}

std::string editor_get_scenario_script_path() {
    return scenario_get_script_path(state->scenario_path.c_str());
}

void editor_render() {
    editor_state_render(state);
}

#else

void editor_init(SDL_Window* /*window*/) {}
void editor_quit() {}
void editor_update() {}

bool editor_requests_playtest() { return false; }
void editor_begin_playtest() {}
void editor_end_playtest() {}
const Scenario* editor_get_scenario() { return NULL; }
std::string editor_get_scenario_script_path() { return ""; }

void editor_render() {}

#endif
